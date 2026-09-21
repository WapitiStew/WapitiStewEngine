//*****************************************************************************************************************
//!
//! @file    GefError.h
//! @brief   \~japanese GEF File操作失敗の安定分類とResult別名を定義する.
//! @brief   \~english  Defines stable categories for GEF file-operation failures and the Result aliases.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-14, 2026   Create the GEF error contract (legacy removal program, LEGACY-008).
//!
//! @details
//!     \~japanese
//!         対象は「正しい呼び出しでも起こり得るFile操作の失敗」だけである（File不在、読込失敗、
//!         不正なData形式）。APIの使い方違反は本契約ではなく標準Exception（`std::logic_error`等）で
//!         報告する。正準の構築・状態規則はdoc/design/ja/ResultContract.mdを参照。
//!     \~english
//!         This contract covers only file-operation failures that can occur on a well-formed call
//!         (a missing file, a failed read, malformed data). API usage violations are reported with
//!         the standard exceptions (`std::logic_error` and friends) instead. The canonical
//!         construction and state rules are doc/design/en/ResultContract.md.
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

#ifndef WONDERSTEWENGINE_GEF_ERROR_GEFERROR_H
#define WONDERSTEWENGINE_GEF_ERROR_GEFERROR_H

#include <cstdint>
#include <string>

#include "../../dynamic.h"
#include "../../wse/utility/wse_Result.h"

namespace wse
{
namespace gef
{
    //! \~japanese GEF File操作失敗の安定分類. \~english Stable GEF file-operation failure categories.
    enum class eGefErrorCategory : std::uint8_t
    {
          None   = 0    //!< \~japanese Errorなし.          \~english No error.
        , Io            //!< \~japanese File入出力の失敗.   \~english A file input/output failure.
        , Format        //!< \~japanese Data形式の失敗.     \~english A data-format failure.
        , Resource      //!< \~japanese 容量制限。 \~english A resource budget failure.
    };

    //! \~japanese GEF File操作失敗の安定Code. \~english Stable GEF file-operation failure codes.
    enum class eGefErrorCode : std::uint16_t
    {
          None           = 0 //!< \~japanese Errorなし.                            \~english No error.
        , FileOpenFailed     //!< \~japanese Fileを開けない.                       \~english The file cannot be opened.
        , ReadFailed         //!< \~japanese Fileから読み込めない.                 \~english The file cannot be read.
        , MalformedData      //!< \~japanese Data構造が契約と一致しない.           \~english The data structure does not match the contract.
        , NoData             //!< \~japanese 変換対象のData行が存在しない.         \~english No data rows exist to convert.
        , WriteFailed        //!< \~japanese 書込・置換失敗。 \~english Write, flush, close or replacement failed.
        , LimitExceeded      //!< \~japanese 読込Budget超過。 \~english A read budget was exceeded.
    };

    //!
    //! @class GefError
    //!
    //! @brief
    //!     \~japanese Portableな識別子と診断Messageを持つGEFのOperation Error値.
    //!     \~english  GEF operation error value with a portable identity and diagnostic message.
    //!
    //! @note
    //!     \~japanese 分岐はCategoryとCodeで行う. MessageとNative codeは診断専用である.
    //!     \~english  Branch on the category and code; the message and native code are diagnostic only.
    //!
    class WSE_API GefError final
    {
        private : eGefErrorCategory m_category;    //!< \~japanese 安定分類.        \~english The stable category.
        private : eGefErrorCode     m_code;        //!< \~japanese 安定Code.        \~english The stable code.
        private : std::string       m_message;     //!< \~japanese 診断Message.     \~english The diagnostic message.
        private : std::int64_t      m_native_code; //!< \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.

        //! \~japanese Errorなしの値を生成する. \~english Creates a no-error value.
        public : GefError() noexcept;

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
        public : GefError(
              const eGefErrorCategory category_in
            , const eGefErrorCode     code_in
            , const std::string&      message_in
            , const std::int64_t      native_code_in = 0
        );

        //! @return \~japanese Errorなしの場合true. \~english True when no error is present.
        public : bool ok() const noexcept;

        //! @return \~japanese 安定分類. \~english The stable category.
        public : eGefErrorCategory category() const noexcept;

        //! @return \~japanese 安定Code. \~english The stable code.
        public : eGefErrorCode code() const noexcept;

        //! @return \~japanese 診断Message. \~english The diagnostic message.
        public : const std::string& message() const noexcept;

        //! @return \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.
        public : std::int64_t nativeCode() const noexcept;
    };

    //! \~japanese GEF File Operationの値付き結果. \~english Result of a value-bearing GEF file operation.
    template< typename T >
    using GefResult = Result< T, GefError >;

    //! \~japanese GEF File Operationの値なし結果. \~english Result of a value-less GEF file operation.
    using GefStatus = Status< GefError >;

} // namespace gef
} // namespace wse

#endif // WONDERSTEWENGINE_GEF_ERROR_GEFERROR_H
