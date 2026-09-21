//*****************************************************************************************************************
//!
//! @file    oui_projection_supersample_golden.cpp
//! @brief   \~japanese Portable ProjectionのBackend共通2倍AAとR8 Alpha mapを検証する.
//! @brief   \~english  Verifies backend-neutral portable projection 2x AA and R8 alpha maps.
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

constexpr std::uint32_t SOURCE_WIDTH       = 2U;
constexpr std::uint32_t SOURCE_HEIGHT      = 2U;
constexpr std::uint32_t TARGET_WIDTH       = 4U;
constexpr std::uint32_t TARGET_HEIGHT      = 4U;
constexpr std::uint32_t SUPERSAMPLE_SCALE  = 2U;
constexpr std::uint32_t INTERMEDIATE_WIDTH = TARGET_WIDTH * SUPERSAMPLE_SCALE;
constexpr std::uint32_t INTERMEDIATE_HEIGHT = TARGET_HEIGHT * SUPERSAMPLE_SCALE;
constexpr std::uint32_t TIMEOUT_MS         = 30000U;
constexpr float         LAYER_OPACITY      = 0.75F;
constexpr std::uint64_t EXPECTED_HASH      = 0xa3ccfe45a48e2d20ULL;

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

//! @brief Packed FrameをSampled textureへUploadする.
//! @param [in,out] p_renderer_inout Renderer.
//! @param [in]     format_in        Pixel format.
//! @param [in]     data_in          Packed pixels.
//! @return Texture handleまたはError.
wse::oui::RendererResult< wse::oui::sTextureHandle > createUploadedTexture(
          wse::oui::Renderer* const        p_renderer_inout
    , const wse::oui::eRendererPixelFormat format_in
    , const std::vector< std::uint8_t >&   data_in
)
{
    wse::oui::sTextureDescription description;
    description.extent = { SOURCE_WIDTH, SOURCE_HEIGHT };
    description.format = format_in;
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

//! @brief D3D clamp Linear samplingのCPU Referenceを計算する.
//! @param [in] data_in         Packed pixel data.
//! @param [in] width_in        Width [pixel].
//! @param [in] height_in       Height [pixel].
//! @param [in] channel_count_in Channel count.
//! @param [in] u_in            Normalized U.
//! @param [in] v_in            Normalized V.
//! @param [in] channel_in      Channel index.
//! @return Normalized sampled value.
double sampleLinear(
      const std::vector< std::uint8_t >& data_in
    , const std::uint32_t                width_in
    , const std::uint32_t                height_in
    , const std::size_t                  channel_count_in
    , const double                       u_in
    , const double                       v_in
    , const std::size_t                  channel_in
)
{
    const double maximum_x = static_cast< double >( width_in - 1U );
    const double maximum_y = static_cast< double >( height_in - 1U );
    const double source_x = std::clamp( u_in * width_in - 0.5, 0.0, maximum_x );
    const double source_y = std::clamp( v_in * height_in - 0.5, 0.0, maximum_y );
    const std::size_t x0 = static_cast< std::size_t >( std::floor( source_x ) );
    const std::size_t y0 = static_cast< std::size_t >( std::floor( source_y ) );
    const std::size_t x1 = std::min< std::size_t >( x0 + 1U, width_in - 1U );
    const std::size_t y1 = std::min< std::size_t >( y0 + 1U, height_in - 1U );
    const double weight_x = source_x - static_cast< double >( x0 );
    const double weight_y = source_y - static_cast< double >( y0 );
    const auto read_channel = [&data_in, width_in, channel_count_in, channel_in](
        const std::size_t x_in, const std::size_t y_in )
    {
        const std::size_t offset =
            ( y_in * static_cast< std::size_t >( width_in ) + x_in ) * channel_count_in;
        return static_cast< double >( data_in[ offset + channel_in ] ) / 255.0;
    };
    const double top = read_channel( x0, y0 ) * ( 1.0 - weight_x ) +
        read_channel( x1, y0 ) * weight_x;
    const double bottom = read_channel( x0, y1 ) * ( 1.0 - weight_x ) +
        read_channel( x1, y1 ) * weight_x;
    return top * ( 1.0 - weight_y ) + bottom * weight_y;
}

//! @brief 2倍Projection中間RGBA8画像のCPU Referenceを生成する.
//! @param [in] color_data_in Packed RGBA source.
//! @param [in] alpha_data_in Packed R8 alpha map.
//! @return Packed intermediate RGBA image.
std::vector< std::uint8_t > makeIntermediateReference(
      const std::vector< std::uint8_t >& color_data_in
    , const std::vector< std::uint8_t >& alpha_data_in
)
{
    std::vector< std::uint8_t > result(
        static_cast< std::size_t >( INTERMEDIATE_WIDTH ) * INTERMEDIATE_HEIGHT * 4U );
    for( std::uint32_t y = 0U; y < INTERMEDIATE_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < INTERMEDIATE_WIDTH; ++x )
        {
            const double u = ( static_cast< double >( x ) + 0.5 ) / INTERMEDIATE_WIDTH;
            const double v = ( static_cast< double >( y ) + 0.5 ) / INTERMEDIATE_HEIGHT;
            const double alpha = sampleLinear(
                alpha_data_in, SOURCE_WIDTH, SOURCE_HEIGHT, 1U, u, v, 0U ) *
                LAYER_OPACITY;
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * INTERMEDIATE_WIDTH + x ) * 4U;
            for( std::size_t channel = 0U; channel < 3U; ++channel )
            {
                result[ offset + channel ] = static_cast< std::uint8_t >( std::lround(
                    sampleLinear(
                        color_data_in, SOURCE_WIDTH, SOURCE_HEIGHT, 4U, u, v, channel ) *
                    alpha * 255.0 ) );
            }
            result[ offset + 3U ] = 255U;
        }
    }
    return result;
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    // 選択Backend、RGBA Color Sourceおよび単Channel R8 Alpha mapを用意する.
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
    const std::vector< std::uint8_t > alpha_data = { 255U, 192U, 128U, 64U };
    const auto source_result = createUploadedTexture(
        &renderer, wse::oui::eRendererPixelFormat::Rgba8Unorm, color_data );
    const auto alpha_result = createUploadedTexture(
        &renderer, wse::oui::eRendererPixelFormat::R8Unorm, alpha_data );
    if( !source_result.succeeded() || !alpha_result.succeeded() )
    {
        fail( "Projection source or R8 alpha-map upload failed." );
        return 1;
    }

    // Legacy Fullscreen meshをPortable Meshへ変換する.
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

    // 2倍中間TargetへProjectionし、Linear samplingで最終Targetへ縮小する.
    wse::oui::sProjectionPassDescription projection_pass;
    projection_pass.color_attachment  = target_result.value();
    projection_pass.render_area.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    projection_pass.clear_color        = { 0.0F, 0.0F, 0.0F, 1.0F };
    projection_pass.final_state        = wse::oui::eTextureState::CopySource;
    projection_pass.supersample_scale  = SUPERSAMPLE_SCALE;
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
        fail( "Supersampled projection render failed: " +
            projection_result.error().message() );
        return 1;
    }
    const bool resources_released =
        renderer.destroyMesh( mesh_result.value() ).succeeded() &&
        renderer.destroyTexture( alpha_result.value() ).succeeded() &&
        renderer.destroyTexture( source_result.value() ).succeeded();
    if( !resources_released || !waitFence( &renderer, projection_result.value() ) )
    {
        fail( "Submitted supersample resources were not retained safely." );
        return 1;
    }
    const auto frame_result = renderer.readTexture( target_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
    {
        fail( "Projection readback failed: " + frame_result.error().message() );
        return 1;
    }

    // CPU 2段Linear／R8 Alpha Referenceと各Channel 2 LSB以内で比較する.
    //
    // 他のGolden Testは1段のFilterであり1 LSBで足りるが、ここだけは2段である。GPUもCPU Referenceも
    // 中間Bufferを8bitへ丸め、出力でもう一度丸める。中間で生じた1 LSBの差は2段目のFilterを通っても
    // 1 LSBのままであり、そこへ出力側の丸めが最大0.5 LSB加わる。したがって正しい実装どうしでも
    // 2 LSBまで離れうる。1 LSBという上限は、たまたまD3D12とSoftware Rasteriserで成立していただけで、
    // 実機のTile GPUでは成立しなかった。
    //
    // Every other golden test filters once and one LSB is enough for it. This one filters twice: the
    // GPU and the CPU reference both round the intermediate buffer to eight bits and round again at
    // the output. A one-LSB difference in the intermediate stays one LSB through the second filter,
    // and the output rounding adds up to half an LSB on top, so two correct implementations can sit
    // two LSB apart. The one-LSB bound held on Direct3D and on a software rasteriser by luck, and
    // did not hold on a real tiled GPU.
    constexpr int MAXIMUM_REFERENCE_DELTA = 2;
    const std::vector< std::uint8_t > intermediate =
        makeIntermediateReference( color_data, alpha_data );
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
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                const int expected = static_cast< int >( std::lround( sampleLinear(
                    intermediate, INTERMEDIATE_WIDTH, INTERMEDIATE_HEIGHT,
                    4U, u, v, channel ) * 255.0 ) );
                const int actual = static_cast< int >( output[ offset + channel ] );
                const int delta = std::abs( expected - actual );
                maximum_lsb_delta = std::max( maximum_lsb_delta, delta );
                pixel_mismatch = pixel_mismatch || delta > MAXIMUM_REFERENCE_DELTA;
                hash ^= output[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    if( pixel_mismatch )
    {
        fail( "Projection output differs from the CPU supersample/R8 reference." );
    }
    if( backend_name == "d3d12" && hash != EXPECTED_HASH )
    {
        fail( "Projection output hash differs from the documented supersample baseline." );
    }

    renderer.destroySurface( surface_result.value() );
    renderer.shutdown();
    std::cout << backend_name << " supersampled projection RGBA8 FNV-1a: 0x"
              << std::hex << hash << std::dec
              << ", maximum CPU-reference delta: " << maximum_lsb_delta << " LSB\n";
    return g_failures == 0 ? 0 : 1;
}
