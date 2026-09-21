//*****************************************************************************************************************
//!
//! @file    wse_capi_iui.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUI Keyboardの平坦C ABI.
//! @brief   \~english  Flat C ABI for the IUI keyboard.
//!
//! @details
//!     \~japanese
//!     @n Keyboardは生成時に監視Threadを持つRAII所有者である。呼出元は`wse_capi_keyboard_destroy`で
//!        1回だけ解放する。
//!     @n 本ABIはPollingだけを公開する。Callbackは関数Pointerと呼出元Tokenを跨いで所有権と実行Threadを
//!        規定する必要があるため、C ABIには含めない。Key stateは`snapshot`で一貫した1時点として取得する。
//!     @n Keyboardが読取不能な間、`snapshot`は`accessState`由来のStructured Errorを返す。
//!
//!     \~english
//!     @n A keyboard is an RAII owner that runs a monitoring thread from construction. The caller
//!        releases it exactly once with `wse_capi_keyboard_destroy`.
//!     @n This ABI exposes polling only. Callbacks are excluded because they would have to define
//!        ownership and the execution thread across a function pointer and a caller token; use
//!        `snapshot` to read one consistent point in time instead.
//!     @n While the keyboard is not readable, `snapshot` returns the structured error derived from
//!        `accessState`.
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

#ifndef WONDERSTEWENGINE_WSE_CAPI_IUI_H
#define WONDERSTEWENGINE_WSE_CAPI_IUI_H

#include "wse_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \~japanese Key group別の要素数. `iui/depend/Metadata.h`と一致する.
//! \~english  Element counts per key group, matching `iui/depend/Metadata.h`.
#define WSE_CAPI_KEYBOARD_ASCII_COUNT    128
#define WSE_CAPI_KEYBOARD_FUNCTION_COUNT  24
#define WSE_CAPI_KEYBOARD_ARROW_COUNT      4
#define WSE_CAPI_KEYBOARD_LOCK_COUNT       3
#define WSE_CAPI_KEYBOARD_COMMAND_COUNT    9

//! \~japanese `iui::KeyboardAccessState`と同じ値のBackend読取可否State.
//! \~english  Backend readiness state with the same values as `iui::KeyboardAccessState`.
enum wse_capi_keyboard_access_state
{
      WSE_CAPI_KEYBOARD_STARTING          = 0
    , WSE_CAPI_KEYBOARD_READY             = 1
    , WSE_CAPI_KEYBOARD_UNAVAILABLE       = 2
    , WSE_CAPI_KEYBOARD_PERMISSION_DENIED = 3
    , WSE_CAPI_KEYBOARD_DISCONNECTED      = 4
};

//! \~japanese 関数名と衝突しないC互換のState型Alias.
//! \~english  C-compatible state alias distinct from the query function name.
typedef enum wse_capi_keyboard_access_state wse_capi_keyboard_access_state_value;

//! \~japanese 1回の更新で一貫したKey入力Snapshot. 非0が押下、0が解放.
//! \~english  Key-input snapshot from one update; non-zero is pressed and zero is released.
typedef struct wse_capi_keyboard_state
{
    wse_capi_bool ascii[WSE_CAPI_KEYBOARD_ASCII_COUNT];
    wse_capi_bool function[WSE_CAPI_KEYBOARD_FUNCTION_COUNT];
    wse_capi_bool arrow[WSE_CAPI_KEYBOARD_ARROW_COUNT];
    wse_capi_bool lock[WSE_CAPI_KEYBOARD_LOCK_COUNT];
    wse_capi_bool command[WSE_CAPI_KEYBOARD_COMMAND_COUNT];
} wse_capi_keyboard_state;

//! \~japanese Keyboard Handle. 所有権は呼出元にある.
//! \~english  Keyboard handle owned by the caller.
typedef struct wse_capi_keyboard_t* wse_capi_keyboard;

//! \~japanese Keyboardを生成し監視を開始する.
//! \~english  Creates a keyboard and starts monitoring.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_create(
    wse_capi_keyboard* p_keyboard_out );

//! \~japanese Keyboardを解放する. `NULL`は無視する.
//! \~english  Releases a keyboard; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_keyboard_destroy(
    wse_capi_keyboard keyboard_inout );

//!
//! \~japanese
//! @brief   Backendの現在の読取可否Stateを取得する.
//! @details 読取不能なStateでも本関数自体は成功する. Stateの判定は呼出元が行う.
//! \~english
//! @brief   Reads the current backend readiness state.
//! @details This call succeeds even for a non-readable state; the caller decides how to react.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_access_state(
      wse_capi_keyboard keyboard_inout
    , int32_t*          p_state_out );

//! \~japanese 1つ以上のKeyboard sourceを読取可能な場合だけ非0を返す.
//! \~english  Reports non-zero only while at least one keyboard source is readable.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_is_available(
      wse_capi_keyboard keyboard_inout
    , wse_capi_bool*    p_available_out );

//!
//! \~japanese
//! @brief   全Key groupを同一更新時点で取得する.
//! @details 読取可能でない場合は`accessState`由来のStructured Errorを返し、`p_state_out`は変更しない.
//! \~english
//! @brief   Reads every key group from one update point.
//! @details When the keyboard is not readable it returns the structured error derived from
//!          `accessState` and leaves `p_state_out` unchanged.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_snapshot(
      wse_capi_keyboard        keyboard_inout
    , wse_capi_keyboard_state* p_state_out );

//!
//! \~japanese
//! @brief   押下中のASCII Codeを1つ取得する.
//! @details 複数入力には未対応で、小さいCodeを返す. 押下が無い場合は0を返す.
//! \~english
//! @brief   Reads one pressed ASCII code.
//! @details Multiple simultaneous input is unsupported and the smaller code wins; zero means none.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_keyboard_pressed_ascii(
      wse_capi_keyboard keyboard_inout
    , int8_t*           p_ascii_out );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_IUI_H
