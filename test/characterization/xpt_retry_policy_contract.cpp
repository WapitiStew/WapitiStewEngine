//*****************************************************************************************************************
//!
//! @file    xpt_retry_policy_contract.cpp
//! @brief   \~japanese XPT Retry Policyの安全側契約を検証する.
//! @brief   \~english  Verifies the fail-safe XPT retry-policy contract.
//!
//! @details
//!     \~japanese
//!         `RetryPolicy::evaluate`は、失敗したTransport Operationを繰り返してよいかを決める唯一の
//!      @n 場所である. 判断が緩めば、副作用のあるRequestが黙って二重に実行され、Cancel済みの
//!      @n Operationが蘇り、Validation失敗やSecurity失敗をServerへ叩き続けることになる.
//!      @n 逆にBackoffの計算が溢れれば、負値や桁外れの待機時間がそのまま呼出側へ渡る.
//!      @n 本Testは決定そのものだけを見る. `evaluate`は待機もOperation再実行も行わない.
//!     \~english
//!         `RetryPolicy::evaluate` is the single place that decides whether a failed transport
//!      @n operation may be repeated. Loosen it and a request with side effects is silently executed
//!      @n twice, a cancelled operation comes back to life, and a validation or security failure is
//!      @n hammered at the server. Let the backoff arithmetic overflow and a negative or absurd
//!      @n delay reaches the caller. This test only inspects the decision: `evaluate` neither sleeps
//!      @n nor re-executes anything.
//!
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

#include <xpt/retry/RetryPolicy.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{

int g_failure_count = 0;

void require( const bool condition_in, const char* const message_in )
{
    if ( !condition_in )
    {
        std::cerr << message_in << '\n';
        ++g_failure_count;
    }
}

// Timeout is on the policy's retryable category list, so this is the neutral failure: it lets a
// retry through whenever nothing else blocks it, and every stop reason a test sees is the gate the
// test named rather than the error itself.
wse::xpt::TransportError makeTimeoutError()
{
    return wse::xpt::TransportError(
          wse::xpt::eTransportErrorCategory::Timeout
        , wse::xpt::eTransportErrorCode::TimedOut
        , "The test operation timed out."
    );
}

wse::xpt::RetryDecision evaluateRetryable(
      const wse::xpt::RetryPolicy& policy_in
    , const std::uint32_t          attempts_completed_in
)
{
    return policy_in.evaluate(
          attempts_completed_in
        , wse::xpt::eRetryOperationSafety::Idempotent
        , wse::xpt::eRetryFailureDisposition::Retryable
        , makeTimeoutError()
    );
}

} // namespace

int main()
{
    using namespace std::chrono_literals;

    // The default policy is valid and disables every retry.
    const wse::xpt::RetryPolicy disabled_policy;
    require( disabled_policy.isValid(), "The default no-retry policy must be valid." );
    require( disabled_policy.maximum_attempts() == 1U,
             "The default policy must permit only the initial attempt." );
    require( !evaluateRetryable( disabled_policy, 1U ).shouldRetry(),
             "The default policy must not retry." );

    // Fixed and exponential policies count the initial attempt and calculate deterministic backoff.
    const wse::xpt::RetryPolicy fixed_policy = wse::xpt::RetryPolicy::createFixed( 3U, 25ms );
    require( fixed_policy.isValid(), "The fixed retry policy must be valid." );
    const wse::xpt::RetryDecision fixed_first = evaluateRetryable( fixed_policy, 1U );
    const wse::xpt::RetryDecision fixed_second = evaluateRetryable( fixed_policy, 2U );
    const wse::xpt::RetryDecision fixed_last = evaluateRetryable( fixed_policy, 3U );
    require( fixed_first.shouldRetry() && fixed_first.delay() == 25ms,
             "The first fixed retry must use the configured delay." );
    require( fixed_second.shouldRetry() && fixed_second.delay() == 25ms,
             "The second fixed retry must keep the configured delay." );
    require( !fixed_last.shouldRetry()
                 && fixed_last.stop_reason() == wse::xpt::eRetryStopReason::AttemptLimitReached,
             "The maximum-attempt limit must include the initial attempt." );

    const wse::xpt::RetryPolicy exponential_policy =
        wse::xpt::RetryPolicy::createExponential( 5U, 10ms, 25ms );
    require( evaluateRetryable( exponential_policy, 1U ).delay() == 10ms,
             "The first exponential retry must use the initial delay." );
    require( evaluateRetryable( exponential_policy, 2U ).delay() == 20ms,
             "The second exponential retry must double the delay." );
    require( evaluateRetryable( exponential_policy, 3U ).delay() == 25ms,
             "Exponential retry delay must be capped." );
    require( evaluateRetryable( exponential_policy, 4U ).delay() == 25ms,
             "The capped retry delay must remain stable." );

    // Invalid values and attempt zero must fail closed.
    const wse::xpt::RetryPolicy invalid_attempts = wse::xpt::RetryPolicy::createFixed( 0U, 1ms );
    const wse::xpt::RetryPolicy invalid_negative(
          2U
        , -1ms
        , 1ms
        , wse::xpt::eRetryBackoffStrategy::Exponential
    );
    const wse::xpt::RetryPolicy invalid_cap(
          2U
        , 2ms
        , 1ms
        , wse::xpt::eRetryBackoffStrategy::Exponential
    );
    require( !invalid_attempts.isValid() && !invalid_negative.isValid() && !invalid_cap.isValid(),
             "Invalid retry-policy values must be observable." );
    require( evaluateRetryable( invalid_attempts, 1U ).stop_reason()
                 == wse::xpt::eRetryStopReason::InvalidPolicy,
             "An invalid policy must fail closed." );
    require( evaluateRetryable( fixed_policy, 0U ).stop_reason()
                 == wse::xpt::eRetryStopReason::InvalidAttempt,
             "Attempt zero must fail closed." );

    // Both operation safety and failure disposition are explicit gates.
    const wse::xpt::RetryDecision non_idempotent = fixed_policy.evaluate(
          1U
        , wse::xpt::eRetryOperationSafety::NonIdempotent
        , wse::xpt::eRetryFailureDisposition::Retryable
        , makeTimeoutError()
    );
    require( !non_idempotent.shouldRetry()
                 && non_idempotent.stop_reason()
                        == wse::xpt::eRetryStopReason::NonIdempotentOperation,
             "A non-idempotent operation must never be retried by the policy." );

    const wse::xpt::RetryDecision permanent_failure = fixed_policy.evaluate(
          1U
        , wse::xpt::eRetryOperationSafety::Idempotent
        , wse::xpt::eRetryFailureDisposition::DoNotRetry
        , makeTimeoutError()
    );
    require( !permanent_failure.shouldRetry()
                 && permanent_failure.stop_reason()
                        == wse::xpt::eRetryStopReason::FailureNotRetryable,
             "A caller-classified permanent failure must not be retried." );

    const wse::xpt::RetryDecision success = fixed_policy.evaluate(
          1U
        , wse::xpt::eRetryOperationSafety::Idempotent
        , wse::xpt::eRetryFailureDisposition::Retryable
        , wse::xpt::TransportError()
    );
    require( !success.shouldRetry() && success.stop_reason() == wse::xpt::eRetryStopReason::NoFailure,
             "A successful result must never be retried." );

    // Cancellation and stable non-transient categories override a Retryable classification.
    const wse::xpt::TransportError cancelled_error(
          wse::xpt::eTransportErrorCategory::Cancellation
        , wse::xpt::eTransportErrorCode::Cancelled
        , "The test operation was cancelled."
    );
    const wse::xpt::RetryDecision cancelled = fixed_policy.evaluate(
          1U
        , wse::xpt::eRetryOperationSafety::Idempotent
        , wse::xpt::eRetryFailureDisposition::Retryable
        , cancelled_error
    );
    require( !cancelled.shouldRetry()
                 && cancelled.stop_reason() == wse::xpt::eRetryStopReason::Cancelled,
             "Cancellation must override a retryable caller classification." );

    const wse::xpt::eTransportErrorCategory blocked_categories[] =
    {
          wse::xpt::eTransportErrorCategory::Validation
        , wse::xpt::eTransportErrorCategory::Protocol
        , wse::xpt::eTransportErrorCategory::Security
    };
    for ( const wse::xpt::eTransportErrorCategory category : blocked_categories )
    {
        const wse::xpt::RetryDecision blocked = fixed_policy.evaluate(
              1U
            , wse::xpt::eRetryOperationSafety::Idempotent
            , wse::xpt::eRetryFailureDisposition::Retryable
            , wse::xpt::TransportError(
                  category
                , wse::xpt::eTransportErrorCode::Unknown
                , "The test category is not retryable."
              )
        );
        require( !blocked.shouldRetry()
                     && blocked.stop_reason() == wse::xpt::eRetryStopReason::FailureNotRetryable,
                 "Validation, protocol, and security failures must fail closed." );
    }

    const wse::xpt::TransportError unsupported_error(
          wse::xpt::eTransportErrorCategory::InputOutput
        , wse::xpt::eTransportErrorCode::Unsupported
        , "The test operation is unsupported."
    );
    require( !fixed_policy.evaluate(
                    1U
                  , wse::xpt::eRetryOperationSafety::Idempotent
                  , wse::xpt::eRetryFailureDisposition::Retryable
                  , unsupported_error
              ).shouldRetry(),
             "Unsupported operations must fail closed." );

    // A future HTTP adapter can opt in only after it classifies the individual failure as transient.
    const wse::xpt::TransportError http_error(
          wse::xpt::eTransportErrorCategory::Http
        , wse::xpt::eTransportErrorCode::Unknown
        , "The test HTTP failure is transient."
    );
    require( fixed_policy.evaluate(
                 1U
               , wse::xpt::eRetryOperationSafety::Idempotent
               , wse::xpt::eRetryFailureDisposition::Retryable
               , http_error
             ).shouldRetry(),
             "An explicitly classified HTTP failure must be eligible for retry." );

    // Saturation prevents signed overflow at the chrono representation boundary.
    const std::chrono::milliseconds large_delay(
        std::numeric_limits<std::int64_t>::max() / 2 + 1 );
    const wse::xpt::RetryPolicy overflow_policy =
        wse::xpt::RetryPolicy::createExponential(
              3U
            , large_delay
            , std::chrono::milliseconds( std::numeric_limits<std::int64_t>::max() )
        );
    require( evaluateRetryable( overflow_policy, 2U ).delay()
                 == std::chrono::milliseconds( std::numeric_limits<std::int64_t>::max() ),
             "Exponential backoff must saturate without integer overflow." );

    if ( g_failure_count != 0 )
    {
        std::cerr << "XPT retry policy contract failures: " << g_failure_count << '\n';
        return 1;
    }

    return 0;
}
