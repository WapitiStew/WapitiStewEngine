//*****************************************************************************************************************
//!
//! @file    xpt_tcp_loopback.cpp
//! @brief   \~japanese Portable XPT TCP／Timeout／Cancellation／Error契約を検証する.
//! @brief   \~english  Verifies the portable XPT TCP, timeout, cancellation, and error contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <xpt/stew.h>

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
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

#if defined( _WIN32 )
using TestSocket = SOCKET;
constexpr TestSocket INVALID_TEST_SOCKET = INVALID_SOCKET;
#else
using TestSocket = int;
constexpr TestSocket INVALID_TEST_SOCKET = -1;
#endif

int failures = 0;

void expect( const bool condition_in, const char* const message_in )
{
    if ( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}

void closeTestSocket( const TestSocket socket_in ) noexcept
{
    if ( socket_in == INVALID_TEST_SOCKET )
    {
        return;
    }
#if defined( _WIN32 )
    closesocket( socket_in );
#else
    close( socket_in );
#endif
}

void shutdownTestSocketSend( const TestSocket socket_in ) noexcept
{
    if ( socket_in == INVALID_TEST_SOCKET )
    {
        return;
    }
#if defined( _WIN32 )
    shutdown( socket_in, SD_SEND );
#else
    shutdown( socket_in, SHUT_WR );
#endif
}

void requestAbortiveClose( const TestSocket socket_in ) noexcept
{
    linger option = {};
    option.l_onoff = 1;
    option.l_linger = 0;
#if defined( _WIN32 )
    setsockopt( socket_in, SOL_SOCKET, SO_LINGER,
                reinterpret_cast<const char *>( &option ), sizeof( option ) );
#else
    setsockopt( socket_in, SOL_SOCKET, SO_LINGER, &option, sizeof( option ) );
#endif
}

bool waitForFlag( const std::atomic_bool& flag_in )
{
    for ( std::size_t attempt = 0U; attempt < 200U; ++attempt )
    {
        if ( flag_in.load() )
        {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
    }
    return flag_in.load();
}

bool initializeTestSockets()
{
#if defined( _WIN32 )
    static std::once_flag flag;
    static int result = WSASYSNOTREADY;
    std::call_once( flag, []() {
        WSADATA data = {};
        result = WSAStartup( MAKEWORD( 2, 2 ), &data );
    } );
    return result == 0;
#else
    return true;
#endif
}

class LoopbackServer final
{
  public:
    using Handler = std::function<void( TestSocket )>;

  private:
    TestSocket m_listener;
    std::uint16_t m_port;
    std::thread m_worker;

  public:
    explicit LoopbackServer( const Handler& handler_in )
        : m_listener ( INVALID_TEST_SOCKET )
        , m_port     ( 0 )
        , m_worker   ()
    {
        if ( !initializeTestSockets() )
        {
            return;
        }

        this->m_listener = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
        if ( this->m_listener == INVALID_TEST_SOCKET )
        {
            return;
        }

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
        address.sin_port = 0;
#if defined( _WIN32 )
        if ( bind( this->m_listener, reinterpret_cast<const sockaddr *>( &address ),
                   static_cast<int>( sizeof( address ) ) ) == SOCKET_ERROR )
#else
        if ( bind( this->m_listener, reinterpret_cast<const sockaddr *>( &address ),
                   sizeof( address ) ) != 0 )
#endif
        {
            closeTestSocket( this->m_listener );
            this->m_listener = INVALID_TEST_SOCKET;
            return;
        }

#if defined( _WIN32 )
        int address_size = sizeof( address );
#else
        socklen_t address_size = sizeof( address );
#endif
        if ( getsockname( this->m_listener, reinterpret_cast<sockaddr *>( &address ),
                          &address_size ) != 0 || listen( this->m_listener, 1 ) != 0 )
        {
            closeTestSocket( this->m_listener );
            this->m_listener = INVALID_TEST_SOCKET;
            return;
        }
        this->m_port = ntohs( address.sin_port );

        this->m_worker = std::thread( [this, handler_in]() {
            fd_set read_set;
            FD_ZERO( &read_set );
            FD_SET( this->m_listener, &read_set );
            timeval timeout = {};
            timeout.tv_sec = 2;
#if defined( _WIN32 )
            const int selected = select( 0, &read_set, nullptr, nullptr, &timeout );
#else
            const int selected = select( this->m_listener + 1, &read_set, nullptr, nullptr, &timeout );
#endif
            if ( selected <= 0 )
            {
                return;
            }

            const TestSocket connection = accept( this->m_listener, nullptr, nullptr );
            if ( connection == INVALID_TEST_SOCKET )
            {
                return;
            }
            handler_in( connection );
            closeTestSocket( connection );
        } );
    }

    ~LoopbackServer()
    {
        if ( this->m_worker.joinable() )
        {
            this->m_worker.join();
        }
        closeTestSocket( this->m_listener );
    }

    bool ready() const noexcept
    {
        return this->m_listener != INVALID_TEST_SOCKET && this->m_port != 0;
    }

    std::uint16_t port() const noexcept
    {
        return this->m_port;
    }
};

void echoOnce( const TestSocket connection_in )
{
    std::uint8_t buffer[ 256 ] = {};
#if defined( _WIN32 )
    const int size = recv( connection_in, reinterpret_cast<char *>( buffer ),
                           static_cast<int>( sizeof( buffer ) ), 0 );
    if ( size > 0 )
    {
        send( connection_in, reinterpret_cast<const char *>( buffer ), size, 0 );
    }
#else
    const int size = static_cast<int>( recv( connection_in, buffer, sizeof( buffer ), 0 ) );
    if ( size > 0 )
    {
        send( connection_in, buffer, static_cast<std::size_t>( size ), MSG_NOSIGNAL );
    }
#endif
}

void holdConnection( const TestSocket socket_in )
{
    (void)socket_in;
    std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
}

} // namespace

int main()
{
    static_assert( !std::is_copy_constructible<wse::xpt::TcpClient>::value,
                   "TcpClient is not copyable" );
    static_assert( std::is_move_constructible<wse::xpt::TcpClient>::value,
                   "TcpClient is movable" );

    const wse::xpt::Endpoint invalid_endpoint;
    const wse::xpt::Endpoint loopback( "127.0.0.1", 1234 );
    expect( !invalid_endpoint.isValid(), "Default endpoint is invalid" );
    expect( loopback.isValid(), "Host and non-zero port form a valid endpoint" );
    expect( loopback.host() == "127.0.0.1" && loopback.port() == 1234,
            "Endpoint preserves host and port" );

    expect( !wse::xpt::Timeout::milliseconds( -1 ).isValid(),
            "Negative timeout is invalid" );
    expect( wse::xpt::Timeout::milliseconds( 0 ).isValid(),
            "Zero timeout is an explicit immediate deadline" );

    wse::xpt::CancellationSource cancellation_source;
    const wse::xpt::CancellationToken copied_token = cancellation_source.token();
    expect( !copied_token.isCancellationRequested(), "New token is not cancelled" );
    cancellation_source.cancel();
    expect( copied_token.isCancellationRequested(), "Cancellation is shared across token copies" );

    wse::xpt::TcpClient validation_client;
    const wse::xpt::OperationContext short_context( wse::xpt::Timeout::milliseconds( 100 ) );
    const auto invalid_result = validation_client.connect( invalid_endpoint, short_context );
    expect( !invalid_result.succeeded() &&
                invalid_result.error().category() == wse::xpt::eTransportErrorCategory::Validation,
            "Invalid endpoint returns a structured validation error" );
    const auto invalid_timeout_result = validation_client.connect(
        loopback, wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( -1 ) ) );
    expect( !invalid_timeout_result.succeeded() &&
                invalid_timeout_result.error().category() ==
                    wse::xpt::eTransportErrorCategory::Validation,
            "Negative timeout returns a structured validation error" );

    const wse::xpt::OperationContext cancelled_context(
        wse::xpt::Timeout::milliseconds( 100 ), cancellation_source.token() );
    const auto cancelled_connect = validation_client.connect(
        wse::xpt::Endpoint( "127.0.0.1", 9 ), cancelled_context );
    expect( !cancelled_connect.succeeded() &&
                cancelled_connect.error().code() == wse::xpt::eTransportErrorCode::Cancelled,
            "Pre-cancelled connect does not access the network" );
    const auto disconnected_peer = validation_client.checkPeerConnection();
    expect( !disconnected_peer.succeeded() &&
                disconnected_peer.error().code() ==
                    wse::xpt::eTransportErrorCode::NotConnected,
            "Peer check reports a structured disconnected state" );

    {
        LoopbackServer server( echoOnce );
        expect( server.ready(), "Echo loopback server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 1000 ) );
            const auto connected = client.connect(
                wse::xpt::Endpoint( "127.0.0.1", server.port() ), context );
            expect( connected.succeeded() && client.isConnected(), "TCP loopback connects" );

            const std::vector<std::uint8_t> message { 'W', 'S', 'E' };
            const auto sent = client.send( message, context );
            expect( sent.succeeded() && sent.value() == message.size(),
                    "TCP send reports the complete byte count" );
            const auto received = client.receive( 64, context );
            expect( received.succeeded() && received.value() == message,
                    "TCP loopback receives the echoed payload" );
            expect( client.getRemoteEndpoint().port() == server.port(),
                    "Connected client exposes its portable remote endpoint" );
            expect( client.getLocalEndpoint().isValid() &&
                        client.getLocalEndpoint().host() == "127.0.0.1",
                    "Connected client exposes its OS-selected local endpoint" );
            client.disconnect();
            expect( !client.isConnected(), "Disconnect is observable and idempotent" );
            expect( !client.getLocalEndpoint().isValid(),
                    "Disconnect clears the local endpoint" );
            client.disconnect();
        }
    }

    {
        std::atomic_bool payload_sent( false );
        const std::vector<std::uint8_t> payload{ 'P', 'E', 'E', 'K' };
        LoopbackServer server( [&payload_sent, &payload]( const TestSocket connection_in ) {
#if defined( _WIN32 )
            const int sent = send( connection_in,
                                   reinterpret_cast<const char *>( payload.data() ),
                                   static_cast<int>( payload.size() ), 0 );
#else
            const int sent = static_cast<int>( send(
                connection_in, payload.data(), payload.size(), MSG_NOSIGNAL ) );
#endif
            payload_sent.store( sent == static_cast<int>( payload.size() ) );
            std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
        } );
        expect( server.ready(), "Peer-check payload server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto context = wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 1000 ) );
            expect( client.connect( wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                                    context ).succeeded(),
                    "Peer-check payload fixture connects" );
            expect( waitForFlag( payload_sent ), "Peer-check payload is available" );
            expect( client.checkPeerConnection().succeeded(),
                    "Peer check preserves a connected socket with pending data" );
            const auto received = client.receive( payload.size(), context );
            expect( received.succeeded() && received.value() == payload,
                    "Peer check does not consume pending TCP data" );
        }
    }

    {
        std::atomic_bool peer_shutdown( false );
        LoopbackServer server( [&peer_shutdown]( const TestSocket connection_in ) {
            shutdownTestSocketSend( connection_in );
            peer_shutdown.store( true );
        } );
        expect( server.ready(), "Peer-check FIN server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto context = wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 1000 ) );
            expect( client.connect( wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                                    context ).succeeded(),
                    "Peer-check FIN fixture connects" );
            expect( waitForFlag( peer_shutdown ), "Peer-check FIN is issued" );
            wse::xpt::TransportStatus checked = wse::xpt::TransportStatus::success();
            for ( std::size_t attempt = 0U; attempt < 200U && checked.succeeded(); ++attempt )
            {
                checked = client.checkPeerConnection();
                if ( checked.succeeded() )
                {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                }
            }
            expect( !checked.succeeded() &&
                        checked.error().code() == wse::xpt::eTransportErrorCode::RemoteClosed,
                    "Peer check reports an orderly FIN without a receive" );
            expect( !client.isConnected(), "Peer-check FIN clears local connection state" );
        }
    }

    {
        std::atomic_bool reset_requested( false );
        LoopbackServer server( [&reset_requested]( const TestSocket connection_in ) {
            requestAbortiveClose( connection_in );
            reset_requested.store( true );
        } );
        expect( server.ready(), "Peer-check reset server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto context = wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 1000 ) );
            expect( client.connect( wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                                    context ).succeeded(),
                    "Peer-check reset fixture connects" );
            expect( waitForFlag( reset_requested ), "Peer-check reset is requested" );
            wse::xpt::TransportStatus checked = wse::xpt::TransportStatus::success();
            for ( std::size_t attempt = 0U; attempt < 200U && checked.succeeded(); ++attempt )
            {
                checked = client.checkPeerConnection();
                if ( checked.succeeded() )
                {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                }
            }
            expect( !checked.succeeded() &&
                        checked.error().category() ==
                            wse::xpt::eTransportErrorCategory::Connection &&
                        ( checked.error().code() ==
                              wse::xpt::eTransportErrorCode::ConnectionReset ||
                          checked.error().code() ==
                              wse::xpt::eTransportErrorCode::RemoteClosed ),
                    "Peer check reports an abortive peer close as a connection failure" );
            expect( !client.isConnected(), "Peer-check reset clears local connection state" );
        }
    }

    {
        LoopbackServer server( holdConnection );
        expect( server.ready(), "Timeout loopback server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto connected = client.connect(
                wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ) ) );
            expect( connected.succeeded(), "Timeout fixture connects" );
            const auto started = std::chrono::steady_clock::now();
            const auto received = client.receive(
                16, wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 50 ) ) );
            const auto elapsed = std::chrono::steady_clock::now() - started;
            expect( !received.succeeded() &&
                        received.error().code() == wse::xpt::eTransportErrorCode::TimedOut,
                    "Receive timeout has a distinguishable error" );
            expect( elapsed < std::chrono::milliseconds( 500 ),
                    "Receive timeout is bounded by a monotonic deadline" );
            expect( client.isConnected(), "Timeout does not silently discard the connection" );
        }
    }

    {
        LoopbackServer server( holdConnection );
        expect( server.ready(), "Cancellation loopback server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto connected = client.connect(
                wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ) ) );
            expect( connected.succeeded(), "Cancellation fixture connects" );

            wse::xpt::CancellationSource source;
            std::thread canceller( [&source]() {
                std::this_thread::sleep_for( std::chrono::milliseconds( 40 ) );
                source.cancel();
            } );
            const auto received = client.receive(
                16, wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ),
                                                source.token() ) );
            canceller.join();
            expect( !received.succeeded() &&
                        received.error().code() == wse::xpt::eTransportErrorCode::Cancelled,
                    "Cross-thread cancellation interrupts a blocking receive" );
        }
    }

    {
        LoopbackServer server( []( TestSocket ) {} );
        expect( server.ready(), "Disconnect loopback server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const auto context = wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 1000 ) );
            expect( client.connect( wse::xpt::Endpoint( "127.0.0.1", server.port() ),
                                    context ).succeeded(),
                    "Disconnect fixture connects" );
            const auto received = client.receive( 16, context );
            expect( !received.succeeded() &&
                        received.error().code() == wse::xpt::eTransportErrorCode::RemoteClosed,
                    "Peer shutdown returns a structured remote-closed error" );
            expect( !client.isConnected(), "Peer shutdown closes the local connection state" );
        }
    }

    // XPT-TCP-02: rejected arguments preserve an existing connection; a validated
    // replacement attempt closes it before resolution/connection and has no rollback.
    {
        LoopbackServer server( holdConnection );
        expect( server.ready(), "Reconnect lifecycle server starts" );
        if ( server.ready() )
        {
            wse::xpt::TcpClient client;
            const wse::xpt::Endpoint endpoint( "127.0.0.1", server.port() );
            const wse::xpt::OperationContext context(
                wse::xpt::Timeout::milliseconds( 1000 ) );
            expect( client.connect( endpoint, context ).succeeded(), "Reconnect fixture connects" );
            const auto invalid = client.connect( wse::xpt::Endpoint( "127.0.0.1", 0 ), context );
            expect( !invalid.succeeded() && client.isConnected(),
                    "Invalid reconnect preserves the existing connection" );
            wse::xpt::CancellationSource cancelled;
            cancelled.cancel();
            const auto rejected = client.connect( endpoint, wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 1000 ), cancelled.token() ) );
            expect( !rejected.succeeded() &&
                        rejected.error().code() == wse::xpt::eTransportErrorCode::Cancelled &&
                        client.isConnected(), "Pre-cancelled reconnect preserves the connection" );
            const auto expired = client.connect( endpoint, wse::xpt::OperationContext(
                wse::xpt::Timeout::milliseconds( 0 ) ) );
            expect( !expired.succeeded() &&
                        expired.error().code() == wse::xpt::eTransportErrorCode::TimedOut &&
                        !client.isConnected(), "Validated reconnect failure leaves no old connection" );
        }
    }

    return failures == 0 ? 0 : 1;
}
