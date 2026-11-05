// @file AsyncLogTest.cpp
// @brief \~english Links and runs the installed async runtime, typed builder, sink and portable error API.
// @brief \~japanese Install済み非同期Runtime・型付きBuilder・Sink・Portable Error APIをLinkして実行する。
#include "AsyncLogTest.h"
#include <wse/utility/AsyncLogRuntime.h>
#include <wse/utility/LogMessageBuilder.h>
#include <memory>
#include <string>
#include <vector>

namespace consumer
{
    namespace
    {
        class AsyncRecordingSink final : public wse::AsyncLogSink
        {
        public:
            std::vector<wse::sAsyncLogRecord> records;
            std::vector<std::string> texts;
            unsigned flushes = 0;

            wse::LogStatus write( const wse::sAsyncLogRecord& record_in, const std::string& text_in ) override
            {
                this->records.push_back( record_in );
                this->texts.push_back( text_in );
                return wse::LogStatus::success();
            }
            wse::LogStatus flush() override
            {
                ++this->flushes;
                return wse::LogStatus::success();
            }
        };

        std::string formatAsyncRecord( const wse::sAsyncLogRecord& record_in )
        {
            return wse::renderLogParts( record_in.parts );
        }
    }

    bool verifyAsyncLog()
    {
        // Only installed public headers and WSE::Core provide these symbols. No source
        // implementation is compiled into the consumer, for either linkage model.
        wse::AsyncLogRuntime runtime;
        const auto p_sink = std::make_shared<AsyncRecordingSink>();
        wse::sAsyncLogSettings settings;
        settings.sinks.push_back( p_sink );
        settings.formatter = &formatAsyncRecord;
        settings.tag_verbosity["ConsumerAsync"] = 2;
        if( !runtime.configure( settings ).succeeded() ) { return false; }

        const auto original_level = wse::minimumLogLevel();
        wse::setMinimumLogLevel( wse::LogLevel::Off );
        wse::AsyncLogLine( &runtime, "ConsumerAsync", 2 ).setLevel( wse::LogLevel::Warning )
            << "runtime" << wse::eLogControl::NewLine << 42 << true;
        const auto flushed = runtime.flush();
        const auto stopped = runtime.stop();
        wse::setMinimumLogLevel( original_level );

        // Successful stop joins the writer, making callback-owned vectors safe to inspect.
        const auto status = runtime.getStatus();
        const wse::LogError diagnostic( wse::eLogErrorCategory::Io,
            wse::eLogErrorCode::OpenFailed, "consumer diagnostic", 7 );
        return flushed.succeeded() && stopped.succeeded() && runtime.stop().succeeded() &&
            status.accepted == 1 && status.completed == 1 && !status.is_running &&
            p_sink->records.size() == 1 && p_sink->records.front().tag == "ConsumerAsync" &&
            p_sink->records.front().level == wse::LogLevel::Warning && p_sink->texts.size() == 1 &&
            p_sink->texts.front() == " runtime" + wse::END_LINE() + " 42 true" && p_sink->flushes > 0 &&
            diagnostic.category() == wse::eLogErrorCategory::Io &&
            diagnostic.code() == wse::eLogErrorCode::OpenFailed && diagnostic.nativeCode() == 7 &&
            !diagnostic.ok() && diagnostic.message() == "consumer diagnostic";
    }
}
