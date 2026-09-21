//*****************************************************************************************************************
//!
//! @file    xpt_addon.cpp
//! @brief   \~japanese XPT Transport面のNode-API addon. XPT無しのBuildでは登録関数が何もせず成功する.
//! @brief   \~english  Node-API addon for the XPT transport surface; in a build without XPT the registration
//!                     function succeeds without registering anything.
//!
//! @date
//!   Sep-01, 2026   Create New.
//*****************************************************************************************************************

#define NAPI_VERSION 8
#include "component_addons.h"

#include <wse/binding/stew.h>
#ifdef WSE_HAS_XPT
#include <xpt/stew.h>
#include <wse/binding/XptErrorAdapter.h>
#include "../../common/transfer_failure.h"
#endif

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace
{

napi_value stringValue( napi_env env_in, const std::string& value_in )
{
    napi_value value = nullptr;
    napi_create_string_utf8( env_in, value_in.c_str(), value_in.size(), &value );
    return value;
}

napi_value int64Value( napi_env env_in, const std::int64_t value_in )
{
    napi_value value = nullptr;
    napi_create_int64( env_in, value_in, &value );
    return value;
}

napi_value uint32Value( napi_env env_in, const std::uint32_t value_in )
{
    napi_value value = nullptr;
    napi_create_uint32( env_in, value_in, &value );
    return value;
}

napi_value boolValue( napi_env env_in, const bool value_in )
{
    napi_value value = nullptr;
    napi_get_boolean( env_in, value_in, &value );
    return value;
}

bool setProperty( napi_env env_in, napi_value object_in,
    const char* name_in, napi_value value_in )
{
    return value_in != nullptr
        && napi_set_named_property( env_in, object_in, name_in, value_in ) == napi_ok;
}

napi_value enumValue( napi_env env_in,
    const std::initializer_list<std::pair<const char*, std::uint32_t>>& values_in )
{
    napi_value result = nullptr;
    if ( napi_create_object( env_in, &result ) != napi_ok ) return nullptr;
    for ( const auto& value : values_in )
    {
        if ( !setProperty( env_in, result, value.first,
                 uint32Value( env_in, value.second ) ) ) return nullptr;
    }
    return result;
}

#ifdef WSE_HAS_XPT

void throwError( napi_env env_in, const wse::binding::Error& error_in )
{
    napi_value failure = nullptr;
    if ( napi_create_error( env_in, nullptr,
             stringValue( env_in, error_in.message() ), &failure ) != napi_ok ) return;
    setProperty( env_in, failure, "category",
        uint32Value( env_in, static_cast<std::uint32_t>( error_in.category() ) ) );
    setProperty( env_in, failure, "code", int64Value( env_in, error_in.code() ) );
    setProperty( env_in, failure, "nativeCode", int64Value( env_in, error_in.nativeCode() ) );
    napi_throw( env_in, failure );
}

void throwTransportError( napi_env env_in, const wse::xpt::TransportError& error_in )
{
    throwError( env_in, wse::binding::fromXptError( error_in ) );
}

//! \~japanese XPT Operationが観測する協調的Cancellationの所有者.
//! \~english  Owner of the cooperative cancellation observed by XPT operations.
struct CancellationHolder final
{
    wse::xpt::CancellationSource source;
};

void finalizeCancellation( napi_env, void* data_in, void* )
{
    delete static_cast<CancellationHolder*>( data_in );
}

CancellationHolder* unwrapCancellation( napi_env env_in, napi_value value_in )
{
    void* data = nullptr;
    if ( napi_unwrap( env_in, value_in, &data ) != napi_ok ) return nullptr;
    return static_cast<CancellationHolder*>( data );
}

//! \~japanese `{ timeoutMs, cancellation }`をXPTの必須制御へ変換する.
//! \~english  Converts `{ timeoutMs, cancellation }` into the required XPT controls.
bool readContext(
      wse::xpt::OperationContext* const p_context_out
    , napi_env env_in, napi_value value_in )
{
    wse::xpt::OperationContext& context_out = *p_context_out;

    napi_value timeout_value = nullptr;
    if ( napi_get_named_property( env_in, value_in, "timeoutMs", &timeout_value ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_XPT_CONTEXT",
            "An operation context requires a timeoutMs property." );
        return false;
    }
    std::int64_t timeout_ms = 0;
    if ( napi_get_value_int64( env_in, timeout_value, &timeout_ms ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_XPT_CONTEXT", "timeoutMs must be an integer." );
        return false;
    }
    if ( timeout_ms < 0 )
    {
        napi_throw_range_error( env_in, "WSE_XPT_CONTEXT",
            "timeoutMs must not be negative." );
        return false;
    }

    const wse::xpt::Timeout timeout = wse::xpt::Timeout::milliseconds( timeout_ms );
    napi_value cancellation_value = nullptr;
    bool present = false;
    if ( napi_has_named_property( env_in, value_in, "cancellation", &present ) == napi_ok && present
         && napi_get_named_property(
                env_in, value_in, "cancellation", &cancellation_value ) == napi_ok )
    {
        napi_valuetype type = napi_undefined;
        napi_typeof( env_in, cancellation_value, &type );
        if ( type == napi_object )
        {
            CancellationHolder* holder = unwrapCancellation( env_in, cancellation_value );
            if ( holder != nullptr )
            {
                context_out = wse::xpt::OperationContext( timeout, holder->source.token() );
                return true;
            }
        }
    }

    context_out = wse::xpt::OperationContext( timeout );
    return true;
}

bool readString(
      std::string* const p_text_out
    , napi_env env_in, napi_value value_in )
{
    std::string& text_out = *p_text_out;

    std::size_t length = 0U;
    if ( napi_get_value_string_utf8( env_in, value_in, nullptr, 0U, &length ) != napi_ok )
    {
        return false;
    }
    text_out.assign( length, '\0' );
    return napi_get_value_string_utf8(
        env_in, value_in, text_out.data(), length + 1U, &length ) == napi_ok;
}

bool readBytes(
      std::vector<std::uint8_t>* const p_bytes_out
    , napi_env env_in, napi_value value_in )
{
    std::vector<std::uint8_t>& bytes_out = *p_bytes_out;

    bool is_typed = false;
    if ( napi_is_typedarray( env_in, value_in, &is_typed ) != napi_ok || !is_typed )
    {
        bool is_buffer = false;
        if ( napi_is_buffer( env_in, value_in, &is_buffer ) != napi_ok || !is_buffer )
        {
            return false;
        }
        void* data = nullptr;
        std::size_t length = 0U;
        if ( napi_get_buffer_info( env_in, value_in, &data, &length ) != napi_ok ) return false;
        const auto* start = static_cast<const std::uint8_t*>( data );
        bytes_out.assign( start, start + length );
        return true;
    }

    napi_typedarray_type type = napi_uint8_array;
    std::size_t count = 0U;
    void* data = nullptr;
    napi_value array_buffer = nullptr;
    std::size_t offset = 0U;
    if ( napi_get_typedarray_info(
             env_in, value_in, &type, &count, &data, &array_buffer, &offset ) != napi_ok )
    {
        return false;
    }
    const auto* start = static_cast<const std::uint8_t*>( data );
    bytes_out.assign( start, start + count );
    return true;
}

napi_value bytesValue( napi_env env_in, const std::vector<std::uint8_t>& bytes_in )
{
    void* data = nullptr;
    napi_value result = nullptr;
    if ( napi_create_buffer_copy(
             env_in, bytes_in.size(), bytes_in.data(), &data, &result ) != napi_ok )
    {
        return nullptr;
    }
    return result;
}

napi_value endpointValue( napi_env env_in, const wse::xpt::Endpoint& endpoint_in )
{
    napi_value result = nullptr;
    if ( napi_create_object( env_in, &result ) != napi_ok ) return nullptr;
    if ( !setProperty( env_in, result, "host", stringValue( env_in, endpoint_in.host() ) )
         || !setProperty( env_in, result, "port", uint32Value( env_in, endpoint_in.port() ) ) )
    {
        return nullptr;
    }
    return result;
}

// Preserve a pending VM exception on any failed conversion. No C++ allocation
// exception from normalization may escape this new error-construction boundary.
void throwTransferError( napi_env env_in,
    const wse::binding::detail::TransferFailureView& failure_in ) noexcept
{
    struct EnsureException
    {
        napi_env env;
        ~EnsureException() noexcept
        {
            bool pending = false;
            if( napi_is_exception_pending( env, &pending ) == napi_ok && !pending )
                (void)napi_throw_error( env, nullptr, "Unable to construct WSE transfer error." );
        }
    } ensure{ env_in };
    try
    {
        const auto error = wse::binding::fromXptError( failure_in.error );
        napi_value failure = nullptr;
        const auto message = stringValue( env_in, error.message() );
        if( !message || napi_create_error( env_in, nullptr, message, &failure ) != napi_ok ) return;
        if( !setProperty( env_in, failure, "category", uint32Value( env_in, static_cast<std::uint32_t>(error.category()) ) )
            || !setProperty( env_in, failure, "code", int64Value( env_in, error.code() ) )
            || !setProperty( env_in, failure, "nativeCode", int64Value( env_in, error.nativeCode() ) )
            || !setProperty( env_in, failure, "bytesTransferred", int64Value( env_in,
                static_cast<std::int64_t>(failure_in.bytes_transferred) ) ) ) return;
        if( failure_in.datagram )
        {
            if( !setProperty( env_in, failure, "receivedData", bytesValue( env_in, failure_in.datagram->payload() ) )
                || !setProperty( env_in, failure, "sourceEndpoint", endpointValue( env_in, failure_in.datagram->source() ) ) ) return;
        }
        (void)napi_throw( env_in, failure );
    }
    catch( ... ) {} // EnsureException supplies a fallback only if the VM has no pending error.
}

//! \~japanese 引数を読み出すHelper. 個数不足はTypeErrorにする.
//! \~english  Argument reader that raises a TypeError when arguments are missing.
struct Arguments final
{
    Arguments()
        : env    ( nullptr )
        , self   ( nullptr )
        , values {}
        , count  ( 0U )
    {
    }
    napi_env env;
    napi_value self;
    napi_value values[ 6 ];
    std::size_t count;

    bool require( const std::size_t expected_in ) const
    {
        if ( this->count < expected_in )
        {
            napi_throw_type_error( this->env, "WSE_XPT_ARGUMENT",
                "The operation requires more arguments." );
            return false;
        }
        return true;
    }
};

Arguments readArguments( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments;
    arguments.env = env_in;
    arguments.count = 6U;
    napi_get_cb_info(
        env_in, info_in, &arguments.count, arguments.values, &arguments.self, nullptr );
    return arguments;
}

template <typename Holder>
Holder* unwrapHolder( napi_env env_in, napi_value self_in, const char* code_in )
{
    void* data = nullptr;
    if ( napi_unwrap( env_in, self_in, &data ) != napi_ok || data == nullptr )
    {
        napi_throw_error( env_in, code_in, "The transport object is not initialized." );
        return nullptr;
    }
    return static_cast<Holder*>( data );
}

// ------------------------------------------------------------------------------------------
// TCP
// ------------------------------------------------------------------------------------------

struct TcpHolder final
{
    wse::xpt::TcpClient client;
};

void finalizeTcp( napi_env, void* data_in, void* ) { delete static_cast<TcpHolder*>( data_in ); }

napi_value tcpConnect( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 3U ) ) return nullptr;
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;

    std::string host;
    std::uint32_t port = 0U;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readString( &host, env_in, arguments.values[ 0 ] )
         || napi_get_value_uint32( env_in, arguments.values[ 1 ], &port ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 2 ] ) )
    {
        return nullptr;
    }

    const auto status = holder->client.connect(
        wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port ) ), context );
    if ( !status.succeeded() ) { throwTransportError( env_in, status.error() ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value tcpDisconnect( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;
    holder->client.disconnect();
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value tcpIsConnected( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;
    return boolValue( env_in, holder->client.isConnected() );
}

napi_value tcpCheckPeerConnection( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;
    const auto status = holder->client.checkPeerConnection();
    if ( !status.succeeded() ) { throwTransportError( env_in, status.error() ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value tcpRemoteEndpoint( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;
    return endpointValue( env_in, holder->client.getRemoteEndpoint() );
}

napi_value tcpLocalEndpoint( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;
    return endpointValue( env_in, holder->client.getLocalEndpoint() );
}

napi_value tcpSend( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 2U ) ) return nullptr;
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;

    std::vector<std::uint8_t> bytes;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readBytes( &bytes, env_in, arguments.values[ 0 ] ) )
    {
        napi_throw_type_error( env_in, "WSE_XPT_ARGUMENT", "data must be a Buffer or TypedArray." );
        return nullptr;
    }
    if ( !readContext( &context, env_in, arguments.values[ 1 ] ) ) return nullptr;

    const auto result = holder->client.send( bytes, context );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return nullptr; }
    return int64Value( env_in, static_cast<std::int64_t>( result.value() ) );
}

napi_value tcpReceive( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 2U ) ) return nullptr;
    TcpHolder* holder = unwrapHolder<TcpHolder>( env_in, arguments.self, "WSE_XPT_TCP" );
    if ( holder == nullptr ) return nullptr;

    std::int64_t maximum = 0;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( napi_get_value_int64( env_in, arguments.values[ 0 ], &maximum ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 1 ] ) )
    {
        return nullptr;
    }

    auto result = holder->client.receive( static_cast<std::size_t>( maximum ), context );
    if ( !result.succeeded() ) { throwTransportError( env_in, result.error() ); return nullptr; }
    return bytesValue( env_in, result.value() );
}

napi_value createTcpClient( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) TcpHolder();
    if ( holder == nullptr ) return nullptr;
    napi_value object = nullptr;
    napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "connect", nullptr, tcpConnect, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "disconnect", nullptr, tcpDisconnect, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "isConnected", nullptr, tcpIsConnected, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "checkPeerConnection", nullptr, tcpCheckPeerConnection, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "remoteEndpoint", nullptr, tcpRemoteEndpoint, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "localEndpoint", nullptr, tcpLocalEndpoint, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "send", nullptr, tcpSend, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "receive", nullptr, tcpReceive, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object,
             sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeTcp, nullptr, nullptr ) != napi_ok )
    {
        delete holder;
        return nullptr;
    }
    return object;
}

// ------------------------------------------------------------------------------------------
// UDP
// ------------------------------------------------------------------------------------------

struct UdpHolder final
{
    wse::xpt::UdpClient client;
};

void finalizeUdp( napi_env, void* data_in, void* ) { delete static_cast<UdpHolder*>( data_in ); }

napi_value udpBind( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 3U ) ) return nullptr;
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;

    std::string host;
    std::uint32_t port = 0U;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readString( &host, env_in, arguments.values[ 0 ] )
         || napi_get_value_uint32( env_in, arguments.values[ 1 ], &port ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 2 ] ) )
    {
        return nullptr;
    }

    const auto status = holder->client.bind(
        wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port ) ), context );
    if ( !status.succeeded() ) { throwTransportError( env_in, status.error() ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value udpClose( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;
    holder->client.close();
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value udpIsOpen( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;
    return boolValue( env_in, holder->client.isOpen() );
}

napi_value udpLocalEndpoint( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;
    return endpointValue( env_in, holder->client.getLocalEndpoint() );
}

napi_value udpSendTo( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 4U ) ) return nullptr;
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;

    std::string host;
    std::uint32_t port = 0U;
    std::vector<std::uint8_t> bytes;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readString( &host, env_in, arguments.values[ 0 ] )
         || napi_get_value_uint32( env_in, arguments.values[ 1 ], &port ) != napi_ok )
    {
        return nullptr;
    }
    if ( !readBytes( &bytes, env_in, arguments.values[ 2 ] ) )
    {
        napi_throw_type_error( env_in, "WSE_XPT_ARGUMENT", "data must be a Buffer or TypedArray." );
        return nullptr;
    }
    if ( !readContext( &context, env_in, arguments.values[ 3 ] ) ) return nullptr;

    const auto result = holder->client.sendTo(
        wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port ) ), bytes, context );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return nullptr; }
    return int64Value( env_in, static_cast<std::int64_t>( result.value() ) );
}

napi_value udpReceiveFrom( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 2U ) ) return nullptr;
    UdpHolder* holder = unwrapHolder<UdpHolder>( env_in, arguments.self, "WSE_XPT_UDP" );
    if ( holder == nullptr ) return nullptr;

    std::int64_t maximum = 0;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( napi_get_value_int64( env_in, arguments.values[ 0 ], &maximum ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 1 ] ) )
    {
        return nullptr;
    }

    auto result = holder->client.receiveFrom( static_cast<std::size_t>( maximum ), context );
    if ( !result.succeeded() )
    {
        const auto failure = wse::binding::detail::transferFailure( result );
        if( failure.datagram ) throwTransferError( env_in, failure );
        else throwTransportError( env_in, result.error() );
        return nullptr;
    }

    const wse::xpt::UdpDatagram& datagram = result.value();
    napi_value object = nullptr;
    if ( napi_create_object( env_in, &object ) != napi_ok ) return nullptr;
    if ( !setProperty( env_in, object, "source", endpointValue( env_in, datagram.source() ) )
         || !setProperty( env_in, object, "payload", bytesValue( env_in, datagram.payload() ) ) )
    {
        return nullptr;
    }
    return object;
}

napi_value createUdpClient( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) UdpHolder();
    if ( holder == nullptr ) return nullptr;
    napi_value object = nullptr;
    napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "bind", nullptr, udpBind, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "close", nullptr, udpClose, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "isOpen", nullptr, udpIsOpen, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "localEndpoint", nullptr, udpLocalEndpoint, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "sendTo", nullptr, udpSendTo, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "receiveFrom", nullptr, udpReceiveFrom, nullptr, nullptr, nullptr,
            napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object,
             sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeUdp, nullptr, nullptr ) != napi_ok )
    {
        delete holder;
        return nullptr;
    }
    return object;
}

napi_value udpMaximumDatagramSize( napi_env env_in, napi_callback_info )
{
    return int64Value( env_in,
        static_cast<std::int64_t>( wse::xpt::UdpClient::maximumDatagramSize() ) );
}

// ------------------------------------------------------------------------------------------
// Serial
// ------------------------------------------------------------------------------------------

struct SerialHolder final
{
    wse::xpt::SerialPort port;
};

void finalizeSerial( napi_env, void* data_in, void* )
{
    delete static_cast<SerialHolder*>( data_in );
}

napi_value serialOpen( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 3U ) ) return nullptr;
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;

    std::string device;
    std::int64_t baud_rate = 0;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readString( &device, env_in, arguments.values[ 0 ] )
         || napi_get_value_int64( env_in, arguments.values[ 1 ], &baud_rate ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 2 ] ) )
    {
        return nullptr;
    }

    const auto status = holder->port.open(
        device, static_cast<std::int32_t>( baud_rate ), context );
    if ( !status.succeeded() ) { throwTransportError( env_in, status.error() ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value serialClose( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;
    holder->port.close();
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value serialIsOpen( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;
    return boolValue( env_in, holder->port.isOpen() );
}

napi_value serialDeviceName( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;
    return stringValue( env_in, holder->port.getDeviceName() );
}

napi_value serialBaudRate( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;
    return int64Value( env_in, holder->port.getBaudRate() );
}

napi_value serialSend( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 2U ) ) return nullptr;
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;

    std::vector<std::uint8_t> bytes;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readBytes( &bytes, env_in, arguments.values[ 0 ] ) )
    {
        napi_throw_type_error( env_in, "WSE_XPT_ARGUMENT", "data must be a Buffer or TypedArray." );
        return nullptr;
    }
    if ( !readContext( &context, env_in, arguments.values[ 1 ] ) ) return nullptr;

    const auto result = holder->port.send( bytes, context );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return nullptr; }
    return int64Value( env_in, static_cast<std::int64_t>( result.value() ) );
}

napi_value serialReceive( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 2U ) ) return nullptr;
    SerialHolder* holder = unwrapHolder<SerialHolder>( env_in, arguments.self, "WSE_XPT_SERIAL" );
    if ( holder == nullptr ) return nullptr;

    std::int64_t maximum = 0;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( napi_get_value_int64( env_in, arguments.values[ 0 ], &maximum ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 1 ] ) )
    {
        return nullptr;
    }

    auto result = holder->port.receive( static_cast<std::size_t>( maximum ), context );
    if ( !result.succeeded() ) { throwTransportError( env_in, result.error() ); return nullptr; }
    return bytesValue( env_in, result.value() );
}

napi_value createSerialPort( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) SerialHolder();
    if ( holder == nullptr ) return nullptr;
    napi_value object = nullptr;
    napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "open", nullptr, serialOpen, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "close", nullptr, serialClose, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "isOpen", nullptr, serialIsOpen, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "deviceName", nullptr, serialDeviceName, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "baudRate", nullptr, serialBaudRate, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "send", nullptr, serialSend, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "receive", nullptr, serialReceive, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object,
             sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeSerial, nullptr, nullptr ) != napi_ok )
    {
        delete holder;
        return nullptr;
    }
    return object;
}

// ------------------------------------------------------------------------------------------
// HTTP
// ------------------------------------------------------------------------------------------

//! \~japanese `{ method, url, headers, body }`からHTTP Requestを組み立てる.
//! \~english  Builds an HTTP request from `{ method, url, headers, body }`.
bool readHttpRequest(
      std::unique_ptr<wse::xpt::HttpRequest>* const p_request_out
    , napi_env env_in, napi_value value_in )
{
    std::unique_ptr<wse::xpt::HttpRequest>& request_out = *p_request_out;

    napi_value method_value = nullptr;
    napi_value url_value = nullptr;
    std::uint32_t method = 0U;
    std::string url;
    if ( napi_get_named_property( env_in, value_in, "method", &method_value ) != napi_ok
         || napi_get_value_uint32( env_in, method_value, &method ) != napi_ok
         || napi_get_named_property( env_in, value_in, "url", &url_value ) != napi_ok
         || !readString( &url, env_in, url_value ) )
    {
        napi_throw_type_error( env_in, "WSE_XPT_HTTP",
            "A request requires an integer method and a string url." );
        return false;
    }
    if ( method > static_cast<std::uint32_t>( wse::xpt::eHttpMethod::Delete ) )
    {
        napi_throw_range_error( env_in, "WSE_XPT_HTTP",
            "method is outside the supported range." );
        return false;
    }

    request_out = std::make_unique<wse::xpt::HttpRequest>(
        static_cast<wse::xpt::eHttpMethod>( method ), url );

    bool present = false;
    napi_value headers_value = nullptr;
    if ( napi_has_named_property( env_in, value_in, "headers", &present ) == napi_ok && present
         && napi_get_named_property( env_in, value_in, "headers", &headers_value ) == napi_ok )
    {
        bool is_array = false;
        if ( napi_is_array( env_in, headers_value, &is_array ) == napi_ok && is_array )
        {
            std::uint32_t length = 0U;
            napi_get_array_length( env_in, headers_value, &length );
            for ( std::uint32_t index = 0U; index < length; ++index )
            {
                napi_value entry = nullptr;
                napi_value name_value = nullptr;
                napi_value header_value = nullptr;
                std::string name;
                std::string text;
                if ( napi_get_element( env_in, headers_value, index, &entry ) != napi_ok
                     || napi_get_named_property( env_in, entry, "name", &name_value ) != napi_ok
                     || napi_get_named_property( env_in, entry, "value", &header_value ) != napi_ok
                     || !readString( &name, env_in, name_value )
                     || !readString( &text, env_in, header_value ) )
                {
                    napi_throw_type_error( env_in, "WSE_XPT_HTTP",
                        "Each header requires a name and a value." );
                    return false;
                }
                request_out->addHeader( name, text );
            }
        }
    }

    napi_value body_value = nullptr;
    present = false;
    if ( napi_has_named_property( env_in, value_in, "body", &present ) == napi_ok && present
         && napi_get_named_property( env_in, value_in, "body", &body_value ) == napi_ok )
    {
        std::vector<std::uint8_t> body;
        if ( readBytes( &body, env_in, body_value ) )
        {
            request_out->setBody( body );
        }
    }
    return true;
}

//! \~japanese HTTP Responseを、成功時と失敗時で同一の平坦なObjectへ変換する.
//! \~english  Converts an HTTP response into the one plain object shape both paths report.
napi_value httpResponseValue( napi_env env_in, const wse::xpt::HttpResponse& response_in )
{
    napi_value headers = nullptr;
    if ( napi_create_array_with_length(
             env_in, response_in.headers().size(), &headers ) != napi_ok ) return nullptr;
    std::uint32_t index = 0U;
    for ( const auto& header : response_in.headers() )
    {
        napi_value entry = nullptr;
        if ( napi_create_object( env_in, &entry ) != napi_ok ) return nullptr;
        if ( !setProperty( env_in, entry, "name", stringValue( env_in, header.name ) )
             || !setProperty( env_in, entry, "value", stringValue( env_in, header.value ) )
             || napi_set_element( env_in, headers, index, entry ) != napi_ok )
        {
            return nullptr;
        }
        ++index;
    }

    napi_value object = nullptr;
    if ( napi_create_object( env_in, &object ) != napi_ok ) return nullptr;
    if ( !setProperty( env_in, object, "statusCode",
             uint32Value( env_in, response_in.status_code() ) )
         || !setProperty( env_in, object, "attemptCount",
             uint32Value( env_in, response_in.attempt_count() ) )
         || !setProperty( env_in, object, "headers", headers )
         || !setProperty( env_in, object, "body", bytesValue( env_in, response_in.body() ) ) )
    {
        return nullptr;
    }
    return object;
}

//! \~japanese 4xx/5xxはErrorであり続けるが、受信済みResponseをそのErrorに載せて渡す.
//! \~english  A 4xx or 5xx stays an error, but the received response rides on that error.
//!
//! `HttpClient::execute` sets the error and still carries the response for an HTTP status
//! failure, so throwing without the response would discard the status code the caller needs.
//! The property is attached for `HttpStatusError` only; every other failure leaves it undefined.
void throwHttpError( napi_env env_in,
    const wse::xpt::TransportError& error_in, const wse::xpt::HttpResponse& response_in )
{
    const wse::binding::Error error = wse::binding::fromXptError( error_in );
    napi_value failure = nullptr;
    if ( napi_create_error( env_in, nullptr,
             stringValue( env_in, error.message() ), &failure ) != napi_ok ) return;
    setProperty( env_in, failure, "category",
        uint32Value( env_in, static_cast<std::uint32_t>( error.category() ) ) );
    setProperty( env_in, failure, "code", int64Value( env_in, error.code() ) );
    setProperty( env_in, failure, "nativeCode", int64Value( env_in, error.nativeCode() ) );
    if ( error_in.code() == wse::xpt::eTransportErrorCode::HttpStatusError )
    {
        setProperty( env_in, failure, "response", httpResponseValue( env_in, response_in ) );
    }
    napi_throw( env_in, failure );
}

napi_value httpExecute( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    if ( !arguments.require( 3U ) ) return nullptr;

    std::unique_ptr<wse::xpt::HttpRequest> request;
    std::int64_t maximum_body = 0;
    wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 0 ) );
    if ( !readHttpRequest( &request, env_in, arguments.values[ 0 ] )
         || napi_get_value_int64( env_in, arguments.values[ 1 ], &maximum_body ) != napi_ok
         || !readContext( &context, env_in, arguments.values[ 2 ] ) )
    {
        return nullptr;
    }

    // The binding never retries on the caller's behalf.
    const wse::xpt::HttpExecutionOptions options(
          static_cast<std::size_t>( maximum_body )
        , wse::xpt::RetryPolicy()
        , wse::xpt::eRetryOperationSafety::NonIdempotent
        , false );

    std::string username;
    std::string secret;
    bool authenticated = false;
    if ( arguments.count >= 5U )
    {
        napi_valuetype type = napi_undefined;
        napi_typeof( env_in, arguments.values[ 3 ], &type );
        if ( type == napi_string )
        {
            authenticated = readString( &username, env_in, arguments.values[ 3 ] )
                && readString( &secret, env_in, arguments.values[ 4 ] );
        }
    }

    const wse::xpt::HttpClient client;
    auto result = authenticated
        ? client.executeAuthenticated( *request, options,
              wse::xpt::HttpAuthentication(
                  wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated, username, secret ),
              context )
        : client.execute( *request, options, context );
    // The plain and the authenticated call share this path, so both report a status failure the
    // same way.
    if ( !result.succeeded() )
    { throwHttpError( env_in, result.error(), result.value() ); return nullptr; }

    return httpResponseValue( env_in, result.value() );
}

// ------------------------------------------------------------------------------------------
// Cancellation
// ------------------------------------------------------------------------------------------

napi_value cancellationCancel( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    CancellationHolder* holder =
        unwrapHolder<CancellationHolder>( env_in, arguments.self, "WSE_XPT_CANCELLATION" );
    if ( holder == nullptr ) return nullptr;
    holder->source.cancel();
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value cancellationIsRequested( napi_env env_in, napi_callback_info info_in )
{
    Arguments arguments = readArguments( env_in, info_in );
    CancellationHolder* holder =
        unwrapHolder<CancellationHolder>( env_in, arguments.self, "WSE_XPT_CANCELLATION" );
    if ( holder == nullptr ) return nullptr;
    return boolValue( env_in, holder->source.isCancellationRequested() );
}

napi_value createCancellationSource( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) CancellationHolder();
    if ( holder == nullptr ) return nullptr;
    napi_value object = nullptr;
    napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "cancel", nullptr, cancellationCancel, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "isCancellationRequested", nullptr, cancellationIsRequested, nullptr, nullptr, nullptr,
            napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object,
             sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeCancellation, nullptr, nullptr ) != napi_ok )
    {
        delete holder;
        return nullptr;
    }
    return object;
}

#endif // WSE_HAS_XPT

} // namespace

napi_status registerXptAddon( napi_env env_in, napi_value exports_in )
{
#ifdef WSE_HAS_XPT
    if ( !setProperty( env_in, exports_in, "TransportErrorCode", enumValue( env_in, {
            { "None", 0U }, { "InvalidArgument", 1U }, { "HostNotFound", 2U },
            { "AddressUnavailable", 3U }, { "ConnectionRefused", 4U },
            { "ConnectionReset", 5U }, { "NetworkUnreachable", 6U },
            { "NotConnected", 7U }, { "RemoteClosed", 8U }, { "TimedOut", 9U },
            { "Cancelled", 10U }, { "BindFailed", 11U }, { "SendFailed", 12U },
            { "ReceiveFailed", 13U }, { "MessageTooLarge", 14U },
            { "DatagramTruncated", 15U }, { "ResourceExhausted", 16U },
            { "Unsupported", 17U }, { "Unknown", 18U }, { "OpenFailed", 19U },
            { "ConfigurationFailed", 20U }, { "HttpStatusError", 21U },
            { "ResponseTooLarge", 22U }, { "SecurityFailed", 23U },
        } ) )
         || !setProperty( env_in, exports_in, "HttpMethod", enumValue( env_in, {
            { "Get", 0U }, { "Head", 1U }, { "Post", 2U },
            { "Put", 3U }, { "Patch", 4U }, { "Delete", 5U },
        } ) ) ) return napi_generic_failure;

    const napi_property_descriptor properties[] = {
        { "_createTcpClient", nullptr, createTcpClient, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "_createUdpClient", nullptr, createUdpClient, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "_createSerialPort", nullptr, createSerialPort, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "_createCancellationSource", nullptr, createCancellationSource, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "_udpMaximumDatagramSize", nullptr, udpMaximumDatagramSize, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "httpExecute", nullptr, httpExecute, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    return napi_define_properties( env_in, exports_in,
        sizeof( properties ) / sizeof( properties[ 0 ] ), properties );
#else
    (void) env_in; (void) exports_in;
    return napi_ok;
#endif
}
