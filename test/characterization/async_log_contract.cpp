// @file async_log_contract.cpp
// @brief Real runtime and filesystem contracts; controlled sinks replace external devices.
#include "wse/utility/LogMessageBuilder.h"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
    int g_failures = 0;
    void expect( const bool condition_in, const char* p_message_in )
    { if( !condition_in ) { std::cerr << p_message_in << '\n'; ++g_failures; } }

    std::string format( const wse::sAsyncLogRecord& record_in )
    { return wse::renderLogParts( record_in.parts ); }

    class Sink final : public wse::AsyncLogSink
    {
    public:
        std::mutex mutex;
        std::condition_variable changed;
        bool block = false;
        bool entered = false;
        bool released = false;
        bool fail = false;
        unsigned flushes = 0;
        std::vector<std::string> texts;
        std::vector<std::thread::id> threads;
        wse::AsyncLogRuntime* p_runtime = nullptr;
        bool reentry_rejected = false;

        wse::LogStatus write( const wse::sAsyncLogRecord& record_in, const std::string& text_in ) override
        {
            std::unique_lock<std::mutex> lock( this->mutex );
            this->entered = true;
            this->changed.notify_all();
            while( this->block && !this->released ) { this->changed.wait( lock ); }
            this->texts.push_back( text_in );
            this->threads.push_back( std::this_thread::get_id() );
            if( this->p_runtime )
            {
                const auto result = this->p_runtime->flush();
                this->reentry_rejected = !result.succeeded() &&
                    result.error().code() == wse::eLogErrorCode::ReentrantControl &&
                    this->p_runtime->tryAppend( record_in ) == wse::eLogSubmitResult::Failed;
            }
            return this->fail ? wse::LogStatus::failure( wse::LogError(
                wse::eLogErrorCategory::Io, wse::eLogErrorCode::WriteFailed, "injected" ) )
                : wse::LogStatus::success();
        }
        wse::LogStatus flush() override
        { std::lock_guard<std::mutex> lock( this->mutex ); ++this->flushes; return wse::LogStatus::success(); }
        bool waitEntered()
        {
            std::unique_lock<std::mutex> lock( this->mutex );
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
            while( !this->entered )
            { if( this->changed.wait_until( lock, deadline ) == std::cv_status::timeout ) { return this->entered; } }
            return true;
        }
        void release()
        { std::lock_guard<std::mutex> lock( this->mutex ); this->released = true; this->changed.notify_all(); }
    };

    wse::sAsyncLogSettings settings( const std::shared_ptr<Sink>& sink_in )
    {
        wse::sAsyncLogSettings value;
        value.sinks.push_back( sink_in );
        value.formatter = &format;
        return value;
    }
    wse::sAsyncLogRecord record( const std::string& text_in )
    {
        wse::sAsyncLogRecord value;
        value.tag = "contract";
        value.parts.push_back( { wse::eLogPartKind::Text, text_in } );
        return value;
    }

    void builderContract()
    {
        // The expected numeric and enum strings use the same public primitives as the synchronous logger.
        const wse::eLogSeparate separators[] = { wse::eLogSeparate::COMMA, wse::eLogSeparate::SPACE,
            wse::eLogSeparate::TAB, wse::eLogSeparate::COLON, wse::eLogSeparate::SEMICOLON };
        enum class Byte : std::uint8_t { Value = 255 };
        for( const auto separator : separators )
        {
            wse::LogMessageBuilder builder( separator );
            builder << "x=" << 42 << true << Byte::Value << "" << 1.25 << '\n';
            const auto sep = wse::getLogSeparator( separator );
            expect( wse::renderLogParts( builder.parts() ) == sep + "x=" + sep + "42" + sep + "true" +
                sep + "0xff" + sep + sep + std::to_string( 1.25 ) + sep + "10", "separator/value compatibility" );
        }
        wse::LogMessageBuilder lines;
        lines << "A\n\r" << wse::eLogControl::NewLine << "B" << wse::eLogControl::NewLine;
        expect( wse::renderLogParts( lines.parts() ) == " A\\n\\r" + wse::END_LINE() + " B" + wse::END_LINE(),
            "text newlines are distinct from typed controls" );
        wse::LogMessageBuilder exact( wse::eLogSeparate::SPACE, 4 );
        exact << "123";
        expect( exact.isValid() && exact.byteCount() == 4, "exact byte limit" );
        exact << "";
        expect( !exact.isValid(), "builder rejects over-limit whole record" );
        wse::LogMessageBuilder alias;
        alias << std::string( 70, 'x' );
        const auto previous = alias.parts().back().text;
        alias.append( std::string_view( alias.parts().back().text ) );
        expect( alias.parts().back().text == previous + " " + previous, "aliased text survives buffer reallocation" );
        wse::LogMessageBuilder edges;
        edges << wse::eLogControl::NewLine << wse::eLogControl::NewLine << "" << wse::eLogControl::NewLine;
        expect( wse::renderLogParts( edges.parts() ) == wse::END_LINE() + wse::END_LINE() + " " + wse::END_LINE(),
            "leading, trailing and consecutive explicit controls" );
        wse::LogMessageBuilder wide;
        wide << std::wstring( L"\u65e5\u672c" ) << std::wstring( L"A\0B", 3 );
        expect( wse::renderLogParts( wide.parts() ) == std::string( u8" \u65e5\u672c A\\0B" ), "wide UTF-8 and embedded NUL" );
        wse::ILogger::registerLogConfig( { "wide-contract", wse::eLogMode::NONE } );
        expect( wse::ILogger::getMessageTip( std::wstring( L"\u65e5\u672c" ), "wide-contract" ) == u8" \u65e5\u672c",
            "synchronous wide UTF-8 conversion remains compatible" );
        wse::ILogger::removeLogConfig( "wide-contract" );
        wse::LogMessageBuilder pointer_limit( wse::eLogSeparate::SPACE, 4 );
        pointer_limit << L"12345";
        expect( !pointer_limit.isValid(), "wide pointer rejects over-limit before copying" );
    }

    void queueAndLifetimeContract()
    {
        wse::AsyncLogRuntime runtime;
        auto sink = std::make_shared<Sink>();
        sink->block = true;
        auto config = settings( sink );
        config.queue_capacity = 2;
        expect( runtime.configure( config ).succeeded(), "configure bounded sink" );
        auto value = record( "owned" );
        expect( runtime.tryAppend( value ) == wse::eLogSubmitResult::Accepted, "accept first record" );
        value.parts.front().text = "changed";
        expect( sink->waitEntered(), "writer entered controlled sink" );
        expect( runtime.tryAppend( record( "second" ) ) == wse::eLogSubmitResult::Accepted, "first pending slot" );
        expect( runtime.tryAppend( record( "third" ) ) == wse::eLogSubmitResult::Accepted, "second pending slot" );
        expect( runtime.tryAppend( record( "fourth" ) ) == wse::eLogSubmitResult::QueueFull, "bounded rejection" );
        sink->release();
        expect( runtime.flush().succeeded(), "capacity error does not poison writer flush" );
        const auto first_thread = sink->threads.front();
        expect( sink->texts.size() == 3 && sink->texts.front() == "owned", "accepted record owns a finite copy" );
        expect( runtime.getStatus().accepted == 3 && runtime.getStatus().completed == 3 &&
            runtime.getStatus().dropped == 1, "accepted/completed/dropped accounting" );
        config.tag_verbosity["contract"] = 2;
        expect( runtime.configure( config ).succeeded() && runtime.getStatus().last_error.ok(), "successful reconfigure clears diagnostic" );
        { wse::AsyncLogLine( &runtime, "contract", 2 ) << "next" << wse::eLogControl::NewLine << false; }
        expect( runtime.flush().succeeded(), "RAII frontend flush" );
        expect( sink->threads.back() == first_thread, "reconfigure reuses writer" );
        expect( sink->texts.back() == " next" + wse::END_LINE() + " false", "RAII frontend renders typed newline" );
        expect( runtime.stop().succeeded() && runtime.stop().succeeded(), "idempotent drain/stop" );
        expect( runtime.getSettings().sinks.empty() && !runtime.getSettings().formatter,
            "stop releases callback and sink captures" );
        expect( runtime.tryAppend( record( "late" ) ) == wse::eLogSubmitResult::NotRunning, "no acceptance after stop" );
    }

    void filteringAndFailureContract()
    {
        wse::AsyncLogRuntime runtime;
        auto sink = std::make_shared<Sink>();
        auto config = settings( sink );
        config.record_byte_limit = 4;
        config.metadata_byte_limit = 64;
        config.record_part_limit = 3;
        config.minimum_level = wse::LogLevel::Warning;
        expect( runtime.configure( config ).succeeded(), "configure filtering" );
        auto value = record( "1234" );
        expect( runtime.tryAppend( value ) == wse::eLogSubmitResult::Filtered, "severity filter" );
        value.level = wse::LogLevel::Error;
        expect( runtime.tryAppend( value ) == wse::eLogSubmitResult::Accepted, "exact record limit" );
        value.parts.front().text += "5";
        expect( runtime.tryAppend( value ) == wse::eLogSubmitResult::RecordTooLarge, "over record limit" );
        value.parts.front().text = "x";
        value.attributes.push_back( { "oversize", std::string( 65, 'x' ) } );
        expect( runtime.tryAppend( value ) == wse::eLogSubmitResult::RecordTooLarge, "bounded metadata" );
        expect( runtime.flush().succeeded(), "flush filtered sequence" );
        config.record_byte_limit = 16384;
        config.minimum_level = wse::LogLevel::Trace;
        sink->fail = true;
        expect( runtime.configure( config ).succeeded(), "prepare controlled writer failure" );
        expect( runtime.tryAppend( record( "fail" ) ) == wse::eLogSubmitResult::Accepted, "accept before I/O failure" );
        const auto result = runtime.flush();
        expect( !result.succeeded() && result.error().code() == wse::eLogErrorCode::WriteFailed, "surface writer error" );
        expect( runtime.tryAppend( record( "after" ) ) == wse::eLogSubmitResult::Failed, "writer failure latches" );
        expect( runtime.tryAppend( record( std::string( 20000, 'x' ) ) ) == wse::eLogSubmitResult::Failed &&
            runtime.getStatus().last_error.code() == wse::eLogErrorCode::WriteFailed,
            "oversized later input preserves the original fatal error" );
        sink->fail = false;
        expect( runtime.configure( config ).succeeded() && runtime.getStatus().last_error.ok(), "reconfigure recovers writer" );
        sink->p_runtime = &runtime;
        expect( runtime.tryAppend( record( "reentry" ) ) == wse::eLogSubmitResult::Accepted, "accept reentry probe" );
        expect( runtime.flush().succeeded() && sink->reentry_rejected, "callback controls and producer recursion rejected" );
        sink->p_runtime = nullptr;
        expect( runtime.stop().succeeded(), "stop recovered runtime" );
    }

    void defaultConsoleContract()
    {
        // Capture the standard WSE stdout destination; the Windows adapter also selects CP_UTF8.
        std::ostringstream captured;
        auto* const p_previous = std::cout.rdbuf( captured.rdbuf() );
        wse::AsyncLogRuntime runtime;
        wse::sAsyncLogSettings config;
        config.output = wse::eLogMode::CONSOLE;
        config.prefix = wse::eLogPrefix::NONE;
        expect( runtime.configure( config ).succeeded(), "default console configuration" );
        { wse::AsyncLogLine( &runtime, "console" ) << u8"\u65e5\u672c" << wse::eLogControl::NewLine << "B"; }
        const auto result = runtime.stop();
        std::cout.rdbuf( p_previous );
        expect( result.succeeded(), "default console stop" );
        expect( captured.str() == std::string( u8"<console> \u65e5\u672c" ) + wse::END_LINE() + " B" + wse::END_LINE(),
            "default console uses WSE stdout with UTF-8 and exact suffix" );
    }

    void fileAndTransactionContract()
    {
        const auto root = std::filesystem::temp_directory_path() / ( "wse-async-" +
            std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
        const auto path = root / std::filesystem::u8path( u8"\u65e5\u672c" ) / "record.log";
        wse::AsyncLogRuntime runtime;
        wse::sAsyncLogSettings config;
        config.output = wse::eLogMode::LOG_FILE;
        config.file_path = path.u8string();
        config.prefix = wse::eLogPrefix::NONE;
        expect( runtime.configure( config ).succeeded(), "Unicode file preparation" );
        { wse::AsyncLogLine( &runtime, "file" ) << "A\n" << wse::eLogControl::NewLine << "B"; }
        auto invalid = config;
        invalid.file_path = ( path / "not-a-directory" ).u8string();
        expect( !runtime.configure( invalid ).succeeded() && runtime.getSettings().file_path == config.file_path,
            "failed candidate preserves configuration" );
        expect( runtime.flush().succeeded(), "file barrier" );
        expect( runtime.stop().succeeded(), "file stop" );
        std::ifstream file( path, std::ios::binary );
        const std::string text( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
        expect( text == "<file> A\\n" + wse::END_LINE() + " B" + wse::END_LINE(),
            "exact suffix with no extra file-adapter LF" );
        file.close();
        config.file_path = ( root / "no-suffix.log" ).u8string();
        config.suffix = wse::eLogSuffix::NONE;
        config.flush_each_record = true;
        expect( runtime.configure( config ).succeeded(), "restart with per-record flush and no suffix" );
        const auto completed = runtime.getStatus().completed;
        auto metadata = record( "text" );
        metadata.tag = "x\ny";
        metadata.file = "folder/a\rb.cpp";
        expect( runtime.tryAppend( metadata ) == wse::eLogSubmitResult::Accepted, "safe metadata record" );
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
        while( runtime.getStatus().completed == completed && std::chrono::steady_clock::now() < deadline )
        { std::this_thread::yield(); }
        expect( runtime.getStatus().completed == completed + 1, "per-record output completed without barrier" );
        std::ifstream live( std::filesystem::u8path( config.file_path ), std::ios::binary );
        const std::string live_text( ( std::istreambuf_iterator<char>( live ) ), std::istreambuf_iterator<char>() );
        expect( live_text == "<x\\ny>( a\\rb.cpp ) text", "per-record file visibility, suffix NONE and escaped metadata" );
        live.close();
        expect( runtime.stop().succeeded(), "stop restarted runtime" );
        std::error_code ignored;
        std::filesystem::remove_all( root, ignored );
    }
}

int main()
{
    builderContract();
    queueAndLifetimeContract();
    filteringAndFailureContract();
    defaultConsoleContract();
    fileAndTransactionContract();
    return g_failures == 0 ? 0 : 1;
}
