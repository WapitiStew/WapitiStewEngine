//*****************************************************************************************************************
//! @file    CameraError.cpp
//! @brief   \~japanese Portable Tmr Camera Errorの実装.
//! @brief   \~english  Portable Tmr camera error implementation.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <tmr/camera/CameraError.h>

namespace wse
{
namespace tmr
{

CameraError::CameraError() noexcept
    : m_category    ( eCameraErrorCategory::None )
    , m_code        ( eCameraErrorCode::None )
    , m_message     ()
    , m_native_code ( 0 )
{
}

CameraError::CameraError(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const std::string&         message_in
    , const std::int64_t         native_code_in )
    : m_category    ( category_in )
    , m_code        ( code_in )
    , m_message     ( message_in )
    , m_native_code ( native_code_in )
{
}

bool CameraError::ok() const noexcept
{
    return this->m_category == eCameraErrorCategory::None
        && this->m_code == eCameraErrorCode::None;
}

eCameraErrorCategory CameraError::category() const noexcept
{
    return this->m_category;
}

eCameraErrorCode CameraError::code() const noexcept
{
    return this->m_code;
}

const std::string& CameraError::message() const noexcept
{
    return this->m_message;
}

std::int64_t CameraError::nativeCode() const noexcept
{
    return this->m_native_code;
}

} // namespace tmr
} // namespace wse
