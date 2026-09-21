//*****************************************************************************************************************
//!
//! @file    oui_d3d12_mesh_golden.cpp
//! @brief   \~japanese Portable D3D12 Texture upload／Mesh描画を決定論的出力で検証する.
//!                    Texelの取り込み、Indexed meshのRaster化、Nearest sampling、および送信済み
//!                    ResourceをFence完了まで保持する契約を1本で押さえる. これが壊れた製品は、
//!                    投影面がClear色のまま出るか、Handle破棄と同時にGPUが読んでいるResourceを
//!                    解放して落ちる.
//! @brief   \~english  Verifies portable D3D12 texture upload and mesh drawing with deterministic output.
//!                    It pins texel upload, indexed mesh rasterization, nearest sampling, and the
//!                    retention of submitted resources until their fence completes. A regression
//!                    here ships a projection that stays at the clear color, or one that frees a
//!                    resource the GPU is still reading the moment the handle is destroyed.
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

#include <oui/renderer/Renderer.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace
{

// 2 x 2 Sourceを8 x 8 Targetへ描くと1 Texelがちょうど4 x 4 Pixelの塊になる. 整数倍なので
// Nearest samplingのTexel中心が境界に乗らず、Rasterizerの丸め方に依らない完全一致を要求できる.
// TIMEOUT_MSはWARPのSoftware描画がCIで遅い場合に備えた上限で、CMakeLists側のCTest TIMEOUT 30 sと
// 同値である. したがってFenceが本当に止まった場合はCTestが先に打ち切る.
// EXPECTED_HASHは製品側で計算した値ではなく、doc/design/{en,ja}/OuiRenderer.mdへ記録済みの
// WARP基準値である. 更新にはDocumentの基準値も同時に書き換える必要がある.
constexpr std::uint32_t SOURCE_WIDTH  = 2U;
constexpr std::uint32_t SOURCE_HEIGHT = 2U;
constexpr std::uint32_t TARGET_WIDTH  = 8U;
constexpr std::uint32_t TARGET_HEIGHT = 8U;
constexpr std::uint32_t TIMEOUT_MS    = 30000U;
constexpr std::uint64_t EXPECTED_HASH = 0x5854021a152e5ba5ULL;

int g_failures = 0;

void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

bool waitFence(
          wse::oui::Renderer* const  p_renderer_inout
    , const wse::oui::sFenceHandle   fence_in
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

} // namespace

int main()
{
    // WARPでShader／Mesh capabilityを持つPortable Rendererを初期化する.
    // use_software_adapterはWARPを選ぶ指定で、出力を実GPU Driverから切り離して決定論にする.
    // 引き換えに実Driverの最適化、Vendor固有のRaster規則、実Displayへの出力はこのTestの
    // 対象外になる. Mesh capabilityが無いBackendでは以降の期待Pixelが意味を持たないため、
    // Skipではなく失敗として即座に落とす.
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
        !capabilities_result.value().supports_mesh_rendering )
    {
        fail( "D3D12 mesh capability is unavailable." );
        return 1;
    }

    // 2 x 2 RGBA SourceをUploadしてPoint samplingの基準を固定する.
    // 4 Texelを赤／緑／青／白と全く別の色にしてあるので、UVの取り違えやRow pitch由来の
    // ずれがあれば出力Quadrantの色が入れ替わって必ず露見する.
    // Sampled | TransferDestinationとCopyDestination初期Stateは、Upload後にShader読取りへ
    // 遷移させる経路をBackendへ要求するための最小組合せである.
    wse::oui::sTextureDescription source_description;
    source_description.extent = { SOURCE_WIDTH, SOURCE_HEIGHT };
    source_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    source_description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    source_description.initial_state = wse::oui::eTextureState::CopyDestination;
    const auto source_result = renderer.createTexture( source_description );
    if( !source_result.succeeded() )
    {
        fail( "Source texture creation failed: " + source_result.error().message() );
        return 1;
    }
    wse::oui::sRendererFrame source_frame;
    source_frame.description.extent = source_description.extent;
    source_frame.description.format = source_description.format;
    source_frame.data = {
          255U,   0U,   0U, 255U
        ,   0U, 255U,   0U, 255U
        ,   0U,   0U, 255U, 255U
        , 255U, 255U, 255U, 255U
    };
    const auto upload_result = renderer.uploadTexture( source_result.value(), source_frame );
    if( !upload_result.succeeded() || !waitFence( &renderer, upload_result.value() ) )
    {
        fail( "Source texture upload failed." );
        return 1;
    }

    // Fullscreen Quadを2 TriangleのPortable Meshとして生成する.
    // Vertexは{ NDC x, NDC y, u, v }で、上端y = +1にv = 0を割り当てる. これがPortable層の
    // Y軸向き契約であり、逆にすると出力が上下反転してQuadrant比較で落ちる.
    // Indexを{ 0,1,2, 0,2,3 }と共有4 Vertexで与えることで、Index bufferが実際に使われている
    // ことも同時に押さえる.
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
        fail( "Mesh creation failed: " + mesh_result.error().message() );
        return 1;
    }

    // 描画先はSurface既定のOffscreenのままにして、Window表示もDisplayも要求しない.
    // 描画Targetの所有はSurface側に残るので、後段では個別解放ではなくSurface解放で片付ける.
    wse::oui::sSurfaceDescription target_description;
    target_description.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    const auto target_surface_result = renderer.createSurface( target_description );
    if( !target_surface_result.succeeded() )
    {
        fail( "Target surface creation failed: " + target_surface_result.error().message() );
        return 1;
    }
    const auto target_result = renderer.getSurfaceTexture( target_surface_result.value() );
    if( !target_result.succeeded() )
    {
        fail( "Target texture lookup failed: " + target_result.error().message() );
        return 1;
    }

    // Clear＋Textured Mesh drawを1 Render passへ送信する.
    // Clear色のMagentaはSourceの4色いずれとも異なるので、Meshが1 Pixelも描かれなかった場合に
    // 出力がMagentaのまま残り、後段の完全一致比較が確実に落ちる目印になる.
    // final_stateをCopySourceにしておくと、Readbackが余分なBarrierを挟まずに走る.
    wse::oui::sRenderPassDescription render_pass;
    render_pass.color_attachment = target_result.value();
    render_pass.load_operation    = wse::oui::eAttachmentLoadOperation::Clear;
    render_pass.clear_color       = { 1.0F, 0.0F, 1.0F, 1.0F };
    render_pass.draw_commands.emplace_back(
        wse::oui::sMeshDrawCommand{ mesh_result.value(), source_result.value() } );
    render_pass.final_state = wse::oui::eTextureState::CopySource;
    const auto render_result = renderer.executeRenderPass( render_pass );
    if( !render_result.succeeded() )
    {
        fail( "Mesh render pass failed: " + render_result.error().message() );
        return 1;
    }

    // Submit後のHandle解放でもGPU ResourceはFence完了まで保持される.
    // Fence待機より前にMeshとSource textureを捨てるのが要点で、Backendが送信中Resourceを
    // 保持していなければGPUが読んでいる最中に解放されてDevice removedかGolden不一致になる.
    // 続く2回目のdestroyMeshは失敗しなければならない. Handleは既に無効で、成功してしまうと
    // 二重解放を許す実装だということになる.
    if( !renderer.destroyMesh( mesh_result.value() ).succeeded() ||
        !renderer.destroyTexture( source_result.value() ).succeeded() ||
        !waitFence( &renderer, render_result.value() ) )
    {
        fail( "Submitted mesh resources were not released safely." );
        return 1;
    }
    if( renderer.destroyMesh( mesh_result.value() ).succeeded() )
    {
        fail( "Stale mesh handle did not fail safely." );
    }

    // Source／Meshを解放済みのままReadbackする. 1 Rowは8 x 4 = 32 Byteしかなく、D3D12の
    // Readback footprintが要求するRow整列とは一致しないため、Packed pitchへ詰め直す経路も通る.
    const auto frame_result = renderer.readTexture( target_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
    {
        fail( "Mesh output readback failed: " + frame_result.error().message() );
        return 1;
    }

    // 2 x 2 Sourceが4 x 4 QuadrantへPoint拡大されることを完全一致で検査する.
    // 期待はUV割当てそのままで、左上=赤、右上=緑、左下=青、右下=白になる. 下の三項式は
    // その4色をRGB成分へ展開したものであり、AlphaはSourceの255がそのまま出ることを見る.
    // 許容差を置かないのは、整数倍のNearest拡大に補間も丸めも入らないためである.
    const auto& output = frame_result.value().data;
    if( output.size() != static_cast< std::size_t >( TARGET_WIDTH ) * TARGET_HEIGHT * 4U )
    {
        fail( "Mesh output frame size is invalid." );
        return 1;
    }
    std::uint64_t hash = 14695981039346656037ULL;
    for( std::uint32_t y = 0U; y < TARGET_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < TARGET_WIDTH; ++x )
        {
            const bool right  = x >= TARGET_WIDTH / 2U;
            const bool bottom = y >= TARGET_HEIGHT / 2U;
            const std::uint8_t expected_red   = bottom ? ( right ? 255U : 0U ) :
                ( right ? 0U : 255U );
            const std::uint8_t expected_green = right ? 255U : 0U;
            const std::uint8_t expected_blue  = bottom ? 255U : 0U;
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            if( output[ offset ] != expected_red ||
                output[ offset + 1U ] != expected_green ||
                output[ offset + 2U ] != expected_blue ||
                output[ offset + 3U ] != 255U )
            {
                fail( "Mesh output pixel differs from the expected quadrant." );
                y = TARGET_HEIGHT;
                break;
            }
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                hash ^= output[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    // Hashは上のPixel比較と同じ全256 Byteから作るため、独立した検出力を足すものではない.
    // 役割はDocumentへ記録した基準値との突合せと、失敗時に実測値を標準出力の1行で人へ渡す
    // ことにある.
    if( hash != EXPECTED_HASH )
    {
        fail( "Mesh output hash differs from the documented D3D12 baseline." );
    }

    renderer.destroySurface( target_surface_result.value() );
    renderer.shutdown();
    std::cout << "D3D12 portable mesh RGBA8 FNV-1a: 0x"
              << std::hex << hash << std::dec << '\n';
    return g_failures == 0 ? 0 : 1;
}
