//*****************************************************************************************************************
//!
//! @file    LicenseError.h
//! @brief   \~japanese License操作失敗の安定分類とResult別名を定義する.
//! @brief   \~english  Defines stable categories for license-operation failures and the Result aliases.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-14, 2026   Create the License error contract (legacy removal program, LEGACY-008).
//!
//! @details
//!     \~japanese
//!         対象は「正しい呼び出しでも起こり得るLicense操作の失敗」だけである（File不在、期限切れ、
//!         非対応Device）。APIの使い方違反（二重Load、不正なEnum値）は本契約ではなく標準Exception
//!         （`std::logic_error`／`std::invalid_argument`）で報告する。正準の構築・状態規則は
//!         doc/design/ja/ResultContract.mdを参照。
//!     \~english
//!         This contract covers only license-operation failures that can occur on a well-formed
//!         call (a missing file, an expired license, an unlicensed device). API usage violations
//!         (a double load, an invalid enum value) are reported with the standard exceptions
//!         (`std::logic_error` / `std::invalid_argument`) instead. The canonical construction and
//!         state rules are doc/design/en/ResultContract.md.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma once

#ifndef WONDERSTEWENGINE_ERROR_LICENSEERROR_H
#define WONDERSTEWENGINE_ERROR_LICENSEERROR_H

#include <cstdint>
#include <string>

#include "../../dynamic.h"
#include "../utility/wse_Result.h"

namespace wse
{
    //! \~japanese License操作失敗の安定分類. \~english Stable license-operation failure categories.
    enum class eLicenseErrorCategory : std::uint8_t
    {
          None         = 0  //!< \~japanese Errorなし.          \~english No error.
        , Io                //!< \~japanese File入出力の失敗.   \~english A file input/output failure.
        , Verification      //!< \~japanese License検証の失敗.  \~english A license verification failure.
    };

    //! \~japanese License操作失敗の安定Code. \~english Stable license-operation failure codes.
    enum class eLicenseErrorCode : std::uint16_t
    {
          None             = 0 //!< \~japanese Errorなし.                          \~english No error.
        , FileOpenFailed       //!< \~japanese Fileを開けない.                     \~english The file cannot be opened.
        , ReadFailed           //!< \~japanese Fileから読み込めない.               \~english The file cannot be read.
        , LicenseExpired       //!< \~japanese Licenseの有効期間外である.          \~english The license is outside its validity period.
        , UnlicensedDevice     //!< \~japanese Licenseが本Deviceを許可していない.  \~english The license does not permit this device.
        , UnusableLicense      //!< \~japanese Licenseが本機能を許可していない.    \~english The license does not permit this feature.
        , UnusableVersion      //!< \~japanese Licenseが本Versionを許可していない. \~english The license does not permit this version.
    };

    //!
    //! @class LicenseError
    //!
    //! @brief
    //!     \~japanese Portableな識別子と診断Messageを持つLicenseのOperation Error値.
    //!     \~english  License operation error value with a portable identity and diagnostic message.
    //!
    //! @note
    //!     \~japanese 分岐はCategoryとCodeで行う. MessageとNative codeは診断専用である.
    //!     \~english  Branch on the category and code; the message and native code are diagnostic only.
    //!
    class WSE_API LicenseError final
    {
        private : eLicenseErrorCategory m_category;    //!< \~japanese 安定分類.        \~english The stable category.
        private : eLicenseErrorCode     m_code;        //!< \~japanese 安定Code.        \~english The stable code.
        private : std::string           m_message;     //!< \~japanese 診断Message.     \~english The diagnostic message.
        private : std::int64_t          m_native_code; //!< \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.

        //! \~japanese Errorなしの値を生成する. \~english Creates a no-error value.
        public : LicenseError() noexcept;

        //!
        //! @brief
        //!     \~japanese Error値を生成する.
        //!     \~english  Creates an error value.
        //!
        //! @param [in] category_in    \~japanese 安定分類.        \~english The stable category.
        //! @param [in] code_in        \~japanese 安定Code.        \~english The stable code.
        //! @param [in] message_in     \~japanese 診断Message.     \~english The diagnostic message.
        //! @param [in] native_code_in \~japanese 任意の補助Code. Portableな分岐へ使用しない.
        //!                            \~english  Optional auxiliary code; never a portable branching contract.
        //!
        public : LicenseError(
              const eLicenseErrorCategory category_in
            , const eLicenseErrorCode     code_in
            , const std::string&          message_in
            , const std::int64_t          native_code_in = 0
        );

        //! @return \~japanese Errorなしの場合true. \~english True when no error is present.
        public : bool ok() const noexcept;

        //! @return \~japanese 安定分類. \~english The stable category.
        public : eLicenseErrorCategory category() const noexcept;

        //! @return \~japanese 安定Code. \~english The stable code.
        public : eLicenseErrorCode code() const noexcept;

        //! @return \~japanese 診断Message. \~english The diagnostic message.
        public : const std::string& message() const noexcept;

        //! @return \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.
        public : std::int64_t nativeCode() const noexcept;
    };

    //! \~japanese License Operationの値付き結果. \~english Result of a value-bearing license operation.
    template< typename T >
    using LicenseResult = Result< T, LicenseError >;

    //! \~japanese License Operationの値なし結果. \~english Result of a value-less license operation.
    using LicenseStatus = Status< LicenseError >;

} // namespace wse

#endif // WONDERSTEWENGINE_ERROR_LICENSEERROR_H
