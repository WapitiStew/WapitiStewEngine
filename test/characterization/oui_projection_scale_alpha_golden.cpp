//*****************************************************************************************************************
//!
//! @file    oui_projection_scale_alpha_golden.cpp
//! @brief   \~japanese Portable ProjectionのBackend共通Linear scale／Alpha合成を検証する.
//! @brief   \~english  Verifies backend-neutral linear scaling and alpha composition.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/renderer/ProjectionMeshAdapter.h>
#include <oui/renderer/ProjectionPipeline.h>

#include "oui_projection_test_backend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr std::uint32_t SOURCE_WIDTH  = 2U;
constexpr std::uint32_t SOURCE_HEIGHT = 2U;
constexpr std::uint32_t TARGET_WIDTH  = 4U;
constexpr std::uint32_t TARGET_HEIGHT = 4U;
constexpr std::uint32_t TIMEOUT_MS    = 30000U;
constexpr float         LAYER_OPACITY = 0.75F;
constexpr std::uint64_t EXPECTED_HASH = 0xce4b43253436392eULL;

int g_failures = 0;

//! @brief Test failureを記録する.
//! @param [in] message_in Failure message.
void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

//! @brief Fence完了を待機する.
//! @param [in,out] p_renderer_inout Renderer.
//! @param [in]     fence_in         Fence handle.
//! @return 成功時true.
bool waitFence(
          wse::oui::Renderer* const p_renderer_inout
    , const wse::oui::sFenceHandle  fence_in
)
{
    const auto wait_result = p_renderer_inout->waitFence( fence_in, TIMEOUT_MS );
    if( !wait_result.succeeded() )
    {
        fail( "Fence wait failed: " + wait_result.error().message() );
        return false;
    }
    return true;
}

//! @brief 2 x 2 RGBA FrameをSampled textureへUploadする.
//! @param [in,out] p_renderer_inout Renderer.
//! @param [in]     data_in          Packed RGBA pixels.
//! @return Texture handleまたはError.
wse::oui::RendererResult< wse::oui::sTextureHandle > createUploadedTexture(
          wse::oui::Renderer* const        p_renderer_inout
    , const std::vector< std::uint8_t >&   data_in
)
{
    wse::oui::sTextureDescription description;
    description.extent = { SOURCE_WIDTH, SOURCE_HEIGHT };
    description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    description.initial_state = wse::oui::eTextureState::CopyDestination;
    const auto texture_result = p_renderer_inout->createTexture( description );
    if( !texture_result.succeeded() )
    {
        return texture_result;
    }

    wse::oui::sRendererFrame frame;
    frame.description.extent = description.extent;
    frame.description.format = description.format;
    frame.data = data_in;
    const auto upload_result = p_renderer_inout->uploadTexture( texture_result.value(), frame );
    if( !upload_result.succeeded() )
    {
        p_renderer_inout->destroyTexture( texture_result.value() );
        return wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( upload_result.error() );
    }
    const auto wait_result = p_renderer_inout->waitFence( upload_result.value(), TIMEOUT_MS );
    if( !wait_result.succeeded() )
    {
        p_renderer_inout->destroyTexture( texture_result.value() );
        return wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( wait_result.error() );
    }
    return texture_result;
}

//! @brief D3D clamp Linear samplingのCPU Referenceを計算する.
//! @param [in] data_in    Packed 2 x 2 RGBA data.
//! @param [in] u_in       Normalized U.
//! @param [in] v_in       Normalized V.
//! @param [in] channel_in RGBA channel index.
//! @return Normalized sampled value.
double sampleLinear(
      const std::vector< std::uint8_t >& data_in
    , const double                       u_in
    , const double                       v_in
    , const std::size_t                  channel_in
)
{
    const double source_x = std::clamp( u_in * SOURCE_WIDTH - 0.5, 0.0, 1.0 );
    const double source_y = std::clamp( v_in * SOURCE_HEIGHT - 0.5, 0.0, 1.0 );
    const std::size_t x0 = static_cast< std::size_t >( std::floor( source_x ) );
    const std::size_t y0 = static_cast< std::size_t >( std::floor( source_y ) );
    const std::size_t x1 = std::min< std::size_t >( x0 + 1U, SOURCE_WIDTH - 1U );
    const std::size_t y1 = std::min< std::size_t >( y0 + 1U, SOURCE_HEIGHT - 1U );
    const double weight_x = source_x - static_cast< double >( x0 );
    const double weight_y = source_y - static_cast< double >( y0 );
    const auto read_channel = [&data_in, channel_in](
        const std::size_t x_in, const std::size_t y_in )
    {
        return static_cast< double >(
            data_in[ ( y_in * SOURCE_WIDTH + x_in ) * 4U + channel_in ] ) / 255.0;
    };
    const double top = read_channel( x0, y0 ) * ( 1.0 - weight_x ) +
        read_channel( x1, y0 ) * weight_x;
    const double bottom = read_channel( x0, y1 ) * ( 1.0 - weight_x ) +
        read_channel( x1, y1 ) * weight_x;
    return top * ( 1.0 - weight_y ) + bottom * weight_y;
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    // 選択Backendと2 x 2 Color／Alpha Sourceを用意する.
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    std::string backend_name;
    if( !wse::oui::test::configureProjectionBackend(
            &configuration, &backend_name, argument_count_in, pp_arguments_in ) )
    {
        fail( "Expected backend argument: d3d12 or vulkan." );
        return 1;
    }
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
    {
        fail( "Renderer initialization failed: " + initialize_result.error().message() );
        return 1;
    }
    const std::vector< std::uint8_t > color_data = {
          255U,   0U,   0U, 255U
        ,   0U, 255U,   0U, 255U
        ,   0U,   0U, 255U, 255U
        , 255U, 255U, 255U, 255U
    };
    const std::vector< std::uint8_t > alpha_data = {
          255U, 255U, 255U, 255U
        , 255U, 255U, 255U, 192U
        , 255U, 255U, 255U, 128U
        , 255U, 255U, 255U,  64U
    };
    const auto source_result = createUploadedTexture( &renderer, color_data );
    const auto alpha_result  = createUploadedTexture( &renderer, alpha_data );
    if( !source_result.succeeded() || !alpha_result.succeeded() )
    {
        fail( "Projection source upload failed." );
        return 1;
    }

    // Legacy Pixel meshをPortable Projection meshへ変換する.
    wse::float64_mesh2d legacy_mesh( 2U, 2U );
    legacy_mesh[ 0U ][ 0U ].src = { 0.0, 0.0 };
    legacy_mesh[ 0U ][ 1U ].src = { 1.0, 0.0 };
    legacy_mesh[ 1U ][ 0U ].src = { 0.0, 1.0 };
    legacy_mesh[ 1U ][ 1U ].src = { 1.0, 1.0 };
    legacy_mesh[ 0U ][ 0U ].dst = { 0.0, 0.0 };
    legacy_mesh[ 0U ][ 1U ].dst = { 3.0, 0.0 };
    legacy_mesh[ 1U ][ 0U ].dst = { 0.0, 3.0 };
    legacy_mesh[ 1U ][ 1U ].dst = { 3.0, 3.0 };
    const auto portable_mesh_result = wse::oui::ProjectionMeshAdapter::createMesh(
          legacy_mesh
        , { TARGET_WIDTH, TARGET_HEIGHT }
        , { SOURCE_WIDTH, SOURCE_HEIGHT }
    );
    if( !portable_mesh_result.succeeded() )
    {
        fail( "Legacy mesh conversion failed: " + portable_mesh_result.error().message() );
        return 1;
    }
    const auto mesh_result = renderer.createMesh( portable_mesh_result.value() );

    wse::oui::sSurfaceDescription target_description;
    target_description.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    const auto surface_result = renderer.createSurface( target_description );
    const auto target_result = surface_result.succeeded()
        ? renderer.getSurfaceTexture( surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( surface_result.error() );
    if( !mesh_result.succeeded() || !target_result.succeeded() )
    {
        fail( "Projection target or mesh creation failed." );
        return 1;
    }

    // Linear scale＋Alpha map＋OpacityをPortable Projection facadeから描画する.
    wse::oui::sProjectionPassDescription projection_pass;
    projection_pass.color_attachment = target_result.value();
    projection_pass.clear_color       = { 0.0F, 0.0F, 0.0F, 1.0F };
    projection_pass.final_state       = wse::oui::eTextureState::CopySource;
    projection_pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_result.value()
        , source_result.value()
        , alpha_result.value()
        , wse::oui::eTextureSamplingFilter::Linear
        , LAYER_OPACITY
    } );
    const auto projection_result =
        wse::oui::ProjectionPipeline::execute( &renderer, projection_pass );
    if( !projection_result.succeeded() )
    {
        fail( "Projection render failed." );
        return 1;
    }
    const bool resources_released =
        renderer.destroyMesh( mesh_result.value() ).succeeded() &&
        renderer.destroyTexture( alpha_result.value() ).succeeded() &&
        renderer.destroyTexture( source_result.value() ).succeeded();
    if( !resources_released || !waitFence( &renderer, projection_result.value() ) )
    {
        fail( "Submitted Projection resources were not retained safely." );
        return 1;
    }
    const auto frame_result = renderer.readTexture( target_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
    {
        fail( "Projection readback failed: " + frame_result.error().message() );
        return 1;
    }

    // CPU Bilinear／Straight-alpha Referenceと各Channel 1 LSB以内で比較する.
    const auto& output = frame_result.value().data;
    std::uint64_t hash = 14695981039346656037ULL;
    bool pixel_mismatch = false;
    int maximum_lsb_delta = 0;
    for( std::uint32_t y = 0U; y < TARGET_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < TARGET_WIDTH; ++x )
        {
            const double u = ( static_cast< double >( x ) + 0.5 ) / TARGET_WIDTH;
            const double v = ( static_cast< double >( y ) + 0.5 ) / TARGET_HEIGHT;
            const double alpha = sampleLinear( alpha_data, u, v, 3U ) * LAYER_OPACITY;
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            for( std::size_t channel = 0U; channel < 3U; ++channel )
            {
                const int expected = static_cast< int >( std::lround(
                    sampleLinear( color_data, u, v, channel ) * alpha * 255.0 ) );
                const int actual = static_cast< int >( output[ offset + channel ] );
                const int delta = std::abs( expected - actual );
                maximum_lsb_delta = std::max( maximum_lsb_delta, delta );
                pixel_mismatch = pixel_mismatch || delta > 1;
            }
            pixel_mismatch = pixel_mismatch || output[ offset + 3U ] != 255U;
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                hash ^= output[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    if( pixel_mismatch )
    {
        fail( "Projection output differs from the CPU scale/alpha reference." );
    }
    if( backend_name == "d3d12" && hash != EXPECTED_HASH )
    {
        fail( "Projection output hash differs from the documented D3D12 baseline." );
    }

    renderer.destroySurface( surface_result.value() );
    renderer.shutdown();
    std::cout << backend_name << " portable projection RGBA8 FNV-1a: 0x"
              << std::hex << hash << std::dec
              << ", maximum CPU-reference delta: " << maximum_lsb_delta << " LSB\n";
    return g_failures == 0 ? 0 : 1;
}
