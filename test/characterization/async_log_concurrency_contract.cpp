// @file async_log_concurrency_contract.cpp
// @brief \~english Checks concurrent ownership, finite flush barriers and reconfiguration lifetimes without hardware.
// @brief \~japanese 実機を使わず、並行所有・有限Flush Barrier・再設定時の寿命を検証する。
#include "wse/utility/AsyncLogRuntime.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    int g_failures = 0;

    void expect( const bool condition_in, const char* p_description_in )
    {
        if( !condition_in )
        {
            std::cerr << p_description_in << '\n';
            ++g_failures;
        }
    }

    class Gate final
    {
    private:
        std::mutex m_mutex;
        std::condition_variable m_changed;
        bool m_is_entered = false;
        bool m_is_released = false;
    public:
        void enterAndWait()
        {
            std::unique_lock<std::mutex> lock( this->m_mutex );
            this->m_is_entered = true;
            this->m_changed.notify_all();
            while( !this->m_is_released ) { this->m_changed.wait( lock ); }
        }
        bool waitEntered()
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
            std::unique_lock<std::mutex> lock( this->m_mutex );
            while( !this->m_is_entered )
            {
                if( this->m_changed.wait_until( lock, deadline ) == std::cv_status::timeout )
                { return this->m_is_entered; }
            }
            return true;
        }
        void release()
        {
            std::lock_guard<std::mutex> lock( this->m_mutex );
            this->m_is_released = true;
            this->m_changed.notify_all();
        }
    };

    struct sTraceEntry
    {
        std::string tag;
        std::string text;
        std::thread::id writer;
    };

    class Trace final
    {
    private:
        std::mutex m_mutex;
        std::vector<sTraceEntry> m_entries;
    public:
        void record( const wse::sAsyncLogRecord& record_in, const std::string& text_in )
        {
            std::lock_guard<std::mutex> lock( this->m_mutex );
            this->m_entries.push_back( { record_in.tag, text_in, std::this_thread::get_id() } );
        }
        std::vector<sTraceEntry> snapshot()
        {
            std::lock_guard<std::mutex> lock( this->m_mutex );
            return this->m_entries;
        }
    };

    class ControlledSink final : public wse::AsyncLogSink
    {
    private:
        std::shared_ptr<Trace> m_ptr_trace;
        std::shared_ptr<Gate> m_ptr_write_gate;
        std::shared_ptr<Gate> m_ptr_flush_gate;
        std::string m_blocked_text;
    public:
        explicit ControlledSink( const std::shared_ptr<Trace>& p_trace_in,
            const std::shared_ptr<Gate>& p_write_gate_in = {}, const std::string& blocked_text_in = {},
            const std::shared_ptr<Gate>& p_flush_gate_in = {} )
            : m_ptr_trace( p_trace_in ), m_ptr_write_gate( p_write_gate_in ),
              m_ptr_flush_gate( p_flush_gate_in ), m_blocked_text( blocked_text_in ) {}

        wse::LogStatus write( const wse::sAsyncLogRecord& record_in, const std::string& text_in ) override
        {
            if( this->m_ptr_write_gate && text_in == this->m_blocked_text )
            { this->m_ptr_write_gate->enterAndWait(); }
            this->m_ptr_trace->record( record_in, text_in );
            return wse::LogStatus::success();
        }
        wse::LogStatus flush() override
        {
            if( this->m_ptr_flush_gate ) { this->m_ptr_flush_gate->enterAndWait(); }
            return wse::LogStatus::success();
        }
    };

    std::string format( const wse::sAsyncLogRecord& record_in )
    { return wse::renderLogParts( record_in.parts ); }

    wse::sAsyncLogRecord record( const std::string& tag_in, const std::string& text_in )
    {
        wse::sAsyncLogRecord result;
        result.tag = tag_in;
        result.parts.push_back( { wse::eLogPartKind::Text, text_in } );
        return result;
    }

    wse::sAsyncLogSettings settings( const std::shared_ptr<ControlledSink>& p_sink_in )
    {
        wse::sAsyncLogSettings result;
        result.sinks.push_back( p_sink_in );
        result.formatter = &format;
        return result;
    }

    class Completion final
    {
    private:
        std::mutex m_mutex;
        std::condition_variable m_changed;
        bool m_is_done = false;
        bool m_is_succeeded = false;
    public:
        void finish( const bool is_succeeded_in )
        {
            std::lock_guard<std::mutex> lock( this->m_mutex );
            this->m_is_succeeded = is_succeeded_in;
            this->m_is_done = true;
            this->m_changed.notify_all();
        }
        bool waitDone()
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
            std::unique_lock<std::mutex> lock( this->m_mutex );
            while( !this->m_is_done )
            {
                if( this->m_changed.wait_until( lock, deadline ) == std::cv_status::timeout )
                { return this->m_is_done; }
            }
            return true;
        }
        bool isDone()
        { std::lock_guard<std::mutex> lock( this->m_mutex ); return this->m_is_done; }
        bool isSucceeded()
        { std::lock_guard<std::mutex> lock( this->m_mutex ); return this->m_is_done && this->m_is_succeeded; }
    };

    struct sFlushTask
    {
        wse::AsyncLogRuntime* p_runtime;
        Completion* p_completion;
        void operator()() const { this->p_completion->finish( this->p_runtime->flush().succeeded() ); }
    };

    struct sConfigureTask
    {
        wse::AsyncLogRuntime* p_runtime;
        wse::sAsyncLogSettings configuration;
        Completion* p_completion;
        void operator()() const
        { this->p_completion->finish( this->p_runtime->configure( this->configuration ).succeeded() ); }
    };

    struct sContinuousProducer
    {
        wse::AsyncLogRuntime* p_runtime;
        std::atomic<bool>* p_stop;
        std::atomic<unsigned>* p_accepted;
        void operator()() const
        {
            const auto value = record( "ongoing", "late" );
            while( !this->p_stop->load() )
            {
                if( this->p_runtime->tryAppend( value ) == wse::eLogSubmitResult::Accepted )
                { this->p_accepted->fetch_add( 1 ); }
                std::this_thread::yield();
            }
        }
    };

    bool waitPositive( const std::atomic<unsigned>& accepted_in )
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
        while( accepted_in.load() == 0 && std::chrono::steady_clock::now() < deadline )
        { std::this_thread::yield(); }
        return accepted_in.load() > 0;
    }

    void flushBarrierContract()
    {
        // Hold the barrier's sink flush before starting later producers. Their blocked write
        // must not become part of that barrier, even while producers continue to append.
        wse::AsyncLogRuntime runtime;
        const auto p_trace = std::make_shared<Trace>();
        const auto p_late_write = std::make_shared<Gate>();
        const auto p_barrier_flush = std::make_shared<Gate>();
        const auto p_sink = std::make_shared<ControlledSink>( p_trace, p_late_write, "late", p_barrier_flush );
        expect( runtime.configure( settings( p_sink ) ).succeeded(), "barrier configure" );
        expect( runtime.tryAppend( record( "initial", "before" ) ) == wse::eLogSubmitResult::Accepted,
            "barrier accepts prior record" );
        Completion completion;
        std::thread flusher( sFlushTask{ &runtime, &completion } );
        expect( p_barrier_flush->waitEntered(), "writer reaches barrier flush" );
        std::atomic<bool> stop{ false };
        std::atomic<unsigned> accepted{ 0 };
        std::thread producer( sContinuousProducer{ &runtime, &stop, &accepted } );
        expect( waitPositive( accepted ), "later producer is active before barrier release" );
        p_barrier_flush->release();
        expect( p_late_write->waitEntered(), "later accepted write remains controlled" );
        const bool finished_before_late_write = completion.waitDone();
        stop.store( true );
        producer.join();
        p_late_write->release();
        flusher.join();
        expect( finished_before_late_write && completion.isSucceeded(),
            "flush completes without waiting for later continuous producer records" );
        expect( runtime.stop().succeeded(), "barrier runtime final drain" );
        const auto entries = p_trace->snapshot();
        expect( entries.size() > 1 && entries.front().text == "before", "barrier covers the prior record" );
    }

    struct sFiniteProducer
    {
        wse::AsyncLogRuntime* p_runtime;
        unsigned producer;
        unsigned count;
        std::atomic<unsigned>* p_rejected;
        void operator()() const
        {
            auto value = record( std::to_string( this->producer ), "" );
            for( unsigned sequence = 0; sequence < this->count; ++sequence )
            {
                value.parts.front().text = std::to_string( sequence );
                if( this->p_runtime->tryAppend( value ) != wse::eLogSubmitResult::Accepted )
                { this->p_rejected->fetch_add( 1 ); }
                value.parts.front().text = "overwritten after acceptance";
            }
        }
    };

    void parallelOwnershipContract()
    {
        // A total order between independent producers is unspecified. Each producer's
        // accepted sequence must remain ordered and own its original mutable input.
        wse::AsyncLogRuntime runtime;
        const auto p_trace = std::make_shared<Trace>();
        const auto p_sink = std::make_shared<ControlledSink>( p_trace );
        expect( runtime.configure( settings( p_sink ) ).succeeded(), "parallel configure" );
        constexpr unsigned PRODUCERS = 4;
        constexpr unsigned RECORDS = 64;
        std::atomic<unsigned> rejected{ 0 };
        std::vector<std::thread> threads;
        for( unsigned producer = 0; producer < PRODUCERS; ++producer )
        { threads.emplace_back( sFiniteProducer{ &runtime, producer, RECORDS, &rejected } ); }
        for( auto& thread : threads ) { thread.join(); }
        expect( runtime.flush().succeeded(), "parallel producer flush" );
        expect( runtime.stop().succeeded(), "parallel producer stop" );
        const auto entries = p_trace->snapshot();
        expect( rejected.load() == 0 && entries.size() == PRODUCERS * RECORDS, "all parallel records retained" );
        std::array<unsigned, PRODUCERS> next{};
        bool ordered = true;
        bool one_writer = !entries.empty();
        for( const auto& entry : entries )
        {
            const auto producer = static_cast<unsigned>( std::stoul( entry.tag ) );
            if( producer >= PRODUCERS || entry.text != std::to_string( next[producer] ) ) { ordered = false; }
            else { ++next[producer]; }
            one_writer = one_writer && entry.writer == entries.front().writer &&
                entry.writer != std::this_thread::get_id();
        }
        for( const auto count : next ) { ordered = ordered && count == RECORDS; }
        expect( ordered, "producer-local FIFO and owned input copies" );
        expect( one_writer, "all producers share one non-caller writer" );
    }

    bool waitAdmissionClosed( const wse::AsyncLogRuntime& runtime_in, Completion* p_completion_inout )
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
        while( std::chrono::steady_clock::now() < deadline )
        {
            if( !runtime_in.getStatus().is_accepting ) { return true; }
            if( p_completion_inout->isDone() ) { return false; }
            std::this_thread::yield();
        }
        return false;
    }

    struct sReleaseObserver
    {
        wse::AsyncLogRuntime* p_runtime;
        std::atomic<unsigned>* p_observations;
        ~sReleaseObserver()
        {
            // Capture destruction is owner-side cleanup after the configuration switch,
            // not a writer callback. Neither the queue nor control lock may be retained.
            const auto status = this->p_runtime->getStatus();
            const auto flushed = this->p_runtime->flush();
            if( status.is_accepting && flushed.succeeded() ) { this->p_observations->fetch_add( 1 ); }
        }
    };

    struct sReleaseObservingFormatter
    {
        std::shared_ptr<sReleaseObserver> p_capture;
        std::string operator()( const wse::sAsyncLogRecord& record_in ) const
        { return wse::renderLogParts( record_in.parts ); }
    };

    void reconfigurationLifetimeContract()
    {
        // The old write stays in flight while configure closes admission. Its sink must
        // survive until that write and old flush complete, then be released on return.
        wse::AsyncLogRuntime runtime;
        const auto p_old_trace = std::make_shared<Trace>();
        const auto p_new_trace = std::make_shared<Trace>();
        const auto p_gate = std::make_shared<Gate>();
        auto p_old_sink = std::make_shared<ControlledSink>( p_old_trace, p_gate, "old" );
        const std::weak_ptr<ControlledSink> old_weak = p_old_sink;
        std::atomic<unsigned> release_observations{ 0 };
        auto p_release_observer = std::make_shared<sReleaseObserver>();
        p_release_observer->p_runtime = &runtime;
        p_release_observer->p_observations = &release_observations;
        auto old_settings = settings( p_old_sink );
        old_settings.formatter = sReleaseObservingFormatter{ p_release_observer };
        expect( runtime.configure( old_settings ).succeeded(), "old sink configure" );
        old_settings = wse::sAsyncLogSettings();
        p_release_observer.reset();
        expect( runtime.tryAppend( record( "generation", "old" ) ) == wse::eLogSubmitResult::Accepted,
            "accept old generation" );
        expect( p_gate->waitEntered(), "old sink holds in-flight record" );
        p_old_sink.reset();
        const auto p_new_sink = std::make_shared<ControlledSink>( p_new_trace );
        Completion completion;
        std::thread configurator( sConfigureTask{ &runtime, settings( p_new_sink ), &completion } );
        expect( waitAdmissionClosed( runtime, &completion ), "configure closes admission before old drain" );
        expect( !completion.isDone() && !old_weak.expired(), "in-flight old sink survives pending switch" );
        p_gate->release();
        configurator.join();
        expect( completion.isSucceeded() && old_weak.expired(), "successful switch releases the old sink" );
        expect( release_observations.load() == 1, "released captures can observe the runtime without a retained lock" );
        expect( runtime.tryAppend( record( "generation", "new" ) ) == wse::eLogSubmitResult::Accepted,
            "accept new generation" );
        expect( runtime.stop().succeeded(), "new generation stop" );
        const auto old_entries = p_old_trace->snapshot();
        const auto new_entries = p_new_trace->snapshot();
        expect( old_entries.size() == 1 && old_entries.front().text == "old" &&
            new_entries.size() == 1 && new_entries.front().text == "new", "records stay with their accepting sink generation" );
        expect( !old_entries.empty() && !new_entries.empty() && old_entries.front().writer == new_entries.front().writer,
            "sink replacement keeps the existing writer" );
    }

    struct sOwningFormatter
    {
        std::shared_ptr<int> p_capture;
        std::string operator()( const wse::sAsyncLogRecord& record_in ) const
        { return wse::renderLogParts( record_in.parts ); }
    };

    void stopCaptureContract()
    {
        wse::AsyncLogRuntime runtime;
        const auto p_trace = std::make_shared<Trace>();
        auto p_sink = std::make_shared<ControlledSink>( p_trace );
        auto p_capture = std::make_shared<int>( 7 );
        const std::weak_ptr<ControlledSink> sink_weak = p_sink;
        const std::weak_ptr<int> capture_weak = p_capture;
        auto configuration = settings( p_sink );
        configuration.formatter = sOwningFormatter{ p_capture };
        expect( runtime.configure( configuration ).succeeded(), "captured formatter configure" );
        configuration = wse::sAsyncLogSettings();
        p_sink.reset();
        p_capture.reset();
        expect( !sink_weak.expired() && !capture_weak.expired(), "runtime owns callback dependencies" );
        expect( runtime.tryAppend( record( "lifetime", "last" ) ) == wse::eLogSubmitResult::Accepted,
            "accept final record" );
        expect( runtime.stop().succeeded(), "stop drains final record" );
        expect( sink_weak.expired() && capture_weak.expired(), "stop releases sink and formatter captures before return" );
        const auto entries = p_trace->snapshot();
        expect( entries.size() == 1 && entries.front().text == "last", "final output precedes capture release" );
    }
}

int main()
{
    flushBarrierContract();
    parallelOwnershipContract();
    reconfigurationLifetimeContract();
    stopCaptureContract();
    return g_failures == 0 ? 0 : 1;
}
