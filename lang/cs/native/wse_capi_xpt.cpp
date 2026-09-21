//*****************************************************************************************************************
//!
//! @file    wse_capi_xpt.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT Transportの平坦C ABI実装.
//! @brief   \~english  Implementation of the flat C ABI for the XPT transports.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/capi/wse_capi_xpt.h>

#include "capi_internal.h"
#include "transfer_result.h"

#include <wse/binding/XptErrorAdapter.h>
#include <xpt/error/TransportError.h>
#include <xpt/http/HttpClient.h>
#include <xpt/network/Endpoint.h>
#include <xpt/operation/OperationContext.h>
#include <xpt/serial/SerialPort.h>
#include <xpt/tcp/TcpClient.h>
#include <xpt/udp/UdpClient.h>

#include <chrono>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <utility>
#include <vector>

//! \~japanese 不透明XPT Handleの実体.
//! \~english  Concrete bodies behind the opaque XPT handles.
struct wse_capi_tcp_client_t final
{
    wse::xpt::TcpClient client;
};

struct wse_capi_udp_client_t final
{
    wse::xpt::UdpClient client;
};

struct wse_capi_udp_datagram_t final
{
    wse::xpt::UdpDatagram datagram;

    explicit wse_capi_udp_datagram_t( wse::xpt::UdpDatagram datagram_in )
        : datagram ( std::move( datagram_in ) )
    {
    }
};

struct wse_capi_serial_port_t final
{
    wse::xpt::SerialPort port;
};

struct wse_capi_http_request_t final
{
    wse::xpt::HttpRequest request;

    wse_capi_http_request_t( const wse::xpt::eHttpMethod method_in, const std::string& url_in )
        : request ( method_in, url_in )
    {
    }
};

struct wse_capi_http_response_t final
{
    wse::xpt::HttpResponse response;

    explicit wse_capi_http_response_t( wse::xpt::HttpResponse response_in )
        : response ( std::move( response_in ) )
    {
    }
};

namespace
{

//! \~japanese XPTのStructured ErrorをC ABIのStatusへ写す. XPTの安定Codeは`code`へ保持する.
//! \~english  Maps an XPT structured error onto a C ABI status, preserving the stable XPT code.
wse_capi_status fromTransportError( const wse::xpt::TransportError& error_in )
{
    return wse::capi::fromBindingError( wse::binding::fromXptError( error_in ) );
}

//! \~japanese Statusの成否をC ABIのStatusへ写す. 成功時はerror()へ触れない.
//! \~english  Maps a status outcome onto a C ABI status without touching error() on success.
wse_capi_status fromTransportStatus( const wse::xpt::TransportStatus& status_in )
{
    return status_in.succeeded()
        ? wse::capi::makeSuccess()
        : fromTransportError( status_in.error() );
}

//! \~japanese C ABIのOperation ContextをXPTの必須制御へ変換する.
//! \~english  Converts a C ABI operation context into the required XPT controls.
bool buildContext(
      wse::xpt::OperationContext* const       p_context_out
    , wse_capi_status* const                  p_status_out
    , const wse_capi_operation_context* const source_in )
{
    wse::xpt::OperationContext& context_out = *p_context_out;
    wse_capi_status&            status_out  = *p_status_out;

    if ( source_in == nullptr )
    {
        status_out = wse::capi::makeInvalidArgument( "context_in must not be null." );
        return false;
    }
    if ( source_in->timeout_milliseconds < 0 )
    {
        status_out = wse::capi::makeInvalidArgument( "timeout_milliseconds must not be negative." );
        return false;
    }

    const wse::xpt::Timeout timeout =
        wse::xpt::Timeout::milliseconds( source_in->timeout_milliseconds );
    if ( source_in->cancellation == nullptr )
    {
        context_out = wse::xpt::OperationContext( timeout );
    }
    else
    {
        // The handle owns an XPT cancellation source of its own, so the token observed here is the
        // one `wse_capi_cancellation_cancel` signals.
        context_out = wse::xpt::OperationContext(
            timeout, source_in->cancellation->transport_source.token() );
    }
    return true;
}

//! \~japanese Endpointをホスト文字列とPortへ書き出す.
//! \~english  Writes an endpoint out as a host string and port.
wse_capi_status writeEndpoint(
      char*                     p_host_buffer_out
    , std::size_t*              p_host_size_out
    , std::uint16_t*            p_port_out
    , const wse::xpt::Endpoint& endpoint_in
    , const std::size_t         host_capacity_in )
{
    if ( p_port_out == nullptr )
    {
        return wse::capi::makeInvalidArgument( "p_port_out must not be null." );
    }
    *p_port_out = endpoint_in.port();
    return wse::capi::copyString(
        p_host_buffer_out, p_host_size_out, endpoint_in.host(), host_capacity_in );
}

//! \~japanese Byte列を2回呼出方式で複製する.
//! \~english  Copies bytes using the two-call pattern.
wse_capi_status copyBytes(
      std::uint8_t*                    p_buffer_out
    , std::size_t*                     p_size_out
    , const std::vector<std::uint8_t>& source_in
    , const std::size_t                capacity_in )
{
    if ( p_size_out == nullptr )
    {
        return wse::capi::makeInvalidArgument( "The output size pointer must not be null." );
    }
    *p_size_out = source_in.size();
    if ( p_buffer_out == nullptr )
    {
        return wse::capi::makeSuccess();
    }
    if ( capacity_in < source_in.size() )
    {
        *p_size_out = source_in.size();
        return wse::capi::makeStatus(
            WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, "Destination buffer is too small." );
    }
    if ( !source_in.empty() )
    {
        std::memcpy( p_buffer_out, source_in.data(), source_in.size() );
    }
    return wse::capi::makeSuccess();
}

//! \~japanese HTTP Requestを共通経路で実行する.
//! \~english  Executes an HTTP request through the shared path.
wse_capi_status executeHttp(
      wse_capi_http_request             request_in
    , wse_capi_http_response*           p_response_out
    , const std::size_t                 maximum_body_in
    , const char*                       username_in
    , const char*                       secret_in
    , const wse_capi_operation_context* context_in )
{
    if ( request_in == nullptr )
    {
        return wse::capi::makeInvalidArgument( "request_in must not be null." );
    }
    if ( p_response_out == nullptr )
    {
        return wse::capi::makeInvalidArgument( "p_response_out must not be null." );
    }
    // Cleared first, so a caller can always read the handle to learn whether one was produced.
    *p_response_out = nullptr;

    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    wse_capi_status status = wse::capi::makeSuccess();
    if ( !buildContext( &context, &status, context_in ) ) return status;

    // The binding never retries on the caller's behalf: a request is classified as non-idempotent
    // and transient-failure retries stay off, so a caller decides when a repeat is safe.
    const wse::xpt::HttpExecutionOptions options(
          maximum_body_in
        , wse::xpt::RetryPolicy()
        , wse::xpt::eRetryOperationSafety::NonIdempotent
        , false );

    const wse::xpt::HttpClient client;
    wse::xpt::HttpResult result =
        ( username_in == nullptr )
            ? client.execute( request_in->request, options, context )
            : client.executeAuthenticated(
                  request_in->request
                , options
                , wse::xpt::HttpAuthentication(
                      wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated
                    , std::string( username_in )
                    , std::string( secret_in == nullptr ? "" : secret_in ) )
                , context );
    if ( !result.succeeded() )
    {
        // An HTTP status error is the one failure that still carries a response: the exchange
        // finished and the server answered, so Core returns the error and the value together.
        // Hand the response over rather than dropping what the caller asked for. Ownership
        // passes on this path too, so the caller destroys it even though the call failed.
        if ( result.error().code() == wse::xpt::eTransportErrorCode::HttpStatusError )
        {
            *p_response_out = new wse_capi_http_response_t( std::move( result.value() ) );
        }
        return fromTransportError( result.error() );
    }
    *p_response_out = new wse_capi_http_response_t( std::move( result.value() ) );
    return wse::capi::makeSuccess();
}

} // namespace

extern "C"
{

// ----------------------------------------------------------------------------------------------
// TCP
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_create( wse_capi_tcp_client* p_client_out )
{
    return wse::capi::guard( [p_client_out]() -> wse_capi_status
    {
        if ( p_client_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_client_out must not be null." );
        }
        *p_client_out = new wse_capi_tcp_client_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_tcp_client_destroy( wse_capi_tcp_client client_inout )
{
    delete client_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_connect(
      wse_capi_tcp_client               client_inout
    , const char*                       host_in
    , uint16_t                          port_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard( [client_inout, host_in, port_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_inout must not be null." );
        }
        if ( host_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "host_in must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        return fromTransportStatus( client_inout->client.connect(
            wse::xpt::Endpoint( host_in, port_in ), operation ));
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_disconnect( wse_capi_tcp_client client_inout )
{
    return wse::capi::guard( [client_inout]() -> wse_capi_status
    {
        if ( client_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_inout must not be null." );
        }
        client_inout->client.disconnect();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_is_connected(
      wse_capi_tcp_client client_in
    , wse_capi_bool*      p_connected_out )
{
    return wse::capi::guard( [client_in, p_connected_out]() -> wse_capi_status
    {
        if ( client_in == nullptr || p_connected_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_in and p_connected_out must not be null." );
        }
        *p_connected_out = client_in->client.isConnected() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_check_peer_connection(
    wse_capi_tcp_client client_inout )
{
    return wse::capi::guard( [client_inout]() -> wse_capi_status
    {
        if ( client_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_inout must not be null." );
        }
        return fromTransportStatus( client_inout->client.checkPeerConnection());
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_remote_endpoint(
      wse_capi_tcp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in )
{
    return wse::capi::guard(
        [client_in, p_host_buffer_out, p_host_size_out, p_port_out, host_capacity_in]()
            -> wse_capi_status
    {
        if ( client_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_in must not be null." );
        }
        return writeEndpoint(
              p_host_buffer_out
            , p_host_size_out
            , p_port_out
            , client_in->client.getRemoteEndpoint()
            , host_capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_local_endpoint(
      wse_capi_tcp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in )
{
    return wse::capi::guard(
        [client_in, p_host_buffer_out, p_host_size_out, p_port_out, host_capacity_in]()
            -> wse_capi_status
    {
        if ( client_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_in must not be null." );
        }
        return writeEndpoint(
              p_host_buffer_out
            , p_host_size_out
            , p_port_out
            , client_in->client.getLocalEndpoint()
            , host_capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_send(
      wse_capi_tcp_client               client_inout
    , size_t*                           p_sent_out
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in )
{
    if ( p_sent_out != nullptr ) *p_sent_out = 0U;
    return wse::capi::guard(
        [client_inout, p_sent_out, data_in, size_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr || p_sent_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_inout and p_sent_out must not be null." );
        }
        if ( data_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument(
                "data_in must not be null when size_in is non-zero." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        const wse::xpt::TransferResult<std::size_t> result =
            client_inout->client.send( data_in, size_in, operation );
        return wse::capi::publishTransferCount( p_sent_out, result );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_receive(
      wse_capi_tcp_client               client_inout
    , wse_capi_frame_buffer*            p_data_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [client_inout, p_data_out, maximum_size_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr || p_data_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_inout and p_data_out must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        wse::xpt::TransferResult<std::vector<std::uint8_t>> result =
            client_inout->client.receive( maximum_size_in, operation );
        if ( !result.succeeded() ) return fromTransportError( result.error() );
        *p_data_out = new wse_capi_frame_buffer_t(
            wse::binding::FrameBuffer( std::move( result.value() ) ) );
        return wse::capi::makeSuccess();
    } );
}

// ----------------------------------------------------------------------------------------------
// UDP
// ----------------------------------------------------------------------------------------------

size_t WSE_CAPI_CALL wse_capi_udp_maximum_datagram_size( void )
{
    return wse::xpt::UdpClient::maximumDatagramSize();
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_create( wse_capi_udp_client* p_client_out )
{
    return wse::capi::guard( [p_client_out]() -> wse_capi_status
    {
        if ( p_client_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_client_out must not be null." );
        }
        *p_client_out = new wse_capi_udp_client_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_udp_client_destroy( wse_capi_udp_client client_inout )
{
    delete client_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_bind(
      wse_capi_udp_client               client_inout
    , const char*                       host_in
    , uint16_t                          port_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard( [client_inout, host_in, port_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr || host_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_inout and host_in must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        return fromTransportStatus( client_inout->client.bind(
            wse::xpt::Endpoint( host_in, port_in ), operation ));
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_close( wse_capi_udp_client client_inout )
{
    return wse::capi::guard( [client_inout]() -> wse_capi_status
    {
        if ( client_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_inout must not be null." );
        }
        client_inout->client.close();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_is_open(
      wse_capi_udp_client client_in
    , wse_capi_bool*      p_open_out )
{
    return wse::capi::guard( [client_in, p_open_out]() -> wse_capi_status
    {
        if ( client_in == nullptr || p_open_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_in and p_open_out must not be null." );
        }
        *p_open_out = client_in->client.isOpen() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_local_endpoint(
      wse_capi_udp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in )
{
    return wse::capi::guard(
        [client_in, p_host_buffer_out, p_host_size_out, p_port_out, host_capacity_in]()
            -> wse_capi_status
    {
        if ( client_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "client_in must not be null." );
        }
        return writeEndpoint(
              p_host_buffer_out
            , p_host_size_out
            , p_port_out
            , client_in->client.getLocalEndpoint()
            , host_capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_send_to(
      wse_capi_udp_client               client_inout
    , size_t*                           p_sent_out
    , const char*                       host_in
    , uint16_t                          port_in
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in )
{
    if ( p_sent_out != nullptr ) *p_sent_out = 0U;
    return wse::capi::guard(
        [client_inout, p_sent_out, host_in, port_in, data_in, size_in, context_in]()
            -> wse_capi_status
    {
        if ( client_inout == nullptr || host_in == nullptr || p_sent_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_inout, host_in, and p_sent_out must not be null." );
        }
        if ( data_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument(
                "data_in must not be null when size_in is non-zero." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        const wse::xpt::TransferResult<std::size_t> result = client_inout->client.sendTo(
            wse::xpt::Endpoint( host_in, port_in ), data_in, size_in, operation );
        return wse::capi::publishTransferCount( p_sent_out, result );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from(
      wse_capi_udp_client               client_inout
    , wse_capi_udp_datagram*            p_datagram_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [client_inout, p_datagram_out, maximum_size_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr || p_datagram_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_inout and p_datagram_out must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        wse::xpt::TransferResult<wse::xpt::UdpDatagram> result =
            client_inout->client.receiveFrom( maximum_size_in, operation );
        return wse::capi::publishDatagram<wse_capi_udp_datagram_t>( p_datagram_out, result, false );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from_with_progress(
      wse_capi_udp_client               client_inout
    , wse_capi_udp_datagram*            p_datagram_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in )
{
    if ( p_datagram_out != nullptr ) *p_datagram_out = nullptr;
    return wse::capi::guard(
        [client_inout, p_datagram_out, maximum_size_in, context_in]() -> wse_capi_status
    {
        if ( client_inout == nullptr || p_datagram_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "client_inout and p_datagram_out must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        wse::xpt::TransferResult<wse::xpt::UdpDatagram> result =
            client_inout->client.receiveFrom( maximum_size_in, operation );
        return wse::capi::publishDatagram<wse_capi_udp_datagram_t>( p_datagram_out, result, true );
    } );
}

void WSE_CAPI_CALL wse_capi_udp_datagram_destroy( wse_capi_udp_datagram datagram_inout )
{
    delete datagram_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_source(
      wse_capi_udp_datagram datagram_in
    , char*                 p_host_buffer_out
    , size_t*               p_host_size_out
    , uint16_t*             p_port_out
    , size_t                host_capacity_in )
{
    return wse::capi::guard(
        [datagram_in, p_host_buffer_out, p_host_size_out, p_port_out, host_capacity_in]()
            -> wse_capi_status
    {
        if ( datagram_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "datagram_in must not be null." );
        }
        return writeEndpoint(
              p_host_buffer_out
            , p_host_size_out
            , p_port_out
            , datagram_in->datagram.source()
            , host_capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_payload(
      wse_capi_udp_datagram datagram_in
    , uint8_t*              p_buffer_out
    , size_t*               p_size_out
    , size_t                capacity_in )
{
    return wse::capi::guard(
        [datagram_in, p_buffer_out, p_size_out, capacity_in]() -> wse_capi_status
    {
        if ( datagram_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "datagram_in must not be null." );
        }
        return copyBytes(
            p_buffer_out, p_size_out, datagram_in->datagram.payload(), capacity_in );
    } );
}

// ----------------------------------------------------------------------------------------------
// Serial
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_create( wse_capi_serial_port* p_port_out )
{
    return wse::capi::guard( [p_port_out]() -> wse_capi_status
    {
        if ( p_port_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_port_out must not be null." );
        }
        *p_port_out = new wse_capi_serial_port_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_serial_port_destroy( wse_capi_serial_port port_inout )
{
    delete port_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_open(
      wse_capi_serial_port              port_inout
    , const char*                       device_name_in
    , int32_t                           baud_rate_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [port_inout, device_name_in, baud_rate_in, context_in]() -> wse_capi_status
    {
        if ( port_inout == nullptr || device_name_in == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "port_inout and device_name_in must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        return fromTransportStatus(
            port_inout->port.open( device_name_in, baud_rate_in, operation ));
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_close( wse_capi_serial_port port_inout )
{
    return wse::capi::guard( [port_inout]() -> wse_capi_status
    {
        if ( port_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "port_inout must not be null." );
        }
        port_inout->port.close();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_is_open(
      wse_capi_serial_port port_in
    , wse_capi_bool*       p_open_out )
{
    return wse::capi::guard( [port_in, p_open_out]() -> wse_capi_status
    {
        if ( port_in == nullptr || p_open_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "port_in and p_open_out must not be null." );
        }
        *p_open_out = port_in->port.isOpen() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_device_name(
      wse_capi_serial_port port_in
    , char*                p_buffer_out
    , size_t*              p_size_out
    , size_t               capacity_in )
{
    return wse::capi::guard(
        [port_in, p_buffer_out, p_size_out, capacity_in]() -> wse_capi_status
    {
        if ( port_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "port_in must not be null." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, port_in->port.getDeviceName(), capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_baud_rate(
      wse_capi_serial_port port_in
    , int32_t*             p_baud_rate_out )
{
    return wse::capi::guard( [port_in, p_baud_rate_out]() -> wse_capi_status
    {
        if ( port_in == nullptr || p_baud_rate_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "port_in and p_baud_rate_out must not be null." );
        }
        *p_baud_rate_out = port_in->port.getBaudRate();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_send(
      wse_capi_serial_port              port_inout
    , size_t*                           p_sent_out
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in )
{
    if ( p_sent_out != nullptr ) *p_sent_out = 0U;
    return wse::capi::guard(
        [port_inout, p_sent_out, data_in, size_in, context_in]() -> wse_capi_status
    {
        if ( port_inout == nullptr || p_sent_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "port_inout and p_sent_out must not be null." );
        }
        if ( data_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument(
                "data_in must not be null when size_in is non-zero." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        const wse::xpt::TransferResult<std::size_t> result =
            port_inout->port.send( data_in, size_in, operation );
        return wse::capi::publishTransferCount( p_sent_out, result );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_receive(
      wse_capi_serial_port              port_inout
    , wse_capi_frame_buffer*            p_data_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [port_inout, p_data_out, maximum_size_in, context_in]() -> wse_capi_status
    {
        if ( port_inout == nullptr || p_data_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "port_inout and p_data_out must not be null." );
        }
        wse::xpt::OperationContext operation( wse::xpt::Timeout::milliseconds( 0 ) );
        wse_capi_status status = wse::capi::makeSuccess();
        if ( !buildContext( &operation, &status, context_in ) ) return status;

        wse::xpt::TransferResult<std::vector<std::uint8_t>> result =
            port_inout->port.receive( maximum_size_in, operation );
        if ( !result.succeeded() ) return fromTransportError( result.error() );
        *p_data_out = new wse_capi_frame_buffer_t(
            wse::binding::FrameBuffer( std::move( result.value() ) ) );
        return wse::capi::makeSuccess();
    } );
}

// ----------------------------------------------------------------------------------------------
// HTTP
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_http_request_create(
      wse_capi_http_request* p_request_out
    , int32_t                method_in
    , const char*            url_in )
{
    return wse::capi::guard( [p_request_out, method_in, url_in]() -> wse_capi_status
    {
        if ( url_in == nullptr || p_request_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "url_in and p_request_out must not be null." );
        }
        if ( method_in < static_cast<std::int32_t>( WSE_CAPI_HTTP_GET )
             || method_in > static_cast<std::int32_t>( WSE_CAPI_HTTP_DELETE ) )
        {
            return wse::capi::makeInvalidArgument( "method_in is outside the supported range." );
        }
        *p_request_out = new wse_capi_http_request_t(
            static_cast<wse::xpt::eHttpMethod>( method_in ), std::string( url_in ) );
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_http_request_destroy( wse_capi_http_request request_inout )
{
    delete request_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_request_add_header(
      wse_capi_http_request request_inout
    , const char*           name_in
    , const char*           value_in )
{
    return wse::capi::guard( [request_inout, name_in, value_in]() -> wse_capi_status
    {
        if ( request_inout == nullptr || name_in == nullptr || value_in == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "request_inout, name_in, and value_in must not be null." );
        }
        request_inout->request.addHeader( name_in, value_in );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_request_set_body(
      wse_capi_http_request request_inout
    , const uint8_t*        body_in
    , size_t                size_in )
{
    return wse::capi::guard( [request_inout, body_in, size_in]() -> wse_capi_status
    {
        if ( request_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "request_inout must not be null." );
        }
        if ( body_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument(
                "body_in must not be null when size_in is non-zero." );
        }
        const std::vector<std::uint8_t> bytes(
            body_in, ( body_in == nullptr ) ? body_in : ( body_in + size_in ) );
        request_inout->request.setBody( bytes );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_execute(
      wse_capi_http_request             request_in
    , wse_capi_http_response*           p_response_out
    , size_t                            maximum_response_body_size_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [request_in, p_response_out, maximum_response_body_size_in, context_in]()
            -> wse_capi_status
    {
        return executeHttp(
              request_in
            , p_response_out
            , maximum_response_body_size_in
            , nullptr
            , nullptr
            , context_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_execute_authenticated(
      wse_capi_http_request             request_in
    , wse_capi_http_response*           p_response_out
    , size_t                            maximum_response_body_size_in
    , const char*                       username_in
    , const char*                       secret_in
    , const wse_capi_operation_context* context_in )
{
    return wse::capi::guard(
        [request_in, p_response_out, maximum_response_body_size_in, username_in, secret_in,
         context_in]() -> wse_capi_status
    {
        if ( username_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "username_in must not be null." );
        }
        return executeHttp(
              request_in
            , p_response_out
            , maximum_response_body_size_in
            , username_in
            , secret_in
            , context_in );
    } );
}

void WSE_CAPI_CALL wse_capi_http_response_destroy( wse_capi_http_response response_inout )
{
    delete response_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_status_code(
      wse_capi_http_response response_in
    , uint16_t*              p_status_code_out )
{
    return wse::capi::guard( [response_in, p_status_code_out]() -> wse_capi_status
    {
        if ( response_in == nullptr || p_status_code_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "response_in and p_status_code_out must not be null." );
        }
        *p_status_code_out = response_in->response.status_code();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_attempt_count(
      wse_capi_http_response response_in
    , uint32_t*              p_attempt_count_out )
{
    return wse::capi::guard( [response_in, p_attempt_count_out]() -> wse_capi_status
    {
        if ( response_in == nullptr || p_attempt_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "response_in and p_attempt_count_out must not be null." );
        }
        *p_attempt_count_out = response_in->response.attempt_count();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_count(
      wse_capi_http_response response_in
    , size_t*                p_count_out )
{
    return wse::capi::guard( [response_in, p_count_out]() -> wse_capi_status
    {
        if ( response_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "response_in and p_count_out must not be null." );
        }
        *p_count_out = response_in->response.headers().size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_name(
      wse_capi_http_response response_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 index_in
    , size_t                 capacity_in )
{
    return wse::capi::guard(
        [response_in, p_buffer_out, p_size_out, index_in, capacity_in]() -> wse_capi_status
    {
        if ( response_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "response_in must not be null." );
        }
        if ( index_in >= response_in->response.headers().size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the header range." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, response_in->response.headers()[ index_in ].name, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_value(
      wse_capi_http_response response_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 index_in
    , size_t                 capacity_in )
{
    return wse::capi::guard(
        [response_in, p_buffer_out, p_size_out, index_in, capacity_in]() -> wse_capi_status
    {
        if ( response_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "response_in must not be null." );
        }
        if ( index_in >= response_in->response.headers().size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the header range." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, response_in->response.headers()[ index_in ].value, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_http_response_body(
      wse_capi_http_response response_in
    , uint8_t*               p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in )
{
    return wse::capi::guard(
        [response_in, p_buffer_out, p_size_out, capacity_in]() -> wse_capi_status
    {
        if ( response_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "response_in must not be null." );
        }
        return copyBytes(
            p_buffer_out, p_size_out, response_in->response.body(), capacity_in );
    } );
}

} // extern "C"
