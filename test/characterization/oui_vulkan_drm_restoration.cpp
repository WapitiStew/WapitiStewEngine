//*****************************************************************************************************************
//!
//! @file    oui_vulkan_drm_restoration.cpp
//! @brief   \~japanese Vulkan DRM/KMS直接表示と元CRTC状態の復元を検証する.
//! @brief   \~english  Verifies Vulkan DRM/KMS direct presentation and original CRTC restoration.
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

#include <algorithm>
#include <iostream>
#include <string>

namespace
{

constexpr int SKIPPED = 77;

bool sameMode(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in )
{
    return left_in.extent.width == right_in.extent.width &&
        left_in.extent.height == right_in.extent.height &&
        left_in.refresh_rate_numerator == right_in.refresh_rate_numerator &&
        left_in.refresh_rate_denominator == right_in.refresh_rate_denominator &&
        left_in.format == right_in.format && left_in.interlaced == right_in.interlaced;
}

int fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    return 1;
}

} // namespace

int main()
{
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend = wse::oui::eRendererBackend::Vulkan12;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
        return fail( "Vulkan initialization failed: " + initialize_result.error().message() );
    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        !capabilities_result.value().supports_direct_display )
    {
        std::cout << "SKIPPED: Vulkan DRM direct-display extensions are unavailable.\n";
        return SKIPPED;
    }
    const auto before_result = renderer.enumerateDisplays();
    if( !before_result.succeeded() )
        return fail( "DRM display enumeration failed." );
    const auto before = std::find_if(
          before_result.value().begin()
        , before_result.value().end()
        , []( const wse::oui::sDisplayDescription& display_in )
          {
              return display_in.id.rfind( "drm:", 0U ) == 0U &&
                  display_in.renderer_compatible;
          }
    );
    if( before == before_result.value().end() )
    {
        std::cout << "SKIPPED: No accessible connected DRM/KMS display is available.\n";
        return SKIPPED;
    }
    const std::string display_id = before->id;
    const wse::oui::sDisplayMode original_mode = before->current_mode;

    wse::oui::sSurfaceDescription description;
    description.type = wse::oui::eSurfaceType::DirectDisplay;
    description.extent = original_mode.extent;
    description.format = original_mode.format;
    description.buffer_count = 2U;
    description.display_id = display_id;
    description.display_mode = original_mode;
    const auto surface_result = renderer.createSurface( description );
    if( !surface_result.succeeded() )
        return fail( "DRM DirectDisplay creation failed: " + surface_result.error().message() );
    const auto state_result = renderer.getSurfaceState( surface_result.value() );
    if( !state_result.succeeded() ||
        state_result.value().type != wse::oui::eSurfaceType::DirectDisplay ||
        state_result.value().window_mode !=
            wse::oui::eSurfaceWindowMode::DisplayModeFullscreen ||
        state_result.value().display_id != display_id ||
        !sameMode( state_result.value().display_mode, original_mode ) )
        return fail( "DRM DirectDisplay state is invalid." );

    const auto texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !texture_result.succeeded() )
        return fail( "DRM swapchain texture lookup failed." );
    wse::oui::sRenderPassDescription pass;
    pass.color_attachment = texture_result.value();
    pass.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
    pass.final_state = wse::oui::eTextureState::Present;
    const auto render_result = renderer.executeRenderPass( pass );
    const auto present_result = render_result.succeeded()
        ? renderer.presentSurface( surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sFenceHandle >::failure( render_result.error() );
    if( !render_result.succeeded() || !present_result.succeeded() )
        return fail( "DRM DirectDisplay render/present failed." );
    const auto destroy_result = renderer.destroySurface( surface_result.value() );
    if( !destroy_result.succeeded() )
        return fail( "DRM/KMS restoration failed: " + destroy_result.error().message() );

    const auto after_result = renderer.enumerateDisplays();
    if( !after_result.succeeded() )
        return fail( "Post-restoration DRM enumeration failed." );
    const auto after = std::find_if(
          after_result.value().begin()
        , after_result.value().end()
        , [&display_id]( const wse::oui::sDisplayDescription& display_in )
          {
              return display_in.id == display_id;
          }
    );
    if( after == after_result.value().end() ||
        !sameMode( after->current_mode, original_mode ) )
        return fail( "Original DRM/KMS display mode was not restored exactly." );
    renderer.shutdown();
    std::cout << "Vulkan DRM/KMS direct-display restoration passed.\n";
    return 0;
}
