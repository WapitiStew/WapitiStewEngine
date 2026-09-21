//*****************************************************************************************************************
//!
//! @file    OperationContext.h
//! @brief   \~japanese Blocking XPT Operation用の明示TimeoutとCancellationを定義する.
//! @brief   \~english  Defines explicit timeout and cancellation inputs for blocking XPT operations.
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

#ifndef WONDERSTEWENGINE_XPT_OPERATION_OPERATIONCONTEXT_H
#define WONDERSTEWENGINE_XPT_OPERATION_OPERATIONCONTEXT_H

#include "Cancellation.h"
#include "../../dynamic.h"

#include <chrono>
#include <cstdint>

namespace wse
{
namespace xpt
{

//! \~japanese `std::chrono::steady_clock`で測定する有限Duration.
//! \~english  Finite duration measured against `std::chrono::steady_clock`.
class WSE_API Timeout final
{
  private:
    std::chrono::milliseconds m_duration;

  public:
    //! @param [in] duration_in Operation上限. 負値は不正.
    explicit Timeout( const std::chrono::milliseconds duration_in ) noexcept;

    //! @param [in] duration_in Operation上限 [ms]. 負値は不正.
    //! @return 指定値を保持するTimeout.
    static Timeout milliseconds( const std::int64_t duration_in ) noexcept;

    //! @return \~japanese 保持するDuration. \~english The stored duration.
    std::chrono::milliseconds duration() const noexcept;

    //! @return \~japanese DurationがZero以上の場合true. \~english True when the duration is non-negative.
    bool isValid() const noexcept;
};

//! \~japanese 必須Operation制御. 意図しない無限待機を避けるためDefault Timeoutを持たない.
//! \~english  Required operation controls. There is deliberately no default timeout.
class WSE_API OperationContext final
{
  private:
    Timeout m_timeout;
    CancellationToken m_cancellation;

  public:
    //! @param [in] timeout_in 明示Operation上限.
    explicit OperationContext( const Timeout& timeout_in ) noexcept;

    //! @param [in] timeout_in 明示Operation上限.
    //! @param [in] cancellation_in 協調的Cancellation Token.
    OperationContext( const Timeout& timeout_in, const CancellationToken& cancellation_in ) noexcept;

    //! @return \~japanese 保持するTimeout. \~english The stored timeout.
    const Timeout &timeout() const noexcept;

    //! @return \~japanese 保持するCancellation Token. \~english The stored cancellation token.
    const CancellationToken &cancellation() const noexcept;

    //! @return \~japanese Timeoutが有効な場合true. \~english True when the timeout is valid.
    bool isValid() const noexcept;
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_OPERATION_OPERATIONCONTEXT_H
