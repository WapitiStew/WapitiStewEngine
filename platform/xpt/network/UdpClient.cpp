//*****************************************************************************************************************
//!
//! @file    UdpClient.cpp
//! @brief   \~japanese WinSock／POSIX Socketを用いてPortable UDP Clientを実装する.
//! @brief   \~english  Implements the portable UDP client with WinSock or POSIX sockets.
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

#include "xpt/udp/UdpClient.h"

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
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
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
constexpr std::size_t MAXIMUM_DATAGRAM_SIZE = 65507;

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
    return code_in == WSAEWOULDBLOCK;
#else
    return code_in == EWOULDBLOCK || code_in == EAGAIN;
#endif
}

bool messageTooLargeError( const int code_in ) noexcept
{
#if defined( _WIN32 )
    return code_in == WSAEMSGSIZE;
#else
    return code_in == EMSGSIZE;
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

TransportError validationError( const char* const message_in )
{
    return TransportError( eTransportErrorCategory::Validation,
                           eTransportErrorCode::InvalidArgument, message_in );
}

TransportError messageTooLargeValidationError( const char* const message_in )
{
    return TransportError( eTransportErrorCategory::Validation,
                           eTransportErrorCode::MessageTooLarge, message_in );
}

TransportError timeoutError()
{
    return TransportError( eTransportErrorCategory::Timeout,
                           eTransportErrorCode::TimedOut,
                           "The transport operation timed out." );
}

TransportError cancellationError()
{
    return TransportError( eTransportErrorCategory::Cancellation,
                           eTransportErrorCode::Cancelled,
                           "The transport operation was cancelled." );
}

TransportError socketError(
      const int                 code_in
    , const eTransportErrorCode fallback_in
    , const char* const         message_in
)
{
    eTransportErrorCode stable_code = fallback_in;
    eTransportErrorCategory category = eTransportErrorCategory::InputOutput;

#if defined( _WIN32 )
    if ( code_in == WSAEADDRINUSE )
#else
    if ( code_in == EADDRINUSE )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::BindFailed;
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
    else if ( code_in == WSAENETUNREACH || code_in == WSAEHOSTUNREACH )
#else
    else if ( code_in == ENETUNREACH || code_in == EHOSTUNREACH )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::NetworkUnreachable;
    }
#if defined( _WIN32 )
    else if ( code_in == WSAECONNRESET || code_in == WSAECONNABORTED )
#else
    else if ( code_in == ECONNRESET || code_in == ECONNABORTED )
#endif
    {
        category = eTransportErrorCategory::Connection;
        stable_code = eTransportErrorCode::ConnectionReset;
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
    else if ( messageTooLargeError( code_in ) )
    {
        category = eTransportErrorCategory::Validation;
        stable_code = eTransportErrorCode::MessageTooLarge;
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

        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline_in - now );
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
        const int result = select( 0,
                                   operation_in == eSocketWait::Read ? &read_set : nullptr,
                                   operation_in == eSocketWait::Write ? &write_set : nullptr,
                                   nullptr, &wait_time );
#else
        const int result = select( socket_in + 1,
                                   operation_in == eSocketWait::Read ? &read_set : nullptr,
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
                            "Waiting for the UDP socket failed." );
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

Endpoint endpointFromAddress( const sockaddr* const address_in, const int address_size_in )
{
    char host[ NI_MAXHOST ] = {};
    char service[ NI_MAXSERV ] = {};
#if defined( _WIN32 )
    const int result = getnameinfo( address_in, address_size_in,
                                    host, static_cast<DWORD>( sizeof( host ) ),
                                    service, static_cast<DWORD>( sizeof( service ) ),
                                    NI_NUMERICHOST | NI_NUMERICSERV );
#else
    const int result = getnameinfo( address_in, static_cast<socklen_t>( address_size_in ),
                                    host, sizeof( host ), service, sizeof( service ),
                                    NI_NUMERICHOST | NI_NUMERICSERV );
#endif
    if ( result != 0 )
    {
        return Endpoint();
    }

    const unsigned long port = std::strtoul( service, nullptr, 10 );
    if ( port == 0 || port > ( std::numeric_limits<std::uint16_t>::max )() )
    {
        return Endpoint();
    }
    return Endpoint( host, static_cast<std::uint16_t>( port ) );
}

Endpoint localEndpoint( const SocketHandle socket_in )
{
    sockaddr_storage address = {};
#if defined( _WIN32 )
    int address_size = sizeof( address );
#else
    socklen_t address_size = sizeof( address );
#endif
    if ( getsockname( socket_in, reinterpret_cast<sockaddr*>( &address ),
                      &address_size ) != 0 )
    {
        return Endpoint();
    }
    return endpointFromAddress( reinterpret_cast<const sockaddr*>( &address ),
                                static_cast<int>( address_size ) );
}

TransportError resolutionError( const int native_code_in, const char* const message_in )
{
    return TransportError( eTransportErrorCategory::Resolution,
                           eTransportErrorCode::HostNotFound,
                           message_in, native_code_in );
}

} // namespace

UdpDatagram::UdpDatagram()
    : m_source  ()
    , m_payload ()
{
}

UdpDatagram::UdpDatagram( const Endpoint& source_in, std::vector<std::uint8_t> payload_in )
    : m_source  ( source_in )
    , m_payload ( std::move( payload_in ) )
{
}

const Endpoint& UdpDatagram::source() const noexcept
{
    return this->m_source;
}

const std::vector<std::uint8_t>& UdpDatagram::payload() const noexcept
{
    return this->m_payload;
}

std::vector<std::uint8_t>& UdpDatagram::payload() noexcept
{
    return this->m_payload;
}

class UdpClient::Impl
{
    //! @brief Construct all members with explicit defaults.
public:
    Impl()
        : socket ( INVALID_SOCKET_HANDLE )
        , family ( AF_UNSPEC )
        , local  ()
    {
    }
private:

  public:
    SocketHandle socket;
    int family;
    Endpoint local;

    void close() noexcept
    {
        closeSocket( this->socket );
        this->socket = INVALID_SOCKET_HANDLE;
        this->family = AF_UNSPEC;
        this->local = Endpoint();
    }

    ~Impl()
    {
        this->close();
    }
};

UdpClient::UdpClient()
    : m_impl ( std::make_unique<Impl>() )
{
}

UdpClient::~UdpClient() = default;

UdpClient::UdpClient( UdpClient&& other_inout ) noexcept = default;

UdpClient& UdpClient::operator=( UdpClient&& other_inout ) noexcept = default;

std::size_t UdpClient::maximumDatagramSize() noexcept
{
    return MAXIMUM_DATAGRAM_SIZE;
}

TransportStatus UdpClient::bind(
      const Endpoint&         local_endpoint_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr )
    {
        return TransportStatus::failure( validationError( "The UDP client was moved from." ) );
    }
    if ( local_endpoint_in.host().empty() )
    {
        return TransportStatus::failure(
                                validationError( "A local host is required for UDP bind." ) );
    }
    if ( !context_in.isValid() )
    {
        return TransportStatus::failure( validationError( "The timeout must not be negative." ) );
    }
    if ( context_in.cancellation().isCancellationRequested() )
    {
        return TransportStatus::failure( cancellationError() );
    }

    const Clock::time_point deadline = operationDeadline( context_in );

    int native_code = 0;
    if ( !initializeSockets( &native_code ) )
    {
        return TransportStatus::failure( socketError( native_code, eTransportErrorCode::Unsupported,
                                "The platform socket runtime could not be initialized." ) );
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* addresses = nullptr;
    const std::string service = std::to_string( local_endpoint_in.port() );
    const int resolve_result = getaddrinfo( local_endpoint_in.host().c_str(), service.c_str(),
                                            &hints, &addresses );
    if ( resolve_result != 0 )
    {
        return TransportStatus::failure( resolutionError(
            resolve_result, "The local UDP bind address could not be resolved." ) );
    }

    TransportError last_error( eTransportErrorCategory::Connection,
                               eTransportErrorCode::BindFailed,
                               "No resolved local UDP address could be bound." );
    for ( addrinfo* address = addresses; address != nullptr; address = address->ai_next )
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
                                      "Creating a UDP socket failed." );
            continue;
        }
        if ( !setNonBlocking( &native_code, candidate ) )
        {
            last_error = socketError( native_code, eTransportErrorCode::Unsupported,
                                      "Enabling non-blocking UDP I/O failed." );
            closeSocket( candidate );
            continue;
        }

#if defined( _WIN32 )
        const int bind_result = ::bind( candidate, address->ai_addr,
                                        static_cast<int>( address->ai_addrlen ) );
        const bool bound = bind_result != SOCKET_ERROR;
#else
        const bool bound = ::bind( candidate, address->ai_addr,
                                   address->ai_addrlen ) == 0;
#endif
        if ( !bound )
        {
            last_error = socketError( nativeSocketError(), eTransportErrorCode::BindFailed,
                                      "Binding the UDP socket failed." );
            closeSocket( candidate );
            continue;
        }

        const Endpoint candidate_local = localEndpoint( candidate );
        if ( !candidate_local.isValid() )
        {
            last_error = TransportError(
                eTransportErrorCategory::InputOutput,
                eTransportErrorCode::BindFailed,
                "The bound UDP local endpoint could not be decoded." );
            closeSocket( candidate );
            continue;
        }

        this->m_impl->close();
        this->m_impl->socket = candidate;
        this->m_impl->family = address->ai_family;
        this->m_impl->local = candidate_local;
        freeaddrinfo( addresses );
        return TransportStatus::success();
    }

    freeaddrinfo( addresses );
    return TransportStatus::failure( std::move( last_error ) );
}

void UdpClient::close() noexcept
{
    if ( this->m_impl != nullptr )
    {
        this->m_impl->close();
    }
}

bool UdpClient::isOpen() const noexcept
{
    return this->m_impl != nullptr && this->m_impl->socket != INVALID_SOCKET_HANDLE;
}

Endpoint UdpClient::getLocalEndpoint() const
{
    return this->m_impl == nullptr ? Endpoint() : this->m_impl->local;
}

TransferResult<std::size_t> UdpClient::sendTo(
      const Endpoint&         remote_endpoint_in
    , const std::uint8_t*     data_in
    , const std::size_t       size_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr )
    {
        return TransferResult<std::size_t>(
            0, validationError( "The UDP client was moved from." ) );
    }
    if ( size_in > MAXIMUM_DATAGRAM_SIZE )
    {
        return TransferResult<std::size_t>(
            0, messageTooLargeValidationError(
                   "The UDP datagram exceeds the portable payload boundary." ) );
    }
    if ( !remote_endpoint_in.isValid() ||
         ( data_in == nullptr && size_in != 0 ) || !context_in.isValid() )
    {
        return TransferResult<std::size_t>(
            0, validationError( "UDP destination, payload size, and timeout must be valid." ) );
    }
    if ( context_in.cancellation().isCancellationRequested() )
    {
        return TransferResult<std::size_t>( 0, cancellationError() );
    }

    const Clock::time_point deadline = operationDeadline( context_in );
    int native_code = 0;
    if ( !initializeSockets( &native_code ) )
    {
        return TransferResult<std::size_t>(
            0, socketError( native_code, eTransportErrorCode::Unsupported,
                            "The platform socket runtime could not be initialized." ) );
    }

    addrinfo hints = {};
    hints.ai_family = this->isOpen() ? this->m_impl->family : AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* addresses = nullptr;
    const std::string service = std::to_string( remote_endpoint_in.port() );
    const int resolve_result = getaddrinfo( remote_endpoint_in.host().c_str(), service.c_str(),
                                            &hints, &addresses );
    if ( resolve_result != 0 )
    {
        return TransferResult<std::size_t>( 0, resolutionError(
            resolve_result, "The remote UDP endpoint could not be resolved." ) );
    }

    const std::uint8_t empty_payload = 0;
    const std::uint8_t* const payload = data_in == nullptr ? &empty_payload : data_in;
    TransportError last_error( eTransportErrorCategory::Connection,
                               eTransportErrorCode::AddressUnavailable,
                               "No resolved UDP destination could be used." );

    for ( addrinfo* address = addresses; address != nullptr; address = address->ai_next )
    {
        SocketHandle candidate = this->m_impl->socket;
        const bool owns_candidate = candidate == INVALID_SOCKET_HANDLE;
        if ( owns_candidate )
        {
            candidate = socket( address->ai_family, address->ai_socktype, address->ai_protocol );
            if ( candidate == INVALID_SOCKET_HANDLE )
            {
                last_error = socketError( nativeSocketError(),
                                          eTransportErrorCode::ResourceExhausted,
                                          "Creating a UDP socket failed." );
                continue;
            }
            if ( !setNonBlocking( &native_code, candidate ) )
            {
                last_error = socketError( native_code, eTransportErrorCode::Unsupported,
                                          "Enabling non-blocking UDP I/O failed." );
                closeSocket( candidate );
                continue;
            }
        }

        for ( ;; )
        {
            const TransportError wait_error = waitForSocket(
                candidate, eSocketWait::Write, deadline, context_in.cancellation() );
            if ( !wait_error.ok() )
            {
                last_error = wait_error;
                if ( owns_candidate )
                {
                    closeSocket( candidate );
                }
                freeaddrinfo( addresses );
                return TransferResult<std::size_t>( 0, std::move( last_error ) );
            }

#if defined( _WIN32 )
            const int sent_size = sendto(
                candidate, reinterpret_cast<const char*>( payload ),
                static_cast<int>( size_in ), 0, address->ai_addr,
                static_cast<int>( address->ai_addrlen ) );
            const bool send_failed = sent_size == SOCKET_ERROR;
#else
            const int sent_size = static_cast<int>( sendto(
                candidate, payload, size_in, 0, address->ai_addr, address->ai_addrlen ) );
            const bool send_failed = sent_size < 0;
#endif
            if ( send_failed )
            {
                const int code = nativeSocketError();
                if ( pendingError( code ) || interruptedError( code ) )
                {
                    continue;
                }
                last_error = socketError( code, eTransportErrorCode::SendFailed,
                                          "Sending the UDP datagram failed." );
                break;
            }

            if ( static_cast<std::size_t>( sent_size ) != size_in )
            {
                last_error = TransportError( eTransportErrorCategory::Protocol,
                                             eTransportErrorCode::SendFailed,
                                             "The UDP datagram was not sent atomically." );
                break;
            }

            if ( owns_candidate )
            {
                this->m_impl->socket = candidate;
                this->m_impl->family = address->ai_family;
                this->m_impl->local = localEndpoint( candidate );
            }
            freeaddrinfo( addresses );
            return TransferResult<std::size_t>( size_in );
        }

        if ( owns_candidate )
        {
            closeSocket( candidate );
        }
    }

    freeaddrinfo( addresses );
    return TransferResult<std::size_t>( 0, std::move( last_error ) );
}

TransferResult<std::size_t> UdpClient::sendTo(
      const Endpoint&                  remote_endpoint_in
    , const std::vector<std::uint8_t>& data_in
    , const OperationContext&          context_in
)
{
    return this->sendTo( remote_endpoint_in, data_in.data(), data_in.size(), context_in );
}

TransferResult<UdpDatagram> UdpClient::receiveFrom(
      const std::size_t       maximum_size_in
    , const OperationContext& context_in
)
{
    if ( this->m_impl == nullptr || !this->isOpen() )
    {
        return TransferResult<UdpDatagram>(
            UdpDatagram(), TransportError( eTransportErrorCategory::Connection,
                                           eTransportErrorCode::NotConnected,
                                           "The UDP socket is not open." ) );
    }
    if ( maximum_size_in == 0 || maximum_size_in > MAXIMUM_DATAGRAM_SIZE ||
         !context_in.isValid() )
    {
        return TransferResult<UdpDatagram>(
            UdpDatagram(), validationError( "UDP receive size and timeout must be valid." ) );
    }

    std::vector<std::uint8_t> payload;
    try
    {
        payload.resize( maximum_size_in );
    }
    catch ( const std::bad_alloc& )
    {
        return TransferResult<UdpDatagram>(
            UdpDatagram(), TransportError( eTransportErrorCategory::InputOutput,
                                           eTransportErrorCode::ResourceExhausted,
                                           "Allocating the UDP receive buffer failed." ) );
    }

    const Clock::time_point deadline = operationDeadline( context_in );
    for ( ;; )
    {
        const TransportError wait_error = waitForSocket(
            this->m_impl->socket, eSocketWait::Read, deadline,
            context_in.cancellation() );
        if ( !wait_error.ok() )
        {
            return TransferResult<UdpDatagram>( UdpDatagram(), wait_error );
        }

        sockaddr_storage source_address = {};
#if defined( _WIN32 )
        int source_size = sizeof( source_address );
        const int received_size = recvfrom(
            this->m_impl->socket, reinterpret_cast<char*>( payload.data() ),
            static_cast<int>( maximum_size_in ), 0,
            reinterpret_cast<sockaddr*>( &source_address ), &source_size );
        if ( received_size == SOCKET_ERROR )
        {
            const int code = nativeSocketError();
            if ( pendingError( code ) || interruptedError( code ) )
            {
                continue;
            }
            if ( messageTooLargeError( code ) )
            {
                const Endpoint source = endpointFromAddress(
                    reinterpret_cast<const sockaddr*>( &source_address ), source_size );
                return TransferResult<UdpDatagram>(
                    UdpDatagram( source, std::move( payload ) ),
                    TransportError( eTransportErrorCategory::Protocol,
                                    eTransportErrorCode::DatagramTruncated,
                                    "The received UDP datagram exceeded the supplied buffer.",
                                    code ) );
            }
            return TransferResult<UdpDatagram>(
                UdpDatagram(), socketError( code, eTransportErrorCode::ReceiveFailed,
                                            "Receiving the UDP datagram failed." ) );
        }
        const bool truncated = false;
        const std::size_t stored_size = static_cast<std::size_t>( received_size );
#else
        iovec buffer = {};
        buffer.iov_base = payload.data();
        buffer.iov_len = maximum_size_in;
        msghdr message = {};
        message.msg_name = &source_address;
        message.msg_namelen = sizeof( source_address );
        message.msg_iov = &buffer;
        message.msg_iovlen = 1;
        const ssize_t received_size = recvmsg( this->m_impl->socket, &message, MSG_TRUNC );
        if ( received_size < 0 )
        {
            const int code = nativeSocketError();
            if ( pendingError( code ) || interruptedError( code ) )
            {
                continue;
            }
            return TransferResult<UdpDatagram>(
                UdpDatagram(), socketError( code, eTransportErrorCode::ReceiveFailed,
                                            "Receiving the UDP datagram failed." ) );
        }
        const int source_size = static_cast<int>( message.msg_namelen );
        const bool truncated = ( message.msg_flags & MSG_TRUNC ) != 0 ||
                               static_cast<std::size_t>( received_size ) > maximum_size_in;
        const std::size_t stored_size = ( std::min )(
            static_cast<std::size_t>( received_size ), maximum_size_in );
#endif

        payload.resize( stored_size );
        const Endpoint source = endpointFromAddress(
            reinterpret_cast<const sockaddr*>( &source_address ), source_size );
        if ( !source.isValid() )
        {
            return TransferResult<UdpDatagram>(
                UdpDatagram(), TransportError( eTransportErrorCategory::InputOutput,
                                               eTransportErrorCode::ReceiveFailed,
                                               "The UDP source endpoint could not be decoded." ) );
        }
        if ( truncated )
        {
            return TransferResult<UdpDatagram>(
                UdpDatagram( source, std::move( payload ) ),
                TransportError( eTransportErrorCategory::Protocol,
                                eTransportErrorCode::DatagramTruncated,
                                "The received UDP datagram exceeded the supplied buffer." ) );
        }
        return TransferResult<UdpDatagram>(
            UdpDatagram( source, std::move( payload ) ) );
    }
}

} // namespace xpt
} // namespace wse
