//*****************************************************************************************************************
//!
//! @file    CoreError.h
//! @brief   \~japanese 計算上起こり得るCore失敗の安定分類とResult別名を定義する.
//! @brief   \~english  Defines stable categories for expectable Core computation failures and the Result aliases.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Create the Core error contract (legacy removal program, LEGACY-008).
//!
//! @details
//!     \~japanese
//!         対象は「正常な入力でも起こり得る計算上の失敗」だけである（特異行列、退化した点集合など）。
//!         APIの使い方違反は本契約ではなく標準Exception（`std::invalid_argument`／`std::out_of_range`／
//!         `std::domain_error`）で報告する。File／Device／OS失敗は各ComponentのError型が担う。
//!         正準の構築・状態規則はdoc/design/ja/ResultContract.mdを参照。
//!     \~english
//!         This contract covers only computation failures that can occur on well-formed input
//!         (a singular matrix, a degenerate point set). API usage violations are reported with the
//!         standard exceptions (`std::invalid_argument` / `std::out_of_range` / `std::domain_error`)
//!         instead, and file/device/OS failures belong to the component error types. The canonical
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

#ifndef WONDERSTEWENGINE_ERROR_COREERROR_H
#define WONDERSTEWENGINE_ERROR_COREERROR_H

#include <cstdint>
#include <string>

#include "../../dynamic.h"
#include "../utility/wse_Result.h"

namespace wse
{
    //! \~japanese Core計算失敗の安定分類. \~english Stable Core computation failure categories.
    enum class eCoreErrorCategory : std::uint8_t
    {
          None        = 0   //!< \~japanese Errorなし.            \~english No error.
        , Computation       //!< \~japanese 数値計算上の失敗.      \~english A numeric computation failure.
    };

    //! \~japanese Core計算失敗の安定Code. \~english Stable Core computation failure codes.
    enum class eCoreErrorCode : std::uint16_t
    {
          None              = 0 //!< \~japanese Errorなし.                              \~english No error.
        , SingularMatrix        //!< \~japanese 特異または準特異な行列で逆計算できない. \~english A singular or near-singular matrix cannot be inverted.
        , DegenerateGeometry    //!< \~japanese 退化した幾何入力から解を決定できない.   \~english A degenerate geometric input determines no solution.
    };

    //!
    //! @class CoreError
    //!
    //! @brief
    //!     \~japanese Portableな識別子と診断Messageを持つCoreのOperation Error値.
    //!     \~english  Core operation error value with a portable identity and diagnostic message.
    //!
    //! @note
    //!     \~japanese 分岐はCategoryとCodeで行う. MessageとNative codeは診断専用である.
    //!     \~english  Branch on the category and code; the message and native code are diagnostic only.
    //!
    class WSE_API CoreError final
    {
        private : eCoreErrorCategory m_category;    //!< \~japanese 安定分類.        \~english The stable category.
        private : eCoreErrorCode     m_code;        //!< \~japanese 安定Code.        \~english The stable code.
        private : std::string        m_message;     //!< \~japanese 診断Message.     \~english The diagnostic message.
        private : std::int64_t       m_native_code; //!< \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.

        //! \~japanese Errorなしの値を生成する. \~english Creates a no-error value.
        public : CoreError() noexcept;

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
        public : CoreError(
              const eCoreErrorCategory category_in
            , const eCoreErrorCode     code_in
            , const std::string&       message_in
            , const std::int64_t       native_code_in = 0
        );

        //! @return \~japanese Errorなしの場合true. \~english True when no error is present.
        public : bool ok() const noexcept;

        //! @return \~japanese 安定分類. \~english The stable category.
        public : eCoreErrorCategory category() const noexcept;

        //! @return \~japanese 安定Code. \~english The stable code.
        public : eCoreErrorCode code() const noexcept;

        //! @return \~japanese 診断Message. \~english The diagnostic message.
        public : const std::string& message() const noexcept;

        //! @return \~japanese 診断用の補助Code. \~english The diagnostic auxiliary code.
        public : std::int64_t nativeCode() const noexcept;
    };

    //! \~japanese Core計算Operationの値付き結果. \~english Result of a value-bearing Core computation.
    template< typename T >
    using CoreResult = Result< T, CoreError >;

    //! \~japanese Core計算Operationの値なし結果. \~english Result of a value-less Core computation.
    using CoreStatus = Status< CoreError >;

} // namespace wse

#endif // WONDERSTEWENGINE_ERROR_COREERROR_H
