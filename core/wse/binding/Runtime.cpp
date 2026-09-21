//*****************************************************************************************************************
//!
//! @file    Runtime.cpp
//! @brief   \~japanese 言語BindingのNative facadeの実装.
//! @brief   \~english  Implements the language-binding native facade.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/Runtime.h>

#include <algorithm>
#include <new>
#include <thread>

#ifndef WSE_BINDING_RUNTIME_VERSION
#define WSE_BINDING_RUNTIME_VERSION "0.0.0"
#endif

namespace wse
{
namespace binding
{

sRuntimeInfo Runtime::info() const
{
    sRuntimeInfo result = {};
    result.version = WSE_BINDING_RUNTIME_VERSION;
    result.binding_abi_version = BINDING_ABI_VERSION;
#if defined( WSE_HAS_XPT )
    result.has_xpt = true;
#endif
#if defined( WSE_HAS_TMR )
    result.has_tmr = true;
#endif
#if defined( WSE_HAS_OUI )
    result.has_oui = true;
#endif
#if defined( WSE_HAS_GEF )
    result.has_gef = true;
#endif
#if defined( WSE_HAS_IUI )
    result.has_iui = true;
#endif
#if defined( WSE_HAS_VPJ )
    result.has_vpj = true;
#endif
    return result;
}

Result<FrameBuffer> Runtime::copyFrame(
    const std::vector<std::uint8_t>& bytes_in ) const
{
    try
    {
        return Result<FrameBuffer>::success( FrameBuffer( bytes_in ) );
    }
    catch ( const std::bad_alloc& )
    {
        return Result<FrameBuffer>::failure(
            Error(
                  eErrorCategory::ResourceExhausted
                , 1
                , "Unable to allocate the binding frame buffer."
            )
        );
    }
}

// Sleeps in slices of at most 5ms instead of one long sleep, so a cancellation is honored within
// that latency. Bindings use this as the canonical cancellable blocking primitive; the final
// check catches a cancellation that arrived during the last slice.
Status Runtime::wait(
      const std::chrono::milliseconds duration_in
    , const CancellationToken& cancellation_in
) const
{
    if ( duration_in.count() < 0 )
    {
        return Status::failure(
            Error(
                  eErrorCategory::InvalidArgument
                , 1
                , "Wait duration must not be negative."
            )
        );
    }

    const auto deadline = std::chrono::steady_clock::now() + duration_in;
    while ( std::chrono::steady_clock::now() < deadline )
    {
        if ( cancellation_in.isCancellationRequested() )
        {
            return Status::failure(
                Error(
                      eErrorCategory::Cancellation
                    , 1
                    , "The binding operation was cancelled."
                )
            );
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now() );
        std::this_thread::sleep_for( std::min( remaining, std::chrono::milliseconds( 5 ) ) );
    }

    if ( cancellation_in.isCancellationRequested() )
    {
        return Status::failure(
            Error(
                  eErrorCategory::Cancellation
                , 1
                , "The binding operation was cancelled."
            )
        );
    }
    return Status::success();
}

} // namespace binding
} // namespace wse
