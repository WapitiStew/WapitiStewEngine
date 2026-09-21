//*****************************************************************************************************************
//!
//! @file    LicenseError.cpp
//! @brief   \~japanese License操作失敗のError値の実装.
//! @brief   \~english  Implements the license-operation error value.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-14, 2026   Create the License error contract (legacy removal program, LEGACY-008).
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include "../../../api/wse/error/LicenseError.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace wse
{
    LicenseError::LicenseError() noexcept
        : m_category    ( eLicenseErrorCategory::None )
        , m_code        ( eLicenseErrorCode::None )
        , m_message     ()
        , m_native_code ( 0 )
    {
    }
    LicenseError::LicenseError(
          const eLicenseErrorCategory category_in
        , const eLicenseErrorCode     code_in
        , const std::string&          message_in
        , const std::int64_t          native_code_in
    )
        : m_category    ( category_in )
        , m_code        ( code_in )
        , m_message     ( message_in )
        , m_native_code ( native_code_in )
    {
    }
    bool LicenseError::ok() const noexcept
    {
        return this->m_category == eLicenseErrorCategory::None;
    }

    eLicenseErrorCategory LicenseError::category() const noexcept
    {
        return this->m_category;
    }

    eLicenseErrorCode LicenseError::code() const noexcept
    {
        return this->m_code;
    }

    const std::string& LicenseError::message() const noexcept
    {
        return this->m_message;
    }

    std::int64_t LicenseError::nativeCode() const noexcept
    {
        return this->m_native_code;
    }

} // namespace wse
