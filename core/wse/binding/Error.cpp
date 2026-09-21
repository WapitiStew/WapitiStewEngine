//*****************************************************************************************************************
//!
//! @file    Error.cpp
//! @brief   \~japanese 共通Binding Error契約の実装.
//! @brief   \~english  Implements the common binding error contract.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/Error.h>

namespace wse
{
namespace binding
{

Error::Error() noexcept
    : m_category    ( eErrorCategory::None )
    , m_code        ( 0 )
    , m_message     ()
    , m_native_code ( 0 )
{
}

Error::Error(
      const eErrorCategory category_in
    , const std::int32_t code_in
    , const std::string& message_in
    , const std::int64_t native_code_in
)
    : m_category    ( category_in )
    , m_code        ( code_in )
    , m_message     ( message_in )
    , m_native_code ( native_code_in )
{
}

bool Error::ok() const noexcept
{
    return this->m_category == eErrorCategory::None;
}

eErrorCategory Error::category() const noexcept
{
    return this->m_category;
}

std::int32_t Error::code() const noexcept
{
    return this->m_code;
}

const std::string& Error::message() const noexcept
{
    return this->m_message;
}

std::int64_t Error::nativeCode() const noexcept
{
    return this->m_native_code;
}

} // namespace binding
} // namespace wse
