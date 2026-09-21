//*****************************************************************************************************************
//!
//! @file    portable_projection.cpp
//! @brief   \~japanese D3D12／Vulkan共通のPortable Projection利用例を示す.
//! @brief   \~english  Demonstrates portable projection shared by D3D12 and Vulkan.
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
#include <wse/stew.h>

#include <cstdint>
#include <string>

namespace
{

constexpr std::uint32_t SOURCE_WIDTH = 2U;
constexpr std::uint32_t SOURCE_HEIGHT = 2U;
constexpr std::uint32_t TARGET_WIDTH = 4U;
constexpr std::uint32_t TARGET_HEIGHT = 4U;
constexpr std::uint32_t TIMEOUT_MS = 30000U;
constexpr std::uint64_t EXPECTED_HASH = 0xe0f9517b5bf10dc5ULL;

int fail( const std::string& message_in )
{
    wse::WLog() << "FAILED:" << message_in;
    return 1;
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    wse::registDefaultLog();
    if( argument_count_in != 2 )
        return fail( "Usage: portable_projection <d3d12|vulkan>" );
    const std::string backend_name = pp_arguments_in[ 1U ];
    wse::oui::sRendererConfiguration configuration;
    configuration.use_software_adapter = true;
    if( backend_name == "d3d12" )
        configuration.backend = wse::oui::eRendererBackend::Direct3D12;
    else if( backend_name == "vulkan" )
        configuration.backend = wse::oui::eRendererBackend::Vulkan12;
    else
        return fail( "Unknown backend argument." );

    wse::oui::Renderer renderer;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
        return fail( "Renderer initialization failed: " + initialize_result.error().message() );
    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        !capabilities_result.value().supports_offscreen ||
        !capabilities_result.value().supports_mesh_rendering )
        return fail( "Required portable projection capabilities are unavailable." );

    wse::oui::sTextureDescription source_description;
    source_description.extent = { SOURCE_WIDTH, SOURCE_HEIGHT };
    source_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    source_description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    source_description.initial_state = wse::oui::eTextureState::CopyDestination;
    const auto source_result = renderer.createTexture( source_description );
    if( !source_result.succeeded() )
        return fail( "Source texture creation failed: " + source_result.error().message() );
    // The source is a Core RGBA image; its type names the pixel format the upload needs.
    // Red, green, blue, and white, one quadrant each.
    constexpr std::uint8_t SOURCE_PIXELS[ SOURCE_HEIGHT ][ SOURCE_WIDTH ][ 4U ] = {
          { { 255U,   0U,   0U, 255U }, {   0U, 255U,   0U, 255U } }
        , { {   0U,   0U, 255U, 255U }, { 255U, 255U, 255U, 255U } }
    };
    wse::img4c08_t source_image( SOURCE_WIDTH, SOURCE_HEIGHT );
    for( std::uint32_t y = 0U; y < SOURCE_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < SOURCE_WIDTH; ++x )
        {
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                source_image[ y ][ x ][ channel ] = SOURCE_PIXELS[ y ][ x ][ channel ];
            }
        }
    }
    const auto upload_result = renderer.uploadTexture( source_result.value(), source_image );
    if( !upload_result.succeeded() ||
        !renderer.waitFence( upload_result.value(), TIMEOUT_MS ).succeeded() )
        return fail( "Source texture upload failed." );

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
        return fail( "Projection mesh creation failed: " + mesh_result.error().message() );

    wse::oui::sSurfaceDescription surface_description;
    surface_description.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    const auto surface_result = renderer.createSurface( surface_description );
    const auto target_result = surface_result.succeeded()
        ? renderer.getSurfaceTexture( surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( surface_result.error() );
    if( !target_result.succeeded() )
        return fail( "Projection target creation failed: " + target_result.error().message() );

    wse::oui::sProjectionPassDescription projection_pass;
    projection_pass.color_attachment = target_result.value();
    // An explicit extent also permits supersample_scale = 2 without changing the target.
    projection_pass.render_area.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    projection_pass.clear_color = { 1.0F, 0.0F, 1.0F, 1.0F };
    projection_pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_result.value()
        , source_result.value()
        , {}
        , wse::oui::eTextureSamplingFilter::Nearest
        , 1.0F
        , {}
    } );
    projection_pass.final_state = wse::oui::eTextureState::CopySource;
    const auto render_result = wse::oui::ProjectionPipeline::execute(
        &renderer, projection_pass );
    if( !render_result.succeeded() )
        return fail( "Projection render failed: " + render_result.error().message() );
    const auto render_wait_result = renderer.waitFence( render_result.value(), TIMEOUT_MS );
    if( !render_wait_result.succeeded() )
        return fail( "Projection fence wait failed: " + render_wait_result.error().message() );
    const auto frame_result = renderer.readTexture( target_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
        return fail( "Projection readback failed: " + frame_result.error().message() );

    const auto& output = frame_result.value().data;
    if( output.size() != static_cast< std::size_t >( TARGET_WIDTH ) * TARGET_HEIGHT * 4U )
        return fail( "Projection output byte size is invalid." );
    std::uint64_t hash = 14695981039346656037ULL;
    for( std::uint32_t y = 0U; y < TARGET_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < TARGET_WIDTH; ++x )
        {
            const bool right = x >= TARGET_WIDTH / 2U;
            const bool bottom = y >= TARGET_HEIGHT / 2U;
            const std::uint8_t expected_red = bottom ? ( right ? 255U : 0U ) :
                ( right ? 0U : 255U );
            const std::uint8_t expected_green = right ? 255U : 0U;
            const std::uint8_t expected_blue = bottom ? 255U : 0U;
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            if( output[ offset ] != expected_red || output[ offset + 1U ] != expected_green ||
                output[ offset + 2U ] != expected_blue || output[ offset + 3U ] != 255U )
                return fail( "Projection output exceeded the shared zero-LSB tolerance." );
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                hash ^= output[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    if( hash != EXPECTED_HASH )
        return fail( "Projection output hash differs from the shared backend baseline." );

    renderer.destroySurface( surface_result.value() );
    renderer.destroyMesh( mesh_result.value() );
    renderer.destroyTexture( source_result.value() );
    renderer.shutdown();
    wse::WLog() << backend_name << "shared projection RGBA8 FNV-1a:" << wse::toHex( hash );
    return 0;
}
