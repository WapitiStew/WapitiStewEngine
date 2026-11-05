// @file AsyncLogRuntime.h
// @brief Explicitly owned, bounded asynchronous logging; existing synchronous logging is independent.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "wse_Log.h"
#include "../error/LogError.h"

namespace wse
{
    enum class eLogControl { NewLine };
    enum class eLogPartKind { Text, NewLine };

    struct WSE_API sLogPart
    {
        eLogPartKind kind = eLogPartKind::Text;
        std::string text;
    };

    //! Producer-owned values are copied at acceptance; explicit NewLine remains distinct from text CR/LF.
    struct WSE_API sAsyncLogRecord
    {
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
        LogLevel level = LogLevel::Info;
        std::string tag;
        std::string source;
        std::string file;
        int line = 0;
        unsigned verbosity = 1;
        std::vector<sLogPart> parts;
        std::vector<std::pair<std::string, std::string>> attributes;
    };

    //! Called only by the runtime writer, without the queue mutex. No runtime control re-entry.
    class WSE_API AsyncLogSink
    {
    public:
        virtual ~AsyncLogSink() = default;
        virtual LogStatus write( const sAsyncLogRecord& record_in, const std::string& text_in ) = 0;
        virtual LogStatus flush() = 0;
    };

    using AsyncLogFormatter = std::function<std::string( const sAsyncLogRecord& )>;
    using AsyncConsoleWriter = std::function<LogStatus( const std::string& )>;

    struct WSE_API sAsyncLogSettings
    {
        eLogMode output = eLogMode::NONE;
        std::string file_path; //!< UTF-8; required for LOG_FILE/FULL. File is opened before reconfiguration commits.
        AsyncConsoleWriter console_writer; //!< Empty uses WSE console output; captures outlive stop completion.
        AsyncLogFormatter formatter; //!< Empty uses the default safe formatter; called on the writer thread.
        std::vector<std::shared_ptr<AsyncLogSink>> sinks;
        std::size_t queue_capacity = 1024;
        std::size_t record_byte_limit = 16 * 1024;
        std::size_t record_part_limit = 1024;
        std::size_t metadata_byte_limit = 16 * 1024;
        LogLevel minimum_level = LogLevel::Trace;
        unsigned default_verbosity = 1;
        std::unordered_map<std::string, unsigned> tag_verbosity;
        eLogSeparate separate = eLogSeparate::SPACE;
        eLogPrefix prefix = eLogPrefix::FULL;
        eLogSuffix suffix = eLogSuffix::LINE_BREAK;
        bool flush_each_record = false; //!< Flush sinks before completing each record; otherwise flush uses barriers.
    };

    enum class eLogSubmitResult { Accepted, Filtered, NotRunning, QueueFull, RecordTooLarge, Failed };

    struct WSE_API sAsyncLogStatus
    {
        bool is_running = false;
        bool is_accepting = false;
        std::size_t queued = 0;
        std::uint64_t accepted = 0;
        std::uint64_t completed = 0;
        std::uint64_t dropped = 0;
        std::uint64_t filtered = 0;
        LogError last_error;
    };

    //! No worker before successful configure. Destructor drains/stops/joins; the owner outlives producers.
    class WSE_API AsyncLogRuntime final
    {
    private:
        class Impl;
        static void deleteImpl( Impl* p_impl_in );
        std::unique_ptr<Impl, void (*)( Impl* )> m_impl;
    public:
        AsyncLogRuntime();
        ~AsyncLogRuntime();
        AsyncLogRuntime( const AsyncLogRuntime& ) = delete;
        AsyncLogRuntime& operator=( const AsyncLogRuntime& ) = delete;
        //! Candidate preparation failure preserves the old configuration. Success drains old work, reuses worker and clears error.
        LogStatus configure( const sAsyncLogSettings& settings_in );
        //! No I/O/formatter on producer; full/oversized records are rejected whole and counted, never silently truncated.
        eLogSubmitResult tryAppend( const sAsyncLogRecord& record_in ) noexcept;
        //! Zero verbosity and Off severity are disabled. Runtime filters are independent of synchronous WSE global filters.
        bool isEnabled( const std::string& tag_in, const unsigned verbosity_in,
            const LogLevel level_in = LogLevel::Info ) const;
        sAsyncLogSettings getSettings() const;
        sAsyncLogStatus getStatus() const;
        //! Writer barrier for records accepted before this call; concurrent later producers do not postpone it.
        LogStatus flush();
        //! Stop acceptance, drain, flush, join; idempotent. No later callback after return. A later configure may restart.
        LogStatus stop();
    };

    //! Escapes CR/LF/NUL in Text and renders explicit NewLine with END_LINE; adds no suffix/prefix.
    WSE_API std::string renderLogParts( const std::vector<sLogPart>& parts_in );
}
