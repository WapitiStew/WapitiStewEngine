//*****************************************************************************************************************
//!
//! @file    capi_internal.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 平坦C ABI共通のStatus生成とThread単位の失敗Messageを実装する.
//! @brief   \~english  Implements shared status construction and the thread-local failure message.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include "capi_internal.h"

#include <cstring>

namespace wse
{
namespace capi
{
namespace
{

//! \~japanese 呼出Thread単位の失敗Message.
//! \~english  Thread-local failure message.
// A failure path must work even when every allocation fails. Reserve the complete
// diagnostic storage in TLS, including its terminating NUL.
thread_local char g_last_error_message[1024] = {};

} // namespace

wse_capi_status makeStatus(
      const wse_capi_error_category category_in
    , const std::int32_t code_in
    , const std::string_view& message_in
    , const std::int64_t native_code_in ) noexcept
{
    std::size_t length = message_in.size();
    if ( length >= sizeof( g_last_error_message ) )
    {
        length = sizeof( g_last_error_message ) - 1U;
        // Do not retain the prefix of a UTF-8 code point at the truncation edge.
        while ( length > 0U &&
            ( static_cast<unsigned char>( message_in[length] ) & 0xc0U ) == 0x80U )
        {
            --length;
        }
    }
    if ( length != 0U )
    {
        // memmove also permits recording a view of the previous diagnostic.
        std::memmove( g_last_error_message, message_in.data(), length );
    }
    g_last_error_message[length] = '\0';

    wse_capi_status status;
    status.category = static_cast<std::int32_t>( category_in );
    status.code = code_in;
    status.native_code = native_code_in;
    return status;
}

wse_capi_status makeSuccess() noexcept
{
    g_last_error_message[0] = '\0';

    wse_capi_status status;
    status.category = static_cast<std::int32_t>( WSE_CAPI_ERROR_NONE );
    status.code = 0;
    status.native_code = 0;
    return status;
}

wse_capi_status makeInvalidArgument( const std::string_view& message_in ) noexcept
{
    return makeStatus( WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, message_in );
}

wse_capi_status fromBindingError( const binding::Error& error_in )
{
    if ( error_in.ok() )
    {
        return makeSuccess();
    }
    return makeStatus(
          static_cast<wse_capi_error_category>( error_in.category() )
        , error_in.code()
        , error_in.message()
        , error_in.nativeCode()
    );
}

wse_capi_status copyString(
      char*              p_buffer_out
    , std::size_t*       p_size_out
    , const std::string& source_in
    , const std::size_t  capacity_in )
{
    if ( p_size_out == nullptr )
    {
        return makeInvalidArgument( "The output size pointer must not be null." );
    }

    const std::size_t required = source_in.size() + 1U;
    *p_size_out = required;

    if ( p_buffer_out == nullptr )
    {
        return makeSuccess();
    }
    if ( capacity_in < required )
    {
        return makeStatus(
            WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, "Destination buffer is too small." );
    }

    if ( !source_in.empty() )
    {
        std::memcpy( p_buffer_out, source_in.data(), source_in.size() );
    }
    p_buffer_out[source_in.size()] = '\0';
    return makeSuccess();
}

const char* lastErrorMessage() noexcept
{
    return g_last_error_message;
}

} // namespace capi
} // namespace wse

extern "C"
{

uint32_t WSE_CAPI_CALL wse_capi_abi_version( void )
{
    return WSE_CAPI_ABI_VERSION;
}

const char* WSE_CAPI_CALL wse_capi_last_error_message( void )
{
    return wse::capi::lastErrorMessage();
}

} // extern "C"
