//*****************************************************************************************************************
//!
//! @file    xpt_http_contract.cpp
//! @brief   \~japanese Portable XPT HTTP／Retry／Timeout／Cancellation契約を検証する.
//! @brief   \~english  Verifies the portable XPT HTTP, retry, timeout, and cancellation contract.
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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

bool initializeTestSockets()
{
#if defined( _WIN32 )
    static std::once_flag flag;
    static int result = WSASYSNOTREADY;
    std::call_once( flag, []()
    {
        WSADATA data = {};
        result = WSAStartup( MAKEWORD( 2, 2 ), &data );
    } );
    return result == 0;
#else
    return true;
#endif
}

std::string toLowerAscii( const std::string& text_in )
{
    std::string result = text_in;
    std::transform(
          result.begin()
        , result.end()
        , result.begin()
        , []( const unsigned char character_in )
          {
              return static_cast<char>( std::tolower( character_in ) );
          }
    );
    return result;
}

std::size_t contentLength( const std::string& request_in )
{
    const std::string lower = toLowerAscii( request_in );
    const std::string key = "\r\ncontent-length:";
    const std::string::size_type position = lower.find( key );
    if ( position == std::string::npos )
    {
        return 0U;
    }
    const std::string::size_type value_start = position + key.size();
    const std::string::size_type value_end = lower.find( "\r\n", value_start );
    try
    {
        return static_cast<std::size_t>( std::stoull(
            lower.substr( value_start, value_end - value_start ) ) );
    }
    catch ( ... )
    {
        return 0U;
    }
}

std::string receiveRequest( const TestSocket connection_in )
{
    std::string request;
    char buffer[ 1024 ] = {};
    std::size_t expected_size = std::string::npos;
    for ( ;; )
    {
#if defined( _WIN32 )
        const int received = recv(
            connection_in, buffer, static_cast<int>( sizeof( buffer ) ), 0 );
#else
        const int received = static_cast<int>( recv(
            connection_in, buffer, sizeof( buffer ), 0 ) );
#endif
        if ( received <= 0 )
        {
            break;
        }
        request.append( buffer, static_cast<std::size_t>( received ) );
        const std::string::size_type header_end = request.find( "\r\n\r\n" );
        if ( header_end != std::string::npos && expected_size == std::string::npos )
        {
            expected_size = header_end + 4U + contentLength( request );
        }
        if ( expected_size != std::string::npos && request.size() >= expected_size )
        {
            break;
        }
    }
    return request;
}

void sendAll( const TestSocket connection_in, const std::string& response_in )
{
    std::size_t offset = 0U;
    while ( offset < response_in.size() )
    {
#if defined( _WIN32 )
        const int sent = send(
              connection_in
            , response_in.data() + offset
            , static_cast<int>( response_in.size() - offset )
            , 0
        );
#else
        const int sent = static_cast<int>( send(
              connection_in
            , response_in.data() + offset
            , response_in.size() - offset
            , MSG_NOSIGNAL
        ) );
#endif
        if ( sent <= 0 )
        {
            return;
        }
        offset += static_cast<std::size_t>( sent );
    }
}

std::string response(
      const std::uint16_t status_in
    , const std::string&  reason_in
    , const std::string&  body_in
    , const std::string&  extra_headers_in = std::string()
)
{
    std::ostringstream stream;
    stream << "HTTP/1.1 " << status_in << ' ' << reason_in << "\r\n"
           << "Content-Length: " << body_in.size() << "\r\n"
           << extra_headers_in
           << "Connection: close\r\n\r\n"
           << body_in;
    return stream.str();
}

class HttpServer final
{
  public:
    using Handler = std::function<std::string( const std::string&, std::uint32_t )>;

  private:
    TestSocket m_listener;
    std::uint16_t m_port;
    std::uint32_t m_expected_requests;
    Handler m_handler;
    std::atomic<std::uint32_t> m_request_count;
    std::thread m_worker;

    void run()
    {
        for ( std::uint32_t index = 1U; index <= this->m_expected_requests; ++index )
        {
            fd_set read_set;
            FD_ZERO( &read_set );
            FD_SET( this->m_listener, &read_set );
            timeval timeout = {};
            timeout.tv_sec = 3;
#if defined( _WIN32 )
            const int selected = select( 0, &read_set, nullptr, nullptr, &timeout );
#else
            const int selected = select(
                this->m_listener + 1, &read_set, nullptr, nullptr, &timeout );
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
            const std::string request = receiveRequest( connection );
            this->m_request_count.store( index );
            sendAll( connection, this->m_handler( request, index ) );
            closeTestSocket( connection );
        }
    }

  public:
    HttpServer( const std::uint32_t expected_requests_in, const Handler& handler_in )
        : m_listener          ( INVALID_TEST_SOCKET )
        , m_port              ( 0U )
        , m_expected_requests ( expected_requests_in )
        , m_handler           ( handler_in )
        , m_request_count     ( 0U )
        , m_worker            ()
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
        address.sin_port = 0U;
#if defined( _WIN32 )
        const int bound = bind(
              this->m_listener
            , reinterpret_cast<const sockaddr*>( &address )
            , static_cast<int>( sizeof( address ) )
        );
        int address_size = sizeof( address );
#else
        const int bound = bind(
              this->m_listener
            , reinterpret_cast<const sockaddr*>( &address )
            , sizeof( address )
        );
        socklen_t address_size = sizeof( address );
#endif
        if ( bound != 0 || getsockname(
                  this->m_listener
                , reinterpret_cast<sockaddr*>( &address )
                , &address_size ) != 0
             || listen( this->m_listener, 4 ) != 0 )
        {
            closeTestSocket( this->m_listener );
            this->m_listener = INVALID_TEST_SOCKET;
            return;
        }

        this->m_port = ntohs( address.sin_port );
        this->m_worker = std::thread( [this]() { this->run(); } );
    }

    ~HttpServer()
    {
        if ( this->m_worker.joinable() )
        {
            this->m_worker.join();
        }
        closeTestSocket( this->m_listener );
    }

    bool ready() const noexcept
    {
        return this->m_listener != INVALID_TEST_SOCKET && this->m_port != 0U;
    }

    std::string url( const std::string& path_in ) const
    {
        return "http://127.0.0.1:" + std::to_string( this->m_port ) + path_in;
    }

    std::uint32_t requestCount() const noexcept
    {
        return this->m_request_count.load();
    }
};

std::string bodyText( const wse::xpt::HttpResponse& response_in )
{
    return std::string( response_in.body().begin(), response_in.body().end() );
}

bool hasHeader(
      const wse::xpt::HttpResponse& response_in
    , const std::string&            name_in
    , const std::string&            value_in
)
{
    const std::string lower_name = toLowerAscii( name_in );
    return std::any_of(
          response_in.headers().begin()
        , response_in.headers().end()
        , [&]( const wse::xpt::sHttpHeader& header_in )
          {
              return toLowerAscii( header_in.name ) == lower_name && header_in.value == value_in;
          }
    );
}

} // namespace

int main()
{
    using namespace std::chrono_literals;
    const wse::xpt::OperationContext context( wse::xpt::Timeout::milliseconds( 1000 ) );
    const wse::xpt::HttpExecutionOptions defaults;
    const wse::xpt::HttpClient client;

    {
        const auto invalid_url = client.execute(
            wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, "ftp://127.0.0.1/" ),
            defaults,
            context );
        expect( !invalid_url.succeeded()
                    && invalid_url.error().code() == wse::xpt::eTransportErrorCode::InvalidArgument,
                "Non-HTTP URL is rejected before backend access" );

        wse::xpt::HttpRequest invalid_header(
            wse::xpt::eHttpMethod::Get, "http://127.0.0.1/" );
        invalid_header.addHeader( "X-WSE", "unsafe\r\nInjected: value" );
        const auto header_result = client.execute( invalid_header, defaults, context );
        expect( !header_result.succeeded()
                    && header_result.error().category()
                        == wse::xpt::eTransportErrorCategory::Validation,
                "Header injection is rejected" );

        wse::xpt::HttpRequest invalid_get_body(
            wse::xpt::eHttpMethod::Get, "http://127.0.0.1/" );
        invalid_get_body.setBody( { 'x' } );
        expect( !client.execute( invalid_get_body, defaults, context ).succeeded(),
                "GET request body is rejected" );

        const wse::xpt::HttpExecutionOptions invalid_options(
              0U
            , wse::xpt::RetryPolicy()
            , wse::xpt::eRetryOperationSafety::NonIdempotent
            , false
        );
        expect( !client.execute(
                    wse::xpt::HttpRequest(
                        wse::xpt::eHttpMethod::Get, "http://127.0.0.1/" ),
                    invalid_options,
                    context ).succeeded(),
                "Zero response limit is rejected" );

        const wse::xpt::HttpAuthentication invalid_authentication(
            wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated,
            std::string(), "not-a-deployment-secret" );
        expect( !client.executeAuthenticated(
                    wse::xpt::HttpRequest(
                        wse::xpt::eHttpMethod::Get, "http://127.0.0.1/" ),
                    defaults,
                    invalid_authentication,
                    context ).succeeded(),
                "Incomplete runtime HTTP authentication is rejected" );
    }

    {
        HttpServer server( 2U, []( const std::string& request_in, const std::uint32_t attempt_in )
        {
            if ( attempt_in == 1U )
            {
                expect( request_in.find( "Authorization:" ) == std::string::npos,
                        "Server-negotiated authentication does not send a secret preemptively" );
                return response(
                    401U, "Unauthorized", "challenge",
                    "WWW-Authenticate: Basic realm=\"loopback\"\r\n" );
            }
            expect( request_in.find( "Authorization: Basic dTpw" ) != std::string::npos,
                    "The backend answers the server-advertised authentication challenge" );
            return response( 200U, "OK", "authenticated" );
        } );
        expect( server.ready(), "Authenticated fake HTTP server starts" );
        if ( server.ready() )
        {
            const wse::xpt::HttpAuthentication authentication(
                wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated, "u", "p" );
            const auto result = client.executeAuthenticated(
                wse::xpt::HttpRequest(
                    wse::xpt::eHttpMethod::Get, server.url( "/authenticated" ) ),
                defaults,
                authentication,
                context );
            expect( result.succeeded() && result.value().status_code() == 200U &&
                        bodyText( result.value() ) == "authenticated" &&
                        server.requestCount() == 2U,
                    "Explicit server-negotiated authentication completes one HTTP operation" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string& request_in, const std::uint32_t )
        {
            expect( request_in.rfind( "GET /ok HTTP/", 0U ) == 0U,
                    "GET request line reaches the local server" );
            return response( 200U, "OK", "WSE", "X-WSE: portable\r\n" );
        } );
        expect( server.ready(), "GET fake HTTP server starts" );
        if ( server.ready() )
        {
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/ok" ) ),
                defaults,
                context );
            expect( result.succeeded() && result.value().status_code() == 200U,
                    "GET returns the HTTP status" );
            expect( bodyText( result.value() ) == "WSE"
                        && hasHeader( result.value(), "x-wse", "portable" ),
                    "GET preserves response headers and bytes" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string& request_in, const std::uint32_t )
        {
            expect( request_in.rfind( "POST /echo HTTP/", 0U ) == 0U
                        && request_in.size() >= 4U
                        && request_in.substr( request_in.size() - 4U ) == "data",
                    "POST method and body reach the local server" );
            return response( 201U, "Created", "stored" );
        } );
        expect( server.ready(), "POST fake HTTP server starts" );
        if ( server.ready() )
        {
            wse::xpt::HttpRequest request(
                wse::xpt::eHttpMethod::Post, server.url( "/echo" ) );
            request.setBody( { 'd', 'a', 't', 'a' } );
            const auto result = client.execute( request, defaults, context );
            expect( result.succeeded() && result.value().status_code() == 201U
                        && bodyText( result.value() ) == "stored",
                    "POST response is returned" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string&, const std::uint32_t )
        {
            return response( 404U, "Not Found", "missing" );
        } );
        expect( server.ready(), "HTTP error fake server starts" );
        if ( server.ready() )
        {
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/missing" ) ),
                defaults,
                context );
            expect( !result.succeeded()
                        && result.error().category() == wse::xpt::eTransportErrorCategory::Http
                        && result.error().code() == wse::xpt::eTransportErrorCode::HttpStatusError,
                    "HTTP 4xx is a portable HTTP error" );
            expect( result.value().status_code() == 404U
                        && bodyText( result.value() ) == "missing",
                    "HTTP error preserves the response" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string&, const std::uint32_t )
        {
            return response( 200U, "OK", "too-large" );
        } );
        expect( server.ready(), "Body limit fake server starts" );
        if ( server.ready() )
        {
            const wse::xpt::HttpExecutionOptions limited(
                  3U
                , wse::xpt::RetryPolicy()
                , wse::xpt::eRetryOperationSafety::NonIdempotent
                , false
            );
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/large" ) ),
                limited,
                context );
            expect( !result.succeeded()
                        && result.error().code()
                            == wse::xpt::eTransportErrorCode::ResponseTooLarge,
                    "Response body limit aborts the transfer with a stable error" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string&, const std::uint32_t )
        {
            std::this_thread::sleep_for( 250ms );
            return response( 200U, "OK", "late" );
        } );
        expect( server.ready(), "Timeout fake server starts" );
        if ( server.ready() )
        {
            const auto started = std::chrono::steady_clock::now();
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/slow" ) ),
                defaults,
                wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 50 ) ) );
            const auto elapsed = std::chrono::steady_clock::now() - started;
            expect( !result.succeeded()
                        && result.error().code() == wse::xpt::eTransportErrorCode::TimedOut,
                    "HTTP deadline returns a stable timeout" );
            expect( elapsed < 500ms, "HTTP timeout is bounded by a monotonic deadline" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string&, const std::uint32_t )
        {
            std::this_thread::sleep_for( 250ms );
            return response( 200U, "OK", "late" );
        } );
        expect( server.ready(), "Cancellation fake server starts" );
        if ( server.ready() )
        {
            wse::xpt::CancellationSource source;
            std::thread canceller( [&source]()
            {
                std::this_thread::sleep_for( 40ms );
                source.cancel();
            } );
            const auto started = std::chrono::steady_clock::now();
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/cancel" ) ),
                defaults,
                wse::xpt::OperationContext(
                    wse::xpt::Timeout::milliseconds( 1000 ), source.token() ) );
            const auto elapsed = std::chrono::steady_clock::now() - started;
            canceller.join();
            expect( !result.succeeded()
                        && result.error().code() == wse::xpt::eTransportErrorCode::Cancelled,
                    "Cross-thread cancellation interrupts HTTP" );
            expect( elapsed < 500ms, "HTTP cancellation latency is bounded" );
        }
    }

    {
        HttpServer server( 3U, []( const std::string&, const std::uint32_t attempt_in )
        {
            return attempt_in < 3U
                ? response( 503U, "Service Unavailable", "retry" )
                : response( 200U, "OK", "ready" );
        } );
        expect( server.ready(), "Retry fake server starts" );
        if ( server.ready() )
        {
            const wse::xpt::HttpExecutionOptions retry_options(
                  1024U
                , wse::xpt::RetryPolicy::createFixed( 3U, 10ms )
                , wse::xpt::eRetryOperationSafety::Idempotent
                , true
            );
            const auto result = client.execute(
                wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, server.url( "/retry" ) ),
                retry_options,
                context );
            expect( result.succeeded() && result.value().attempt_count() == 3U
                        && server.requestCount() == 3U,
                    "Explicit idempotent retry reaches the configured attempt limit" );
        }
    }

    {
        HttpServer server( 1U, []( const std::string&, const std::uint32_t )
        {
            return response( 503U, "Service Unavailable", "do-not-repeat" );
        } );
        expect( server.ready(), "Non-idempotent fake server starts" );
        if ( server.ready() )
        {
            const wse::xpt::HttpExecutionOptions retry_options(
                  1024U
                , wse::xpt::RetryPolicy::createFixed( 3U, 10ms )
                , wse::xpt::eRetryOperationSafety::NonIdempotent
                , true
            );
            wse::xpt::HttpRequest request(
                wse::xpt::eHttpMethod::Post, server.url( "/side-effect" ) );
            request.setBody( { 'x' } );
            const auto result = client.execute( request, retry_options, context );
            expect( !result.succeeded() && result.value().attempt_count() == 1U
                        && server.requestCount() == 1U,
                    "Non-idempotent HTTP request is never retried implicitly" );
        }
    }

    return failures == 0 ? 0 : 1;
}
