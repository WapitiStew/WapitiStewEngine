//*****************************************************************************************************************
//!
//! @file    wse_capi_core.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Core(Runtime、FrameBuffer、Cancellation)の平坦C ABI.
//! @brief   \~english  Flat C ABI for Core: runtime, frame buffer, and cancellation.
//!
//! @details
//!     \~japanese
//!     @n `wse::binding::Runtime`と同じ意味論をC ABIで公開する。Handleの所有者は常に呼出元であり、
//!        対応する`*_destroy`を1回だけ呼ぶ。`NULL` Handleに対する`*_destroy`は無害である。
//!     @n 文字列取得は2回呼出方式である。`p_buffer_out`に`NULL`を渡すと必要Byte数だけを`p_size_out`へ返す。
//!
//!     \~english
//!     @n Exposes the semantics of `wse::binding::Runtime` through a C ABI. The caller always owns
//!        each handle and calls the matching `*_destroy` exactly once; destroying `NULL` is a no-op.
//!     @n String getters use the two-call pattern: pass `NULL` for `buffer` to learn the required size.
//!
//! @date
//!   Sep-19, 2026   Clarify independent frame-buffer ownership.
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

#ifndef WONDERSTEWENGINE_WSE_CAPI_CORE_H
#define WONDERSTEWENGINE_WSE_CAPI_CORE_H

#include "wse_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \~japanese Buildへ組み込まれたOptional Componentの有無.
//! \~english  Availability of the optional components compiled into this build.
typedef struct wse_capi_components
{
    wse_capi_bool has_xpt;
    wse_capi_bool has_tmr;
    wse_capi_bool has_oui;
    wse_capi_bool has_gef;
    wse_capi_bool has_iui;
    wse_capi_bool has_vpj;
} wse_capi_components;

//!
//! \~japanese
//! @brief   Runtimeを生成する.
//! @param [out] p_runtime_out  生成したHandleの格納先. `NULL`不可.
//! \~english
//! @brief   Creates a runtime.
//! @param [out] p_runtime_out  Destination for the created handle; must not be `NULL`.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_create(
    wse_capi_runtime* p_runtime_out );

//! \~japanese Runtimeを解放する. `NULL`は無視する.
//! \~english  Releases a runtime; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_runtime_destroy(
    wse_capi_runtime runtime_inout );

//!
//! \~japanese
//! @brief   Semantic version文字列をUTF-8で取得する.
//! @param [in]  runtime_in      取得元Runtime handle. `NULL`のときInvalidArgumentを返す.
//! @param [out] p_buffer_out    格納先. `NULL`のとき必要Byte数のみを返す.
//! @param [in]  capacity_in     `p_buffer_out`のByte数.
//! @param [out] p_size_out      終端NULを含む必要Byte数. `NULL`不可.
//! \~english
//! @brief   Reads the semantic version string as UTF-8.
//! @param [in]  runtime_in      Runtime to read from; `NULL` yields InvalidArgument.
//! @param [out] p_buffer_out    Destination, or `NULL` to query the required size only.
//! @param [in]  capacity_in     Size of `p_buffer_out` in bytes.
//! @param [out] p_size_out      Required size including the terminating NUL; must not be `NULL`.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_version(
      wse_capi_runtime runtime_in
    , char*            p_buffer_out
    , size_t*          p_size_out
    , size_t           capacity_in );

//! \~japanese Binding ABI版を取得する.
//! \~english  Reads the binding ABI version.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_binding_abi_version(
      wse_capi_runtime runtime_in
    , uint32_t*        p_version_out );

//! \~japanese 組み込み済みOptional Componentを取得する.
//! \~english  Reads which optional components are compiled in.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_components(
      wse_capi_runtime     runtime_in
    , wse_capi_components* p_components_out );

//!
//! \~japanese
//! @brief   呼出元Byte列を独立所有のBufferへCopyする.
//! @details 入力Byte列を借用し続けない. 成功時のHandleは呼出元が所有し、入力の変更やRuntimeの
//!          破棄後も有効である. `wse_capi_frame_buffer_destroy`で別途1回解放する.
//!          `size_in`が0のときは空Bufferを返す.
//! \~english
//! @brief   Copies caller bytes into an independently owned buffer.
//! @details Does not keep borrowing the input bytes. On success, the caller owns the returned handle;
//!          it remains valid after input mutation or Runtime destruction. Release it separately,
//!          exactly once, with `wse_capi_frame_buffer_destroy`. A zero `size_in` yields an empty buffer.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_copy_frame(
      wse_capi_runtime       runtime_inout
    , wse_capi_frame_buffer* p_buffer_out
    , const uint8_t*         bytes_in
    , size_t                 size_in );

//!
//! \~japanese
//! @brief   Cancellationを協調的に観測する有限待機.
//! @param [in] runtime_in        待機を行うRuntime handle. `NULL`のときInvalidArgumentを返す.
//! @param [in] milliseconds_in   0以上の待機時間.
//! @param [in] cancellation_in   観測するHandle. `NULL`のとき取消なしで待機する.
//! \~english
//! @brief   Finite wait that cooperatively observes cancellation.
//! @param [in] runtime_in        Runtime that performs the wait; `NULL` yields InvalidArgument.
//! @param [in] milliseconds_in   Non-negative wait duration.
//! @param [in] cancellation_in   Handle to observe, or `NULL` to wait without cancellation.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_runtime_wait(
      wse_capi_runtime      runtime_in
    , int64_t               milliseconds_in
    , wse_capi_cancellation cancellation_in );

//! \~japanese Bufferを解放する. `NULL`は無視する.
//! \~english  Releases a buffer; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_frame_buffer_destroy(
    wse_capi_frame_buffer buffer_inout );

//! \~japanese BufferのByte数を取得する.
//! \~english  Reads the buffer size in bytes.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_size(
      wse_capi_frame_buffer buffer_in
    , size_t*               p_size_out );

//!
//! \~japanese
//! @brief   Buffer内容を呼出元Memoryへ複製する.
//! @param [in]  buffer_in          複製元Buffer handle. `NULL`のときInvalidArgumentを返す.
//! @param [out] p_destination_out  格納先. `NULL`のとき必要Byte数のみを返す.
//! @param [in]  capacity_in        `p_destination_out`のByte数.
//! @param [out] p_written_out      複製したByte数. `NULL`不可.
//! \~english
//! @brief   Copies the buffer contents into caller memory.
//! @param [in]  buffer_in          Source buffer; `NULL` yields InvalidArgument.
//! @param [out] p_destination_out  Destination, or `NULL` to query the required size only.
//! @param [in]  capacity_in        Size of `p_destination_out` in bytes.
//! @param [out] p_written_out      Number of bytes copied; must not be `NULL`.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_copy_to(
      wse_capi_frame_buffer buffer_in
    , uint8_t*              p_destination_out
    , size_t*               p_written_out
    , size_t                capacity_in );

//! \~japanese Cancellation Sourceを生成する.
//! \~english  Creates a cancellation source.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_create(
    wse_capi_cancellation* p_cancellation_out );

//! \~japanese Cancellationを解放する. `NULL`は無視する.
//! \~english  Releases a cancellation source; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_cancellation_destroy(
    wse_capi_cancellation cancellation_inout );

//! \~japanese 取消を要求する. 何度呼んでも安全である.
//! \~english  Requests cancellation; safe to call repeatedly.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_cancel(
    wse_capi_cancellation cancellation_inout );

//! \~japanese 取消要求の有無を取得する.
//! \~english  Reads whether cancellation was requested.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_is_requested(
      wse_capi_cancellation cancellation_in
    , wse_capi_bool*        p_requested_out );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_CORE_H
