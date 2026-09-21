#include "ApplicationLog.h"

namespace consumer
{
    const char APPLICATION_TAG[] = "ConsumerApp";

    void writeApplicationLog()
    {
        ApplicationLog() << "stream record";
        wse::writeLog(wse::LogLevel::Warning, APPLICATION_TAG, "worker", "structured record", "details");
    }

    const char* applicationTagAddress()
    {
        return APPLICATION_TAG;
    }
}
