//*****************************************************************************************************************
//!
//! @file    oui_vulkan_wayland_surface.cpp
//! @brief   \~japanese Vulkan Wayland Projection／Fullscreen／Resize／Presentを検証する.
//! @brief   \~english  Verifies Vulkan Wayland projection, fullscreen, resize and presentation.
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

#include <oui/renderer/ProjectionPipeline.h>

#include <algorithm>
#include <iostream>
#include <string>

namespace
{

constexpr int SKIPPED = 77;

int fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    return 1;
}

bool projectAndPresent(
      wse::oui::Renderer* const p_renderer_inout
    , const wse::oui::sSurfaceHandle surface_in
    , const wse::oui::sMeshHandle mesh_in
    , const wse::oui::sTextureHandle source_in
)
{
    const auto texture_result = p_renderer_inout->getSurfaceTexture( surface_in );
    if( !texture_result.succeeded() ) return false;
    wse::oui::sProjectionPassDescription pass;
    pass.color_attachment = texture_result.value();
    pass.clear_color = { 0.1F, 0.2F, 0.3F, 1.0F };
    pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_in
        , source_in
        , {}
        , wse::oui::eTextureSamplingFilter::Linear
        , 1.0F
        , { 0.15F, 0.15F, 0.0F, 0.0F, wse::oui::eEdgeBlendCurve::Smoothstep }
    } );
    pass.final_state = wse::oui::eTextureState::Present;
    const auto render_result = wse::oui::ProjectionPipeline::execute(
        p_renderer_inout, pass );
    if( !render_result.succeeded() ||
        !p_renderer_inout->waitFence( render_result.value(), 30000U ).succeeded() )
        return false;
    const auto present_result = p_renderer_inout->presentSurface( surface_in );
    return present_result.succeeded() &&
        p_renderer_inout->waitFence( present_result.value(), 30000U ).succeeded();
}

} // namespace

int main()
{
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend = wse::oui::eRendererBackend::Vulkan12;
    configuration.use_software_adapter = true;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
        return fail( "Vulkan initialization failed: " + initialize_result.error().message() );
    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        !capabilities_result.value().supports_window )
    {
        std::cout << "SKIPPED: Wayland Vulkan presentation is unavailable.\n";
        return SKIPPED;
    }
    const auto displays_result = renderer.enumerateDisplays();
    if( !displays_result.succeeded() )
        return fail( "Wayland display enumeration failed." );
    const auto display = std::find_if(
          displays_result.value().begin()
        , displays_result.value().end()
        , []( const wse::oui::sDisplayDescription& display_in )
          {
              return display_in.id.rfind( "wayland:", 0U ) == 0U &&
                  display_in.renderer_compatible;
          }
    );
    if( display == displays_result.value().end() )
    {
        std::cout << "SKIPPED: No Wayland output is available.\n";
        return SKIPPED;
    }

    wse::oui::sTextureDescription source_description;
    source_description.extent = { 2U, 2U };
    source_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    source_description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    source_description.initial_state = wse::oui::eTextureState::CopyDestination;
    const auto source_result = renderer.createTexture( source_description );
    if( !source_result.succeeded() )
        return fail( "Wayland projection source creation failed." );
    wse::oui::sRendererFrame source_frame;
    source_frame.description.extent = source_description.extent;
    source_frame.description.format = source_description.format;
    source_frame.data = {
          255U, 64U,   0U, 255U,   0U, 255U,  64U, 255U
        ,   0U, 64U, 255U, 255U, 255U, 255U, 255U, 255U
    };
    const auto upload_result = renderer.uploadTexture(
        source_result.value(), source_frame );
    if( !upload_result.succeeded() ||
        !renderer.waitFence( upload_result.value(), 30000U ).succeeded() )
        return fail( "Wayland projection source upload failed." );
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
        return fail( "Wayland projection mesh creation failed." );

    wse::oui::sSurfaceDescription description;
    description.type = wse::oui::eSurfaceType::Window;
    description.extent = { 160U, 120U };
    description.format = wse::oui::eRendererPixelFormat::Bgra8Unorm;
    description.buffer_count = 2U;
    description.visible = false;
    description.title = "WSE Vulkan Wayland Characterization";
    const auto surface_result = renderer.createSurface( description );
    if( !surface_result.succeeded() )
        return fail( "Wayland surface creation failed: " + surface_result.error().message() );
    const auto initial_texture = renderer.getSurfaceTexture( surface_result.value() );
    if( !initial_texture.succeeded() || !projectAndPresent(
            &renderer, surface_result.value(), mesh_result.value(), source_result.value() ) )
        return fail( "Initial Wayland projection/present failed." );

    wse::oui::sSurfaceWindowModeRequest fullscreen;
    fullscreen.mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
    fullscreen.display_id = display->id;
    const auto fullscreen_result = renderer.setSurfaceWindowMode(
        surface_result.value(), fullscreen );
    const auto fullscreen_state = renderer.getSurfaceState( surface_result.value() );
    if( !fullscreen_result.succeeded() || !fullscreen_state.succeeded() ||
        fullscreen_state.value().window_mode !=
            wse::oui::eSurfaceWindowMode::BorderlessFullscreen ||
        !projectAndPresent(
            &renderer, surface_result.value(), mesh_result.value(), source_result.value() ) )
        return fail( "Wayland borderless fullscreen projection failed." );

    wse::oui::sSurfaceWindowModeRequest windowed;
    windowed.mode = wse::oui::eSurfaceWindowMode::Windowed;
    if( !renderer.setSurfaceWindowMode( surface_result.value(), windowed ).succeeded() ||
        !renderer.resizeSurface( surface_result.value(), { 128U, 96U } ).succeeded() )
        return fail( "Wayland windowed restoration/resize failed." );
    const auto events_result = renderer.pollSurfaceEvents( surface_result.value() );
    const auto resized_texture = renderer.getSurfaceTexture( surface_result.value() );
    if( !events_result.succeeded() || !resized_texture.succeeded() ||
        resized_texture.value().value == initial_texture.value().value ||
        !projectAndPresent(
            &renderer, surface_result.value(), mesh_result.value(), source_result.value() ) )
        return fail( "Wayland resized swapchain state is invalid." );
    if( renderer.destroyTexture( initial_texture.value() ).succeeded() )
        return fail( "Stale Wayland swapchain texture did not fail safely." );
    if( !renderer.destroySurface( surface_result.value() ).succeeded() )
        return fail( "Wayland surface destruction failed." );
    if( !renderer.destroyMesh( mesh_result.value() ).succeeded() ||
        !renderer.destroyTexture( source_result.value() ).succeeded() )
        return fail( "Wayland projection resource destruction failed." );
    renderer.shutdown();
    std::cout << "Vulkan Wayland projection/fullscreen/resize/present passed.\n";
    return 0;
}
