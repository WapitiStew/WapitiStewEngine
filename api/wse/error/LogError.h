// @file LogError.h
// @brief Portable operational errors for explicitly owned asynchronous logging.
#pragma once

#include <cstdint>
#include <string>
#include "../../dynamic.h"
#include "../utility/wse_Result.h"

namespace wse
{
    enum class eLogErrorCategory : std::uint8_t
    {
        None, Configuration, Capacity, Io, Callback, Lifecycle, Internal
    };

    enum class eLogErrorCode : std::uint16_t
    {
        None, InvalidSettings, NotRunning, QueueFull, RecordTooLarge,
        OpenFailed, WriteFailed, FlushFailed, CallbackFailed, ReentrantControl,
        AllocationFailed, ThreadFailed
    };

    //! Stable code/category are branching contracts; message/nativeCode are diagnostics.
    class WSE_API LogError final
    {
    private:
        eLogErrorCategory m_category;
        eLogErrorCode m_code;
        std::string m_message;
        std::int64_t m_native_code;
    public:
        LogError() noexcept;
        LogError( const eLogErrorCategory category_in, const eLogErrorCode code_in,
            const std::string& message_in = {}, const std::int64_t native_code_in = 0 );
        bool ok() const noexcept;
        eLogErrorCategory category() const noexcept;
        eLogErrorCode code() const noexcept;
        const std::string& message() const noexcept;
        std::int64_t nativeCode() const noexcept;
    };

    using LogStatus = Status<LogError>;
}
