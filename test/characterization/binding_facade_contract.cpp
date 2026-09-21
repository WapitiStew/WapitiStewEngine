//*****************************************************************************************************************
//!
//! @file    binding_facade_contract.cpp
//! @brief   Verifies the language-neutral native binding facade contract.
//!
//! @details
//!   Every language adapter (C#, Java, JavaScript, Python) is written against this one facade
//!   rather than against each component's own error type, so what is pinned here is what those
//!   adapters are allowed to assume. A failure means a managed caller could observe a different
//!   engine depending on which language it called from: a binding ABI version that no longer
//!   matches the number the adapters compile against, a frame that still points at caller memory
//!   a garbage collector is free to move or free, a cancelled wait that surfaces as a timeout or
//!   as a success, or an XPT/OUI/Tmr failure that lands in a category the adapter does not map to
//!   the exception the user is told to catch.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/stew.h>

#if defined( WSE_HAS_XPT )
#include <xpt/error/TransportError.h>
#endif
#if defined( WSE_HAS_OUI )
#include <oui/renderer/RendererError.h>
#endif
#if defined( WSE_HAS_TMR )
#include <tmr/camera/CameraError.h>
#endif

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace
{

int failures = 0;

// Records a failure and keeps going rather than aborting, so one run reports every broken clause
// instead of only the first. The count becomes the process exit code.
void expect( const bool condition_in, const char* const message_in )
{
    if ( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    using namespace std::chrono_literals;

    // The two numbers a managed adapter reads before it trusts the library it just loaded. The
    // version string is stamped in from WSE_BINDING_RUNTIME_VERSION at build time, so an empty one
    // means the define was lost rather than that the value is wrong. The literal 1 is the ABI
    // number all four adapters are compiled against; changing it is a deliberate break of every
    // binding, and the literal is here so it cannot happen quietly.
    const wse::binding::Runtime runtime;
    const wse::binding::sRuntimeInfo info = runtime.info();
    expect( !info.version.empty(), "Runtime version is stable and non-empty" );
    expect( info.binding_abi_version == 1U, "Binding ABI version starts at one" );

    // The ownership rule at the binding boundary: the runtime must copy, never alias. Overwriting
    // the caller's byte after the call is the whole point of the block, because a managed heap may
    // move or free that buffer the moment the call returns; reading 1 rather than 99 back proves
    // the frame is not a view onto it.
    std::vector<std::uint8_t> source = { 0U, 1U, 127U, 255U };
    const auto frame_result = runtime.copyFrame( source );
    source[ 1 ] = 99U;
    expect( frame_result.succeeded(), "Frame copy succeeds" );
    expect( frame_result.value().size() == 4U
                && frame_result.value().bytes()[ 1 ] == 1U,
            "Frame owns a copy and does not retain caller memory" );

    // A negative duration is rejected at the boundary instead of being clamped or wrapped into a
    // huge unsigned wait, and it is reported through the portable category rather than by throwing,
    // because an exception cannot cross into a C ABI adapter.
    const auto invalid_wait = runtime.wait( -1ms );
    expect( !invalid_wait.succeeded()
                && invalid_wait.error().category()
                    == wse::binding::eErrorCategory::InvalidArgument,
            "Negative wait maps to InvalidArgument" );

    // The canceller thread stands in for whatever cancels from outside the waiting thread in a
    // managed adapter: a .NET CancellationTokenSource, a JavaScript AbortController, a Python
    // KeyboardInterrupt handler. It deliberately does not simulate cancellation arriving while the
    // native side sits inside a blocking device or socket call; Runtime::wait only polls the flag
    // between short sleeps, so this covers the cooperative path alone.
    // The 20 ms sleep puts the cancel well inside the 1000 ms wait rather than racing its start.
    wse::binding::CancellationSource cancellation;
    const wse::binding::CancellationToken token = cancellation.token();
    std::thread canceller( [&cancellation]()
    {
        std::this_thread::sleep_for( 20ms );
        cancellation.cancel();
    } );
    const auto started = std::chrono::steady_clock::now();
    const auto cancelled_wait = runtime.wait( 1000ms, token );
    const auto elapsed = std::chrono::steady_clock::now() - started;
    canceller.join();
    expect( !cancelled_wait.succeeded()
                && cancelled_wait.error().category()
                    == wse::binding::eErrorCategory::Cancellation,
            "Cancellation maps to the shared category" );
    // Nominal latency is the 20 ms sleep plus at most one 5 ms poll of the flag. 250 ms is slack
    // for a loaded scheduler and still an order of magnitude below the 1000 ms the wait would take
    // if the token were ignored, which is the failure this bound exists to separate. Raising it
    // toward 1000 ms would stop it telling those two apart.
    expect( elapsed < 250ms, "Binding cancellation latency is bounded" );

    // A wait with the default, never-cancelled token still runs to its deadline and reports
    // success, so the cancellation path above cannot be passing by refusing every wait.
    expect( runtime.wait( 1ms ).succeeded(), "Finite wait completes" );

    // The three blocks below pin the one-way adapters that PublicApiPolicy names as the only
    // sanctioned conversion into the binding error contract; a language adapter that reimplemented
    // the category switch would drift from these. Each is compiled out when its component is not
    // in the build, so a minimal configuration leaves that mapping uncovered.

    // A component category that keeps its name across the boundary. The stable code survives as
    // code(), and the fixture's 123 stands for the driver or OS number that has no portable
    // meaning: it must ride along in nativeCode() for diagnosis rather than be flattened away.
#if defined( WSE_HAS_XPT )
    const wse::xpt::TransportError transport_error(
          wse::xpt::eTransportErrorCategory::Timeout
        , wse::xpt::eTransportErrorCode::TimedOut
        , "fixture timeout"
        , 123
    );
    const wse::binding::Error binding_error = wse::binding::fromXptError( transport_error );
    expect( binding_error.category() == wse::binding::eErrorCategory::Timeout,
            "XPT timeout maps to the shared Timeout category" );
    expect( binding_error.code()
                == static_cast<std::int32_t>( wse::xpt::eTransportErrorCode::TimedOut )
                && binding_error.nativeCode() == 123,
            "XPT stable and diagnostic codes are preserved" );
#endif

    // The renderer reaches the same shared Timeout category from its own enumeration, so a caller
    // can retry on a timeout without knowing which component produced it.
#if defined( WSE_HAS_OUI )
    const wse::oui::RendererError renderer_error(
          wse::oui::eRendererErrorCategory::Timeout
        , wse::oui::eRendererErrorCode::TimedOut
        , "fixture timeout"
        , 456 );
    const wse::binding::Error renderer_binding_error =
        wse::binding::fromOuiError( renderer_error );
    expect( renderer_binding_error.category() == wse::binding::eErrorCategory::Timeout
            && renderer_binding_error.nativeCode() == 456,
        "OUI errors use the canonical binding adapter" );
#endif

    // The camera mapping is the one that renames rather than passes through: the camera's Device
    // category becomes the shared NotFound, which is what makes "the webcam was unplugged" reach a
    // managed caller as a missing-device exception instead of a generic internal error.
#if defined( WSE_HAS_TMR )
    const wse::tmr::CameraError camera_error(
          wse::tmr::eCameraErrorCategory::Device
        , wse::tmr::eCameraErrorCode::DeviceNotFound
        , "fixture device"
        , 789 );
    const wse::binding::Error camera_binding_error =
        wse::binding::fromTmrError( camera_error );
    expect( camera_binding_error.category() == wse::binding::eErrorCategory::NotFound
            && camera_binding_error.nativeCode() == 789,
        "Tmr errors use the canonical binding adapter" );
#endif

    return failures == 0 ? 0 : 1;
}
