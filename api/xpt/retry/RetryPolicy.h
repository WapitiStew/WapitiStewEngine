//*****************************************************************************************************************
//!
//! @file    RetryPolicy.h
//! @brief   \~japanese 明示的で安全側のXPT Retry Policyを定義する.
//! @brief   \~english  Defines an explicit, fail-safe XPT retry policy.
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

#pragma once

#ifndef WONDERSTEWENGINE_XPT_RETRY_RETRYPOLICY_H
#define WONDERSTEWENGINE_XPT_RETRY_RETRYPOLICY_H

#include "../error/TransportError.h"
#include "../../dynamic.h"

#include <chrono>
#include <cstdint>

namespace wse
{
namespace xpt
{

//! \~japanese 呼出側が表明するOperationの再実行安全性. \~english Safety asserted by the caller for re-execution.
enum class eRetryOperationSafety : std::uint8_t
{
      NonIdempotent = 0  //!< 再実行で副作用が重複し得る.
    , Idempotent          //!< 同じ意味のOperationを安全に再実行できる.
};

//! \~japanese 呼出側がErrorを分類したRetry可否. \~english Caller classification of whether a failure is retryable.
enum class eRetryFailureDisposition : std::uint8_t
{
      DoNotRetry = 0  //!< この失敗は再試行しない.
    , Retryable       //!< この失敗は一時的で再試行可能と判断した.
};

//! \~japanese Retry間隔の計算方式. \~english Backoff calculation strategy.
enum class eRetryBackoffStrategy : std::uint8_t
{
      Fixed = 0   //!< 常に初期間隔を使用する.
    , Exponential //!< 初期間隔を2倍し、最大間隔で制限する.
};

//! \~japanese Retryしない理由. `None`はRetryを許可したDecisionだけで使用する.
//! \~english  Reason retry stopped. `None` is used only for a decision that permits retry.
enum class eRetryStopReason : std::uint8_t
{
      None = 0
    , InvalidPolicy
    , InvalidAttempt
    , NoFailure
    , NonIdempotentOperation
    , FailureNotRetryable
    , AttemptLimitReached
    , Cancelled
};

class RetryPolicy;

//! \~japanese 1回の失敗後にRetryするかと待機間隔を保持する値.
//! \~english  Value describing whether to retry after one failure and how long to wait.
class WSE_API RetryDecision final
{
  private:
    bool m_should_retry;
    std::chrono::milliseconds m_delay;
    eRetryStopReason m_stop_reason;

    RetryDecision(
          const bool                      should_retry_in
        , const std::chrono::milliseconds delay_in
        , const eRetryStopReason          stop_reason_in
    ) noexcept;
    friend class RetryPolicy;

  public:
    //! @return \~japanese Retryを許可する場合true. \~english True when another attempt is permitted.
    bool shouldRetry() const noexcept;

    //! @return \~japanese 次の試行前の待機時間. Stop時はZero. \~english Delay before the next attempt, or zero when stopped.
    std::chrono::milliseconds delay() const noexcept;

    //! @return \~japanese Stop理由. Retry時は`None`. \~english Stop reason, or `None` when retrying.
    eRetryStopReason stop_reason() const noexcept;
};

//! \~japanese 回数とBackoffだけを決定し、Operationを自動実行しない明示Retry Policy.
//! \~english  Explicit retry policy that decides attempt count and backoff without executing operations.
class WSE_API RetryPolicy final
{
  private:
    std::uint32_t m_maximum_attempts;
    std::chrono::milliseconds m_initial_delay;
    std::chrono::milliseconds m_maximum_delay;
    eRetryBackoffStrategy m_backoff_strategy;

    std::chrono::milliseconds calculateDelay( const std::uint32_t attempts_completed_in ) const noexcept;
    static RetryDecision createStopDecision( const eRetryStopReason reason_in ) noexcept;

  public:
    //! \~japanese Retry無効の有効なPolicyを生成する. 最大試行回数は初回だけの1回.
    //! \~english  Creates a valid no-retry policy whose only permitted attempt is the initial one.
    RetryPolicy() noexcept;

    //! \~japanese Retry Policy値を生成する. 不正値は保持され、`isValid()`がfalseを返す.
    //! \~english  Creates a retry policy. Invalid values are retained and reported by `isValid()`.
    //! @param [in] maximum_attempts_in 初回を含む最大試行回数. 1以上.
    //! @param [in] initial_delay_in 最初のRetry前の待機時間. Zero以上.
    //! @param [in] maximum_delay_in Backoff上限. `initial_delay_in`以上.
    //! @param [in] backoff_strategy_in FixedまたはExponential.
    RetryPolicy(
          const std::uint32_t              maximum_attempts_in
        , const std::chrono::milliseconds initial_delay_in
        , const std::chrono::milliseconds maximum_delay_in
        , const eRetryBackoffStrategy      backoff_strategy_in
    ) noexcept;

    //! \~japanese 固定間隔Policyを生成する. \~english Creates a fixed-delay policy.
    //! @param [in] maximum_attempts_in 初回を含む最大試行回数.
    //! @param [in] delay_in Retry間の固定待機時間.
    //! @return 指定値を保持するPolicy.
    static RetryPolicy createFixed(
          const std::uint32_t              maximum_attempts_in
        , const std::chrono::milliseconds delay_in
    ) noexcept;

    //! \~japanese 2倍指数Backoff Policyを生成する. \~english Creates a doubling exponential-backoff policy.
    //! @param [in] maximum_attempts_in 初回を含む最大試行回数.
    //! @param [in] initial_delay_in 最初のRetry前の待機時間.
    //! @param [in] maximum_delay_in Backoff上限.
    //! @return 指定値を保持するPolicy.
    static RetryPolicy createExponential(
          const std::uint32_t              maximum_attempts_in
        , const std::chrono::milliseconds initial_delay_in
        , const std::chrono::milliseconds maximum_delay_in
    ) noexcept;

    //! @return \~japanese Policy値が有効な場合true. \~english True when all policy values are valid.
    bool isValid() const noexcept;

    //! @return \~japanese 初回を含む最大試行回数. \~english Maximum attempts including the initial attempt.
    std::uint32_t maximum_attempts() const noexcept;

    //! @return \~japanese 最初のRetry前の待機時間. \~english Delay before the first retry.
    std::chrono::milliseconds initial_delay() const noexcept;

    //! @return \~japanese Backoff上限. \~english Backoff upper bound.
    std::chrono::milliseconds maximum_delay() const noexcept;

    //! @return \~japanese Backoff方式. \~english Backoff strategy.
    eRetryBackoffStrategy backoff_strategy() const noexcept;

    //! \~japanese 失敗した試行の後にRetry可否と待機時間を決定する.
    //! \~english  Decides whether and when to retry after a failed attempt.
    //! @param [in] attempts_completed_in 完了済み試行数. 最初の失敗後は1.
    //! @param [in] operation_safety_in 呼出側が表明するOperationの冪等性.
    //! @param [in] failure_disposition_in 呼出側が明示するFailure分類.
    //! @param [in] error_in 直前の試行が返したStructured Error.
    //! @return Retry Decision. 本Methodは待機もOperation再実行も行わない.
    RetryDecision evaluate(
          const std::uint32_t              attempts_completed_in
        , const eRetryOperationSafety      operation_safety_in
        , const eRetryFailureDisposition  failure_disposition_in
        , const TransportError&            error_in
    ) const noexcept;
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_RETRY_RETRYPOLICY_H
