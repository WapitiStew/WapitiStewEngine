//*****************************************************************************************************************
//!
//! @file    GefError.cpp
//! @brief   \~japanese GEF File操作失敗のError値の実装.
//! @brief   \~english  Implements the GEF file-operation error value.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-14, 2026   Create the GEF error contract (legacy removal program, LEGACY-008).
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#include "../../../api/gef/error/GefError.h"
#pragma warning(pop)

namespace wse
{
namespace gef
{
    GefError::GefError() noexcept
        : m_category    ( eGefErrorCategory::None )
        , m_code        ( eGefErrorCode::None )
        , m_message     ()
        , m_native_code ( 0 )
    {
    }
    GefError::GefError(
          const eGefErrorCategory category_in
        , const eGefErrorCode     code_in
        , const std::string&      message_in
        , const std::int64_t      native_code_in
    )
        : m_category    ( category_in )
        , m_code        ( code_in )
        , m_message     ( message_in )
        , m_native_code ( native_code_in )
    {
    }
    bool GefError::ok() const noexcept
    {
        return this->m_category == eGefErrorCategory::None;
    }

    eGefErrorCategory GefError::category() const noexcept
    {
        return this->m_category;
    }

    eGefErrorCode GefError::code() const noexcept
    {
        return this->m_code;
    }

    const std::string& GefError::message() const noexcept
    {
        return this->m_message;
    }

    std::int64_t GefError::nativeCode() const noexcept
    {
        return this->m_native_code;
    }

} // namespace gef
} // namespace wse
