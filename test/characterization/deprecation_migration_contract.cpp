// @file deprecation_migration_contract.cpp
// @brief Compiles canonical replacements with deprecation warnings promoted to errors.
// @details There is nothing to assert at run time here: the build is the assertion. CMake compiles
//          this file with /we4996 on MSVC and -Werror=deprecated-declarations elsewhere, so every
//          call below fails the build the moment its spelling acquires a [[deprecated]] attribute
//          or stops existing. Each call is the "after" side of a row in doc/en/DeprecationMigration.md.
//          What breaks if this test fails is the migration advice itself: the guide would be
//          telling a consumer to move onto a surface that now warns, or that has been removed under
//          them, and the shared/static installed-package consumers that compile with deprecation
//          warnings as errors would stop building.

#include <wse/stew.h>

#include <chrono>

#if defined( WSE_HAS_XPT )
#include <xpt/stew.h>
#endif
#if defined( WSE_HAS_IUI )
#include <iui/stew.h>
#endif
#if defined( WSE_HAS_OUI )
#include <oui/stew.h>
#endif
#if defined( WSE_HAS_TMR )
#include <tmr/stew.h>
#endif

namespace
{

// Stands for the Core timer surface: the composition-based lifecycle that replaced the
// inherited worker API - start( interval, callback ), setInterval(), and stop(). The timer
// arrives as a reference the caller owns, and this function is only address-taken, so no timer
// ever runs here; the intervals only have to type-check.
void compileCoreMigration( wse::Timer& timer_in )
{
    (void)timer_in.start( std::chrono::milliseconds( 10 ), []() {} );
    timer_in.setInterval( std::chrono::milliseconds( 20 ) );
    timer_in.stop();
}

// The checked Map surface that replaced the shape-unsafe accessors: elements() for the mutable
// vector() (value access without shape access), front() for the reference-returning begin(), and
// at()/row() as the bounds-checked reads the guide recommends over unchecked operator[].
void compileMapMigration( wse::F32_MAP& map_in )
{
    (void)map_in.elements();
    (void)map_in.front();
    (void)map_in.at( 0, 0 );
    (void)map_in.row( 0 );
}

// nativeCode() is the canonical spelling that replaced native_code(); the policy names it as the
// one accessor, so a return of the snake-case name would be caught here.
#if defined( WSE_HAS_XPT )
void compileXptMigration( wse::xpt::TransportError& error_in )
{
    (void)error_in.nativeCode();
}
#endif

// The two ways to read keyboard state that replaced ref_*_state(). Those returned references into
// arrays the input worker keeps writing to; these hand back a synchronized snapshot and an owned
// array instead, which is the whole point of the migration.
#if defined( WSE_HAS_IUI )
void compileIuiMigration( wse::iui::Keyboard& keyboard_in )
{
    (void)keyboard_in.snapshot();
    (void)keyboard_in.ascii_state();
}
#endif

// The portable Renderer entry points that replaced the five removed legacy classes:
// getCapabilities() for InterfaceGPU::target_desc(), enumerateDisplays() for OS_Display, and
// nativeCode() for RendererError::native_code().
#if defined( WSE_HAS_OUI )
void compileOuiMigration( wse::oui::Renderer& renderer_in, wse::oui::RendererError& error_in )
{
    (void)renderer_in.getCapabilities();
    (void)renderer_in.enumerateDisplays();
    (void)error_in.nativeCode();
}
#endif

// The whole replacement lifecycle for the 42 historical WebCamera methods, in the order a caller
// uses it. 100U is a per-frame timeout in milliseconds and only has to type-check, since this
// function is never invoked: no camera is opened and no frame is read anywhere in this file.
#if defined( WSE_HAS_TMR )
void compileTmrMigration(
      wse::tmr::WebCamera& camera_in
    , const wse::tmr::sCameraDeviceInfo& device_in
    , const wse::tmr::sCameraStreamConfiguration& configuration_in )
{
    (void)camera_in.open( device_in, configuration_in );
    (void)camera_in.start( []( const wse::tmr::CameraResult< wse::tmr::sCameraFrame >& ) {} );
    (void)camera_in.readFrame( 100U );
    (void)camera_in.stop();
    (void)camera_in.close();
}
#endif

} // namespace

int main()
{
    // Nothing below checks a value, and the return is unconditionally zero. Every helper above is
    // compiled whether or not it is reached, so the deprecation gate has already fired before main
    // runs; what is left here is only keeping the helpers referenced.
    // The two that are actually called are handed default-constructed objects, which incidentally
    // pins that those error and renderer types stay default constructible. The Renderer is never
    // initialize()d, so getCapabilities() and enumerateDisplays() return the not-initialized error
    // immediately and no GPU or display is touched by this test.
    // The rest are only address-taken, which keeps an unused-function warning quiet without
    // bringing a keyboard, a camera, or a worker thread into a compile-only check.
#if defined( WSE_HAS_XPT )
    wse::xpt::TransportError transport_error;
    compileXptMigration( transport_error );
#endif
#if defined( WSE_HAS_OUI )
    wse::oui::Renderer renderer;
    wse::oui::RendererError renderer_error;
    compileOuiMigration( renderer, renderer_error );
#endif
    (void)&compileCoreMigration;
    (void)&compileMapMigration;
#if defined( WSE_HAS_IUI )
    (void)&compileIuiMigration;
#endif
#if defined( WSE_HAS_TMR )
    (void)&compileTmrMigration;
#endif
    return 0;
}
