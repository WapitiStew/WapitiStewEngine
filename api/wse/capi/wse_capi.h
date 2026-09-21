//*****************************************************************************************************************
//!
//! @file    wse_capi.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese C ABIとして公開する共通型、Error契約およびExport定義.
//! @brief   \~english  Shared types, error contract, and export definitions for the flat C ABI.
//!
//! @details
//!     \~japanese
//!     @n 本Headerは、C++ Classを直接呼び出せないLanguage Runtime（C#のP/Invoke等）へ、
//!        安定した平坦C ABIを公開する。公開面は`wse::binding`の facade と同じ意味論を持つ。
//!     @n Handleは不透明Struct Pointerであり、Raw void pointerもOS Native handleも公開しない。
//!
//!     \~english
//!     @n This header exposes a stable flat C ABI for language runtimes that cannot call C++
//!        classes directly, such as C# P/Invoke. Its semantics match the `wse::binding` facade.
//!     @n Handles are opaque struct pointers; neither raw void pointers nor OS native handles leak.
//!
//! @date
//!   Sep-19, 2026   Clarify status-returning operations and thread-local diagnostic lifetime.
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

#ifndef WONDERSTEWENGINE_WSE_CAPI_H
#define WONDERSTEWENGINE_WSE_CAPI_H

#include <stddef.h>
#include <stdint.h>

//! \~japanese
//! C ABI用のExport定義. C++の`WSE_API`とは独立させ、C翻訳単位でも使用できるようにする.
//! Core側のLibrary種別(`WSE_STATIC`)とは独立に判定する。C ABIはP/Invokeが解決できる共有Library
//! として建てるため、Coreを静的Linkした構成でもExportを抑止してはならない。
//! \~english
//! Export definition for the C ABI, kept independent of `WSE_API` so C units can use it.
//! It is decided independently of the core library type (`WSE_STATIC`): the C ABI ships as a shared
//! library that P/Invoke can resolve, so exports must not be suppressed when the core is static.
#if defined( WSE_CAPI_STATIC )
  #define WSE_CAPI
#elif defined( _WIN32 ) || defined( _WIN64 ) || defined( __CYGWIN__ )
  #if defined( __WSE_CAPI_EXPORTS__ )
    #define WSE_CAPI __declspec( dllexport )
  #else
    #define WSE_CAPI __declspec( dllimport )
  #endif
#else
  #if defined( __WSE_CAPI_EXPORTS__ )
    #define WSE_CAPI __attribute__(( visibility( "default" ) ))
  #else
    #define WSE_CAPI
  #endif
#endif

#if defined( _WIN32 ) || defined( _WIN64 )
  #define WSE_CAPI_CALL __cdecl
#else
  #define WSE_CAPI_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

//!
//! \~japanese
//!     平坦C ABI自体の版.
//!  @n Binding ABI版(`wse::binding::Runtime::BINDING_ABI_VERSION`)とは独立した番号である.
//!  @n 呼出側は`wse_capi_abi_version()`で実際にLoadしたLibraryの版を読み、構築時の
//!     `WSE_CAPI_ABI_VERSION`と一致することを確認する.
//!  @n 関数の追加は互換であり版を変えない. 構造体の拡張と引数の変更は非互換であり版を上げる.
//! \~english
//!     Version of the flat C ABI itself.
//!  @n It is a number of its own, independent of the binding ABI version reported by
//!     `wse::binding::Runtime::BINDING_ABI_VERSION`.
//!  @n A caller reads the version of the library it actually loaded with `wse_capi_abi_version()`
//!     and checks it against the `WSE_CAPI_ABI_VERSION` it was built against.
//!  @n Adding a function is compatible and leaves the number alone. Growing a structure or
//!     changing an argument list is not, and raises it.
//!
#define WSE_CAPI_ABI_VERSION 1U

//! \~japanese C ABIのBoolean. 0が偽、非0が真.
//! \~english  Boolean for the C ABI: zero is false and non-zero is true.
typedef int32_t wse_capi_bool;

//! \~japanese `wse::binding::eErrorCategory`と同じ値のPortable Error分類.
//! \~english  Portable error categories with the same values as `wse::binding::eErrorCategory`.
typedef enum wse_capi_error_category
{
      WSE_CAPI_ERROR_NONE               = 0
    , WSE_CAPI_ERROR_INVALID_ARGUMENT   = 1
    , WSE_CAPI_ERROR_NOT_FOUND          = 2
    , WSE_CAPI_ERROR_INVALID_STATE      = 3
    , WSE_CAPI_ERROR_INPUT_OUTPUT       = 4
    , WSE_CAPI_ERROR_TIMEOUT            = 5
    , WSE_CAPI_ERROR_CANCELLATION       = 6
    , WSE_CAPI_ERROR_PROTOCOL           = 7
    , WSE_CAPI_ERROR_SECURITY           = 8
    , WSE_CAPI_ERROR_UNSUPPORTED        = 9
    , WSE_CAPI_ERROR_RESOURCE_EXHAUSTED = 10
    , WSE_CAPI_ERROR_INTERNAL           = 11
} wse_capi_error_category;

//! \~japanese Statusを返すC ABI操作のError契約. `category`が`WSE_CAPI_ERROR_NONE`のとき成功.
//! \~english  Error contract for status-returning C ABI operations; success when `category` is none.
typedef struct wse_capi_status
{
    int32_t category;    //!< \~japanese `wse_capi_error_category`の値. \~english A `wse_capi_error_category` value.
    int32_t code;        //!< \~japanese Component固有Code. \~english Component-specific code.
    int64_t native_code; //!< \~japanese OS/第三者Libraryの生Code. \~english Native OS or third-party code.
} wse_capi_status;

//! \~japanese Runtime Handle. 所有権は呼出元にあり、`wse_capi_runtime_destroy`で解放する.
//! \~english  Runtime handle owned by the caller and released with `wse_capi_runtime_destroy`.
typedef struct wse_capi_runtime_t* wse_capi_runtime;

//! \~japanese 不変Byte列Handle. 所有権は呼出元にあり、`wse_capi_frame_buffer_destroy`で解放する.
//! \~english  Immutable byte-buffer handle released with `wse_capi_frame_buffer_destroy`.
typedef struct wse_capi_frame_buffer_t* wse_capi_frame_buffer;

//! \~japanese Cancellation Handle. 所有権は呼出元にあり、`wse_capi_cancellation_destroy`で解放する.
//! \~english  Cancellation handle released with `wse_capi_cancellation_destroy`.
typedef struct wse_capi_cancellation_t* wse_capi_cancellation;

//! \~japanese 実装が公開する平坦C ABIの版を返す.
//! \~english  Returns the flat C ABI version implemented by this library.
WSE_CAPI uint32_t WSE_CAPI_CALL wse_capi_abi_version( void );

//!
//! \~japanese
//! @brief   直近の失敗Messageを返す.
//! @details 呼出Thread単位で保持するUTF-8 NUL終端文字列を借用する.
//!          同一Thread上の次のStatusを返す操作より前にCopyする. その操作は成功時に診断を消去し、
//!          失敗時に置換する. 他Threadの操作はこの診断を変更しない.
//!          診断が無い場合は空文字列を返す. 返却Pointerを解放してはならない.
//!          診断保存は動的確保を行わず、最大1023 Byteと終端NULを保持する.
//!          有効なUTF-8入力はCode point境界で切り詰める. Category／Codeは切り詰めない.
//! \~english
//! @brief   Returns the most recent failure message.
//! @details Borrows a UTF-8 NUL-terminated string stored per calling thread. Copy it before the next
//!          status-returning operation on that thread: success clears the diagnostic and failure
//!          replaces it. Calls on another thread do not change this diagnostic.
//!          An empty string is returned when no diagnostic is stored. Do not free the pointer.
//!          Diagnostic storage does not allocate: at most 1023 bytes plus NUL are retained.
//!          Valid UTF-8 input is truncated at a code-point boundary; category/code are unchanged.
//!
WSE_CAPI const char* WSE_CAPI_CALL wse_capi_last_error_message( void );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_H
