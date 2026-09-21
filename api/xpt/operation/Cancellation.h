//*****************************************************************************************************************
//!
//! @file    Cancellation.h
//! @brief   \~japanese XPT Operation間で共有する協調的Cancellationを定義する.
//! @brief   \~english  Defines cooperative cancellation shared by XPT operations.
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

#ifndef WONDERSTEWENGINE_XPT_OPERATION_CANCELLATION_H
#define WONDERSTEWENGINE_XPT_OPERATION_CANCELLATION_H

#include "../../dynamic.h"

#include <atomic>
#include <memory>

namespace wse
{
namespace xpt
{

class CancellationSource;

//! \~japanese Copy可能なCancellation観測Handle. Default TokenはCancelされない.
//! \~english  Copyable observation handle. A default token is never cancelled.
class WSE_API CancellationToken final
{
  private:
    std::shared_ptr<std::atomic_bool> m_state;

    explicit CancellationToken( const std::shared_ptr<std::atomic_bool>& state_in ) noexcept;
    friend class CancellationSource;

  public:
    //! \~japanese CancelされないTokenを生成する. \~english Creates a token that is never cancelled.
    CancellationToken() noexcept;

    //! @return \~japanese SourceがCancel済みの場合true. \~english True after its source requests cancellation.
    bool isCancellationRequested() const noexcept;
};

//! \~japanese Copyされた全TokenへThread-safeにCancellationを要求するOwner.
//! \~english  Thread-safe owner used to request cancellation of all copied tokens.
class WSE_API CancellationSource final
{
  private:
    std::shared_ptr<std::atomic_bool> m_state;

  public:
    //! \~japanese 未Cancel状態のSourceを生成する. \~english Creates a source in the non-cancelled state.
    CancellationSource();
    CancellationSource( const CancellationSource & ) = delete;
    CancellationSource &operator=( const CancellationSource & ) = delete;
    CancellationSource( CancellationSource && ) noexcept = default;
    CancellationSource &operator=( CancellationSource && ) noexcept = default;
    ~CancellationSource() = default;

    //! @return \~japanese このSourceと状態を共有するToken. \~english A token sharing this source's state.
    CancellationToken token() const noexcept;

    //! \~japanese 共有TokenへCancellationを一度だけ要求する. \~english Requests cancellation for all shared tokens.
    void cancel() noexcept;

    //! @return \~japanese Cancel要求済みの場合true. \~english True after cancellation is requested.
    bool isCancellationRequested() const noexcept;
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_OPERATION_CANCELLATION_H
