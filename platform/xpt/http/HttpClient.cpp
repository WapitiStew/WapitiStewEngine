//*****************************************************************************************************************
//!
//! @file    HttpClient.cpp
//! @brief   \~japanese libcurl Backendによる同期HTTP Clientを実装する.
//! @brief   \~english  Implements the synchronous HTTP client with a libcurl backend.
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

#include "xpt/http/HttpClient.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <climits>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace wse
{
namespace xpt
{
namespace
{

using Clock = std::chrono::steady_clock;

const std::size_t DEFAULT_MAXIMUM_RESPONSE_BODY_SIZE = 8U * 1024U * 1024U; //!< 8 MiB.
const std::chrono::milliseconds CANCELLATION_POLL_INTERVAL( 20 );           //!< Poll interval.

void eraseString( std::string* const p_value_inout ) noexcept
{
    std::string& value_inout = *p_value_inout;

    volatile char* data = value_inout.empty() ? nullptr : &value_inout[ 0 ];
    for ( std::size_t index = 0U; index < value_inout.size(); ++index )
    {
        data[ index ] = 0;
    }
    value_inout.clear();
}

bool validAuthenticationText( const std::string& value_in )
{
    return !value_in.empty() && std::none_of(
        value_in.begin(), value_in.end(), []( const unsigned char value_in )
        {
            return value_in == 0U || value_in == '\r' || value_in == '\n';
        } );
}

//! \~japanese libcurl Global lifecycle Owner. \~english Owns the libcurl global lifecycle.
class CurlRuntime final
{
  private:
    CURLcode m_initialization_result;

  public:
    CurlRuntime() noexcept
        : m_initialization_result ( curl_global_init( CURL_GLOBAL_DEFAULT ) )
    {
    }

    ~CurlRuntime()
    {
        if ( this->m_initialization_result == CURLE_OK )
        {
            curl_global_cleanup();
        }
    }

    CurlRuntime( const CurlRuntime& ) = delete;
    CurlRuntime& operator=( const CurlRuntime& ) = delete;

    bool isReady() const noexcept
    {
        return this->m_initialization_result == CURLE_OK;
    }

    CURLcode result() const noexcept
    {
        return this->m_initialization_result;
    }
};

struct sResponseAccumulator final
{
    std::vector<sHttpHeader> headers;
    std::vector<std::uint8_t> body;
    std::size_t maximum_body_size;
    bool body_limit_exceeded;
    bool allocation_failed;
};

//! @return Process-wide libcurl lifecycle Owner.
CurlRuntime& getCurlRuntime() noexcept
{
    static CurlRuntime runtime;
    return runtime;
}

//! @param [in] text_in Text.
//! @return ASCII lower-case copy.
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

//! @param [in] text_in Header fragment.
//! @return Leading／Trailing SP／HTABを除いたText.
std::string trimHttpWhitespace( const std::string& text_in )
{
    const std::string::size_type first = text_in.find_first_not_of( " \t" );
    if ( first == std::string::npos )
    {
        return std::string();
    }
    const std::string::size_type last = text_in.find_last_not_of( " \t" );
    return text_in.substr( first, last - first + 1U );
}

//! @param [in] url_in Absolute URL.
//! @return URLがHTTP／HTTPS Absolute URLとして安全な場合true.
bool isValidUrl( const std::string& url_in )
{
    const std::string lower = toLowerAscii( url_in );
    if ( lower.rfind( "http://", 0U ) != 0U && lower.rfind( "https://", 0U ) != 0U )
    {
        return false;
    }
    if ( lower.size() <= ( lower.rfind( "https://", 0U ) == 0U ? 8U : 7U ) )
    {
        return false;
    }
    return std::none_of(
          url_in.begin()
        , url_in.end()
        , []( const unsigned char character_in )
          {
              return character_in <= 0x20U || character_in == 0x7FU;
          }
    );
}

//! @param [in] method_in Method値.
//! @return 対応Methodの場合true.
bool isValidMethod( const eHttpMethod method_in ) noexcept
{
    return method_in >= eHttpMethod::Get && method_in <= eHttpMethod::Delete;
}

//! @param [in] header_in Header.
//! @return Header injectionを含まない場合true.
bool isValidHeader( const sHttpHeader& header_in )
{
    if ( header_in.name.empty() || header_in.name.find( ':' ) != std::string::npos )
    {
        return false;
    }
    const auto contains_control = []( const std::string& text_in )
    {
        return text_in.find( '\r' ) != std::string::npos
            || text_in.find( '\n' ) != std::string::npos;
    };
    return !contains_control( header_in.name ) && !contains_control( header_in.value );
}

//! @param [in] context_in Operation control.
//! @return Saturating absolute Deadline.
Clock::time_point calculateDeadline( const OperationContext& context_in ) noexcept
{
    const Clock::time_point now = Clock::now();
    const Clock::duration duration = context_in.timeout().duration();
    if ( duration >= ( Clock::time_point::max )() - now )
    {
        return ( Clock::time_point::max )();
    }
    return now + duration;
}

//! @param [in] deadline_in Absolute Deadline.
//! @return libcurlへ渡す残り時間 [ms]. 期限切れ時0.
long remainingMilliseconds( const Clock::time_point deadline_in ) noexcept
{
    const Clock::time_point now = Clock::now();
    if ( now >= deadline_in )
    {
        return 0L;
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>( deadline_in - now );
    const auto rounded = remaining.count() <= 0 ? 1 : remaining.count();
    return static_cast<long>( ( std::min )(
          static_cast<std::int64_t>( LONG_MAX )
        , static_cast<std::int64_t>( rounded ) ) );
}

TransportError validationError( const std::string& message_in )
{
    return TransportError(
          eTransportErrorCategory::Validation
        , eTransportErrorCode::InvalidArgument
        , message_in
    );
}

TransportError timeoutError()
{
    return TransportError(
          eTransportErrorCategory::Timeout
        , eTransportErrorCode::TimedOut
        , "The HTTP operation exceeded its deadline."
    );
}

TransportError cancellationError()
{
    return TransportError(
          eTransportErrorCategory::Cancellation
        , eTransportErrorCode::Cancelled
        , "The HTTP operation was cancelled."
    );
}

//! @param [in] result_in libcurl Error.
//! @return Portable Structured Error.
TransportError convertCurlError( const CURLcode result_in )
{
    const std::string message = curl_easy_strerror( result_in );
    switch ( result_in )
    {
    case CURLE_URL_MALFORMAT:
        return validationError( message );
    case CURLE_UNSUPPORTED_PROTOCOL:
        return TransportError(
              eTransportErrorCategory::Protocol
            , eTransportErrorCode::Unsupported
            , message
            , result_in
        );
    case CURLE_COULDNT_RESOLVE_HOST:
        return TransportError(
              eTransportErrorCategory::Resolution
            , eTransportErrorCode::HostNotFound
            , message
            , result_in
        );
    case CURLE_COULDNT_CONNECT:
        return TransportError(
              eTransportErrorCategory::Connection
            , eTransportErrorCode::ConnectionRefused
            , message
            , result_in
        );
    case CURLE_SEND_ERROR:
        return TransportError(
              eTransportErrorCategory::InputOutput
            , eTransportErrorCode::SendFailed
            , message
            , result_in
        );
    case CURLE_RECV_ERROR:
    case CURLE_PARTIAL_FILE:
        return TransportError(
              eTransportErrorCategory::InputOutput
            , eTransportErrorCode::ReceiveFailed
            , message
            , result_in
        );
    case CURLE_OPERATION_TIMEDOUT:
        return timeoutError();
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CACERT_BADFILE:
        return TransportError(
              eTransportErrorCategory::Security
            , eTransportErrorCode::SecurityFailed
            , message
            , result_in
        );
    case CURLE_OUT_OF_MEMORY:
        return TransportError(
              eTransportErrorCategory::InputOutput
            , eTransportErrorCode::ResourceExhausted
            , message
            , result_in
        );
    default:
        return TransportError(
              eTransportErrorCategory::InputOutput
            , eTransportErrorCode::Unknown
            , message
            , result_in
        );
    }
}

//! @param [in] content_in Received bytes.
//! @param [in] size_in Element size.
//! @param [in] count_in Element count.
//! @param [in,out] accumulator_inout Response accumulator.
//! @return Consumed byte count. Zero aborts transfer.
//! @note libcurl decides this parameter list, so the accumulator stays last. The public API policy
//!       exempts a function bound to a foreign callback typedef for exactly this.
std::size_t writeBody(
          char*             content_in
        , const std::size_t size_in
        , const std::size_t count_in
        , void*             p_accumulator_inout
)
{
    auto& accumulator = *static_cast<sResponseAccumulator*>( p_accumulator_inout );
    if ( size_in != 0U && count_in > ( std::numeric_limits<std::size_t>::max )() / size_in )
    {
        accumulator.body_limit_exceeded = true;
        return 0U;
    }
    const std::size_t total = size_in * count_in;
    if ( total > accumulator.maximum_body_size - accumulator.body.size() )
    {
        accumulator.body_limit_exceeded = true;
        return 0U;
    }
    try
    {
        const auto* first = reinterpret_cast<const std::uint8_t*>( content_in );
        accumulator.body.insert( accumulator.body.end(), first, first + total );
    }
    catch ( const std::bad_alloc& )
    {
        accumulator.allocation_failed = true;
        return 0U;
    }
    return total;
}

//! @param [in] content_in Received header bytes.
//! @param [in] size_in Element size.
//! @param [in] count_in Element count.
//! @param [in,out] accumulator_inout Response accumulator.
//! @return Consumed byte count. Zero aborts transfer.
//! @note libcurl decides this parameter list, so the accumulator stays last. The public API policy
//!       exempts a function bound to a foreign callback typedef for exactly this.
std::size_t writeHeader(
          char*             content_in
        , const std::size_t size_in
        , const std::size_t count_in
        , void*             p_accumulator_inout
)
{
    auto& accumulator = *static_cast<sResponseAccumulator*>( p_accumulator_inout );
    if ( size_in != 0U && count_in > ( std::numeric_limits<std::size_t>::max )() / size_in )
    {
        accumulator.allocation_failed = true;
        return 0U;
    }
    const std::size_t total = size_in * count_in;
    try
    {
        std::string line( content_in, total );
        while ( !line.empty() && ( line.back() == '\r' || line.back() == '\n' ) )
        {
            line.pop_back();
        }
        const std::string::size_type separator = line.find( ':' );
        if ( separator != std::string::npos )
        {
            accumulator.headers.emplace_back(
                  trimHttpWhitespace( line.substr( 0U, separator ) )
                , trimHttpWhitespace( line.substr( separator + 1U ) )
            );
        }
    }
    catch ( const std::bad_alloc& )
    {
        accumulator.allocation_failed = true;
        return 0U;
    }
    return total;
}

//! @param [in] method_in HTTP Method.
//! @return curl custom method name or nullptr for dedicated options.
const char* customMethod( const eHttpMethod method_in ) noexcept
{
    switch ( method_in )
    {
    case eHttpMethod::Put:
        return "PUT";
    case eHttpMethod::Patch:
        return "PATCH";
    case eHttpMethod::Delete:
        return "DELETE";
    default:
        return nullptr;
    }
}

//! @param [in] status_code_in HTTP Status.
//! @return Retry候補Statusの場合true.
bool isTransientHttpStatus( const std::uint16_t status_code_in ) noexcept
{
    switch ( status_code_in )
    {
    case 408U:
    case 425U:
    case 429U:
    case 500U:
    case 502U:
    case 503U:
    case 504U:
        return true;
    default:
        return false;
    }
}

//! @param [in] result_in HTTP attempt result.
//! @return 一時Failureの場合true.
bool isTransientFailure( const HttpResult& result_in ) noexcept
{
    if ( result_in.succeeded() )
    {
        return false;
    }
    if ( result_in.error().code() == eTransportErrorCode::HttpStatusError )
    {
        return isTransientHttpStatus( result_in.value().status_code() );
    }
    switch ( result_in.error().category() )
    {
    case eTransportErrorCategory::Resolution:
    case eTransportErrorCategory::Connection:
    case eTransportErrorCategory::InputOutput:
    case eTransportErrorCategory::Timeout:
        return result_in.error().code() != eTransportErrorCode::ResponseTooLarge
            && result_in.error().code() != eTransportErrorCode::ResourceExhausted;
    default:
        return false;
    }
}

//! @param [in] delay_in Requested delay.
//! @param [in] deadline_in Overall deadline.
//! @param [in] cancellation_in Cancellation token.
//! @return Wait completion status.
TransportStatus waitForRetry(
      const std::chrono::milliseconds delay_in
    , const Clock::time_point         deadline_in
    , const CancellationToken&        cancellation_in
)
{
    const Clock::time_point now = Clock::now();
    const Clock::duration delay = delay_in;
    const Clock::time_point requested_end = delay >= ( Clock::time_point::max )() - now
        ? ( Clock::time_point::max )()
        : now + delay;
    while ( Clock::now() < requested_end )
    {
        if ( cancellation_in.isCancellationRequested() )
        {
            return TransportStatus::failure( cancellationError() );
        }
        const Clock::time_point now = Clock::now();
        if ( now >= deadline_in )
        {
            return TransportStatus::failure( timeoutError() );
        }
        const auto remaining_retry = std::chrono::duration_cast<std::chrono::milliseconds>(
            requested_end - now );
        const auto remaining_total = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline_in - now );
        const auto sleep_duration = ( std::min )(
              CANCELLATION_POLL_INTERVAL
            , ( std::min )( remaining_retry, remaining_total )
        );
        if ( sleep_duration.count() > 0 )
        {
            std::this_thread::sleep_for( sleep_duration );
        }
    }
    return TransportStatus::success();
}

//! @param [in] request_in Request.
//! @param [in] maximum_body_size_in Response limit [byte].
//! @param [in] deadline_in Overall deadline.
//! @param [in] cancellation_in Cancellation token.
//! @param [in] attempt_count_in Current attempt number.
//! @return One attempt result.
HttpResult executeAttempt(
      const HttpRequest&       request_in
    , const std::size_t        maximum_body_size_in
    , const Clock::time_point  deadline_in
    , const CancellationToken& cancellation_in
    , const std::uint32_t      attempt_count_in
    , const HttpAuthentication* const authentication_in
)
{
    CURL* easy = curl_easy_init();
    CURLM* multi = curl_multi_init();
    curl_slist* header_list = nullptr;
    sResponseAccumulator accumulator = {
          {
          }
        , {}
        , maximum_body_size_in
        , false
        , false
    };
    if ( easy == nullptr || multi == nullptr )
    {
        if ( easy != nullptr )
        {
            curl_easy_cleanup( easy );
        }
        if ( multi != nullptr )
        {
            curl_multi_cleanup( multi );
        }
        return HttpResult(
            HttpResponse(),
            TransportError(
                  eTransportErrorCategory::InputOutput
                , eTransportErrorCode::ResourceExhausted
                , "Creating the libcurl request handles failed."
            )
        );
    }

    // Request Headerをlibcurl listへ変換する.
    for ( const sHttpHeader& header : request_in.headers() )
    {
        const std::string line = header.name + ": " + header.value;
        curl_slist* appended = curl_slist_append( header_list, line.c_str() );
        if ( appended == nullptr )
        {
            curl_slist_free_all( header_list );
            curl_easy_cleanup( easy );
            curl_multi_cleanup( multi );
            return HttpResult(
                HttpResponse(),
                TransportError(
                      eTransportErrorCategory::InputOutput
                    , eTransportErrorCode::ResourceExhausted
                    , "Allocating the libcurl header list failed."
                )
            );
        }
        header_list = appended;
    }

    const long timeout_ms = remainingMilliseconds( deadline_in );
    CURLcode setup_result = CURLE_OK;
    const auto set_option = [&]( const CURLoption option_in, const auto value_in )
    {
        if ( setup_result == CURLE_OK )
        {
            setup_result = curl_easy_setopt( easy, option_in, value_in );
        }
    };

    // Backend共通Optionを設定する.
    set_option( CURLOPT_URL, request_in.url().c_str() );
    set_option( CURLOPT_NOSIGNAL, 1L );
    set_option( CURLOPT_FOLLOWLOCATION, 0L );
    set_option( CURLOPT_FAILONERROR, 0L );
    set_option( CURLOPT_ACCEPT_ENCODING, "" );
    set_option( CURLOPT_USERAGENT, "WSE/0.1.0" );
    set_option( CURLOPT_TIMEOUT_MS, timeout_ms );
    set_option( CURLOPT_CONNECTTIMEOUT_MS, timeout_ms );
    set_option( CURLOPT_WRITEFUNCTION, &writeBody );
    set_option( CURLOPT_WRITEDATA, &accumulator );
    set_option( CURLOPT_HEADERFUNCTION, &writeHeader );
    set_option( CURLOPT_HEADERDATA, &accumulator );
    set_option( CURLOPT_HTTPHEADER, header_list );
    if ( authentication_in != nullptr )
    {
        set_option( CURLOPT_HTTPAUTH, CURLAUTH_ANY );
        set_option( CURLOPT_USERNAME, authentication_in->username().c_str() );
        set_option( CURLOPT_PASSWORD, authentication_in->secret().c_str() );
    }

    // MethodとBodyを設定する.
    if ( request_in.method() == eHttpMethod::Get )
    {
        set_option( CURLOPT_HTTPGET, 1L );
    }
    else if ( request_in.method() == eHttpMethod::Head )
    {
        set_option( CURLOPT_NOBODY, 1L );
    }
    else
    {
        if ( request_in.method() == eHttpMethod::Post )
        {
            set_option( CURLOPT_POST, 1L );
        }
        else
        {
            set_option( CURLOPT_CUSTOMREQUEST, customMethod( request_in.method() ) );
        }
        const char* body = request_in.body().empty()
            ? ""
            : reinterpret_cast<const char*>( request_in.body().data() );
        set_option( CURLOPT_POSTFIELDS, body );
        set_option(
              CURLOPT_POSTFIELDSIZE_LARGE
            , static_cast<curl_off_t>( request_in.body().size() )
        );
    }

    if ( setup_result != CURLE_OK )
    {
        curl_slist_free_all( header_list );
        curl_easy_cleanup( easy );
        curl_multi_cleanup( multi );
        return HttpResult( HttpResponse(), convertCurlError( setup_result ) );
    }

    // Multi interfaceで最大20 msごとにCancellationと全体Deadlineを確認する.
    CURLcode transfer_result = CURLE_FAILED_INIT;
    const CURLMcode add_result = curl_multi_add_handle( multi, easy );
    int running = 0;
    if ( add_result == CURLM_OK )
    {
        CURLMcode multi_result = curl_multi_perform( multi, &running );
        while ( multi_result == CURLM_OK && running > 0 )
        {
            if ( cancellation_in.isCancellationRequested() )
            {
                transfer_result = CURLE_ABORTED_BY_CALLBACK;
                break;
            }
            const long remaining_ms = remainingMilliseconds( deadline_in );
            if ( remaining_ms <= 0L )
            {
                transfer_result = CURLE_OPERATION_TIMEDOUT;
                break;
            }
            const int poll_ms = static_cast<int>( ( std::min )(
                  remaining_ms
                , static_cast<long>( CANCELLATION_POLL_INTERVAL.count() ) ) );
            int descriptor_count = 0;
            multi_result = curl_multi_poll( multi, nullptr, 0U, poll_ms, &descriptor_count );
            if ( multi_result == CURLM_OK )
            {
                multi_result = curl_multi_perform( multi, &running );
            }
        }
        if ( multi_result != CURLM_OK )
        {
            transfer_result = CURLE_RECV_ERROR;
        }
        else if ( transfer_result == CURLE_FAILED_INIT )
        {
            int message_count = 0;
            for ( CURLMsg* message = curl_multi_info_read( multi, &message_count );
                  message != nullptr;
                  message = curl_multi_info_read( multi, &message_count ) )
            {
                if ( message->msg == CURLMSG_DONE && message->easy_handle == easy )
                {
                    transfer_result = message->data.result;
                    break;
                }
            }
        }
        curl_multi_remove_handle( multi, easy );
    }

    long status_code = 0L;
    curl_easy_getinfo( easy, CURLINFO_RESPONSE_CODE, &status_code );
    curl_slist_free_all( header_list );
    curl_easy_cleanup( easy );
    curl_multi_cleanup( multi );

    const std::uint16_t portable_status = status_code >= 0L && status_code <= 65535L
        ? static_cast<std::uint16_t>( status_code )
        : 0U;
    const HttpResponse response(
          portable_status
        , accumulator.headers
        , accumulator.body
        , attempt_count_in
    );
    if ( cancellation_in.isCancellationRequested() )
    {
        return HttpResult( response, cancellationError() );
    }
    if ( remainingMilliseconds( deadline_in ) <= 0L || transfer_result == CURLE_OPERATION_TIMEDOUT )
    {
        return HttpResult( response, timeoutError() );
    }
    if ( accumulator.body_limit_exceeded )
    {
        return HttpResult(
            response,
            TransportError(
                  eTransportErrorCategory::Http
                , eTransportErrorCode::ResponseTooLarge
                , "The HTTP response body exceeded the configured limit."
            )
        );
    }
    if ( accumulator.allocation_failed )
    {
        return HttpResult(
            response,
            TransportError(
                  eTransportErrorCategory::InputOutput
                , eTransportErrorCode::ResourceExhausted
                , "Allocating the HTTP response failed."
            )
        );
    }
    if ( transfer_result != CURLE_OK )
    {
        return HttpResult( response, convertCurlError( transfer_result ) );
    }
    if ( portable_status >= 400U )
    {
        return HttpResult(
            response,
            TransportError(
                  eTransportErrorCategory::Http
                , eTransportErrorCode::HttpStatusError
                , "The HTTP server returned an error status."
                , portable_status
            )
        );
    }
    return HttpResult( response );
}

HttpResult executeRequest(
      const HttpRequest&          request_in
    , const HttpExecutionOptions& options_in
    , const OperationContext&     context_in
    , const HttpAuthentication* const authentication_in )
{
    if ( !context_in.isValid() || !options_in.isValid() || !isValidMethod( request_in.method() )
         || !isValidUrl( request_in.url() ) ||
         ( authentication_in != nullptr && !authentication_in->isValid() ) )
    {
        return HttpResult(
            HttpResponse(), validationError( "The HTTP request, authentication, options, and timeout must be valid." ) );
    }
    if ( ( request_in.method() == eHttpMethod::Get || request_in.method() == eHttpMethod::Head )
         && !request_in.body().empty() )
    {
        return HttpResult(
            HttpResponse(), validationError( "GET and HEAD requests must not contain a body." ) );
    }
    if ( !std::all_of( request_in.headers().begin(), request_in.headers().end(), &isValidHeader ) )
    {
        return HttpResult(
            HttpResponse(), validationError( "The HTTP request contains an invalid header." ) );
    }
    if ( context_in.cancellation().isCancellationRequested() )
    {
        return HttpResult( HttpResponse(), cancellationError() );
    }

    CurlRuntime& runtime = getCurlRuntime();
    if ( !runtime.isReady() )
    {
        return HttpResult(
            HttpResponse(), convertCurlError( runtime.result() ) );
    }

    const Clock::time_point deadline = calculateDeadline( context_in );
    std::uint32_t attempt_count = 0U;
    for ( ;; )
    {
        ++attempt_count;
        HttpResult result = executeAttempt(
              request_in
            , options_in.maximum_response_body_size()
            , deadline
            , context_in.cancellation()
            , attempt_count
            , authentication_in );
        if ( result.succeeded() )
        {
            return result;
        }

        const eRetryFailureDisposition disposition =
            options_in.shouldRetryTransientFailures() && isTransientFailure( result )
            ? eRetryFailureDisposition::Retryable
            : eRetryFailureDisposition::DoNotRetry;
        const RetryDecision decision = options_in.retry_policy().evaluate(
              attempt_count
            , options_in.operation_safety()
            , disposition
            , result.error() );
        if ( !decision.shouldRetry() )
        {
            return result;
        }

        const TransportStatus waited = waitForRetry(
              decision.delay()
            , deadline
            , context_in.cancellation() );
        if ( !waited.succeeded() )
        {
            return HttpResult( result.value(), waited.error() );
        }
    }
}

} // namespace

HttpAuthentication::HttpAuthentication(
      const eHttpAuthenticationPolicy policy_in
    , const std::string& username_in
    , const std::string& secret_in )
    : m_policy   ( policy_in )
    , m_username ( username_in )
    , m_secret   ( secret_in )
{
}

HttpAuthentication::~HttpAuthentication()
{
    eraseString( &this->m_username );
    eraseString( &this->m_secret );
}

HttpAuthentication::HttpAuthentication( HttpAuthentication&& other_inout ) noexcept
    : m_policy   ( other_inout.m_policy )
    , m_username ( std::move( other_inout.m_username ) )
    , m_secret   ( std::move( other_inout.m_secret ) )
{
    eraseString( &other_inout.m_username );
    eraseString( &other_inout.m_secret );
}

HttpAuthentication& HttpAuthentication::operator=( HttpAuthentication&& other_inout ) noexcept
{
    if ( this != &other_inout )
    {
        eraseString( &this->m_username );
        eraseString( &this->m_secret );
        this->m_policy = other_inout.m_policy;
        this->m_username = std::move( other_inout.m_username );
        this->m_secret = std::move( other_inout.m_secret );
        eraseString( &other_inout.m_username );
        eraseString( &other_inout.m_secret );
    }
    return *this;
}

eHttpAuthenticationPolicy HttpAuthentication::policy() const noexcept
{
    return this->m_policy;
}

const std::string& HttpAuthentication::username() const noexcept
{
    return this->m_username;
}

const std::string& HttpAuthentication::secret() const noexcept
{
    return this->m_secret;
}

bool HttpAuthentication::isValid() const noexcept
{
    return this->m_policy == eHttpAuthenticationPolicy::ServerNegotiated &&
           validAuthenticationText( this->m_username ) &&
           validAuthenticationText( this->m_secret );
}

sHttpHeader::sHttpHeader()
    : name  ()
    , value ()
{
}

sHttpHeader::sHttpHeader( const std::string& name_in, const std::string& value_in )
    : name  ( name_in )
    , value ( value_in )
{
}

HttpRequest::HttpRequest( const eHttpMethod method_in, const std::string& url_in )
    : m_method  ( method_in )
    , m_url     ( url_in )
    , m_headers ()
    , m_body    ()
{
}

eHttpMethod HttpRequest::method() const noexcept
{
    return this->m_method;
}

const std::string& HttpRequest::url() const noexcept
{
    return this->m_url;
}

const std::vector<sHttpHeader>& HttpRequest::headers() const noexcept
{
    return this->m_headers;
}

const std::vector<std::uint8_t>& HttpRequest::body() const noexcept
{
    return this->m_body;
}

void HttpRequest::addHeader( const std::string& name_in, const std::string& value_in )
{
    this->m_headers.emplace_back( name_in, value_in );
}

void HttpRequest::setBody( const std::vector<std::uint8_t>& body_in )
{
    this->m_body = body_in;
}

HttpResponse::HttpResponse() noexcept
    : m_status_code   ( 0U )
    , m_headers       ()
    , m_body          ()
    , m_attempt_count ( 0U )
{
}

HttpResponse::HttpResponse(
      const std::uint16_t              status_code_in
    , const std::vector<sHttpHeader>&  headers_in
    , const std::vector<std::uint8_t>& body_in
    , const std::uint32_t              attempt_count_in
)
    : m_status_code   ( status_code_in )
    , m_headers       ( headers_in )
    , m_body          ( body_in )
    , m_attempt_count ( attempt_count_in )
{
}

std::uint16_t HttpResponse::status_code() const noexcept
{
    return this->m_status_code;
}

const std::vector<sHttpHeader>& HttpResponse::headers() const noexcept
{
    return this->m_headers;
}

const std::vector<std::uint8_t>& HttpResponse::body() const noexcept
{
    return this->m_body;
}

std::uint32_t HttpResponse::attempt_count() const noexcept
{
    return this->m_attempt_count;
}

HttpExecutionOptions::HttpExecutionOptions() noexcept
    : m_maximum_response_body_size ( DEFAULT_MAXIMUM_RESPONSE_BODY_SIZE )
    , m_retry_policy               ()
    , m_operation_safety           ( eRetryOperationSafety::NonIdempotent )
    , m_retry_transient_failures   ( false )
{
}

HttpExecutionOptions::HttpExecutionOptions(
      const std::size_t            maximum_response_body_size_in
    , const RetryPolicy&           retry_policy_in
    , const eRetryOperationSafety  operation_safety_in
    , const bool                   retry_transient_failures_in
) noexcept
    : m_maximum_response_body_size ( maximum_response_body_size_in )
    , m_retry_policy               ( retry_policy_in )
    , m_operation_safety           ( operation_safety_in )
    , m_retry_transient_failures   ( retry_transient_failures_in )
{
}

std::size_t HttpExecutionOptions::maximum_response_body_size() const noexcept
{
    return this->m_maximum_response_body_size;
}

const RetryPolicy& HttpExecutionOptions::retry_policy() const noexcept
{
    return this->m_retry_policy;
}

eRetryOperationSafety HttpExecutionOptions::operation_safety() const noexcept
{
    return this->m_operation_safety;
}

bool HttpExecutionOptions::shouldRetryTransientFailures() const noexcept
{
    return this->m_retry_transient_failures;
}

bool HttpExecutionOptions::isValid() const noexcept
{
    return this->m_maximum_response_body_size > 0U
        && this->m_retry_policy.isValid()
        && ( this->m_operation_safety == eRetryOperationSafety::NonIdempotent
             || this->m_operation_safety == eRetryOperationSafety::Idempotent );
}

HttpResult HttpClient::execute(
      const HttpRequest&          request_in
    , const HttpExecutionOptions& options_in
    , const OperationContext&     context_in
) const
{
    return executeRequest( request_in, options_in, context_in, nullptr );
}

HttpResult HttpClient::executeAuthenticated(
      const HttpRequest&          request_in
    , const HttpExecutionOptions& options_in
    , const HttpAuthentication&   authentication_in
    , const OperationContext&     context_in
) const
{
    return executeRequest( request_in, options_in, context_in, &authentication_in );
}

} // namespace xpt
} // namespace wse
