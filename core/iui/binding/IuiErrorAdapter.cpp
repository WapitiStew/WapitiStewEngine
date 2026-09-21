//*****************************************************************************************************************
//!
//! @file    IuiErrorAdapter.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUI Keyboard StateからBinding Errorへの変換を実装する.
//! @brief   \~english  Implements conversion from the IUI keyboard state to binding errors.
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

#include <wse/binding/IuiErrorAdapter.h>
#include <iui/device/Keyboard.h>

namespace wse
{
namespace binding
{

Error fromIuiKeyboardState( const iui::KeyboardAccessState state_in )
{
    switch ( state_in )
    {
        case iui::KeyboardAccessState::Ready:
            return Error();

        case iui::KeyboardAccessState::Starting:
            return Error(
                  eErrorCategory::InvalidState
                , static_cast<std::int32_t>( state_in )
                , "The keyboard backend is still starting."
            );

        case iui::KeyboardAccessState::Unavailable:
            return Error(
                  eErrorCategory::NotFound
                , static_cast<std::int32_t>( state_in )
                , "No readable keyboard source is available."
            );

        case iui::KeyboardAccessState::PermissionDenied:
            return Error(
                  eErrorCategory::Security
                , static_cast<std::int32_t>( state_in )
                , "Reading the keyboard was refused by the operating system."
            );

        case iui::KeyboardAccessState::Disconnected:
            return Error(
                  eErrorCategory::InputOutput
                , static_cast<std::int32_t>( state_in )
                , "The keyboard source was disconnected."
            );
    }

    return Error(
          eErrorCategory::Internal
        , static_cast<std::int32_t>( state_in )
        , "The keyboard backend reported an unknown readiness state."
    );
}

} // namespace binding
} // namespace wse
