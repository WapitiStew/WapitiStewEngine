//*****************************************************************************************************************
//!
//! @file    OperationContext.cpp
//! @brief   \~japanese XPT OperationのTimeout／Cancellation Contextを実装する.
//! @brief   \~english  Implements timeout and cancellation context for XPT operations.
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

#include "xpt/operation/OperationContext.h"

namespace wse
{
namespace xpt
{

Timeout::Timeout( const std::chrono::milliseconds duration_in ) noexcept
    : m_duration ( duration_in )
{
}

Timeout Timeout::milliseconds( const std::int64_t duration_in ) noexcept
{
    return Timeout( std::chrono::milliseconds( duration_in ) );
}

std::chrono::milliseconds Timeout::duration() const noexcept
{
    return this->m_duration;
}

bool Timeout::isValid() const noexcept
{
    return this->m_duration.count() >= 0;
}

OperationContext::OperationContext( const Timeout& timeout_in ) noexcept
    : m_timeout      ( timeout_in )
    , m_cancellation ()
{
}

OperationContext::OperationContext(
      const Timeout&           timeout_in
    , const CancellationToken& cancellation_in
) noexcept
    : m_timeout      ( timeout_in )
    , m_cancellation ( cancellation_in )
{
}

const Timeout &OperationContext::timeout() const noexcept
{
    return this->m_timeout;
}

const CancellationToken &OperationContext::cancellation() const noexcept
{
    return this->m_cancellation;
}

bool OperationContext::isValid() const noexcept
{
    return this->m_timeout.isValid();
}

} // namespace xpt
} // namespace wse
