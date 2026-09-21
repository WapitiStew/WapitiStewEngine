//*****************************************************************************************************************
//!
//! @file    Win32WindowTransition.h
//! @brief   \~japanese 内部所有Win32 WindowのResize／Borderless復元境界を定義する.
//! @brief   \~english  Defines resize and borderless-restoration boundaries for owned Win32 windows.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
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

#pragma once
#ifndef WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_WIN32WINDOWTRANSITION_H
#define WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_WIN32WINDOWTRANSITION_H

#include <utility>
#include "../../../../api/oui/renderer/RendererTypes.h"

#include <Windows.h>

namespace wse
{
namespace oui
{
namespace internal
{

//! \~japanese Borderless遷移前の復元情報. \~english Restoration data captured before borderless transition.
struct sWin32WindowedState
{
    LONG_PTR        style;
    LONG_PTR        extended_style;
    WINDOWPLACEMENT placement;
    bool            visible;

    //! @brief Construct all members with explicit defaults.
    sWin32WindowedState(
          const LONG_PTR& style_in = 0
        , const LONG_PTR& extended_style_in = 0
        , const WINDOWPLACEMENT& placement_in = { sizeof( WINDOWPLACEMENT ) }
        , bool visible_in = false
    )
        : style          ( style_in )
        , extended_style ( extended_style_in )
        , placement      ( placement_in )
        , visible        ( visible_in )
    {
    }
};

//! @return \~japanese 現在のWindowed復元情報. \~english Current windowed restoration data.
RendererResult< sWin32WindowedState > captureWin32WindowedState( HWND window_in );

//! @return \~japanese Client extent変更結果. \~english Client-extent resize result.
RendererError resizeWin32WindowClient(
      HWND                    window_in
    , const sRendererExtent2D extent_in
);

//! @return \~japanese 指定Desktop領域へのBorderless遷移結果. \~english Borderless transition result.
RendererError applyWin32BorderlessWindow(
      HWND                     window_in
    , const sWin32WindowedState& restore_state_in
    , const sDisplayPosition2D position_in
    , const sRendererExtent2D  extent_in
);

//! @return \~japanese 保存済みWindowed状態への復元結果. \~english Saved-windowed-state restoration result.
RendererError restoreWin32WindowedState(
      HWND                     window_in
    , const sWin32WindowedState& state_in
);

} // namespace internal
} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_WIN32WINDOWTRANSITION_H
