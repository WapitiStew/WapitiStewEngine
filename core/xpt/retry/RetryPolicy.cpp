//*****************************************************************************************************************
//!
//! @file    RetryPolicy.cpp
//! @brief   \~japanese 明示的なXPT Retry Policyを実装する.
//! @brief   \~english  Implements the explicit XPT retry policy.
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

#include "xpt/retry/RetryPolicy.h"

namespace wse
{
namespace xpt
{
namespace
{

bool isValidStrategy( const eRetryBackoffStrategy strategy_in ) noexcept
{
    return strategy_in == eRetryBackoffStrategy::Fixed
        || strategy_in == eRetryBackoffStrategy::Exponential;
}

bool isRetryableSafety( const eRetryOperationSafety safety_in ) noexcept
{
    return safety_in == eRetryOperationSafety::Idempotent;
}

bool isRetryableDisposition( const eRetryFailureDisposition disposition_in ) noexcept
{
    return disposition_in == eRetryFailureDisposition::Retryable;
}

bool isPolicyRetryableError( const TransportError& error_in ) noexcept
{
    if ( error_in.ok()
         || error_in.code() == eTransportErrorCode::Cancelled
         || error_in.code() == eTransportErrorCode::Unsupported )
    {
        return false;
    }

    switch ( error_in.category() )
    {
        case eTransportErrorCategory::Resolution:
        case eTransportErrorCategory::Connection:
        case eTransportErrorCategory::InputOutput:
        case eTransportErrorCategory::Timeout:
        case eTransportErrorCategory::Http:
            return true;

        case eTransportErrorCategory::None:
        case eTransportErrorCategory::Validation:
        case eTransportErrorCategory::Cancellation:
        case eTransportErrorCategory::Protocol:
        case eTransportErrorCategory::Security:
        default:
            return false;
    }
}

} // namespace

RetryDecision::RetryDecision(
      const bool                      should_retry_in
    , const std::chrono::milliseconds delay_in
    , const eRetryStopReason          stop_reason_in
) noexcept
    : m_should_retry ( should_retry_in )
    , m_delay        ( delay_in )
    , m_stop_reason  ( stop_reason_in )
{
}

bool RetryDecision::shouldRetry() const noexcept
{
    return this->m_should_retry;
}

std::chrono::milliseconds RetryDecision::delay() const noexcept
{
    return this->m_delay;
}

eRetryStopReason RetryDecision::stop_reason() const noexcept
{
    return this->m_stop_reason;
}

RetryPolicy::RetryPolicy() noexcept
    : m_maximum_attempts ( 1U )
    , m_initial_delay    ( 0 )
    , m_maximum_delay    ( 0 )
    , m_backoff_strategy ( eRetryBackoffStrategy::Fixed )
{
}

RetryPolicy::RetryPolicy(
      const std::uint32_t              maximum_attempts_in
    , const std::chrono::milliseconds initial_delay_in
    , const std::chrono::milliseconds maximum_delay_in
    , const eRetryBackoffStrategy      backoff_strategy_in
) noexcept
    : m_maximum_attempts ( maximum_attempts_in )
    , m_initial_delay    ( initial_delay_in )
    , m_maximum_delay    ( maximum_delay_in )
    , m_backoff_strategy ( backoff_strategy_in )
{
}

RetryPolicy RetryPolicy::createFixed(
      const std::uint32_t              maximum_attempts_in
    , const std::chrono::milliseconds delay_in
) noexcept
{
    return RetryPolicy(
          maximum_attempts_in
        , delay_in
        , delay_in
        , eRetryBackoffStrategy::Fixed
    );
}

RetryPolicy RetryPolicy::createExponential(
      const std::uint32_t              maximum_attempts_in
    , const std::chrono::milliseconds initial_delay_in
    , const std::chrono::milliseconds maximum_delay_in
) noexcept
{
    return RetryPolicy(
          maximum_attempts_in
        , initial_delay_in
        , maximum_delay_in
        , eRetryBackoffStrategy::Exponential
    );
}

bool RetryPolicy::isValid() const noexcept
{
    return this->m_maximum_attempts >= 1U
        && this->m_initial_delay.count() >= 0
        && this->m_maximum_delay >= this->m_initial_delay
        && isValidStrategy( this->m_backoff_strategy );
}

std::uint32_t RetryPolicy::maximum_attempts() const noexcept
{
    return this->m_maximum_attempts;
}

std::chrono::milliseconds RetryPolicy::initial_delay() const noexcept
{
    return this->m_initial_delay;
}

std::chrono::milliseconds RetryPolicy::maximum_delay() const noexcept
{
    return this->m_maximum_delay;
}

eRetryBackoffStrategy RetryPolicy::backoff_strategy() const noexcept
{
    return this->m_backoff_strategy;
}

std::chrono::milliseconds RetryPolicy::calculateDelay(
    const std::uint32_t attempts_completed_in
) const noexcept
{
    if ( this->m_backoff_strategy == eRetryBackoffStrategy::Fixed
         || this->m_initial_delay.count() == 0 )
    {
        return this->m_initial_delay;
    }

    std::int64_t delay_ms = this->m_initial_delay.count();
    const std::int64_t maximum_delay_ms = this->m_maximum_delay.count();

    // The first failed attempt uses the initial delay. Later attempts double it without overflow.
    for ( std::uint32_t attempt = 1U;
          attempt < attempts_completed_in && delay_ms < maximum_delay_ms;
          ++attempt )
    {
        if ( delay_ms > maximum_delay_ms / 2 )
        {
            delay_ms = maximum_delay_ms;
            break;
        }

        delay_ms *= 2;
    }

    return std::chrono::milliseconds( delay_ms );
}

RetryDecision RetryPolicy::createStopDecision( const eRetryStopReason reason_in ) noexcept
{
    return RetryDecision( false, std::chrono::milliseconds( 0 ), reason_in );
}

RetryDecision RetryPolicy::evaluate(
      const std::uint32_t             attempts_completed_in
    , const eRetryOperationSafety     operation_safety_in
    , const eRetryFailureDisposition failure_disposition_in
    , const TransportError&           error_in
) const noexcept
{
    // Reject invalid policy state and attempt numbering before examining the operation result.
    if ( !this->isValid() )
    {
        return createStopDecision( eRetryStopReason::InvalidPolicy );
    }

    if ( attempts_completed_in == 0U )
    {
        return createStopDecision( eRetryStopReason::InvalidAttempt );
    }

    // A successful or cancelled operation is terminal regardless of caller classification.
    if ( error_in.ok() )
    {
        return createStopDecision( eRetryStopReason::NoFailure );
    }

    if ( error_in.category() == eTransportErrorCategory::Cancellation
         || error_in.code() == eTransportErrorCode::Cancelled )
    {
        return createStopDecision( eRetryStopReason::Cancelled );
    }

    // Require explicit idempotency and transient-failure classification, then apply the hard category denylist.
    if ( !isRetryableSafety( operation_safety_in ) )
    {
        return createStopDecision( eRetryStopReason::NonIdempotentOperation );
    }

    if ( !isRetryableDisposition( failure_disposition_in )
         || !isPolicyRetryableError( error_in ) )
    {
        return createStopDecision( eRetryStopReason::FailureNotRetryable );
    }

    // The configured limit counts the initial attempt.
    if ( attempts_completed_in >= this->m_maximum_attempts )
    {
        return createStopDecision( eRetryStopReason::AttemptLimitReached );
    }

    return RetryDecision(
          true
        , this->calculateDelay( attempts_completed_in )
        , eRetryStopReason::None
    );
}

} // namespace xpt
} // namespace wse
