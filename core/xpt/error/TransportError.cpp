//*****************************************************************************************************************
//!
//! @file    TransportError.cpp
//! @brief   \~japanese XPT Transport Error値を実装する.
//! @brief   \~english  Implements XPT transport error values.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "xpt/error/TransportError.h"

namespace wse
{
namespace xpt
{

TransportError::TransportError() noexcept
    : m_category    ( eTransportErrorCategory::None )
    , m_code        ( eTransportErrorCode::None )
    , m_message     ()
    , m_native_code ( 0 )
{
}

TransportError::TransportError(
      const eTransportErrorCategory category_in
    , const eTransportErrorCode     code_in
    , const std::string&            message_in
    , const std::int64_t            native_code_in
)
    : m_category    ( category_in )
    , m_code        ( code_in )
    , m_message     ( message_in )
    , m_native_code ( native_code_in )
{
}

bool TransportError::ok() const noexcept
{
    return this->m_category == eTransportErrorCategory::None
        && this->m_code == eTransportErrorCode::None;
}

eTransportErrorCategory TransportError::category() const noexcept
{
    return this->m_category;
}

eTransportErrorCode TransportError::code() const noexcept
{
    return this->m_code;
}

const std::string &TransportError::message() const noexcept
{
    return this->m_message;
}

std::int64_t TransportError::nativeCode() const noexcept
{
    return this->m_native_code;
}


} // namespace xpt
} // namespace wse
