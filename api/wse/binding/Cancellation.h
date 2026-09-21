//*****************************************************************************************************************
//!
//! @file    Cancellation.h
//! @brief   \~japanese Binding共通の協調的Cancellation契約を定義する.
//! @brief   \~english  Defines cooperative cancellation shared by language bindings.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_CANCELLATION_H
#define WONDERSTEWENGINE_WSE_BINDING_CANCELLATION_H

#include "../../dynamic.h"

#include <atomic>
#include <memory>

namespace wse
{
namespace binding
{

class CancellationSource;

//! \~japanese Copy可能なCancellation観測Handle.
//! \~english  Copyable cancellation observation handle.
class WSE_API CancellationToken final
{
  private:
    std::shared_ptr<std::atomic_bool> m_state;

    explicit CancellationToken( const std::shared_ptr<std::atomic_bool>& state_in ) noexcept;
    friend class CancellationSource;

  public:
    CancellationToken() noexcept;
    bool isCancellationRequested() const noexcept;
};

//! \~japanese Node.js、Python、JavaのCancellation操作が共有するOwner.
//! \~english  Owner shared by Node.js, Python, and Java cancellation adapters.
class WSE_API CancellationSource final
{
  private:
    std::shared_ptr<std::atomic_bool> m_state;

  public:
    CancellationSource();
    CancellationSource( const CancellationSource& ) = delete;
    CancellationSource& operator=( const CancellationSource& ) = delete;
    CancellationSource( CancellationSource&& ) noexcept = default;
    CancellationSource& operator=( CancellationSource&& ) noexcept = default;

    CancellationToken token() const noexcept;
    void cancel() noexcept;
    bool isCancellationRequested() const noexcept;
};

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_CANCELLATION_H
