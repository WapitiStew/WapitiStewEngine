//*****************************************************************************************************************
//!
//! @file    CoreError.cpp
//! @brief   \~japanese Core計算失敗のError値の実装.
//! @brief   \~english  Implements the Core computation error value.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Create the Core error contract (legacy removal program, LEGACY-008).
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
#include "../../../api/wse/error/CoreError.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace wse
{
    CoreError::CoreError() noexcept
        : m_category    ( eCoreErrorCategory::None )
        , m_code        ( eCoreErrorCode::None )
        , m_message     ()
        , m_native_code ( 0 )
    {
    }
    CoreError::CoreError(
          const eCoreErrorCategory category_in
        , const eCoreErrorCode     code_in
        , const std::string&       message_in
        , const std::int64_t       native_code_in
    )
        : m_category    ( category_in )
        , m_code        ( code_in )
        , m_message     ( message_in )
        , m_native_code ( native_code_in )
    {
    }
    bool CoreError::ok() const noexcept
    {
        return this->m_category == eCoreErrorCategory::None;
    }

    eCoreErrorCategory CoreError::category() const noexcept
    {
        return this->m_category;
    }

    eCoreErrorCode CoreError::code() const noexcept
    {
        return this->m_code;
    }

    const std::string& CoreError::message() const noexcept
    {
        return this->m_message;
    }

    std::int64_t CoreError::nativeCode() const noexcept
    {
        return this->m_native_code;
    }

} // namespace wse
