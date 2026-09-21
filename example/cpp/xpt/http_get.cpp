// @file http_get.cpp
// @brief Portable XPT HTTP client: one GET request with the default execution options,
//        plus the structured error a caller sees when the server is unreachable.
//
// Requires WSE_BUILD_XPT=ON. The URL below is a source constant; without network access
// the sample logs the structured error and exits cleanly.
//
// The sample returns 0 whatever happens on the wire. Neither an unreachable host nor a refused
// connection is a defect here: both are the structured error this file exists to show, and the
// samples have to run on a machine with no network at all.
//
// An HTTP status of 4xx or 5xx is a failure of the call, not a success with a bad code. It
// arrives as an Http-category error whose value() still holds the response that was received,
// so a caller reads the status and body from the failed result rather than from a second call.

#include <xpt/stew.h>
#include <wse/stew.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>

namespace
{

//! A stable, small, plain-HTTP document reserved for exactly this use. Point it at any absolute
//! http:// or https:// URL; the scheme has to be present, because the request takes an absolute
//! URL and never a bare host.
constexpr const char URL[] = "http://example.com/";
//! TCP port 9 (discard) is a conventional closed port for the failure demonstration.
// Loopback is used so the refusal comes back immediately from the local stack instead of
// waiting out the deadline: a routable address with no listener would usually time out instead,
// which is a different category and a slower sample.
constexpr const char UNREACHABLE_URL[] = "http://127.0.0.1:9/";
//! Log preview length only. The whole body is already in memory by then; this bounds what is
//! printed, not what is fetched, which is what the option's body ceiling does.
constexpr std::size_t BODY_PREVIEW_BYTES = 80U;

void logTransportError( const std::string& operation_in, const wse::xpt::TransportError& error_in )
{
    wse::WLog() << "ERROR:" << operation_in << "failed."
                << "category=" << static_cast< int >( error_in.category() )
                << "code=" << static_cast< int >( error_in.code() )
                << "message=" << error_in.message();
}

} // namespace

int main()
{
    wse::registDefaultLog();

    // XPT has no default timeout, so every call takes an explicit one and no caller can wait
    // forever by accident. Five seconds covers name resolution, the connect, and the transfer
    // together, and it is the budget for every attempt and every backoff between them, not per
    // attempt; a retry policy has to fit inside it. The value is larger than the other XPT
    // samples use because a real name lookup is involved. Exceeding it gives a Timeout category
    // rather than a partial response. The same context is reused below: it carries a duration,
    // not a start time, so each call gets its own five seconds.
    const wse::xpt::OperationContext context(
        wse::xpt::Timeout( std::chrono::milliseconds( 5000 ) ) );

    // The default options are 8 MiB maximum body, no retry, non-idempotent safety.
    // A response past the ceiling is refused as ResponseTooLarge rather than silently truncated,
    // so raising it is what a caller does to fetch something larger. The default retry policy
    // permits one attempt only, and the NonIdempotent safety would refuse a retry even under a
    // policy that allowed one, because only the caller can say that repeating a request is safe.
    // A GET could honestly be declared Idempotent; the defaults are deliberately the cautious
    // choice and this sample does not override them.
    const wse::xpt::HttpExecutionOptions options;
    // The client holds no connection and no state; both requests below go through the same
    // const object, and there is nothing to close afterwards.
    const wse::xpt::HttpClient client;

    const auto result = client.execute(
        wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, URL ), options, context );
    if ( result.succeeded() )
    {
        // The response owns its headers and body, and it is a value: it stays usable after the
        // client goes out of scope, and nothing has to be released.
        const auto& response = result.value();
        const std::string preview( response.body().begin(),
                                   response.body().begin()
                                       + std::min( response.body().size(), BODY_PREVIEW_BYTES ) );
        wse::WLog() << "GET" << URL << ": status=" << response.status_code()
                    << "headers=" << response.headers().size()
                    << "body_bytes=" << response.body().size();
        wse::WLog() << "body preview:" << preview;
    }
    else
    {
        // A 4xx/5xx answer is also reported here, with the received response preserved
        // in result.value(); a transport problem carries an empty response instead.
        logTransportError( std::string( "GET " ) + URL, result.error() );
        wse::WLog() << "received status=" << result.value().status_code()
                    << "(0 means the response never arrived)";
    }

    // The same call shape against a closed port yields a Connection-category error.
    const auto refused = client.execute(
        wse::xpt::HttpRequest( wse::xpt::eHttpMethod::Get, UNREACHABLE_URL ), options, context );
    // Success here means the assumption behind the constant failed, not that the sample did.
    if ( refused.succeeded() )
    {
        wse::WLog() << "the unreachable URL unexpectedly answered; another service owns the port";
    }
    else
    {
        wse::WLog() << "unreachable URL reported: category="
                    << static_cast< int >( refused.error().category() )
                    << "code=" << static_cast< int >( refused.error().code() )
                    << "(branch on category and code, not on the message text)";
    }

    // Both requests were allowed to fail, so the process still succeeds. A tool built on this
    // would map the category onto its own exit codes here.
    return 0;
}
