//*****************************************************************************************************************
//!
//! @file    Cancellation.cpp
//! @brief   \~japanese Binding共通の協調Cancellationの実装.
//! @brief   \~english  Implements binding-wide cooperative cancellation.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/Cancellation.h>

namespace wse
{
namespace binding
{

CancellationToken::CancellationToken(
    const std::shared_ptr<std::atomic_bool>& state_in ) noexcept
    : m_state ( state_in )
{
}

CancellationToken::CancellationToken() noexcept
    : m_state ()
{
}

bool CancellationToken::isCancellationRequested() const noexcept
{
    return this->m_state
        && this->m_state->load( std::memory_order_acquire );
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
    this->m_state->store( true, std::memory_order_release );
}

bool CancellationSource::isCancellationRequested() const noexcept
{
    return this->m_state->load( std::memory_order_acquire );
}

} // namespace binding
} // namespace wse
