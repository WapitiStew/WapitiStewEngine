// @file AsyncLogRuntime.cpp
// @brief Explicit asynchronous logging ownership, transactional configuration and bounded delivery barriers.
#include "../../../api/wse/utility/AsyncLogRuntime.h"
#if defined(_WIN32)
#include "../../../platform/wse/win/utility/wse_LogOutput.h"
#else
#include "../../../platform/wse/linux/utility/wse_LogOutput.h"
#endif
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace wse
{
    namespace
    {
        thread_local const void* g_async_writer = nullptr;
        std::mutex g_async_console_mutex;

        LogStatus failure( const eLogErrorCategory category_in, const eLogErrorCode code_in,
            const std::string& message_in = {} )
        { return LogStatus::failure( LogError( category_in, code_in, message_in ) ); }

        bool hasFile( const eLogMode mode_in )
        { return mode_in == eLogMode::LOG_FILE || mode_in == eLogMode::FULL; }
        bool hasConsole( const eLogMode mode_in )
        { return mode_in == eLogMode::CONSOLE || mode_in == eLogMode::FULL; }

        bool addBytes( std::size_t* p_total_inout, const std::size_t count_in, const std::size_t limit_in )
        {
            auto& total = *p_total_inout;
            if( count_in > limit_in - total ) { return false; }
            total += count_in;
            return true;
        }

        bool validRecord( const sAsyncLogRecord& record_in, const sAsyncLogSettings& settings_in )
        {
            if( record_in.parts.size() > settings_in.record_part_limit ||
                record_in.attributes.size() > settings_in.record_part_limit ) { return false; }
            std::size_t bytes = 0;
            for( const auto& part : record_in.parts )
            {
                if( part.kind == eLogPartKind::Text )
                {
                    if( !addBytes( &bytes, part.text.size(), settings_in.record_byte_limit ) ) { return false; }
                }
                else if( part.kind == eLogPartKind::NewLine && part.text.empty() )
                {
                    if( !addBytes( &bytes, END_LINE().size(), settings_in.record_byte_limit ) ) { return false; }
                }
                else { return false; }
            }
            bytes = 0;
            if( !addBytes( &bytes, record_in.tag.size(), settings_in.metadata_byte_limit ) ||
                !addBytes( &bytes, record_in.source.size(), settings_in.metadata_byte_limit ) ||
                !addBytes( &bytes, record_in.file.size(), settings_in.metadata_byte_limit ) ) { return false; }
            for( const auto& attribute : record_in.attributes )
            {
                if( !addBytes( &bytes, attribute.first.size(), settings_in.metadata_byte_limit ) ||
                    !addBytes( &bytes, attribute.second.size(), settings_in.metadata_byte_limit ) ) { return false; }
            }
            return true;
        }

        bool validSettings( const sAsyncLogSettings& settings_in )
        {
            if( static_cast<unsigned>( settings_in.output ) > 3 ||
                static_cast<unsigned>( settings_in.minimum_level ) > static_cast<unsigned>( LogLevel::Off ) ||
                static_cast<unsigned>( settings_in.separate ) > 4 ||
                static_cast<unsigned>( settings_in.prefix ) > 3 ||
                static_cast<unsigned>( settings_in.suffix ) > 1 ||
                settings_in.queue_capacity == 0 || settings_in.record_byte_limit == 0 ||
                settings_in.record_part_limit == 0 || settings_in.metadata_byte_limit == 0 ||
                ( hasFile( settings_in.output ) && settings_in.file_path.empty() ) ) { return false; }
            for( const auto& sink : settings_in.sinks ) { if( !sink ) { return false; } }
            return true;
        }

        std::string timestampPrefix( const sAsyncLogRecord& record_in, const eLogPrefix prefix_in )
        {
            if( prefix_in == eLogPrefix::NONE ) { return {}; }
            const auto time = std::chrono::system_clock::to_time_t( record_in.timestamp );
            std::tm local{};
#if defined(_WIN32)
            localtime_s( &local, &time );
#else
            localtime_r( &time, &local );
#endif
            std::ostringstream stream;
            stream << "[ ";
            if( prefix_in == eLogPrefix::DATE || prefix_in == eLogPrefix::FULL )
            { stream << std::put_time( &local, "%y/%m/%d" ); }
            if( prefix_in == eLogPrefix::FULL ) { stream << '-'; }
            if( prefix_in == eLogPrefix::TIME || prefix_in == eLogPrefix::FULL )
            {
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    record_in.timestamp.time_since_epoch() ).count() % 1000;
                if( milliseconds < 0 ) { milliseconds += 1000; }
                stream << std::put_time( &local, "%H:%M:%S" ) << '.' << std::setw( 3 )
                       << std::setfill( '0' ) << milliseconds;
            }
            stream << " ]";
            return stream.str();
        }

        std::string defaultFormat( const sAsyncLogRecord& record_in, const sAsyncLogSettings& settings_in )
        {
            // Reuse the standard filename/line layout while fixing the timestamp at production time.
            const auto safe_tag = renderLogParts( { { eLogPartKind::Text, record_in.tag } } );
            const auto separator = record_in.file.find_last_of( "/\\" );
            const auto basename = separator == std::string::npos ? record_in.file : record_in.file.substr( separator + 1 );
            const auto safe_file = renderLogParts( { { eLogPartKind::Text, basename } } );
            const sLogConfig config{ safe_tag, eLogMode::NONE, settings_in.separate,
                eLogPrefix::NONE, settings_in.suffix };
            std::ostringstream location;
            if( !record_in.file.empty() ) { location << "( " << safe_file << " ) "; }
            if( record_in.line != 0 ) { location << std::left << std::setw( 6 ) << record_in.line; }
            auto text = formatLogMessage( config, location.str() + renderLogParts( record_in.parts ), {}, 0 );
            text.insert( safe_tag.size() + 2, timestampPrefix( record_in, settings_in.prefix ) );
            return text;
        }
    }

    std::string renderLogParts( const std::vector<sLogPart>& parts_in )
    {
        std::string text;
        for( const auto& part : parts_in )
        {
            if( part.kind == eLogPartKind::NewLine ) { text += END_LINE(); continue; }
            for( const auto character : part.text )
            {
                switch( character )
                {
                    case '\n': text += "\\n"; break;
                    case '\r': text += "\\r"; break;
                    case '\0': text += "\\0"; break;
                    default: text += character; break;
                }
            }
        }
        return text;
    }

    class AsyncLogRuntime::Impl
    {
    public:
        struct Barrier { bool done = false; LogError error; };
        struct Entry { sAsyncLogRecord record; std::shared_ptr<Barrier> barrier; };
        mutable std::mutex mutex;
        std::mutex control;
        std::condition_variable changed;
        std::deque<Entry> queue;
        std::thread worker;
        sAsyncLogSettings settings;
        sAsyncLogStatus status;
        std::ofstream file;
        bool stopping = false;
        bool writer_failed = false;

        bool enabled( const std::string& tag_in, const unsigned verbosity_in, const LogLevel level_in ) const
        {
            if( !this->status.is_accepting || verbosity_in == 0 ||
                static_cast<unsigned>( level_in ) >= static_cast<unsigned>( LogLevel::Off ) ||
                level_in < this->settings.minimum_level ||
                ( this->settings.output == eLogMode::NONE && this->settings.sinks.empty() ) ) { return false; }
            const auto found = this->settings.tag_verbosity.find( tag_in );
            const auto maximum = found == this->settings.tag_verbosity.end()
                ? this->settings.default_verbosity : found->second;
            return verbosity_in <= maximum;
        }

        LogStatus write( const sAsyncLogRecord& record_in )
        {
            std::string text;
            try { text = this->settings.formatter ? this->settings.formatter( record_in )
                : defaultFormat( record_in, this->settings ); }
            catch( ... ) { return failure( eLogErrorCategory::Callback, eLogErrorCode::CallbackFailed ); }
            if( hasConsole( this->settings.output ) )
            {
                if( this->settings.console_writer )
                {
                    try
                    {
                        const auto result = this->settings.console_writer( text );
                        if( !result.succeeded() ) { return result; }
                    }
                    catch( ... ) { return failure( eLogErrorCategory::Callback, eLogErrorCode::CallbackFailed ); }
                }
                else
                {
                    std::lock_guard<std::mutex> lock( g_async_console_mutex );
                    log::outputConsole( text );
                    if( std::cout.fail() ) { return failure( eLogErrorCategory::Io, eLogErrorCode::WriteFailed ); }
                }
            }
            if( hasFile( this->settings.output ) )
            {
                this->file << text;
                if( this->file.fail() ) { return failure( eLogErrorCategory::Io, eLogErrorCode::WriteFailed ); }
            }
            for( const auto& sink : this->settings.sinks )
            {
                try
                {
                    const auto result = sink->write( record_in, text );
                    if( !result.succeeded() ) { return result; }
                }
                catch( ... ) { return failure( eLogErrorCategory::Callback, eLogErrorCode::CallbackFailed ); }
            }
            return this->settings.flush_each_record ? this->flushSinks() : LogStatus::success();
        }

        LogStatus flushSinks()
        {
            if( hasConsole( this->settings.output ) && !this->settings.console_writer )
            {
                std::lock_guard<std::mutex> lock( g_async_console_mutex );
                std::cout.flush();
                if( std::cout.fail() ) { return failure( eLogErrorCategory::Io, eLogErrorCode::FlushFailed ); }
            }
            if( hasFile( this->settings.output ) )
            {
                this->file.flush();
                if( this->file.fail() ) { return failure( eLogErrorCategory::Io, eLogErrorCode::FlushFailed ); }
            }
            for( const auto& sink : this->settings.sinks )
            {
                try
                {
                    const auto result = sink->flush();
                    if( !result.succeeded() ) { return result; }
                }
                catch( ... ) { return failure( eLogErrorCategory::Callback, eLogErrorCode::CallbackFailed ); }
            }
            return LogStatus::success();
        }

        void run() noexcept
        {
            g_async_writer = this;
            std::unique_lock<std::mutex> lock( this->mutex );
            while( true )
            {
                while( this->queue.empty() && !this->stopping ) { this->changed.wait( lock ); }
                if( this->queue.empty() && this->stopping ) { break; }
                auto entry = std::move( this->queue.front() );
                this->queue.pop_front();
                if( !entry.barrier ) { --this->status.queued; }
                const bool skip = this->writer_failed && !entry.barrier;
                lock.unlock();
                LogError error;
                try
                {
                    const auto result = entry.barrier ? this->flushSinks()
                        : skip ? LogStatus::success() : this->write( entry.record );
                    if( !result.succeeded() ) { error = result.error(); }
                }
                catch( ... ) { error = LogError( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
                lock.lock();
                if( !error.ok() )
                {
                    this->status.last_error = std::move( error );
                    this->writer_failed = true;
                }
                if( entry.barrier )
                {
                    if( this->writer_failed )
                    {
                        try { entry.barrier->error = this->status.last_error; }
                        catch( ... ) { entry.barrier->error = LogError( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
                    }
                    entry.barrier->done = true;
                }
                else
                {
                    ++this->status.completed;
                    if( skip || this->writer_failed ) { ++this->status.dropped; }
                }
                this->changed.notify_all();
            }
            g_async_writer = nullptr;
        }

        LogStatus barrier()
        {
            auto barrier = std::make_shared<Barrier>();
            std::unique_lock<std::mutex> lock( this->mutex );
            if( !this->status.is_running )
            { return !this->writer_failed ? LogStatus::success() : LogStatus::failure( this->status.last_error ); }
            // The one control operation is outside producer capacity and remains bounded by the control mutex.
            this->queue.push_back( { {}, barrier } );
            this->changed.notify_all();
            while( !barrier->done ) { this->changed.wait( lock ); }
            return barrier->error.ok() ? LogStatus::success() : LogStatus::failure( barrier->error );
        }
    };

    void AsyncLogRuntime::deleteImpl( Impl* p_impl_in ) { delete p_impl_in; }
    AsyncLogRuntime::AsyncLogRuntime() : m_impl( new Impl(), &AsyncLogRuntime::deleteImpl ) {}
    AsyncLogRuntime::~AsyncLogRuntime() { try { (void)this->stop(); } catch( ... ) {} }

    LogStatus AsyncLogRuntime::configure( const sAsyncLogSettings& settings_in )
    {
        if( g_async_writer == this->m_impl.get() )
        { return failure( eLogErrorCategory::Lifecycle, eLogErrorCode::ReentrantControl ); }
        if( !validSettings( settings_in ) )
        { return failure( eLogErrorCategory::Configuration, eLogErrorCode::InvalidSettings ); }
        auto& impl = *this->m_impl;
        sAsyncLogSettings candidate;
        sAsyncLogSettings released;
        std::ofstream candidate_file;
        std::shared_ptr<Impl::Barrier> barrier;
        try
        {
            candidate = settings_in;
            barrier = std::make_shared<Impl::Barrier>();
            if( hasFile( candidate.output ) )
            {
                const auto path = std::filesystem::u8path( candidate.file_path );
                if( !path.parent_path().empty() ) { std::filesystem::create_directories( path.parent_path() ); }
                candidate_file.open( path, std::ios::binary | std::ios::app );
                if( !candidate_file ) { return failure( eLogErrorCategory::Io, eLogErrorCode::OpenFailed, candidate.file_path ); }
            }
        }
        catch( const std::bad_alloc& ) { return failure( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
        catch( ... ) { return failure( eLogErrorCategory::Io, eLogErrorCode::OpenFailed ); }
        // Candidate copy/open and its rollback destruction cannot execute user capture code under a runtime lock.
        std::unique_lock<std::mutex> control( impl.control );
        std::unique_lock<std::mutex> lock( impl.mutex );
        // Allocate the control queue slot before changing acceptance or starting a worker.
        try { impl.queue.push_back( { {}, barrier } ); }
        catch( ... ) { return failure( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
        if( !impl.status.is_running )
        {
            impl.stopping = false;
            try { impl.worker = std::thread( &Impl::run, &impl ); }
            catch( ... )
            {
                impl.queue.pop_back();
                return failure( eLogErrorCategory::Internal, eLogErrorCode::ThreadFailed );
            }
            impl.status.is_running = true;
        }
        // No accepted old work may observe the new formatter, settings or stream.
        impl.status.is_accepting = false;
        impl.changed.notify_all();
        while( !barrier->done ) { impl.changed.wait( lock ); }
        released = std::move( impl.settings );
        impl.settings = std::move( candidate );
        impl.file = std::move( candidate_file );
        impl.status.last_error = LogError();
        impl.writer_failed = false;
        impl.status.is_accepting = true;
        lock.unlock();
        control.unlock();
        return LogStatus::success();
    }

    eLogSubmitResult AsyncLogRuntime::tryAppend( const sAsyncLogRecord& record_in ) noexcept
    {
        auto& impl = *this->m_impl;
        try
        {
            std::lock_guard<std::mutex> lock( impl.mutex );
            if( g_async_writer == &impl )
            {
                ++impl.status.dropped;
                if( !impl.writer_failed )
                { impl.status.last_error = LogError( eLogErrorCategory::Lifecycle, eLogErrorCode::ReentrantControl ); }
                return eLogSubmitResult::Failed;
            }
            if( !impl.status.is_accepting ) { return eLogSubmitResult::NotRunning; }
            if( !impl.enabled( record_in.tag, record_in.verbosity, record_in.level ) )
            { ++impl.status.filtered; return eLogSubmitResult::Filtered; }
            if( impl.writer_failed ) { ++impl.status.dropped; return eLogSubmitResult::Failed; }
            if( !validRecord( record_in, impl.settings ) )
            {
                ++impl.status.dropped;
                impl.status.last_error = LogError( eLogErrorCategory::Capacity, eLogErrorCode::RecordTooLarge );
                return eLogSubmitResult::RecordTooLarge;
            }
            if( impl.status.queued >= impl.settings.queue_capacity )
            {
                ++impl.status.dropped;
                impl.status.last_error = LogError( eLogErrorCategory::Capacity, eLogErrorCode::QueueFull );
                return eLogSubmitResult::QueueFull;
            }
            impl.queue.push_back( { record_in, {} } );
            ++impl.status.queued;
            ++impl.status.accepted;
            impl.changed.notify_all();
            return eLogSubmitResult::Accepted;
        }
        catch( ... )
        {
            try
            {
                std::lock_guard<std::mutex> lock( impl.mutex );
                ++impl.status.dropped;
                if( !impl.writer_failed )
                { impl.status.last_error = LogError( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
            }
            catch( ... ) {}
            return eLogSubmitResult::Failed;
        }
    }

    bool AsyncLogRuntime::isEnabled( const std::string& tag_in, const unsigned verbosity_in, const LogLevel level_in ) const
    { std::lock_guard<std::mutex> lock( this->m_impl->mutex ); return this->m_impl->enabled( tag_in, verbosity_in, level_in ); }
    sAsyncLogSettings AsyncLogRuntime::getSettings() const
    { std::lock_guard<std::mutex> lock( this->m_impl->mutex ); return this->m_impl->settings; }
    sAsyncLogStatus AsyncLogRuntime::getStatus() const
    { std::lock_guard<std::mutex> lock( this->m_impl->mutex ); return this->m_impl->status; }
    LogStatus AsyncLogRuntime::flush()
    {
        if( g_async_writer == this->m_impl.get() )
        { return failure( eLogErrorCategory::Lifecycle, eLogErrorCode::ReentrantControl ); }
        std::lock_guard<std::mutex> control( this->m_impl->control );
        return this->m_impl->barrier();
    }
    LogStatus AsyncLogRuntime::stop()
    {
        if( g_async_writer == this->m_impl.get() )
        { return failure( eLogErrorCategory::Lifecycle, eLogErrorCode::ReentrantControl ); }
        auto& impl = *this->m_impl;
        std::unique_lock<std::mutex> control( impl.control );
        {
            std::lock_guard<std::mutex> lock( impl.mutex );
            impl.status.is_accepting = false;
        }
        LogStatus result = LogStatus::success();
        try { result = impl.barrier(); }
        catch( ... ) { result = failure( eLogErrorCategory::Internal, eLogErrorCode::AllocationFailed ); }
        {
            std::lock_guard<std::mutex> lock( impl.mutex );
            impl.stopping = true;
            impl.changed.notify_all();
        }
        if( impl.worker.joinable() ) { impl.worker.join(); }
        // Destroy captures after the writer has joined, outside the queue mutex.
        sAsyncLogSettings released;
        {
            std::lock_guard<std::mutex> lock( impl.mutex );
            impl.status.is_running = false;
            released.formatter = std::move( impl.settings.formatter );
            released.console_writer = std::move( impl.settings.console_writer );
            released.sinks = std::move( impl.settings.sinks );
            impl.file.close();
        }
        control.unlock();
        return result;
    }
}
