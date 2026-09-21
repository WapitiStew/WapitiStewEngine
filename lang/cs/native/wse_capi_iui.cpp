//*****************************************************************************************************************
//!
//! @file    wse_capi_iui.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUI Keyboardの平坦C ABI実装.
//! @brief   \~english  Implementation of the flat C ABI for the IUI keyboard.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/capi/wse_capi_iui.h>

#include "capi_internal.h"

#include <wse/binding/IuiErrorAdapter.h>
#include <iui/stew.h>

#include <array>
#include <cstddef>
#include <new>

namespace
{

//! \~japanese C ABIのArray長がIUIのMetadataと一致することをCompile時に固定する.
//! \~english  Pins the C ABI array lengths to the IUI metadata at compile time.
static_assert(
      WSE_CAPI_KEYBOARD_ASCII_COUNT == static_cast<int>( wse::iui::ASCII_NUM )
    , "The C ABI ASCII key count must match iui::ASCII_NUM." );
static_assert(
      WSE_CAPI_KEYBOARD_FUNCTION_COUNT == static_cast<int>( wse::iui::FANCTION_NUM )
    , "The C ABI function key count must match iui::FANCTION_NUM." );
static_assert(
      WSE_CAPI_KEYBOARD_ARROW_COUNT == static_cast<int>( wse::iui::ARROW_NUM )
    , "The C ABI arrow key count must match iui::ARROW_NUM." );
static_assert(
      WSE_CAPI_KEYBOARD_LOCK_COUNT == static_cast<int>( wse::iui::LOCK_NUM )
    , "The C ABI lock key count must match iui::LOCK_NUM." );
static_assert(
      WSE_CAPI_KEYBOARD_COMMAND_COUNT == static_cast<int>( wse::iui::COMMAND_NUM )
    , "The C ABI command key count must match iui::COMMAND_NUM." );

template <std::size_t Count>
void copyGroup( const std::array<bool, Count>& source_in, wse_capi_bool* destination_in )
{
    for ( std::size_t index = 0U; index < Count; ++index )
    {
        destination_in[index] = source_in[index] ? 1 : 0;
    }
}

} // namespace

//! \~japanese Keyboard Handleの実体.
//! \~english  Concrete body behind the opaque keyboard handle.
struct wse_capi_keyboard_t final
{
    wse::iui::Keyboard keyboard;
};

extern "C"
{

wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_create( wse_capi_keyboard* p_keyboard_out )
{
    return wse::capi::guard( [p_keyboard_out]() -> wse_capi_status
    {
        if ( p_keyboard_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_keyboard_out must not be null." );
        }
        *p_keyboard_out = new wse_capi_keyboard_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_keyboard_destroy( wse_capi_keyboard keyboard_inout )
{
    delete keyboard_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_access_state(
      wse_capi_keyboard keyboard_inout
    , int32_t* p_state_out )
{
    return wse::capi::guard( [keyboard_inout, p_state_out]() -> wse_capi_status
    {
        if ( keyboard_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "keyboard_inout must not be null." );
        }
        if ( p_state_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_state_out must not be null." );
        }
        *p_state_out = static_cast<std::int32_t>( keyboard_inout->keyboard.accessState() );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_is_available(
      wse_capi_keyboard keyboard_inout
    , wse_capi_bool* p_available_out )
{
    return wse::capi::guard( [keyboard_inout, p_available_out]() -> wse_capi_status
    {
        if ( keyboard_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "keyboard_inout must not be null." );
        }
        if ( p_available_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_available_out must not be null." );
        }
        *p_available_out = keyboard_inout->keyboard.isAvailable() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_snapshot(
      wse_capi_keyboard keyboard_inout
    , wse_capi_keyboard_state* p_state_out )
{
    return wse::capi::guard( [keyboard_inout, p_state_out]() -> wse_capi_status
    {
        if ( keyboard_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "keyboard_inout must not be null." );
        }
        if ( p_state_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_state_out must not be null." );
        }

        const wse::binding::Error readiness =
            wse::binding::fromIuiKeyboardState( keyboard_inout->keyboard.accessState() );
        if ( !readiness.ok() )
        {
            return wse::capi::fromBindingError( readiness );
        }

        const wse::iui::KeyboardState snapshot = keyboard_inout->keyboard.snapshot();
        copyGroup( snapshot.ascii, p_state_out->ascii );
        copyGroup( snapshot.function, p_state_out->function );
        copyGroup( snapshot.arrow, p_state_out->arrow );
        copyGroup( snapshot.lock, p_state_out->lock );
        copyGroup( snapshot.command, p_state_out->command );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_pressed_ascii(
      wse_capi_keyboard keyboard_inout
    , int8_t* p_ascii_out )
{
    return wse::capi::guard( [keyboard_inout, p_ascii_out]() -> wse_capi_status
    {
        if ( keyboard_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "keyboard_inout must not be null." );
        }
        if ( p_ascii_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_ascii_out must not be null." );
        }

        const wse::binding::Error readiness =
            wse::binding::fromIuiKeyboardState( keyboard_inout->keyboard.accessState() );
        if ( !readiness.ok() )
        {
            return wse::capi::fromBindingError( readiness );
        }

        *p_ascii_out = keyboard_inout->keyboard.getASCII();
        return wse::capi::makeSuccess();
    } );
}

} // extern "C"
