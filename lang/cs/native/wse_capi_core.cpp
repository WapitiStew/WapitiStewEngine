//*****************************************************************************************************************
//!
//! @file    wse_capi_core.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Core平坦C ABIの実装. `wse::binding` facadeへ委譲する.
//! @brief   \~english  Implementation of the Core flat C ABI, delegating to the `wse::binding` facade.
//!
//! @details
//!     \~japanese
//!     @n C ABI境界からC++例外を伝播させない。全ての公開関数はException boundaryで捕捉し、
//!        `wse_capi_status`へ変換する。失敗Messageは呼出Thread単位に保持する。
//!     \~english
//!     @n No C++ exception crosses the C ABI boundary. Every exported function catches at the
//!        exception boundary and converts to `wse_capi_status`; failure messages are thread-local.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/capi/wse_capi_core.h>

#include "capi_internal.h"

#include <wse/binding/stew.h>

#include <chrono>
#include <cstring>
#include <utility>
#include <vector>

extern "C"
{

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_create( wse_capi_runtime* p_runtime_out )
{
    return wse::capi::guard( [p_runtime_out]() -> wse_capi_status
    {
        if( p_runtime_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_runtime_out must not be null." );
        }
        *p_runtime_out = new wse_capi_runtime_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_runtime_destroy( wse_capi_runtime runtime_inout )
{
    delete runtime_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_version(
      wse_capi_runtime runtime_in
    , char*            p_buffer_out
    , size_t*          p_size_out
    , size_t           capacity_in )
{
    return wse::capi::guard( [runtime_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if( runtime_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "runtime_in must not be null." );
        }
        const wse::binding::sRuntimeInfo info = runtime_in->runtime.info();
        return wse::capi::copyString( p_buffer_out, p_size_out, info.version, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_binding_abi_version(
      wse_capi_runtime runtime_in
    , uint32_t* p_version_out )
{
    return wse::capi::guard( [runtime_in, p_version_out]() -> wse_capi_status
    {
        if( runtime_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "runtime_in must not be null." );
        }
        if( p_version_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_version_out must not be null." );
        }
        *p_version_out = runtime_in->runtime.info().binding_abi_version;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_components(
      wse_capi_runtime runtime_in
    , wse_capi_components* p_components_out )
{
    return wse::capi::guard( [runtime_in, p_components_out]() -> wse_capi_status
    {
        if( runtime_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "runtime_in must not be null." );
        }
        if( p_components_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_components_out must not be null." );
        }
        const wse::binding::sRuntimeInfo info = runtime_in->runtime.info();
        p_components_out->has_xpt = info.has_xpt ? 1 : 0;
        p_components_out->has_tmr = info.has_tmr ? 1 : 0;
        p_components_out->has_oui = info.has_oui ? 1 : 0;
        p_components_out->has_gef = info.has_gef ? 1 : 0;
        p_components_out->has_iui = info.has_iui ? 1 : 0;
        p_components_out->has_vpj = info.has_vpj ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_copy_frame(
      wse_capi_runtime       runtime_inout
    , wse_capi_frame_buffer* p_buffer_out
    , const uint8_t*         bytes_in
    , size_t                 size_in )
{
    return wse::capi::guard( [runtime_inout, bytes_in, size_in, p_buffer_out]() -> wse_capi_status
    {
        if( runtime_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "runtime_inout must not be null." );
        }
        if( p_buffer_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_buffer_out must not be null." );
        }
        if( bytes_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument( "bytes_in must not be null when size_in is non-zero." );
        }

        const std::vector<std::uint8_t> source(
              bytes_in
            , ( bytes_in == nullptr ) ? bytes_in : ( bytes_in + size_in ) );
        const wse::binding::Result<wse::binding::FrameBuffer> result =
            runtime_inout->runtime.copyFrame( source );
        if( !result.succeeded() )
        {
            return wse::capi::fromBindingError( result.error() );
        }
        *p_buffer_out = new wse_capi_frame_buffer_t( result.value() );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_wait(
      wse_capi_runtime runtime_in
    , int64_t milliseconds_in
    , wse_capi_cancellation cancellation_in )
{
    return wse::capi::guard( [runtime_in, milliseconds_in, cancellation_in]() -> wse_capi_status
    {
        if( runtime_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "runtime_in must not be null." );
        }
        if( milliseconds_in < 0 )
        {
            return wse::capi::makeInvalidArgument( "milliseconds_in must not be negative." );
        }

        const std::chrono::milliseconds duration( milliseconds_in );
        const wse::binding::CancellationToken token =
            ( cancellation_in == nullptr )
                ? wse::binding::CancellationToken()
                : cancellation_in->source.token();
        const wse::binding::Status status = runtime_in->runtime.wait( duration, token );
        return status.succeeded()
            ? wse::capi::makeSuccess()
            : wse::capi::fromBindingError( status.error() );
    } );
}

void WSE_CAPI_CALL wse_capi_frame_buffer_destroy( wse_capi_frame_buffer buffer_inout )
{
    delete buffer_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_size(
      wse_capi_frame_buffer buffer_in
    , size_t* p_size_out )
{
    return wse::capi::guard( [buffer_in, p_size_out]() -> wse_capi_status
    {
        if( buffer_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "buffer_in must not be null." );
        }
        if( p_size_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_size_out must not be null." );
        }
        *p_size_out = buffer_in->buffer.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_copy_to(
      wse_capi_frame_buffer buffer_in
    , uint8_t*              p_destination_out
    , size_t*               p_written_out
    , size_t                capacity_in )
{
    return wse::capi::guard( [buffer_in, p_destination_out, capacity_in, p_written_out]() -> wse_capi_status
    {
        if( buffer_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "buffer_in must not be null." );
        }
        if( p_written_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_written_out must not be null." );
        }

        const std::vector<std::uint8_t>& bytes = buffer_in->buffer.bytes();
        *p_written_out = bytes.size();
        if( p_destination_out == nullptr )
        {
            return wse::capi::makeSuccess();
        }
        if( capacity_in < bytes.size() )
        {
            *p_written_out = 0U;
            return wse::capi::makeStatus(
                WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, "Destination buffer_in is too small." );
        }
        if( !bytes.empty() )
        {
            std::memcpy( p_destination_out, bytes.data(), bytes.size() );
        }
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_create(
    wse_capi_cancellation* p_cancellation_out )
{
    return wse::capi::guard( [p_cancellation_out]() -> wse_capi_status
    {
        if( p_cancellation_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_cancellation_out must not be null." );
        }
        *p_cancellation_out = new wse_capi_cancellation_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_cancellation_destroy( wse_capi_cancellation cancellation_inout )
{
    delete cancellation_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_cancel( wse_capi_cancellation cancellation_inout )
{
    return wse::capi::guard( [cancellation_inout]() -> wse_capi_status
    {
        if( cancellation_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "cancellation_inout must not be null." );
        }
        cancellation_inout->cancelAll();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_cancellation_is_requested(
      wse_capi_cancellation cancellation_in
    , wse_capi_bool* p_requested_out )
{
    return wse::capi::guard( [cancellation_in, p_requested_out]() -> wse_capi_status
    {
        if( cancellation_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "cancellation_in must not be null." );
        }
        if( p_requested_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_requested_out must not be null." );
        }
        *p_requested_out = cancellation_in->source.isCancellationRequested() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

} // extern "C"
