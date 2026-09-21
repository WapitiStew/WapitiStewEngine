//*****************************************************************************************************************
//!
//! @file    oui_d3d12_window_resize_fullscreen.cpp
//! @brief   \~japanese Window Resize／Borderless Fullscreen／復元契約を検証する.
//! @brief   \~english  Verifies window resize, borderless fullscreen, and restoration contracts.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/renderer/Renderer.h>

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

constexpr std::uint32_t INITIAL_WIDTH  = 160U;
constexpr std::uint32_t INITIAL_HEIGHT = 120U;
constexpr std::uint32_t RESIZED_WIDTH  = 320U;
constexpr std::uint32_t RESIZED_HEIGHT = 180U;
constexpr std::uint32_t INTERACTIVE_WIDTH  = 400U;
constexpr std::uint32_t INTERACTIVE_HEIGHT = 240U;
constexpr std::uint32_t TIMEOUT_MS     = 30000U;

int g_failures = 0;

void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

bool sameHandle(
      const wse::oui::sTextureHandle left_in
    , const wse::oui::sTextureHandle right_in
) noexcept
{
    return left_in.value == right_in.value && left_in.generation == right_in.generation;
}

bool sameMode(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in
) noexcept
{
    return left_in.extent.width == right_in.extent.width &&
        left_in.extent.height == right_in.extent.height &&
        left_in.refresh_rate_numerator == right_in.refresh_rate_numerator &&
        left_in.refresh_rate_denominator == right_in.refresh_rate_denominator &&
        left_in.format == right_in.format && left_in.interlaced == right_in.interlaced;
}

bool expectWindowState(
      wse::oui::Renderer* const       p_renderer_in
    , const wse::oui::sSurfaceHandle surface_in
    , const wse::oui::sRendererExtent2D expected_extent_in
    , const wse::oui::eSurfaceWindowMode expected_mode_in
    , const std::string&              expected_display_id_in
)
{
    const auto state_result = p_renderer_in->getSurfaceState( surface_in );
    if( !state_result.succeeded() )
    {
        fail( "Surface-state query failed: " + state_result.error().message() );
        return false;
    }
    const auto& state = state_result.value();
    if( !wse::oui::validateSurfaceState( state ).ok() ||
        state.type != wse::oui::eSurfaceType::Window ||
        state.extent.width != expected_extent_in.width ||
        state.extent.height != expected_extent_in.height ||
        state.window_mode != expected_mode_in || state.display_id != expected_display_id_in )
    {
        fail( "Surface state does not match the expected window transition: actual=" +
            std::to_string( state.extent.width ) + "x" +
            std::to_string( state.extent.height ) + ", expected=" +
            std::to_string( expected_extent_in.width ) + "x" +
            std::to_string( expected_extent_in.height ) + "." );
        return false;
    }
    return true;
}

bool resizeNativeClient(
      const HWND          window_in
    , const std::uint32_t width_in
    , const std::uint32_t height_in
)
{
    RECT rectangle = {
          0L
        , 0L
        , static_cast< LONG >( width_in )
        , static_cast< LONG >( height_in )
    };
    const DWORD style = static_cast< DWORD >( GetWindowLongPtrW( window_in, GWL_STYLE ) );
    const DWORD extended_style = static_cast< DWORD >(
        GetWindowLongPtrW( window_in, GWL_EXSTYLE ) );
    if( AdjustWindowRectEx( &rectangle, style, FALSE, extended_style ) == FALSE )
    {
        return false;
    }
    RECT current = {};
    return GetWindowRect( window_in, &current ) != FALSE &&
        SetWindowPos(
              window_in
            , nullptr
            , current.left
            , current.top
            , rectangle.right - rectangle.left
            , rectangle.bottom - rectangle.top
            , SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER ) != FALSE;
}

} // namespace

int main()
{
    // 非表示WindowとWARPを使い、Display modeを変更せずにWindow／Swap chainだけを検証する.
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend              = wse::oui::eRendererBackend::Direct3D12;
    configuration.use_software_adapter = true;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
    {
        fail( "Renderer initialization failed: " + initialize_result.error().message() );
        return 1;
    }
    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        !capabilities_result.value().supports_surface_resize ||
        !capabilities_result.value().supports_borderless_fullscreen ||
        !capabilities_result.value().supports_display_mode_fullscreen ||
        !capabilities_result.value().supports_direct_display ||
        !capabilities_result.value().supports_interactive_resize ||
        !capabilities_result.value().supports_display_hotplug )
    {
        fail( "D3D12 resize or borderless-fullscreen capability is unavailable." );
        return 1;
    }

    wse::oui::sSurfaceDescription offscreen_description;
    offscreen_description.extent = { 8U, 8U };
    const auto offscreen_result = renderer.createSurface( offscreen_description );
    if( !offscreen_result.succeeded() )
    {
        fail( "Offscreen control surface creation failed." );
        return 1;
    }
    const auto offscreen_state_result = renderer.getSurfaceState( offscreen_result.value() );
    if( !offscreen_state_result.succeeded() ||
        !wse::oui::validateSurfaceState( offscreen_state_result.value() ).ok() ||
        renderer.resizeSurface( offscreen_result.value(), { 10U, 10U } ).succeeded() ||
        renderer.setSurfaceWindowMode(
            offscreen_result.value(), wse::oui::sSurfaceWindowModeRequest{} ).succeeded() )
    {
        fail( "Non-window surface accepted a window-only transition." );
    }
    renderer.destroySurface( offscreen_result.value() );

    wse::oui::sSurfaceDescription surface_description;
    surface_description.type          = wse::oui::eSurfaceType::Window;
    surface_description.extent        = { INITIAL_WIDTH, INITIAL_HEIGHT };
    surface_description.format        = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    surface_description.buffer_count  = 2U;
    surface_description.vertical_sync = false;
    surface_description.visible       = false;
    // Separate SHARED/STATIC test processes must never discover each other's native window.
    const auto process_id = GetCurrentProcessId();
    surface_description.title         = "WSE resize and borderless test " + std::to_string(process_id);
    const auto surface_result = renderer.createSurface( surface_description );
    if( !surface_result.succeeded() )
    {
        fail( "Window surface creation failed: " + surface_result.error().message() );
        return 1;
    }
    if( !expectWindowState(
            &renderer, surface_result.value(), { INITIAL_WIDTH, INITIAL_HEIGHT },
            wse::oui::eSurfaceWindowMode::Windowed, {} ) )
    {
        return 1;
    }
    const auto initial_texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !initial_texture_result.succeeded() )
    {
        fail( "Initial window back-buffer lookup failed." );
        return 1;
    }

    const auto resize_result = renderer.resizeSurface(
        surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT } );
    if( !resize_result.succeeded() ||
        !expectWindowState(
            &renderer, surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT },
            wse::oui::eSurfaceWindowMode::Windowed, {} ) )
    {
        fail( "Windowed surface resize failed: " + resize_result.error().message() );
        return 1;
    }
    const auto resized_texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !resized_texture_result.succeeded() ||
        sameHandle( initial_texture_result.value(), resized_texture_result.value() ) ||
        renderer.readTexture( initial_texture_result.value(), 0U ).succeeded() )
    {
        fail( "Resize did not replace and invalidate the old backbuffer." );
        return 1;
    }
    if( !renderer.resizeSurface(
            surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT } ).succeeded() ||
        !sameHandle( resized_texture_result.value(),
            renderer.getSurfaceTexture( surface_result.value() ).value() ) )
    {
        fail( "Idempotent resize unexpectedly replaced the backbuffer." );
    }

    // 新しいBackbuffer extentを実描画とReadbackで検証する.
    wse::oui::sRenderPassDescription clear_pass;
    clear_pass.color_attachment = resized_texture_result.value();
    clear_pass.clear_color      = { 0.125F, 0.25F, 0.5F, 1.0F };
    clear_pass.final_state      = wse::oui::eTextureState::CopySource;
    const auto clear_result = renderer.executeRenderPass( clear_pass );
    if( !clear_result.succeeded() ||
        !renderer.waitFence( clear_result.value(), TIMEOUT_MS ).succeeded() )
    {
        fail( "Resized-window clear failed." );
        return 1;
    }
    const auto frame_result = renderer.readTexture( resized_texture_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() ||
        frame_result.value().description.extent.width != RESIZED_WIDTH ||
        frame_result.value().description.extent.height != RESIZED_HEIGHT ||
        frame_result.value().data.size() != RESIZED_WIDTH * RESIZED_HEIGHT * 4U )
    {
        fail( "Resized-window readback extent is invalid." );
        return 1;
    }

    wse::oui::sSurfaceWindowModeRequest invalid_fullscreen;
    invalid_fullscreen.mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
    invalid_fullscreen.display_id = "dxgi:not-active";
    if( renderer.setSurfaceWindowMode( surface_result.value(), invalid_fullscreen ).succeeded() ||
        !expectWindowState(
            &renderer, surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT },
            wse::oui::eSurfaceWindowMode::Windowed, {} ) ||
        !sameHandle( resized_texture_result.value(),
            renderer.getSurfaceTexture( surface_result.value() ).value() ) )
    {
        fail( "Invalid display request changed the window surface." );
    }

    const auto displays_result = renderer.enumerateDisplays();
    if( !displays_result.succeeded() )
    {
        fail( "Display enumeration failed before borderless transition." );
        return 1;
    }
    if( !displays_result.value().empty() )
    {
        const auto primary = std::find_if(
              displays_result.value().begin()
            , displays_result.value().end()
            , []( const wse::oui::sDisplayDescription& display_in )
              {
                  return display_in.primary;
              }
        );
        const auto& display = primary != displays_result.value().end()
            ? *primary : displays_result.value().front();

        wse::oui::sSurfaceWindowModeRequest display_mode_request;
        display_mode_request.mode = wse::oui::eSurfaceWindowMode::DisplayModeFullscreen;
        display_mode_request.display_id = display.id;
        display_mode_request.display_mode = display.current_mode;
        if( renderer.setSurfaceWindowMode(
                surface_result.value(), display_mode_request ).succeeded() ||
            !expectWindowState(
                &renderer, surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT },
                wse::oui::eSurfaceWindowMode::Windowed, {} ) ||
            !sameHandle( resized_texture_result.value(),
                renderer.getSurfaceTexture( surface_result.value() ).value() ) )
        {
            fail( "WARP accepted a display-mode transition on an incompatible output." );
        }

        wse::oui::sSurfaceDescription direct_description;
        direct_description.type = wse::oui::eSurfaceType::DirectDisplay;
        direct_description.extent = display.current_mode.extent;
        direct_description.format = display.current_mode.format;
        direct_description.buffer_count = 2U;
        direct_description.visible = true;
        direct_description.title = "WSE direct-display negative test";
        direct_description.display_id = display.id;
        direct_description.display_mode = display.current_mode;
        if( renderer.createSurface( direct_description ).succeeded() )
        {
            fail( "WARP created a direct surface for an incompatible display." );
        }

        wse::oui::sSurfaceWindowModeRequest borderless_request;
        borderless_request.mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
        borderless_request.display_id = display.id;
        if( !renderer.setSurfaceWindowMode(
                surface_result.value(), borderless_request ).succeeded() ||
            !expectWindowState(
                &renderer, surface_result.value(), display.desktop_extent,
                wse::oui::eSurfaceWindowMode::BorderlessFullscreen, display.id ) )
        {
            fail( "Borderless-fullscreen transition failed." );
            return 1;
        }
        const auto fullscreen_texture_result = renderer.getSurfaceTexture( surface_result.value() );
        if( !fullscreen_texture_result.succeeded() ||
            sameHandle( resized_texture_result.value(), fullscreen_texture_result.value() ) ||
            renderer.readTexture( resized_texture_result.value(), 0U ).succeeded() )
        {
            fail( "Borderless transition did not replace the backbuffer." );
            return 1;
        }
        if( !renderer.setSurfaceWindowMode(
                surface_result.value(), borderless_request ).succeeded() ||
            !sameHandle( fullscreen_texture_result.value(),
                renderer.getSurfaceTexture( surface_result.value() ).value() ) ||
            renderer.resizeSurface( surface_result.value(), { 32U, 24U } ).succeeded() )
        {
            fail( "Borderless idempotence or direct-resize guard failed." );
        }

        const wse::oui::sSurfaceWindowModeRequest windowed_request;
        if( !renderer.setSurfaceWindowMode(
                surface_result.value(), windowed_request ).succeeded() ||
            !expectWindowState(
                &renderer, surface_result.value(), { RESIZED_WIDTH, RESIZED_HEIGHT },
                wse::oui::eSurfaceWindowMode::Windowed, {} ) )
        {
            fail( "Windowed-state restoration failed." );
            return 1;
        }
        const auto restored_texture_result = renderer.getSurfaceTexture( surface_result.value() );
        if( !restored_texture_result.succeeded() ||
            sameHandle( fullscreen_texture_result.value(), restored_texture_result.value() ) ||
            renderer.readTexture( fullscreen_texture_result.value(), 0U ).succeeded() ||
            !renderer.setSurfaceWindowMode(
                surface_result.value(), windowed_request ).succeeded() ||
            !sameHandle( restored_texture_result.value(),
                renderer.getSurfaceTexture( surface_result.value() ).value() ) )
        {
            fail( "Restoration did not replace the backbuffer exactly once." );
        }

        const auto restored_displays_result = renderer.enumerateDisplays();
        const auto restored_display = restored_displays_result.succeeded()
            ? std::find_if(
                  restored_displays_result.value().begin()
                , restored_displays_result.value().end()
                , [&display]( const wse::oui::sDisplayDescription& candidate_in )
                  {
                      return candidate_in.id == display.id;
                  } )
            : restored_displays_result.value().end();
        if( !restored_displays_result.succeeded() ||
            restored_display == restored_displays_result.value().end() ||
            restored_display->position.x != display.position.x ||
            restored_display->position.y != display.position.y ||
            restored_display->desktop_extent.width != display.desktop_extent.width ||
            restored_display->desktop_extent.height != display.desktop_extent.height ||
            !sameMode( restored_display->current_mode, display.current_mode ) )
        {
            fail( "Borderless transition changed the operating-system display configuration." );
        }
    }
    else
    {
        std::cout << "Borderless positive path skipped on a headless host.\n";
    }

    const std::wstring native_title = L"WSE resize and borderless test " + std::to_wstring(process_id);
    const HWND window = FindWindowW( nullptr, native_title.c_str() );
    const auto pre_event_texture = renderer.getSurfaceTexture( surface_result.value() );
    if( window == nullptr || !pre_event_texture.succeeded() ||
        !resizeNativeClient( window, INTERACTIVE_WIDTH, INTERACTIVE_HEIGHT ) )
    {
        fail( "Interactive-resize test could not access the owned test window." );
        return 1;
    }
    const auto resize_events = renderer.pollSurfaceEvents( surface_result.value() );
    const auto interactive_texture = renderer.getSurfaceTexture( surface_result.value() );
    if( !resize_events.succeeded() || !resize_events.value().alive ||
        !resize_events.value().extent_changed ||
        resize_events.value().extent.width != INTERACTIVE_WIDTH ||
        resize_events.value().extent.height != INTERACTIVE_HEIGHT ||
        !interactive_texture.succeeded() ||
        sameHandle( pre_event_texture.value(), interactive_texture.value() ) ||
        renderer.readTexture( pre_event_texture.value(), 0U ).succeeded() ||
        !expectWindowState(
            &renderer, surface_result.value(), { INTERACTIVE_WIDTH, INTERACTIVE_HEIGHT },
            wse::oui::eSurfaceWindowMode::Windowed, {} ) )
    {
        fail( "Interactive frame resize did not synchronize the swap chain." );
    }

    SendMessageW( window, WM_DISPLAYCHANGE, 32U,
        MAKELPARAM( INTERACTIVE_WIDTH, INTERACTIVE_HEIGHT ) );
    const auto display_events = renderer.pollSurfaceEvents( surface_result.value() );
    const auto cleared_events = renderer.pollSurfaceEvents( surface_result.value() );
    if( !display_events.succeeded() || !display_events.value().display_topology_changed ||
        !cleared_events.succeeded() || cleared_events.value().display_topology_changed )
    {
        fail( "Display-topology event was not delivered exactly once." );
    }

    if( !renderer.destroySurface( surface_result.value() ).succeeded() ||
        renderer.getSurfaceState( surface_result.value() ).succeeded() )
    {
        fail( "Window transition stale-handle lifecycle failed." );
    }
    renderer.shutdown();
    return g_failures == 0 ? 0 : 1;
}
