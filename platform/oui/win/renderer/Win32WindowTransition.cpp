//*****************************************************************************************************************
//!
//! @file    Win32WindowTransition.cpp
//! @brief   \~japanese 内部所有Win32 WindowのResize／Borderless復元を実装する.
//! @brief   \~english  Implements resize and borderless restoration for owned Win32 windows.
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

#include "Win32WindowTransition.h"

#include <cstdint>
#include <string>

namespace
{

wse::oui::RendererError windowError(
      const std::string& message_in
    , const DWORD        native_error_in
)
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Backend
        , wse::oui::eRendererErrorCode::BackendFailure
        , message_in
        , static_cast< std::int64_t >( HRESULT_FROM_WIN32( native_error_in ) )
    );
}

wse::oui::RendererResult< LONG_PTR > getWindowValue(
      const HWND window_in
    , const int  index_in
    , const char* const failure_message_in
)
{
    SetLastError( ERROR_SUCCESS );
    const LONG_PTR value = GetWindowLongPtrW( window_in, index_in );
    const DWORD native_error = GetLastError();
    if( value == 0 && native_error != ERROR_SUCCESS )
    {
        return wse::oui::RendererResult< LONG_PTR >::failure( windowError( failure_message_in, native_error ) );
    }
    return wse::oui::RendererResult< LONG_PTR >::success( value );
}

wse::oui::RendererError setWindowValue(
      const HWND     window_in
    , const int      index_in
    , const LONG_PTR value_in
    , const char* const failure_message_in
)
{
    SetLastError( ERROR_SUCCESS );
    const LONG_PTR previous_value = SetWindowLongPtrW( window_in, index_in, value_in );
    const DWORD native_error = GetLastError();
    if( previous_value == 0 && native_error != ERROR_SUCCESS )
    {
        return windowError( failure_message_in, native_error );
    }
    return wse::oui::RendererError();
}

wse::oui::RendererError verifyClientExtent(
      const HWND                       window_in
    , const wse::oui::sRendererExtent2D expected_extent_in
)
{
    RECT client_rectangle = {};
    if( GetClientRect( window_in, &client_rectangle ) == FALSE )
    {
        return windowError( "Failed to query the renderer client extent.", GetLastError() );
    }
    const LONG width = client_rectangle.right - client_rectangle.left;
    const LONG height = client_rectangle.bottom - client_rectangle.top;
    if( width != static_cast< LONG >( expected_extent_in.width ) ||
        height != static_cast< LONG >( expected_extent_in.height ) )
    {
        return wse::oui::RendererError(
              wse::oui::eRendererErrorCategory::Backend
            , wse::oui::eRendererErrorCode::BackendFailure
            , "Renderer client extent does not match the requested extent."
        );
    }
    return wse::oui::RendererError();
}

wse::oui::RendererError transitionWithRollbackError(
      const wse::oui::RendererError& transition_error_in
    , const wse::oui::RendererError& rollback_error_in
)
{
    if( rollback_error_in.ok() )
    {
        return transition_error_in;
    }
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Backend
        , wse::oui::eRendererErrorCode::BackendFailure
        , transition_error_in.message() + " Rollback also failed: " + rollback_error_in.message()
        , rollback_error_in.nativeCode()
    );
}

} // namespace

namespace wse
{
namespace oui
{
namespace internal
{

RendererResult< sWin32WindowedState > captureWin32WindowedState( const HWND window_in )
{
    if( window_in == nullptr || IsWindow( window_in ) == FALSE )
    {
        return RendererResult< sWin32WindowedState >::failure( RendererError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::ResourceNotFound
                , "Renderer window is unavailable."
            )
        );
    }
    const auto style_result = getWindowValue(
        window_in, GWL_STYLE, "Failed to query the renderer window style." );
    if( !style_result.succeeded() )
    {
        return RendererResult< sWin32WindowedState >::failure( style_result.error() );
    }
    const auto extended_style_result = getWindowValue(
        window_in, GWL_EXSTYLE, "Failed to query the renderer extended window style." );
    if( !extended_style_result.succeeded() )
    {
        return RendererResult< sWin32WindowedState >::failure( extended_style_result.error() );
    }

    sWin32WindowedState state;
    state.style          = style_result.value();
    state.extended_style = extended_style_result.value();
    state.visible        = IsWindowVisible( window_in ) != FALSE;
    state.placement.length = sizeof( WINDOWPLACEMENT );
    if( GetWindowPlacement( window_in, &state.placement ) == FALSE )
    {
        return RendererResult< sWin32WindowedState >::failure( windowError( "Failed to capture renderer window placement.", GetLastError() ) );
    }
    return RendererResult< sWin32WindowedState >::success( state );
}

RendererError resizeWin32WindowClient(
      const HWND              window_in
    , const sRendererExtent2D extent_in
)
{
    if( window_in == nullptr || IsWindow( window_in ) == FALSE )
    {
        return RendererError(
              eRendererErrorCategory::Lifecycle
            , eRendererErrorCode::ResourceNotFound
            , "Renderer window is unavailable."
        );
    }
    const auto style_result = getWindowValue(
        window_in, GWL_STYLE, "Failed to query the renderer window style." );
    const auto extended_style_result = getWindowValue(
        window_in, GWL_EXSTYLE, "Failed to query the renderer extended window style." );
    if( !style_result.succeeded() )
    {
        return style_result.error();
    }
    if( !extended_style_result.succeeded() )
    {
        return extended_style_result.error();
    }
    RECT outer_rectangle = {
          0L
        , 0L
        , static_cast< LONG >( extent_in.width )
        , static_cast< LONG >( extent_in.height )
    };
    if( AdjustWindowRectEx(
            &outer_rectangle
          , static_cast< DWORD >( style_result.value() )
          , FALSE
          , static_cast< DWORD >( extended_style_result.value() ) ) == FALSE )
    {
        return windowError( "Failed to calculate resized window framing.", GetLastError() );
    }
    RECT current_rectangle = {};
    if( GetWindowRect( window_in, &current_rectangle ) == FALSE )
    {
        return windowError( "Failed to query renderer window bounds.", GetLastError() );
    }
    if( SetWindowPos(
            window_in
          , nullptr
          , current_rectangle.left
          , current_rectangle.top
          , outer_rectangle.right - outer_rectangle.left
          , outer_rectangle.bottom - outer_rectangle.top
          , SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER ) == FALSE )
    {
        return windowError( "Failed to resize the renderer window.", GetLastError() );
    }
    return verifyClientExtent( window_in, extent_in );
}

RendererError applyWin32BorderlessWindow(
      const HWND                       window_in
    , const sWin32WindowedState&       restore_state_in
    , const sDisplayPosition2D         position_in
    , const sRendererExtent2D          extent_in
)
{
    const LONG_PTR borderless_style =
        ( restore_state_in.style & ~static_cast< LONG_PTR >( WS_OVERLAPPEDWINDOW ) ) |
        static_cast< LONG_PTR >( WS_POPUP );
    const RendererError style_error = setWindowValue(
        window_in, GWL_STYLE, borderless_style,
        "Failed to apply the borderless renderer window style." );
    if( !style_error.ok() )
    {
        return style_error;
    }
    if( SetWindowPos(
            window_in
          , HWND_TOP
          , position_in.x
          , position_in.y
          , static_cast< int >( extent_in.width )
          , static_cast< int >( extent_in.height )
          , SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER ) == FALSE )
    {
        const DWORD native_error = GetLastError();
        const RendererError transition_error = windowError(
            "Failed to place the borderless renderer window.", native_error );
        const RendererError rollback_error = restoreWin32WindowedState(
            window_in, restore_state_in );
        return transitionWithRollbackError( transition_error, rollback_error );
    }
    const RendererError extent_error = verifyClientExtent( window_in, extent_in );
    if( !extent_error.ok() )
    {
        const RendererError rollback_error = restoreWin32WindowedState(
            window_in, restore_state_in );
        return transitionWithRollbackError( extent_error, rollback_error );
    }
    return RendererError();
}

RendererError restoreWin32WindowedState(
      const HWND                       window_in
    , const sWin32WindowedState&       state_in
)
{
    const RendererError style_error = setWindowValue(
        window_in, GWL_STYLE, state_in.style,
        "Failed to restore the renderer window style." );
    if( !style_error.ok() )
    {
        return style_error;
    }
    const RendererError extended_style_error = setWindowValue(
        window_in, GWL_EXSTYLE, state_in.extended_style,
        "Failed to restore the renderer extended window style." );
    if( !extended_style_error.ok() )
    {
        return extended_style_error;
    }
    WINDOWPLACEMENT placement = state_in.placement;
    placement.length = sizeof( WINDOWPLACEMENT );
    if( SetWindowPlacement( window_in, &placement ) == FALSE )
    {
        return windowError( "Failed to restore renderer window placement.", GetLastError() );
    }
    const UINT visibility_flag = state_in.visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    if( SetWindowPos(
            window_in
          , nullptr
          , 0
          , 0
          , 0
          , 0
          , SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                SWP_NOOWNERZORDER | SWP_NOZORDER | visibility_flag ) == FALSE )
    {
        return windowError( "Failed to refresh restored renderer window framing.", GetLastError() );
    }
    return RendererError();
}

} // namespace internal
} // namespace oui
} // namespace wse
