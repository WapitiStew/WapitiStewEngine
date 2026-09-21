#pragma once
#ifndef WSE_CONSUMER_APPLICATION_LOG_H
#define WSE_CONSUMER_APPLICATION_LOG_H

#include <wse/utility/wse_Log.h>

namespace consumer
{
    extern const char APPLICATION_TAG[];
    using ApplicationLog = wse::Logger<APPLICATION_TAG>;
    void writeApplicationLog();
    const char* applicationTagAddress();
    bool verifyApplicationLog();
}

#endif
