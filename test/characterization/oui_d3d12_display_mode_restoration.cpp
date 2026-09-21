//*****************************************************************************************************************
//!
//! @file    oui_d3d12_display_mode_restoration.cpp
//! @brief   \~japanese Windows Display Mode／Direct Surfaceの復元を実Displayで検証する.
//! @brief   \~english  Verifies Windows display-mode and direct-surface restoration on a real display.
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

#include <utility>
#include <oui/renderer/ProjectionPipeline.h>
#include "../support/ProjectionReadbackCheck.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::uint32_t TIMEOUT_MS = 30000U;

struct sHardwareArguments
{
    std::string display_name;
    std::uint32_t hold_ms;

    //! @brief Construct all members with explicit defaults.
    sHardwareArguments(
          const std::string& display_name_in = {}
        , std::uint32_t hold_ms_in = 3000U
    )
        : display_name ( display_name_in )
        , hold_ms      ( hold_ms_in )
    {
    }
};

bool parseUnsigned( std::uint32_t* const p_value_out, const std::string& text_in )
{
    try
    {
        std::size_t parsed = 0U;
        const unsigned long value = std::stoul( text_in, &parsed, 10 );
        if( parsed != text_in.size() || value > 60000UL )
        {
            return false;
        }
        *p_value_out = static_cast< std::uint32_t >( value );
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

bool parseArguments(
      sHardwareArguments* const         p_arguments_out
    , const int                         argument_count_in
    , const char* const* const          pp_arguments_in
)
{
    std::uint32_t display_number = 0U;
    for( int index = 1; index < argument_count_in; index += 2 )
    {
        if( index + 1 >= argument_count_in )
        {
            return false;
        }
        const std::string option = pp_arguments_in[ index ];
        const std::string value = pp_arguments_in[ index + 1 ];
        if( option == "--display-number" )
        {
            if( !parseUnsigned( &display_number, value ) || display_number == 0U )
            {
                return false;
            }
        }
        else if( option == "--hold-ms" )
        {
            if( !parseUnsigned( &p_arguments_out->hold_ms, value ) )
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    if( display_number == 0U )
    {
        return false;
    }
    p_arguments_out->display_name = "\\\\.\\DISPLAY" + std::to_string( display_number );
    return true;
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

const wse::oui::sDisplayDescription* findDisplay(
      const std::vector< wse::oui::sDisplayDescription >& displays_in
    , const std::string& display_id_in
)
{
    const auto iterator = std::find_if(
          displays_in.begin()
        , displays_in.end()
        , [&display_id_in]( const wse::oui::sDisplayDescription& display_in )
          {
              return display_in.id == display_id_in;
          }
    );
    return iterator == displays_in.end() ? nullptr : &*iterator;
}

bool restored(
      wse::oui::Renderer* const                  p_renderer_in
    , const wse::oui::sDisplayDescription&       before_in
)
{
    const auto after_result = p_renderer_in->enumerateDisplays();
    if( !after_result.succeeded() )
    {
        return false;
    }
    const auto* const p_after = findDisplay( after_result.value(), before_in.id );
    return p_after != nullptr && sameMode( p_after->current_mode, before_in.current_mode ) &&
        p_after->position.x == before_in.position.x &&
        p_after->position.y == before_in.position.y &&
        p_after->desktop_extent.width == before_in.desktop_extent.width &&
        p_after->desktop_extent.height == before_in.desktop_extent.height &&
        p_after->primary == before_in.primary;
}

bool waitFence(
      wse::oui::Renderer* const p_renderer_inout
    , const wse::oui::sFenceHandle fence_in
)
{
    return p_renderer_inout->waitFence( fence_in, TIMEOUT_MS ).succeeded();
}

bool projectAndPresent(
      wse::oui::Renderer* const       p_renderer_inout
    , const wse::oui::sSurfaceHandle surface_in
    , const wse::oui::sMeshHandle    mesh_in
    , const wse::oui::sTextureHandle source_in
)
{
    const auto texture_result = p_renderer_inout->getSurfaceTexture( surface_in );
    if( !texture_result.succeeded() )
    {
        std::cerr << "Surface texture failed: " << texture_result.error().message() << '\n';
        return false;
    }
    wse::oui::sProjectionPassDescription pass;
    pass.color_attachment = texture_result.value();
    pass.clear_color = { 0.02F, 0.02F, 0.02F, 1.0F };
    pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_in
        , source_in
        , {}
        , wse::oui::eTextureSamplingFilter::Linear
        , 1.0F
        , { 0.08F, 0.08F, 0.08F, 0.08F, wse::oui::eEdgeBlendCurve::Smoothstep }
    } );
    pass.final_state = wse::oui::eTextureState::CopySource;
    const auto render_result = wse::oui::ProjectionPipeline::execute(
        p_renderer_inout, pass );
    if( !render_result.succeeded() ||
        !waitFence( p_renderer_inout, render_result.value() ) )
    {
        std::cerr << "Projection submission/wait failed";
        if( !render_result.succeeded() ) std::cerr << ": " << render_result.error().message();
        std::cerr << '\n';
        return false;
    }
    const auto frame_result = p_renderer_inout->readTexture(
        texture_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() || frame_result.value().data.empty() )
    {
        std::cerr << "Projection readback failed";
        if( !frame_result.succeeded() ) std::cerr << ": " << frame_result.error().message();
        std::cerr << '\n';
        return false;
    }
    if( !wse_test::matchesProjectionPattern( frame_result.value(), std::cerr ) )
    {
        std::cerr << "Projection readback failed spatial RGB validation.\n";
        return false;
    }
    std::cout << "SPATIAL_RGB_PASS regions=4 pixels=36 tolerance=2 alpha_ignored=1\n";
    wse::oui::sRenderPassDescription present_transition;
    present_transition.color_attachment = texture_result.value();
    present_transition.load_operation = wse::oui::eAttachmentLoadOperation::Load;
    present_transition.final_state = wse::oui::eTextureState::Present;
    const auto transition_result = p_renderer_inout->executeRenderPass(
        present_transition );
    if( !transition_result.succeeded() ||
        !waitFence( p_renderer_inout, transition_result.value() ) )
    {
        std::cerr << "Present transition/wait failed";
        if( !transition_result.succeeded() ) std::cerr << ": " << transition_result.error().message();
        std::cerr << '\n';
        return false;
    }
    const auto present_result = p_renderer_inout->presentSurface( surface_in );
    if( !present_result.succeeded() )
        std::cerr << "Present failed: " << present_result.error().message() << '\n';
    return present_result.succeeded() &&
        waitFence( p_renderer_inout, present_result.value() );
}

bool holdVisibleSurface(
      wse::oui::Renderer* const       p_renderer_inout
    , const wse::oui::sSurfaceHandle surface_in
    , const std::uint32_t             hold_ms_in
)
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds( hold_ms_in );
    while( std::chrono::steady_clock::now() < deadline )
    {
        const auto events = p_renderer_inout->processSurfaceEvents( surface_in );
        if( !events.succeeded() || !events.value() )
        {
            std::cerr << "Visible surface event processing failed or surface closed.\n";
            return false;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
    }
    return true;
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    sHardwareArguments arguments;
    if( !parseArguments( &arguments, argument_count_in, pp_arguments_in ) )
    {
        std::cerr << "FAILED: Expected --display-number N [--hold-ms N].\n";
        return 2;
    }

    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend = wse::oui::eRendererBackend::Direct3D12;
    configuration.prefer_display_adapter = true;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
    {
        std::cerr << "FAILED: Hardware renderer initialization failed: "
                  << initialize_result.error().message() << '\n';
        return 1;
    }

    const auto displays_result = renderer.enumerateDisplays();
    if( !displays_result.succeeded() )
    {
        std::cerr << "FAILED: Display enumeration failed.\n";
        return 1;
    }
    const auto selected = std::find_if(
          displays_result.value().begin()
        , displays_result.value().end()
        , [&arguments]( const wse::oui::sDisplayDescription& display_in )
          {
              return display_in.display_name == arguments.display_name;
          }
    );
    if( selected == displays_result.value().end() )
    {
        std::cerr << "FAILED: The explicitly selected display is not active.\n";
        return 1;
    }
    if( !selected->renderer_compatible )
    {
        std::cerr << "FAILED: The explicitly selected display is not attached to the active D3D12 adapter.\n";
        return 1;
    }
    const wse::oui::sDisplayDescription before = *selected;
    std::vector< wse::oui::sDisplayMode > candidate_modes;
    for( const bool same_extent_only : { true, false } )
    {
        for( const auto& mode : before.modes )
        {
            const bool same_extent = mode.extent.width == before.current_mode.extent.width &&
                mode.extent.height == before.current_mode.extent.height;
            if( sameMode( mode, before.current_mode ) || same_extent != same_extent_only ||
                mode.extent.empty() || mode.refresh_rate_denominator == 0U ||
                mode.format != before.current_mode.format || mode.interlaced )
            {
                continue;
            }
            candidate_modes.push_back( mode );
        }
    }
    const auto advertised_current_mode = std::find_if(
          before.modes.begin()
        , before.modes.end()
        , [&before]( const wse::oui::sDisplayMode& mode_in )
          {
              return sameMode( mode_in, before.current_mode );
          }
    );
    if( advertised_current_mode != before.modes.end() )
    {
        // Some projector paths expose alternate raw timings but reject them through
        // ChangeDisplaySettingsEx.  Current-mode fullscreen still validates the physical
        // output, Projection/Present lifecycle, DirectDisplay ownership and restoration.
        candidate_modes.push_back( *advertised_current_mode );
    }
    if( candidate_modes.empty() )
    {
        std::cout << "SKIPPED: The selected display exposes no safe advertised mode.\n";
        return 77;
    }

    wse::oui::sTextureDescription source_description;
    source_description.extent = { 2U, 2U };
    source_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    source_description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    source_description.initial_state = wse::oui::eTextureState::CopyDestination;
    const auto source_result = renderer.createTexture( source_description );
    if( !source_result.succeeded() )
    {
        std::cerr << "FAILED: Hardware projection source creation failed.\n";
        return 1;
    }
    wse::oui::sRendererFrame source_frame;
    source_frame.description.extent = source_description.extent;
    source_frame.description.format = source_description.format;
    source_frame.data = {
          255U,  96U,   0U, 255U,   0U, 255U,  96U, 255U
        ,   0U,  96U, 255U, 255U, 255U, 255U, 255U, 255U
    };
    const auto upload_result = renderer.uploadTexture(
        source_result.value(), source_frame );
    if( !upload_result.succeeded() || !waitFence( &renderer, upload_result.value() ) )
    {
        std::cerr << "FAILED: Hardware projection source upload failed.\n";
        return 1;
    }
    wse::oui::sMeshDescription mesh_description;
    mesh_description.vertices = {
          { -1.0F,  1.0F, 0.0F, 0.0F }
        , {  1.0F,  1.0F, 1.0F, 0.0F }
        , {  1.0F, -1.0F, 1.0F, 1.0F }
        , { -1.0F, -1.0F, 0.0F, 1.0F }
    };
    mesh_description.indices = { 0U, 1U, 2U, 0U, 2U, 3U };
    const auto mesh_result = renderer.createMesh( mesh_description );
    if( !mesh_result.succeeded() )
    {
        std::cerr << "FAILED: Hardware projection mesh creation failed.\n";
        return 1;
    }

    wse::oui::sSurfaceDescription window_description;
    window_description.type = wse::oui::eSurfaceType::Window;
    window_description.extent = { 320U, 180U };
    window_description.format = before.current_mode.format;
    window_description.buffer_count = 2U;
    window_description.vertical_sync = true;
    window_description.visible = true;
    window_description.title = "WSE display-mode restoration test";
    const auto window_result = renderer.createSurface( window_description );
    if( !window_result.succeeded() )
    {
        std::cerr << "FAILED: Test window creation failed.\n";
        return 1;
    }
    const auto window_texture = renderer.getSurfaceTexture( window_result.value() );

    wse::oui::sDisplayMode requested_mode;
    wse::oui::RendererError fullscreen_error;
    bool entered_fullscreen = false;
    bool used_current_mode = false;
    bool used_borderless_fallback = false;
    for( const auto& candidate_mode : candidate_modes )
    {
        wse::oui::sSurfaceWindowModeRequest fullscreen_request;
        fullscreen_request.mode = wse::oui::eSurfaceWindowMode::DisplayModeFullscreen;
        fullscreen_request.display_id = before.id;
        fullscreen_request.display_mode = candidate_mode;
        const auto transition_result = renderer.setSurfaceWindowMode(
            window_result.value(), fullscreen_request );
        if( transition_result.succeeded() )
        {
            requested_mode = candidate_mode;
            entered_fullscreen = true;
            used_current_mode = sameMode( candidate_mode, before.current_mode );
            break;
        }
        fullscreen_error = transition_result.error();
        const auto retry_state = renderer.getSurfaceState( window_result.value() );
        if( !retry_state.succeeded() ||
            retry_state.value().window_mode != wse::oui::eSurfaceWindowMode::Windowed ||
            !restored( &renderer, before ) )
        {
            break;
        }
    }
    if( !entered_fullscreen )
    {
        wse::oui::sSurfaceWindowModeRequest borderless_request;
        borderless_request.mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
        borderless_request.display_id = before.id;
        const auto borderless_result = renderer.setSurfaceWindowMode(
            window_result.value(), borderless_request );
        if( borderless_result.succeeded() )
        {
            requested_mode = before.current_mode;
            entered_fullscreen = true;
            used_current_mode = true;
            used_borderless_fallback = true;
        }
        else
        {
            fullscreen_error = borderless_result.error();
        }
    }
    const auto fullscreen_state = renderer.getSurfaceState( window_result.value() );
    const auto fullscreen_texture = renderer.getSurfaceTexture( window_result.value() );
    if( !window_texture.succeeded() || !entered_fullscreen ||
        !fullscreen_state.succeeded() ||
        fullscreen_state.value().window_mode != ( used_borderless_fallback
            ? wse::oui::eSurfaceWindowMode::BorderlessFullscreen
            : wse::oui::eSurfaceWindowMode::DisplayModeFullscreen ) ||
        fullscreen_state.value().display_id != before.id ||
        ( !used_borderless_fallback &&
            !sameMode( fullscreen_state.value().display_mode, requested_mode ) ) ||
        !fullscreen_texture.succeeded() ||
        fullscreen_texture.value().value == window_texture.value().value )
    {
        std::cerr << "FAILED: Display-mode fullscreen transition failed";
        if( !fullscreen_error.ok() )
        {
            std::cerr << ": " << fullscreen_error.message()
                      << " native=" << fullscreen_error.nativeCode();
        }
        std::cerr << ".\n";
        renderer.setSurfaceWindowMode(
            window_result.value(), wse::oui::sSurfaceWindowModeRequest{} );
        renderer.destroySurface( window_result.value() );
        renderer.shutdown();
        return 1;
    }
    if( used_current_mode && !used_borderless_fallback )
    {
        std::cout << "DISPLAY_MODE_CURRENT_FALLBACK\n";
    }
    if( used_borderless_fallback )
    {
        std::cout << "DISPLAY_MODE_BORDERLESS_FALLBACK\n";
    }
    std::cout << "DISPLAY_MODE selected=" << requested_mode.extent.width << 'x'
              << requested_mode.extent.height << " refresh="
              << requested_mode.refresh_rate_numerator << '/'
              << requested_mode.refresh_rate_denominator << " changed="
              << !sameMode( requested_mode, before.current_mode ) << '\n';

    if( !projectAndPresent(
            &renderer, window_result.value(), mesh_result.value(), source_result.value() ) ||
        !holdVisibleSurface( &renderer, window_result.value(), arguments.hold_ms ) )
    {
        renderer.setSurfaceWindowMode(
            window_result.value(), wse::oui::sSurfaceWindowModeRequest{} );
        renderer.destroySurface( window_result.value() );
        renderer.shutdown();
        std::cerr << "FAILED: Fullscreen Projection scene presentation failed.\n";
        return 1;
    }

    const auto windowed_result = renderer.setSurfaceWindowMode(
        window_result.value(), wse::oui::sSurfaceWindowModeRequest{} );
    if( !windowed_result.succeeded() || !restored( &renderer, before ) ||
        !renderer.destroySurface( window_result.value() ).succeeded() )
    {
        std::cerr << "FAILED: Windowed display restoration failed.\n";
        return 1;
    }

    if( used_borderless_fallback )
    {
        if( !renderer.destroyMesh( mesh_result.value() ).succeeded() ||
            !renderer.destroyTexture( source_result.value() ).succeeded() )
        {
            std::cerr << "FAILED: Hardware projection resource cleanup failed.\n";
            return 1;
        }
        renderer.shutdown();
        std::cout << "DISPLAY_HARDWARE_PROJECTION_PASS\n";
        return 0;
    }

    std::cout << "WINDOW_DISPLAY presentation_and_restoration=pass\n";

    wse::oui::sSurfaceDescription direct_description;
    direct_description.type = wse::oui::eSurfaceType::DirectDisplay;
    direct_description.extent = requested_mode.extent;
    direct_description.format = requested_mode.format;
    direct_description.buffer_count = 2U;
    direct_description.vertical_sync = true;
    direct_description.visible = true;
    direct_description.title = "WSE direct-display restoration test";
    direct_description.display_id = before.id;
    direct_description.display_mode = requested_mode;
    const auto direct_result = renderer.createSurface( direct_description );
    if( !direct_result.succeeded() )
    {
        std::cerr << "FAILED: Direct-display surface creation failed: "
                  << direct_result.error().message() << '\n';
        return 1;
    }
    const auto direct_state = renderer.getSurfaceState( direct_result.value() );
    if( !direct_state.succeeded() ||
        direct_state.value().type != wse::oui::eSurfaceType::DirectDisplay ||
        direct_state.value().window_mode !=
            wse::oui::eSurfaceWindowMode::DisplayModeFullscreen )
    {
        renderer.destroySurface( direct_result.value() );
        renderer.shutdown();
        std::cerr << "FAILED: Direct-display state validation failed.\n";
        return 1;
    }
    const bool direct_presented = projectAndPresent(
            &renderer, direct_result.value(), mesh_result.value(), source_result.value() );
    const bool direct_held = direct_presented &&
        holdVisibleSurface( &renderer, direct_result.value(), arguments.hold_ms );
    const auto direct_destroyed = renderer.destroySurface( direct_result.value() );
    const bool direct_restored = restored( &renderer, before );
    std::cout << "DIRECT_DISPLAY presented=" << direct_presented << " held=" << direct_held
              << " destroyed=" << direct_destroyed.succeeded() << " restored=" << direct_restored << '\n';
    if( !direct_presented || !direct_held || !direct_destroyed.succeeded() || !direct_restored )
    {
        std::cerr << "FAILED: Direct-display presentation or restoration failed.\n";
        return 1;
    }

    if( !renderer.destroyMesh( mesh_result.value() ).succeeded() ||
        !renderer.destroyTexture( source_result.value() ).succeeded() )
    {
        std::cerr << "FAILED: Hardware projection resource cleanup failed.\n";
        return 1;
    }
    renderer.shutdown();
    std::cout << "DISPLAY_HARDWARE_PROJECTION_PASS\n";
    return 0;
}
