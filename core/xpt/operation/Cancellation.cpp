//*****************************************************************************************************************
//!
//! @file    Cancellation.cpp
//! @brief   \~japanese XPT Operation用の協調的Cancellationを実装する.
//! @brief   \~english  Implements cooperative cancellation for XPT operations.
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

#include "xpt/operation/Cancellation.h"

namespace wse
{
namespace xpt
{

CancellationToken::CancellationToken() noexcept
    : m_state ()
{
}

CancellationToken::CancellationToken( const std::shared_ptr<std::atomic_bool>& state_in ) noexcept
    : m_state ( state_in )
{
}

bool CancellationToken::isCancellationRequested() const noexcept
{
    return this->m_state && this->m_state->load( std::memory_order_acquire );
}

CancellationSource::CancellationSource()
    : m_state ( std::make_shared<std::atomic_bool>( false ) )
{
}

CancellationToken CancellationSource::token() const noexcept
{
    return CancellationToken( this->m_state );
}

void CancellationSource::cancel() noexcept
{
    if ( this->m_state )
    {
        this->m_state->store( true, std::memory_order_release );
    }
}

bool CancellationSource::isCancellationRequested() const noexcept
{
    return this->m_state && this->m_state->load( std::memory_order_acquire );
}

} // namespace xpt
} // namespace wse
