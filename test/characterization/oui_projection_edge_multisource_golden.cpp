//*****************************************************************************************************************
//!
//! @file    oui_projection_edge_multisource_golden.cpp
//! @brief   \~japanese Portable ProjectionのBackend共通Edge blendと順序付きMulti-source合成を検証する.
//! @brief   \~english  Verifies backend-neutral edge blend and ordered multi-source composition.
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

constexpr std::uint32_t TARGET_WIDTH  = 8U;
constexpr std::uint32_t TARGET_HEIGHT = 4U;
constexpr std::uint32_t TIMEOUT_MS    = 30000U;
constexpr float         BLUE_OPACITY  = 0.75F;
constexpr std::uint64_t EXPECTED_HASH = 0x17b97761907effd5ULL;

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

//! @brief 1 x 1 RGBA Color textureを生成する.
//! @param [in,out] p_renderer_inout Renderer.
//! @param [in]     color_in         Packed RGBA color.
//! @return Texture handleまたはError.
wse::oui::RendererResult< wse::oui::sTextureHandle > createColorTexture(
          wse::oui::Renderer* const             p_renderer_inout
    , const std::vector< std::uint8_t >&        color_in
)
{
    wse::oui::sTextureDescription description;
    description.extent = { 1U, 1U };
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
    frame.data = color_in;
    const auto upload_result =
        p_renderer_inout->uploadTexture( texture_result.value(), frame );
    if( !upload_result.succeeded() )
    {
        p_renderer_inout->destroyTexture( texture_result.value() );
        return wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( upload_result.error() );
    }
    const auto wait_result =
        p_renderer_inout->waitFence( upload_result.value(), TIMEOUT_MS );
    if( !wait_result.succeeded() )
    {
        p_renderer_inout->destroyTexture( texture_result.value() );
        return wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( wait_result.error() );
    }
    return texture_result;
}

//! @brief Smoothstep edge係数を計算する.
//! @param [in] value_in 正規化前のEdge距離.
//! @return 0から1の係数.
double smoothstep( const double value_in )
{
    const double value = std::clamp( value_in, 0.0, 1.0 );
    return value * value * ( 3.0 - 2.0 * value );
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    // 選択Backend、Red／Blue source、Fullscreen meshを用意する.
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
    const auto red_result = createColorTexture(
        &renderer, { 255U, 0U, 0U, 255U } );
    const auto blue_result = createColorTexture(
        &renderer, { 0U, 0U, 255U, 255U } );

    wse::oui::sMeshDescription mesh_description;
    mesh_description.vertices = {
          { -1.0F,  1.0F, 0.0F, 0.0F }
        , {  1.0F,  1.0F, 1.0F, 0.0F }
        , {  1.0F, -1.0F, 1.0F, 1.0F }
        , { -1.0F, -1.0F, 0.0F, 1.0F }
    };
    mesh_description.indices = { 0U, 1U, 2U, 0U, 2U, 3U };
    const auto mesh_result = renderer.createMesh( mesh_description );

    wse::oui::sSurfaceDescription target_description;
    target_description.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    const auto surface_result = renderer.createSurface( target_description );
    const auto target_result = surface_result.succeeded()
        ? renderer.getSurfaceTexture( surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( surface_result.error() );
    if( !red_result.succeeded() || !blue_result.succeeded() ||
        !mesh_result.succeeded() || !target_result.succeeded() )
    {
        fail( "Projection source, mesh, or target creation failed." );
        return 1;
    }

    // Red right-fade後にBlue left-fadeを描き、Layer順序を明示する.
    wse::oui::sProjectionPassDescription projection_pass;
    projection_pass.color_attachment = target_result.value();
    projection_pass.clear_color       = { 0.0F, 0.0F, 0.0F, 1.0F };
    projection_pass.final_state       = wse::oui::eTextureState::CopySource;
    projection_pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_result.value()
        , red_result.value()
        , {}
        , wse::oui::eTextureSamplingFilter::Nearest
        , 1.0F
        , { 0.0F, 1.0F, 0.0F, 0.0F, wse::oui::eEdgeBlendCurve::Smoothstep }
    } );
    projection_pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_result.value()
        , blue_result.value()
        , {}
        , wse::oui::eTextureSamplingFilter::Nearest
        , BLUE_OPACITY
        , { 1.0F, 0.0F, 0.0F, 0.0F, wse::oui::eEdgeBlendCurve::Smoothstep }
    } );
    const auto projection_result =
        wse::oui::ProjectionPipeline::execute( &renderer, projection_pass );
    if( !projection_result.succeeded() )
    {
        fail( "Edge/multi-source projection render failed: " +
            projection_result.error().message() );
        return 1;
    }
    const bool resources_released =
        renderer.destroyMesh( mesh_result.value() ).succeeded() &&
        renderer.destroyTexture( blue_result.value() ).succeeded() &&
        renderer.destroyTexture( red_result.value() ).succeeded();
    if( !resources_released || !waitFence( &renderer, projection_result.value() ) )
    {
        fail( "Submitted edge/multi-source resources were not retained safely." );
        return 1;
    }
    const auto frame_result = renderer.readTexture( target_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
    {
        fail( "Projection readback failed: " + frame_result.error().message() );
        return 1;
    }

    // CPU Smoothstep／順序付きStraight-alpha Referenceと1 LSB以内で比較する.
    const auto& output = frame_result.value().data;
    std::uint64_t hash = 14695981039346656037ULL;
    bool pixel_mismatch = false;
    int maximum_lsb_delta = 0;
    for( std::uint32_t y = 0U; y < TARGET_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < TARGET_WIDTH; ++x )
        {
            const double u = ( static_cast< double >( x ) + 0.5 ) / TARGET_WIDTH;
            const double red_alpha = smoothstep( 1.0 - u );
            const double blue_alpha = smoothstep( u ) * BLUE_OPACITY;
            const double red_stored = std::lround( red_alpha * 255.0 ) / 255.0;
            const int expected_red = static_cast< int >( std::lround(
                red_stored * ( 1.0 - blue_alpha ) * 255.0 ) );
            const int expected_blue = static_cast< int >( std::lround(
                blue_alpha * 255.0 ) );
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            const int red_delta =
                std::abs( expected_red - static_cast< int >( output[ offset ] ) );
            const int blue_delta = std::abs(
                expected_blue - static_cast< int >( output[ offset + 2U ] ) );
            maximum_lsb_delta = std::max(
                maximum_lsb_delta, std::max( red_delta, blue_delta ) );
            pixel_mismatch = pixel_mismatch || red_delta > 1 ||
                output[ offset + 1U ] != 0U ||
                blue_delta > 1 ||
                output[ offset + 3U ] != 255U;
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                hash ^= output[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    if( pixel_mismatch )
    {
        fail( "Projection output differs from the CPU edge/multi-source reference." );
    }
    if( backend_name == "d3d12" && hash != EXPECTED_HASH )
    {
        fail( "Projection output hash differs from the documented edge/multi-source baseline." );
    }

    renderer.destroySurface( surface_result.value() );
    renderer.shutdown();
    std::cout << backend_name << " projection edge/multi-source RGBA8 FNV-1a: 0x"
              << std::hex << hash << std::dec
              << ", maximum CPU-reference delta: " << maximum_lsb_delta << " LSB\n";
    return g_failures == 0 ? 0 : 1;
}
