//*****************************************************************************************************************
//!
//! @file    RendererError.cpp
//! @brief   \~japanese OUI Renderer Errorを実装する.
//! @brief   \~english  Implements OUI renderer errors.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "../../../api/oui/renderer/RendererError.h"

namespace wse
{
namespace oui
{

RendererError::RendererError() noexcept
    : m_category    ( eRendererErrorCategory::None )
    , m_code        ( eRendererErrorCode::None )
    , m_message     ()
    , m_native_code ( 0 )
{
}

RendererError::RendererError(
      const eRendererErrorCategory category_in
    , const eRendererErrorCode     code_in
    , const std::string&           message_in
    , const std::int64_t           native_code_in
)
    : m_category    ( category_in )
    , m_code        ( code_in )
    , m_message     ( message_in )
    , m_native_code ( native_code_in )
{
}

bool RendererError::ok() const noexcept
{
    return this->m_category == eRendererErrorCategory::None &&
        this->m_code == eRendererErrorCode::None;
}

eRendererErrorCategory RendererError::category() const noexcept
{
    return this->m_category;
}

eRendererErrorCode RendererError::code() const noexcept
{
    return this->m_code;
}

const std::string& RendererError::message() const noexcept
{
    return this->m_message;
}

std::int64_t RendererError::nativeCode() const noexcept
{
    return this->m_native_code;
}


} // namespace oui
} // namespace wse
