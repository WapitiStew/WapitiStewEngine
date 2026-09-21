// @file main.cpp
// @brief Smoke test for an installed WSE package: constructs the Core and XPT value types a
//        consumer meets first and drives one HTTP call that is rejected before it reaches the
//        network. Link WSE::Core and WSE::Xpt; see the CMakeLists beside this file.
//
// Every check here is deliberately offline. The whole point of the exercise is that it runs on
// an air-gapped machine, so nothing may depend on a name resolving, a port answering, or a
// clock beyond the steady one. The exit code says which step failed: 1 a value type that did
// not behave, 2 a request that failed for something other than validation, which would mean
// the call left the process.

#include <wse/stew.h>
#include <xpt/stew.h>

int main()
{
    // Endpoint applies exactly two rules and no more: the host must not be empty and the port
    // must not be zero. Port 1 is therefore valid even though nothing is listening on it, and
    // the host string is stored verbatim without any syntax or DNS check. Zero is reserved to
    // mean "let the operating system assign one" on a UDP bind, which is why it cannot also
    // stand for a usable remote endpoint.
    const wse::xpt::Endpoint endpoint("127.0.0.1", 1);
    // A default RetryPolicy is valid but permits only the initial attempt, so retry is off
    // until a caller asks for it. The count includes that first attempt, which is why the
    // no-retry value is 1 rather than 0; 0 is rejected as an invalid policy.
    const wse::xpt::RetryPolicy retryPolicy;
    // The client holds no state and no connection, so constructing one costs nothing and one
    // per call is as reasonable as one per process.
    const wse::xpt::HttpClient httpClient;
    // The request constructor validates nothing; it stores the method and the URL and lets
    // execute() judge them. "invalid-url" carries no http:// or https:// scheme, which is the
    // first thing the URL check rejects, so this call can never open a socket.
    const wse::xpt::HttpRequest invalidRequest(wse::xpt::eHttpMethod::Get, "invalid-url");
    // The default options are an 8 MiB response ceiling, a no-retry policy, and non-idempotent
    // operation safety, so this request would be attempted exactly once for three independent
    // reasons. The context is required rather than defaulted: XPT refuses to invent a deadline,
    // because an accidental infinite wait is worse than an argument the caller had to think
    // about. Its 100 ms covers the whole operation including every attempt and any backoff, not
    // one attempt, and it is never consumed here because validation fails before any wait
    // begins. It still has to be a valid, non-negative duration: an invalid context is itself a
    // validation failure and would make the check below pass for the wrong reason.
    const wse::xpt::HttpResult result = httpClient.execute(
        invalidRequest,
        wse::xpt::HttpExecutionOptions(),
        wse::xpt::OperationContext(wse::xpt::Timeout::milliseconds(100)));

    // Three preconditions in one branch: the endpoint rules held, retry really is off by
    // default, and the malformed URL was refused rather than sent.
    if (!endpoint.isValid() || retryPolicy.maximum_attempts() != 1U || result.succeeded())
    {
        return 1;
    }
    // The category is what makes this test offline rather than merely failing. Validation means
    // the contract was checked inside the process, before resolution, so no attempt was made
    // and none would have been retried. Any other category — Resolution, Connection, Timeout —
    // would mean the call reached the network, which on an air-gapped machine it must not.
    // Branch on the category, never on the message or the native code.
    return result.error().category() == wse::xpt::eTransportErrorCategory::Validation ? 0 : 2;
}
