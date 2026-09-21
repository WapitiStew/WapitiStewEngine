//*****************************************************************************************************************
//!
//! @file    IuiErrorAdapter.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUIのKeyboard読取可否Stateを共通Binding Errorへ正規化する.
//! @brief   \~english  Normalizes the IUI keyboard readiness state into the common binding error.
//!
//! @details
//!     \~japanese
//!     @n IUIはStructured Error型を持たず、`iui::KeyboardAccessState`で読取可否を表す。
//!        本Adapterはその値を共通Categoryへ写し、元のStateを安定Codeとして保持する。
//!     \~english
//!     @n IUI has no structured error type and reports readiness through `iui::KeyboardAccessState`.
//!        This adapter maps that value to a common category while preserving the state as a stable code.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_IUIERRORADAPTER_H
#define WONDERSTEWENGINE_WSE_BINDING_IUIERRORADAPTER_H

#include "Error.h"
#include "../../dynamic.h"

#include <cstdint>

namespace wse
{
namespace iui
{
enum class KeyboardAccessState : std::uint8_t;
}

namespace binding
{

//! \~japanese Keyboard読取可否Stateを共通Categoryへ正規化する. `Ready`は成功を返す.
//! \~english  Normalizes the keyboard readiness state into a common category; `Ready` yields success.
WSE_API Error fromIuiKeyboardState( const iui::KeyboardAccessState state_in );

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_IUIERRORADAPTER_H
