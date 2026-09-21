//*****************************************************************************************************************
//!
//! @file    Error.h
//! @brief   \~japanese 多言語Bindingへ公開する安定Error契約を定義する.
//! @brief   \~english  Defines the stable error contract exposed to language bindings.
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_ERROR_H
#define WONDERSTEWENGINE_WSE_BINDING_ERROR_H

#include "../../dynamic.h"
#include "../utility/wse_Result.h"

#include <cstdint>
#include <string>
#include <utility>

namespace wse
{
namespace binding
{

//! \~japanese 全Bindingで同じ値を使うPortable Error分類.
//! \~english  Portable error categories shared by every binding.
enum class eErrorCategory : std::uint8_t
{
      None              = 0
    , InvalidArgument   = 1
    , NotFound          = 2
    , InvalidState      = 3
    , InputOutput       = 4
    , Timeout           = 5
    , Cancellation      = 6
    , Protocol          = 7
    , Security          = 8
    , Unsupported       = 9
    , ResourceExhausted = 10
    , Internal          = 11
};

//! \~japanese Language Runtime固有Exceptionへ変換する前のError値.
//! \~english  Error value used before conversion to a language-runtime exception.
class WSE_API Error final
{
  private:
    eErrorCategory m_category;
    std::int32_t m_code;
    std::string m_message;
    std::int64_t m_native_code;

  public:
    Error() noexcept;
    Error(
          eErrorCategory category_in
        , std::int32_t code_in
        , const std::string& message_in
        , std::int64_t native_code_in = 0
    );

    //! \~japanese 失敗していないか. Categoryが`None`のとき真.
    //! \~english  Whether nothing failed, meaning the category is `None`.
    bool ok() const noexcept;

    //! \~japanese Portableな分類. Bindingはこれで分岐する.
    //! \~english  The portable category. A binding branches on this.
    eErrorCategory category() const noexcept;

    //! \~japanese Component固有Code. 同じCategoryの中で原因を分ける.
    //! \~english  The component-specific code, which separates causes inside one category.
    std::int32_t code() const noexcept;

    //! \~japanese 人間向けMessage. 分岐条件には使わない.
    //! \~english  The human-readable message. It is not a thing to branch on.
    const std::string& message() const noexcept;

    //! \~japanese OSまたは第三者Libraryの生Code. 分類できない詳細を保つ.
    //! \~english  The raw operating-system or third-party code, kept for detail the categories
    //!            cannot carry.
    std::int64_t nativeCode() const noexcept;
};

//! \~japanese 正準Result契約 (doc/design/ja/ResultContract.md) をBindingのError型で使う別名.
//!            成功は値だけ、失敗はErrorだけを運び、構築はFactory経由に限られる.
//! \~english  Alias applying the canonical result contract (doc/design/en/ResultContract.md) to
//!            the binding error type: only a value on success, only an error on failure,
//!            factory-only construction.
template <typename T>
using Result = wse::Result< T, Error >;

//! \~japanese 値なしOperationの成否. 無意味なbool Payloadは持たない.
//! \~english  Outcome of a value-less operation, without the meaningless bool payload.
using Status = wse::Status< Error >;

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_ERROR_H
