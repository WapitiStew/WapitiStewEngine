#include "ApplicationLog.h"

#include <memory>
#include <vector>

namespace consumer
{
    class RecordingSink final : public wse::LogSink
    {
    public:
        std::vector<wse::LogRecord> records;

        void onLog(const wse::LogRecord& record_in) override
        {
            records.push_back(record_in);
        }
    };

    bool verifyApplicationLog()
    {
        const auto original_level = wse::minimumLogLevel();
        const auto sink = std::make_shared<RecordingSink>();
        const auto handle = wse::registerLogSink(sink);
        ApplicationLog::registerLogConfig({APPLICATION_TAG, wse::eLogMode::CONSOLE});
        wse::setMinimumLogLevel(wse::LogLevel::Info);
        writeApplicationLog();
        wse::setMinimumLogLevel(wse::LogLevel::Off);
        writeApplicationLog();
        wse::setMinimumLogLevel(original_level);
        wse::unregisterLogSink(handle);
        wse::ILogger::removeLogConfig(APPLICATION_TAG);
        return handle != 0 && applicationTagAddress() == APPLICATION_TAG &&
            ApplicationLog::TAG() == "ConsumerApp" && sink->records.size() == 2 &&
            sink->records[0].tag == "ConsumerApp" &&
            sink->records[0].message.find("stream record") != std::string::npos &&
            sink->records[1].tag == "ConsumerApp" && sink->records[1].source == "worker" &&
            sink->records[1].message == "structured record" &&
            sink->records[1].level == wse::LogLevel::Warning &&
            sink->records[1].details == "details";
    }
}
