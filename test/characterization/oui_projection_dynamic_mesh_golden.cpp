//*****************************************************************************************************************
//!
//! @file    oui_projection_dynamic_mesh_golden.cpp
//! @brief   \~japanese Portable ProjectionのBackend共通Dynamic Mesh更新とFence安全性を検証する.
//! @brief   \~english  Verifies backend-neutral dynamic-mesh updates and fence safety.
//!
//! @details
//!     \~japanese
//!     @n 姉妹Golden Testが1回のPassの画素計算を固定するのに対し、本Testは1本のOpaque Mesh handleへ
//!        2回描画し、その2回の送信の間に`updateMesh()`を挟む点だけが異なる.
//!     @n 送信済みの左半分PassはFence完了まで旧Vertex／Index Bufferを描き続け、後続Passは同じHandleから
//!        新しい右半分形状を解決する. どちらが崩れても2つのTargetは同じ絵になる.
//!     @n これが壊れると、毎FrameでKeystone Meshを更新するProjection応用は、送信済みFrameを次Frameの
//!        形状で描くか、解放済みVertex Bufferを参照して破綻する.
//!     @n 破棄済みHandleへの`updateMesh()`が`ResourceNotFound`を返し、Handleを蘇生させないことも
//!        併せて固定する.
//!
//!     \~english
//!     @n Where the sibling golden tests pin the pixel arithmetic of a single pass, this one differs
//!        only in drawing twice through one opaque mesh handle, with an `updateMesh()` between the two
//!        submissions.
//!     @n The already-submitted left-half pass must keep drawing the old vertex and index buffers
//!        until its fence signals, while the later pass resolves the same handle to the new right-half
//!        shape. If either half of that contract breaks, both targets come out identical.
//!     @n Were this broken, a projection application that re-warps its keystone mesh every frame would
//!        draw an already-submitted frame with the next frame's geometry, or read a freed vertex
//!        buffer.
//!     @n It also pins that `updateMesh()` on a destroyed handle reports `ResourceNotFound` rather
//!        than resurrecting the handle.
//!
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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// TargetのWidthは偶数でなければならない. 半分Meshの境界はNDC 0であり、幅8では画素境界x = 4に
// 正確に落ちるので、Rasteriserの端点丸め規則に依存せず期待値が一意に決まる.
// TIMEOUT_MSはFence待ちの上限だが、CMakeが本TestへCTest TIMEOUT 30秒を設定しているため、実際に
// Fenceが止まった場合はCTestが先にProcessを落とし、"Fence wait failed"は表に出にくい.
// EXPECTED_*_HASHはdoc/design/en/OuiProjection.mdが記録する出力全体のRGBA8 FNV-1aである.
// 1 x 1 Red SourceのNearest samplingは0か255しか出さないので、姉妹TestのようなLSB許容は不要で、
// D3D12 WARPとVulkanが同じ2値へ完全一致する. だからHash検証をBackendで分岐させていない.
constexpr std::uint32_t TARGET_WIDTH  = 8U;
constexpr std::uint32_t TARGET_HEIGHT = 8U;
constexpr std::uint32_t TIMEOUT_MS    = 30000U;
constexpr std::uint64_t EXPECTED_LEFT_HASH  = 0xbbb816f5e80169e5ULL;
constexpr std::uint64_t EXPECTED_RIGHT_HASH = 0xb39c749492b617e5ULL;

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
//! @return Texture handleまたはError.
wse::oui::RendererResult< wse::oui::sTextureHandle > createRedTexture(
    wse::oui::Renderer* const p_renderer_inout )
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

    // Upload Fenceをここで待ち切ってから返す. Sourceの内容がProjection送信前に確定していれば、
    // 出力Hashに残る自由度はMesh形状だけになり、本Testが計るものが1つに絞られる.
    wse::oui::sRendererFrame frame;
    frame.description.extent = description.extent;
    frame.description.format = description.format;
    frame.data = { 255U, 0U, 0U, 255U };
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

//! @brief 左または右半分を覆うMeshを生成する.
//! @param [in] left_in 左半分の場合true.
//! @return Half-screen triangle-list mesh.
wse::oui::sMeshDescription makeHalfMesh( const bool left_in )
{
    // 座標はNDCで、Yは上が+1である. 分割線はNDC 0、すなわちTargetの画素境界に一致する.
    // UVはどちらの形状でも0から1の全面を張るため、覆われた画素は同じRedを読む. 変わるのは
    // 覆う範囲だけであり、2つの出力の違いはMesh差し替えが効いた証拠になる.
    const float left  = left_in ? -1.0F : 0.0F;
    const float right = left_in ? 0.0F : 1.0F;
    wse::oui::sMeshDescription mesh;
    mesh.vertices = {
          { left,   1.0F, 0.0F, 0.0F }
        , { right,  1.0F, 1.0F, 0.0F }
        , { right, -1.0F, 1.0F, 1.0F }
        , { left,  -1.0F, 0.0F, 1.0F }
    };
    mesh.indices = { 0U, 1U, 2U, 0U, 2U, 3U };
    return mesh;
}

//! @brief Projection passを生成する.
//! @param [in] target_in Color attachment.
//! @param [in] mesh_in   Mesh handle.
//! @param [in] source_in Source texture.
//! @return Projection pass.
wse::oui::sProjectionPassDescription makeProjectionPass(
      const wse::oui::sTextureHandle target_in
    , const wse::oui::sMeshHandle    mesh_in
    , const wse::oui::sTextureHandle source_in
)
{
    // Opacity 1.0、Alpha map無し、Edge blend無し、Nearest、supersample_scale既定の1に絞る.
    // 姉妹Testが計る合成やFilterの要素をすべて外し、Mesh形状以外で出力が動かないようにする.
    // final_stateをCopySourceにするのは、Fence後にreadTexture()が追加のTransitionなしで
    // 読み戻せるようにするためである. render_areaは空のままでAttachment全体を指す.
    wse::oui::sProjectionPassDescription pass;
    pass.color_attachment = target_in;
    pass.clear_color       = { 0.0F, 0.0F, 0.0F, 1.0F };
    pass.final_state       = wse::oui::eTextureState::CopySource;
    pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          mesh_in
        , source_in
        , {}
        , wse::oui::eTextureSamplingFilter::Nearest
        , 1.0F
        , {}
    } );
    return pass;
}

//! @brief Half-screen Red Goldenを検証してHashを計算する.
//! @param [in] frame_in Packed RGBA8 frame.
//! @param [in] left_in 左半分がRedの場合true.
//! @return RGBA FNV-1a.
std::uint64_t verifyHalfFrame(
      const wse::oui::sRendererFrame& frame_in
    , const bool                      left_in
)
{
    // 全画素を厳密比較しつつ、同じ列からFNV-1a（Offset basis 14695981039346656037、Prime
    // 1099511628211）を積む. 期待は「覆った側はClear色ではなくRed、覆っていない側はClear色のまま」で、
    // これがMesh形状の差そのものである.
    //
    // Hashは今検証した同じByte列から作るので、Golden Hashの不一致はここのPixel検証を必ず先に失敗させる.
    // Hashが独立に捕まえるものは無く、文書の記録値と目視照合するための出力用である.
    std::uint64_t hash = 14695981039346656037ULL;
    bool pixel_mismatch = false;
    for( std::uint32_t y = 0U; y < TARGET_HEIGHT; ++y )
    {
        for( std::uint32_t x = 0U; x < TARGET_WIDTH; ++x )
        {
            const bool expected_red = left_in ? x < TARGET_WIDTH / 2U : x >= TARGET_WIDTH / 2U;
            const std::size_t offset =
                ( static_cast< std::size_t >( y ) * TARGET_WIDTH + x ) * 4U;
            pixel_mismatch = pixel_mismatch ||
                frame_in.data[ offset ] != ( expected_red ? 255U : 0U ) ||
                frame_in.data[ offset + 1U ] != 0U ||
                frame_in.data[ offset + 2U ] != 0U ||
                frame_in.data[ offset + 3U ] != 255U;
            for( std::size_t channel = 0U; channel < 4U; ++channel )
            {
                hash ^= frame_in.data[ offset + channel ];
                hash *= 1099511628211ULL;
            }
        }
    }
    if( pixel_mismatch )
    {
        fail( left_in ? "Initial mesh output differs from the left-half reference."
                      : "Updated mesh output differs from the right-half reference." );
    }
    return hash;
}

} // namespace

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    // 選択Backend、Red source、左半分Meshおよび2つのTargetを用意する.
    // Backend引数が無い、または未知の場合はSkipではなくFailである. Backendの取り違えを黙って
    // 見逃さないことを優先している. Backendの選択自体はCMakeが行い、WindowsはD3D12、他はVulkanになる.
    // 2つのTargetを別々に用意するのが本Testの肝で、同じTargetへ2回描くと後の描画が前を潰してしまい、
    // 更新前後の分離が観測できなくなる.
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
    const auto source_result = createRedTexture( &renderer );
    const auto mesh_result = renderer.createMesh( makeHalfMesh( true ) );
    wse::oui::sSurfaceDescription target_description;
    target_description.extent = { TARGET_WIDTH, TARGET_HEIGHT };
    const auto first_surface_result = renderer.createSurface( target_description );
    const auto second_surface_result = renderer.createSurface( target_description );
    const auto first_target_result = first_surface_result.succeeded()
        ? renderer.getSurfaceTexture( first_surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( first_surface_result.error() );
    const auto second_target_result = second_surface_result.succeeded()
        ? renderer.getSurfaceTexture( second_surface_result.value() )
        : wse::oui::RendererResult< wse::oui::sTextureHandle >::failure( second_surface_result.error() );
    if( !source_result.succeeded() || !mesh_result.succeeded() ||
        !first_target_result.succeeded() || !second_target_result.succeeded() )
    {
        fail( "Dynamic-mesh projection resources could not be created." );
        return 1;
    }

    // 左Mesh送信後、Fence待機前に同一Handleを右Meshへ交換して再送信する.
    // 待たずに更新するのが要点である. Backendは送信ごとに旧Vertex／Index Bufferへの参照を保持し、
    // updateMesh()は既存Recordを書き換えるのではなく、新しいRecordを作り切ってから差し替える.
    // 更新が破壊的なら、この時点で1本目のPassが読む先が右半分Meshに化ける.
    const auto first_projection_result = wse::oui::ProjectionPipeline::execute(
          &renderer
        , makeProjectionPass(
              first_target_result.value(), mesh_result.value(), source_result.value() )
    );
    const auto update_result = renderer.updateMesh(
        mesh_result.value(), makeHalfMesh( false ) );
    const auto second_projection_result = wse::oui::ProjectionPipeline::execute(
          &renderer
        , makeProjectionPass(
              second_target_result.value(), mesh_result.value(), source_result.value() )
    );
    if( !first_projection_result.succeeded() || !update_result.succeeded() ||
        !second_projection_result.succeeded() )
    {
        fail( "Dynamic-mesh projection submission or update failed." );
        return 1;
    }

    // Handle破棄後も旧／新Bufferが各Fenceまで保持され、破棄済みHandle更新は失敗する.
    // 未完了の送信が2本ある状態でHandleを捨てる. 破棄はHandle表からの削除であって、Backendが
    // 送信へ結び付けて持っている参照は生き続ける、という契約を突く配置である.
    // 続く更新は破棄済みHandleへの操作なので、成功してはならず、Error codeもResourceNotFoundで
    // なければならない. ここを緩めると、生成し直したHandleが偶然同じ値で再利用された時に
    // 別Resourceを書き換えてしまう.
    // 最後に両Fenceを待ってから読み戻す. なお条件は左から短絡するため、前段が偽ならFence待ちは
    // 実行されない.
    const bool resources_released =
        renderer.destroyMesh( mesh_result.value() ).succeeded() &&
        renderer.destroyTexture( source_result.value() ).succeeded();
    const auto destroyed_update_result = renderer.updateMesh(
        mesh_result.value(), makeHalfMesh( true ) );
    if( !resources_released || destroyed_update_result.succeeded() ||
        destroyed_update_result.error().code() !=
            wse::oui::eRendererErrorCode::ResourceNotFound ||
        !waitFence( &renderer, first_projection_result.value() ) ||
        !waitFence( &renderer, second_projection_result.value() ) )
    {
        fail( "Dynamic-mesh resource retention or destroyed-handle contract failed." );
        return 1;
    }

    // 2つのTargetを読み戻し、旧形状と新形状が別々に残っていることを確かめる.
    // 保持契約が壊れていれば、両方が同じ絵になるか、解放済みBufferを読んだ不定な絵になる.
    const auto first_frame_result =
        renderer.readTexture( first_target_result.value(), TIMEOUT_MS );
    const auto second_frame_result =
        renderer.readTexture( second_target_result.value(), TIMEOUT_MS );
    if( !first_frame_result.succeeded() || !second_frame_result.succeeded() )
    {
        fail( "Dynamic-mesh projection readback failed." );
        return 1;
    }
    const std::uint64_t left_hash = verifyHalfFrame( first_frame_result.value(), true );
    const std::uint64_t right_hash = verifyHalfFrame( second_frame_result.value(), false );
    if( left_hash != EXPECTED_LEFT_HASH || right_hash != EXPECTED_RIGHT_HASH )
    {
        fail( "Dynamic-mesh output hash differs from the documented baseline." );
    }

    // Surfaceを逆順に解放してShutdownし、実測Hashを標準出力へ残す.
    // Baselineを更新する時はこの出力をdoc/design配下の記録値へ写す.
    //
    // なお本Testが走るのはSoftware adapter（WARPまたはSoftware Vulkan）であり、実GPUではない.
    // よって固定できるのは論理的な保持契約と画素値だけで、実Driverの遅延解放、Memory再利用による
    // Use-after-freeの顕在化、実機固有のTimingは対象外である.
    renderer.destroySurface( second_surface_result.value() );
    renderer.destroySurface( first_surface_result.value() );
    renderer.shutdown();
    std::cout << backend_name << " projection dynamic mesh left/right RGBA8 FNV-1a: 0x"
              << std::hex << left_hash << " / 0x" << right_hash << std::dec << '\n';
    return g_failures == 0 ? 0 : 1;
}
