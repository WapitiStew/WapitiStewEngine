// @file logging.cpp
// @brief Tour of the WSE log facility: structured records, level filtering, sinks,
//        the stream logger, and file output. Hardware-free; link WSE::Core.
//
// Two things a caller has to know before using any of this. Every destination is reached
// through a named profile, and a profile has to be registered before anything routed to it
// appears: registDefaultLog() registers "WSE", and writing to a tag nobody registered is
// silently dropped on the console and in the file rather than reported. Level filtering, by
// contrast, is one process-wide setting applied before any destination is reached, so raising
// it silences sinks as well as the console.

#include <utility>
#include <wse/stew.h>

#include <memory>
#include <string>

namespace
{

//! A LogSink receives every structured record that passes the minimum-level filter.
//! It sees records the console never shows: sink delivery is not gated by the output profile,
//! so a record written to an unregistered tag still arrives here. onLog() runs on whichever
//! thread called writeLog(), outside the logger's own locks, so a sink used from more than one
//! thread has to guard its own members; this tour is single-threaded and does not. A throw out
//! of onLog() is swallowed, because logging must not turn into a product failure.
class CountingSink final : public wse::LogSink
{
    //! @brief Construct all members with explicit defaults.
public:
    CountingSink()
        : delivered ( 0 )
        , last      ()
    {
    }
private:

  public:
    int delivered;
    wse::LogRecord last;

    void onLog( const wse::LogRecord& record_in ) override
    {
        ++delivered;
        last = record_in;
    }
};

} // namespace

int main()
{
    // 1. Register the default "WSE" profile: console output, space separator,
    //    date-and-time prefix, and a trailing line break. Registration is idempotent and does
    //    not overwrite, so a second call leaves an already-tuned profile alone; changing a
    //    registered profile is what changeDefaultLog() in step 6 is for.
    wse::registDefaultLog();

    // 2. Structured logging. The tag identifies the caller; the optional details
    //    field carries machine-readable context. Console format:
    //    [LEVEL] [tag] message | details
    wse::writeLog( wse::LogLevel::Info, "LogExample", "structured info record" );
    wse::writeLog( wse::LogLevel::Warning, "LogExample", "structured warning record",
                   "details travel in their own field" );
    // The five-argument overload separates the application tag from the feature source.
    // Its first tag selects the output profile, so it must name a registered one ("WSE"
    // here); the four-argument overload above always routes through the "WSE" profile.
    // Naming an unregistered tag is not diagnosed: console and file output go quiet while
    // sinks keep receiving, which is a hard thing to notice from the console alone.
    wse::writeLog( wse::LogLevel::Error, "WSE", "FeatureSource",
                   "structured error record", "code=42" );

    // 3. Level filtering applies to every destination at once. It is one process-wide value,
    //    not a per-profile or per-sink one, and it is tested before anything is formatted, so
    //    a filtered record costs nothing. The default is Info; Off silences everything. The
    //    minimum is restored below because the closing check reads it back.
    wse::setMinimumLogLevel( wse::LogLevel::Warning );
    wse::writeLog( wse::LogLevel::Info, "LogExample",
                   "this info record is filtered out and never printed" );
    wse::writeLog( wse::LogLevel::Warning, "LogExample",
                   "warnings still pass the raised minimum level" );
    wse::setMinimumLogLevel( wse::LogLevel::Info );

    // 4. A custom sink observes structured records in-process. Ownership is shared: the
    //    registry keeps its own shared_ptr, so the sink survives until the last reference
    //    goes, and the local one here is what lets the summary read `delivered` after the
    //    handle has been given back. Registering a null sink returns handle zero and adds
    //    nothing, so a zero handle is the never-valid one.
    const auto sink = std::make_shared< CountingSink >();
    const wse::LogSinkHandle handle = wse::registerLogSink( sink );
    wse::writeLog( wse::LogLevel::Info, "LogExample", "delivered to the sink" );
    wse::flushLog();
    wse::unregisterLogSink( handle );
    wse::writeLog( wse::LogLevel::Info, "LogExample", "sink summary",
                   std::to_string( sink->delivered ) + " record(s), last message \""
                       + sink->last.message + "\"" );

    // 5. The tag-templated stream logger emits its accumulated message on destruction.
    //    (The DWLog() convenience macro expands to this expression unqualified, so it is
    //    meant for code inside namespace wse; spelled out it works from anywhere.)
    //    Because emission is the destructor's job, the record appears at the end of the full
    //    expression: naming the temporary in a longer-lived variable would delay the line and
    //    interleave it with whatever was logged in between. A stream line is a structured
    //    record too, at level Info for every tag except "DEV", so sinks see these as well.
    wse::WLog() << "stream logger:" << " value=" << 42 << " flag=" << true;
    wse::Logger< wse::WSE_TAG >( __FILE__, __LINE__ ) << "stream logger with file and line";

    // 6. Redirect the default profile to console and file, then restore console-only.
    //    FULL means both destinations; LOG_FILE would drop the console and NONE would silence
    //    the profile while still feeding sinks. The directory and the base name are joined
    //    verbatim, with no extension added, so this writes "./wse_logging_example"; missing
    //    directories are created and every record is appended.
    wse::changeDefaultLog( wse::eLogMode::FULL, ".", "wse_logging_example" );
    wse::writeLog( wse::LogLevel::Info, "LogExample", "this record also lands in the log file" );
    wse::flushLog();
    //    The path has to be read before the mode is put back: this overload recomputes the
    //    stored path from its two arguments every time, so a CONSOLE call that defaults them
    //    also clears the recorded file name. Reversing these two lines would log an empty one.
    const std::string logfile = wse::WLog::fullpath();
    wse::changeDefaultLog( wse::eLogMode::CONSOLE );
    wse::writeLog( wse::LogLevel::Info, "LogExample", "log file", logfile );

    // Exactly one record was written while the sink was registered, so anything other than one
    // means either the filter leaked or unregisterLogSink() did not take effect.
    const bool consistent = sink->delivered == 1 && wse::minimumLogLevel() == wse::LogLevel::Info;
    wse::writeLog( consistent ? wse::LogLevel::Info : wse::LogLevel::Error, "LogExample",
                   consistent ? "logging tour succeeded" : "logging tour failed" );
    wse::flushLog();
    return consistent ? 0 : 1;
}
