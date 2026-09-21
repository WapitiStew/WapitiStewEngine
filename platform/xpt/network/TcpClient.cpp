//*****************************************************************************************************************
//!
//! @file    TcpClient.cpp
//! @brief   \~japanese WinSock／POSIX Socketを用いてPortable TCP Clientを実装する.
//! @brief   \~english  Implements the portable TCP client with WinSock or POSIX sockets.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "xpt/tcp/TcpClient.h"

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <utility>

namespace wse
{
namespace xpt
{

namespace
{

#if defined( _WIN32 )
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

using Clock = std::chrono::steady_clock;

enum class eSocketWait
{
      Read = 0
    , Write
};

int nativeSocketError() noexcept
{
#if defined( _WIN32 )
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool interruptedError( const int code_in ) noexcept
{
#if defined( _WIN32 )
    return code_in == WSAEINTR;
#else
    return code_in == EINTR;
#endif
}

bool pendingError( const int code_in ) noexcept
{
#if defined( _WIN32 )
    return code_in == WSAEINPROGRESS || code_in == WSAEWOULDBLOCK || code_in == WSAEINVAL;
#else
    return code_in == EINPROGRESS || code_in == EWOULDBLOCK || code_in == EAGAIN;
#endif
}

bool wouldBlockError( const int code_in ) noexcept
{
#if defined( _WIN32 )
    return code_in == WSAEWOULDBLOCK;
#else
    return code_in == EWOULDBLOCK || code_in == EAGAIN;
#endif
}

void closeSocket( const SocketHandle socket_in ) noexcept
{
    if ( socket_in == INVALID_SOCKET_HANDLE )
    {
        return;
    }
#if defined( _WIN32 )
    closesocket( socket_in );
#else
    close( socket_in );
#endif
}

void shutdownSocket( const SocketHandle socket_in ) noexcept
{
    if ( socket_in == INVALID_SOCKET_HANDLE )
    {
        return;
    }
#if defined( _WIN32 )
    shutdown( socket_in, SD_BOTH );
#else
    shutdown( socket_in, SHUT_RDWR );
#endif
}

bool initializeSockets( int* p_native_code_out ) noexcept
{
    int& native_code_out = *p_native_code_out;
#if defined( _WIN32 )
    static std::once_flag flag;
    static int startup_result = WSASYSNOTREADY;
    std::call_once( flag, []() {
        WSADATA data = {};
        startup_result = WSAStartup( MAKEWORD( 2, 2 ), &data );
    } );
    native_code_out = startup_result;
    return startup_result == 0;
#else
    native_code_out = 0;
    return true;
#endif
}

bool setNonBlocking( int* p_native_code_out, const SocketHandle socket_in ) noexcept
{
    int& native_code_out = *p_native_code_out;
#if defined( _WIN32 )
    u_long enabled = 1;
    if ( ioctlsocket( socket_in, FIONBIO, &enabled ) == SOCKET_ERROR )
    {
        native_code_out = nativeSocketError();
        return false;
    }
#else
    const int flags = fcntl( socket_in, F_GETFL, 0 );
    if ( flags < 0 || fcntl( socket_in, F_SETFL, flags | O_NONBLOCK ) < 0 )
    {
        native_code_out = nativeSocketError();
        return false;
    }
#endif
    native_code_out = 0;
    return true;
}

TransportError validationError( const char* message_in )
{
    return TransportError( eTransportErrorCategory::Validation,
                           eTransportErrorCode::InvalidArgument, message_in );
}

TransportError timeoutError()
{
    return TransportError( eTransportErrorCategory::Timeout,
                           eTransportErrorCode::TimedOut, "The transport operation timed out." );
}

TransportError cancellationError()
{
    return TransportError( eTransportErrorCategory::Cancellation,
                           eTransportErrorCode::Cancelled, "The transport operation was cancelled." );
}

TransportError socketError(
      const int                 code_in
    , const eTransportErrorCode fallback_in
    , const char*               message_in
)
{
    eTransportErrorCode stable_code = fallback_in;
    eTransportErrorCategory category = eTransportErrorCategory::InputOutput;

#if defined( _WIN32 )
    if ( code_in == WSAECONNREFUSED )
#else
    if ( code_in == ECONNREFUSED )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::ConnectionRefused;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAECONNRESET || code_in == WSAECONNABORTED )
#else
    else if ( code_in == ECONNRESET || code_in == ECONNABORTED || code_in == EPIPE )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::ConnectionReset;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAENETUNREACH || code_in == WSAEHOSTUNREACH )
#else
    else if ( code_in == ENETUNREACH || code_in == EHOSTUNREACH )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::NetworkUnreachable;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAEADDRNOTAVAIL )
#else
    else if ( code_in == EADDRNOTAVAIL )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::AddressUnavailable;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAENOTCONN )
#else
    else if ( code_in == ENOTCONN )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::NotConnected;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAETIMEDOUT )
#else
    else if ( code_in == ETIMEDOUT )
#endif
    {
        category = eTransportErrorCategory::Timeout;
        stable_code = eTransportErrorCode::TimedOut;
    }

    return TransportError( category, stable_code, message_in, code_in );
}

TransportError waitForSocket(
      const SocketHandle       socket_in
    , const eSocketWait        operation_in
    , const Clock::time_point  deadline_in
    , const CancellationToken& cancellation_in
)
{
    for ( ;; )
    {
        if ( cancellation_in.isCancellationRequested() )
        {
            return cancellationError();
        }

        const Clock::time_point now = Clock::now();
        if ( now >= deadline_in )
        {
            return timeoutError();
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>( deadline_in - now );
        const auto slice = ( std::min )(
            remaining, std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::milliseconds( 20 ) ) );
        timeval wait_time = {};
        wait_time.tv_sec = static_cast<long>( slice.count() / 1000000 );
        wait_time.tv_usec = static_cast<long>( slice.count() % 1000000 );

        fd_set read_set;
        fd_set write_set;
        FD_ZERO( &read_set );
        FD_ZERO( &write_set );
        if ( operation_in == eSocketWait::Read )
        {
            FD_SET( socket_in, &read_set );
        }
        else
        {
            FD_SET( socket_in, &write_set );
        }

#if defined( _WIN32 )
        const int result = select( 0, operation_in == eSocketWait::Read ? &read_set : nullptr,
                                   operation_in == eSocketWait::Write ? &write_set : nullptr,
                                   nullptr, &wait_time );
#else
        const int result = select( socket_in + 1, operation_in == eSocketWait::Read ? &read_set : nullptr,
                                   operation_in == eSocketWait::Write ? &write_set : nullptr,
                                   nullptr, &wait_time );
#endif
        if ( result > 0 )
        {
            return TransportError();
        }
        if ( result == 0 )
        {
            continue;
        }

        const int code = nativeSocketError();
        if ( interruptedError( code ) )
        {
            continue;
        }
        return socketError( code, eTransportErrorCode::Unknown,
                            "Waiting for the socket failed." );
    }
}

Clock::time_point operationDeadline( const OperationContext& context_in )
{
    const auto duration = context_in.timeout().duration();
    const Clock::time_point now = Clock::now();
    const auto maximum = std::chrono::duration_cast<std::chrono::milliseconds>(
        ( Clock::time_point::max )() - now );
    if ( duration >= maximum )
    {
        return ( Clock::time_point::max )();
    }
    return now + duration;
}

Endpoint localEndpoint( const SocketHandle socket_in )
{
    sockaddr_storage address = {};
#if defined( _WIN32 )
    int address_size = sizeof( address );
#else
    socklen_t address_size = sizeof( address );
#endif
    if ( getsockname( socket_in, reinterpret_cast<sockaddr*>( &address ), &address_size ) != 0 )
    {
        return Endpoint();
    }

    char host[NI_MAXHOST] = {};
    char service[NI_MAXSERV] = {};
    if ( getnameinfo( reinterpret_cast<const sockaddr*>( &address ), address_size,
                      host, sizeof( host ), service, sizeof( service ),
                      NI_NUMERICHOST | NI_NUMERICSERV ) != 0 )
    {
        return Endpoint();
    }
    char* end = nullptr;
    const unsigned long port = std::strtoul( service, &end, 10 );
    if ( end == service || *end != '\0' || port > 65535UL )
    {
        return Endpoint();
    }
    return Endpoint( host, static_cast<std::uint16_t>( port ) );
}

} // namespace

class TcpClient::Impl
{
    //! @brief Construct all members with explicit defaults.
public:
    Impl()
        : socket ( INVALID_SOCKET_HANDLE )
        , remote ()
        , local  ()
    {
    }
private:

  public:
    SocketHandle socket;
    Endpoint remote;
    Endpoint local;

    void close() noexcept
    {
        shutdownSocket( this->socket );
        closeSocket( this->socket );
        this->socket = INVALID_SOCKET_HANDLE;
        this->remote = Endpoint();
        this->local = Endpoint();
    }

    ~Impl()
    {
        this->close();
    }
};

TcpClient::TcpClient()
    : m_impl ( std::make_unique<Impl>() )
{
}

TcpClient::~TcpClient() = default;

TcpClient::TcpClient( TcpClient&& other_inout ) noexcept = default;

TcpClient& TcpClient::operator=( TcpClient&& other_inout ) noexcept = default;

TransportStatus TcpClient::connect(
      const Endpoint&         endpoint_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr )
    {
        return TransportStatus::failure( validationError( "The TCP client was moved from." ) );
    }
    if ( !endpoint_in.isValid() )
    {
        return TransportStatus::failure( validationError( "A host and non-zero port are required." ) );
    }
    if ( !context_in.isValid() )
    {
        return TransportStatus::failure( validationError( "The timeout must not be negative." ) );
    }
    if ( context_in.cancellation().isCancellationRequested() )
    {
        return TransportStatus::failure( cancellationError() );
    }

    this->m_impl->close();
    const Clock::time_point deadline = operationDeadline( context_in );

    int native_code = 0;
    if ( !initializeSockets( &native_code ) )
    {
        return TransportStatus::failure( socketError( native_code, eTransportErrorCode::Unsupported,
                                "The platform socket runtime could not be initialized." ) );
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *addresses = nullptr;
    const std::string service = std::to_string( endpoint_in.port() );
    const int resolve_result = getaddrinfo( endpoint_in.host().c_str(), service.c_str(),
                                            &hints, &addresses );
    if ( resolve_result != 0 )
    {
        return TransportStatus::failure(
            TransportError( eTransportErrorCategory::Resolution,
                            eTransportErrorCode::HostNotFound,
                            "The endpoint host could not be resolved.", resolve_result ) );
    }

    TransportError last_error( eTransportErrorCategory::Connection,
                               eTransportErrorCode::AddressUnavailable,
                               "No resolved address could be connected." );
    for ( addrinfo *address = addresses; address != nullptr; address = address->ai_next )
    {
        if ( context_in.cancellation().isCancellationRequested() )
        {
            last_error = cancellationError();
            break;
        }
        if ( Clock::now() >= deadline )
        {
            last_error = timeoutError();
            break;
        }

        const SocketHandle candidate = socket( address->ai_family, address->ai_socktype,
                                               address->ai_protocol );
        if ( candidate == INVALID_SOCKET_HANDLE )
        {
            last_error = socketError( nativeSocketError(),
                                      eTransportErrorCode::ResourceExhausted,
                                      "Creating a TCP socket failed." );
            continue;
        }

        if ( !setNonBlocking( &native_code, candidate ) )
        {
            last_error = socketError( native_code, eTransportErrorCode::Unsupported,
                                      "Enabling non-blocking socket I/O failed." );
            closeSocket( candidate );
            continue;
        }

#if defined( _WIN32 )
        const int connect_result = ::connect( candidate, address->ai_addr,
                                              static_cast<int>( address->ai_addrlen ) );
        const bool connected_immediately = connect_result != SOCKET_ERROR;
#else
        const int connect_result = ::connect( candidate, address->ai_addr, address->ai_addrlen );
        const bool connected_immediately = connect_result == 0;
#endif
        if ( !connected_immediately )
        {
            native_code = nativeSocketError();
            if ( !pendingError( native_code ) )
            {
                last_error = socketError( native_code, eTransportErrorCode::Unknown,
                                          "Connecting the TCP socket failed." );
                closeSocket( candidate );
                continue;
            }

            const TransportError wait_error = waitForSocket(
                candidate, eSocketWait::Write, deadline, context_in.cancellation() );
            if ( !wait_error.ok() )
            {
                last_error = wait_error;
                closeSocket( candidate );
                if ( wait_error.category() == eTransportErrorCategory::Timeout ||
                     wait_error.category() == eTransportErrorCategory::Cancellation )
                {
                    break;
                }
                continue;
            }

            int connection_error = 0;
#if defined( _WIN32 )
            int error_size = sizeof( connection_error );
#else
            socklen_t error_size = sizeof( connection_error );
#endif
            if ( getsockopt( candidate, SOL_SOCKET, SO_ERROR,
#if defined( _WIN32 )
                             reinterpret_cast<char *>( &connection_error ),
#else
                             &connection_error,
#endif
                             &error_size ) != 0 || connection_error != 0 )
            {
                if ( connection_error == 0 )
                {
                    connection_error = nativeSocketError();
                }
                last_error = socketError( connection_error, eTransportErrorCode::Unknown,
                                          "Connecting the TCP socket failed." );
                closeSocket( candidate );
                continue;
            }
        }

        this->m_impl->socket = candidate;
        this->m_impl->remote = endpoint_in;
        this->m_impl->local = localEndpoint( candidate );
        freeaddrinfo( addresses );
        return TransportStatus::success();
    }

    freeaddrinfo( addresses );
    return TransportStatus::failure( std::move( last_error ) );
}

void TcpClient::disconnect() noexcept
{
    if ( this->m_impl != nullptr )
    {
        this->m_impl->close();
    }
}

bool TcpClient::isConnected() const noexcept
{
    return this->m_impl != nullptr
        && this->m_impl->socket != INVALID_SOCKET_HANDLE;
}

TransportStatus TcpClient::checkPeerConnection()
{
    if ( this->m_impl == nullptr || !this->isConnected() )
    {
        return TransportStatus::failure( TransportError( eTransportErrorCategory::Connection,
                                   eTransportErrorCode::NotConnected,
                                   "The TCP client is not connected." ) );
    }

    std::uint8_t pending = 0U;
    for ( std::size_t attempt = 0U; attempt < 4U; ++attempt )
    {
#if defined( _WIN32 )
        const int received = recv( this->m_impl->socket,
                                   reinterpret_cast<char *>( &pending ), 1, MSG_PEEK );
        if ( received != SOCKET_ERROR )
#else
        const int received = static_cast<int>( recv(
            this->m_impl->socket, &pending, 1U, MSG_PEEK ) );
        if ( received >= 0 )
#endif
        {
            if ( received > 0 )
            {
                return TransportStatus::success();
            }
            this->m_impl->close();
            return TransportStatus::failure( TransportError( eTransportErrorCategory::Connection,
                                       eTransportErrorCode::RemoteClosed,
                                       "The TCP peer closed the connection." ) );
        }

        const int code = nativeSocketError();
        if ( wouldBlockError( code ) )
        {
            return TransportStatus::success();
        }
        if ( interruptedError( code ) && attempt + 1U < 4U )
        {
            continue;
        }

        const TransportError error = socketError(
            code, eTransportErrorCode::ReceiveFailed,
            "Checking the TCP peer connection failed." );
        this->m_impl->close();
        return TransportStatus::failure( error );
    }

    this->m_impl->close();
    return TransportStatus::failure( TransportError( eTransportErrorCategory::InputOutput,
                               eTransportErrorCode::ReceiveFailed,
                               "Checking the TCP peer connection was repeatedly interrupted." ) );
}

Endpoint TcpClient::getRemoteEndpoint() const
{
    return this->m_impl == nullptr ? Endpoint() : this->m_impl->remote;
}

Endpoint TcpClient::getLocalEndpoint() const
{
    return this->m_impl == nullptr ? Endpoint() : this->m_impl->local;
}

TransferResult<std::size_t> TcpClient::send(
      const std::uint8_t*     data_in
    , const std::size_t       size_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr || !this->isConnected() )
    {
        return TransferResult<std::size_t>(
            0, TransportError( eTransportErrorCategory::Connection,
                               eTransportErrorCode::NotConnected,
                               "The TCP client is not connected." ) );
    }
    if ( ( data_in == nullptr && size_in != 0 ) || !context_in.isValid() )
    {
        return TransferResult<std::size_t>(
            0, validationError( "Send data and timeout must be valid." ) );
    }
    if ( size_in == 0 )
    {
        return TransferResult<std::size_t>( 0 );
    }

    const Clock::time_point deadline = operationDeadline( context_in );
    std::size_t sent_total = 0;
    while ( sent_total < size_in )
    {
        const TransportError wait_error = waitForSocket(
            this->m_impl->socket, eSocketWait::Write, deadline, context_in.cancellation() );
        if ( !wait_error.ok() )
        {
            return TransferResult<std::size_t>( sent_total, wait_error );
        }

        const std::size_t remaining = size_in - sent_total;
        const int request_size = static_cast<int>(
            ( std::min )( remaining, static_cast<std::size_t>( INT_MAX ) ) );
#if defined( _WIN32 )
        const int sent = ::send( this->m_impl->socket,
                                 reinterpret_cast<const char *>( data_in + sent_total ),
                                 request_size, 0 );
        if ( sent == SOCKET_ERROR )
#else
        const int sent = static_cast<int>( ::send( this->m_impl->socket, data_in + sent_total,
                                                   static_cast<std::size_t>( request_size ),
                                                   MSG_NOSIGNAL ) );
        if ( sent < 0 )
#endif
        {
            const int code = nativeSocketError();
            if ( pendingError( code ) || interruptedError( code ) )
            {
                continue;
            }
            const TransportError error = socketError(
                code, eTransportErrorCode::SendFailed, "Sending TCP data failed." );
            if ( error.category() == eTransportErrorCategory::Connection )
            {
                this->m_impl->close();
            }
            return TransferResult<std::size_t>( sent_total, error );
        }
        if ( sent == 0 )
        {
            this->m_impl->close();
            return TransferResult<std::size_t>(
                sent_total, TransportError( eTransportErrorCategory::Connection,
                                             eTransportErrorCode::RemoteClosed,
                                             "The TCP peer closed the connection." ) );
        }
        sent_total += static_cast<std::size_t>( sent );
    }
    return TransferResult<std::size_t>( sent_total );
}

TransferResult<std::size_t> TcpClient::send(
      const std::vector<std::uint8_t>& data_in
    , const OperationContext&          context_in
)
{
    return this->send( data_in.data(), data_in.size(), context_in );
}

TransferResult<std::vector<std::uint8_t>> TcpClient::receive(
      const std::size_t       maximum_size_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr || !this->isConnected() )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, TransportError( eTransportErrorCategory::Connection,
                                eTransportErrorCode::NotConnected,
                                "The TCP client is not connected." ) );
    }
    if ( maximum_size_in == 0 || maximum_size_in > static_cast<std::size_t>( INT_MAX ) ||
         !context_in.isValid() )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, validationError( "Receive size and timeout must be valid." ) );
    }

    std::vector<std::uint8_t> received;
    try
    {
        received.resize( maximum_size_in );
    }
    catch ( const std::bad_alloc & )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, TransportError( eTransportErrorCategory::InputOutput,
                                eTransportErrorCode::ResourceExhausted,
                                "Allocating the receive buffer failed." ) );
    }

    const Clock::time_point deadline = operationDeadline( context_in );
    for ( ;; )
    {
        const TransportError wait_error = waitForSocket(
            this->m_impl->socket, eSocketWait::Read, deadline, context_in.cancellation() );
        if ( !wait_error.ok() )
        {
            return TransferResult<std::vector<std::uint8_t>>( {}, wait_error );
        }

#if defined( _WIN32 )
        const int received_size = recv( this->m_impl->socket,
                                        reinterpret_cast<char *>( received.data() ),
                                        static_cast<int>( maximum_size_in ), 0 );
        if ( received_size == SOCKET_ERROR )
#else
        const int received_size = static_cast<int>( recv( this->m_impl->socket, received.data(),
                                                          maximum_size_in, 0 ) );
        if ( received_size < 0 )
#endif
        {
            const int code = nativeSocketError();
            if ( pendingError( code ) || interruptedError( code ) )
            {
                continue;
            }
            const TransportError error = socketError(
                code, eTransportErrorCode::ReceiveFailed, "Receiving TCP data failed." );
            if ( error.category() == eTransportErrorCategory::Connection )
            {
                this->m_impl->close();
            }
            return TransferResult<std::vector<std::uint8_t>>( {}, error );
        }
        if ( received_size == 0 )
        {
            this->m_impl->close();
            return TransferResult<std::vector<std::uint8_t>>(
                {}, TransportError( eTransportErrorCategory::Connection,
                                    eTransportErrorCode::RemoteClosed,
                                    "The TCP peer closed the connection." ) );
        }

        received.resize( static_cast<std::size_t>( received_size ) );
        return TransferResult<std::vector<std::uint8_t>>( std::move( received ) );
    }
}

} // namespace xpt
} // namespace wse
