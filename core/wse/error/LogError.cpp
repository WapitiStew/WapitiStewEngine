// @file LogError.cpp
// @brief Stable asynchronous logging error values.
#include "../../../api/wse/error/LogError.h"

namespace wse
{
    LogError::LogError() noexcept
        : m_category( eLogErrorCategory::None ), m_code( eLogErrorCode::None ), m_message(), m_native_code( 0 ) {}
    LogError::LogError( const eLogErrorCategory category_in, const eLogErrorCode code_in,
        const std::string& message_in, const std::int64_t native_code_in )
        : m_category( category_in ), m_code( code_in ), m_message( message_in ), m_native_code( native_code_in ) {}
    bool LogError::ok() const noexcept { return this->m_category == eLogErrorCategory::None; }
    eLogErrorCategory LogError::category() const noexcept { return this->m_category; }
    eLogErrorCode LogError::code() const noexcept { return this->m_code; }
    const std::string& LogError::message() const noexcept { return this->m_message; }
    std::int64_t LogError::nativeCode() const noexcept { return this->m_native_code; }
}
