//*****************************************************************************************************************
//!
//! @file    oui_renderer_contract.cpp
//! @brief   \~japanese Backend非依存OUI Renderer descriptorとError契約を検証する.
//! @brief   \~english  Verifies backend-independent OUI renderer descriptor and error contracts.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
//!
//! @details
//!     \~japanese
//!         このTestが守っているのは、Backendを1つも起動せずにDescriptorの可否を決める層である。
//!      @n Frame Layout、Texture Usage、Mesh Index、Render Pass、Surface、Display Modeの各Descriptorは
//!      @n Backendへ渡る前にここで止まる。この層が緩むと、同じ設定がD3D12では通りVulkanでは落ちる、
//!      @n あるいはDriver内部でCrashするという移植性の無い失敗へ変わる。
//!      @n 併せて、未初期化Rendererと未対応Backendが例外やCrashではなく構造化Errorとして
//!      @n 観測できること、Move後も安全であることを固定する。
//!      @n GPUもDisplayも要らないため、この契約はどのBuild機でも必ず実行される。
//!     \~english
//!         Pins down the layer that decides whether a descriptor is acceptable without starting any
//!         backend. Frame layout, texture usage, mesh indices, render passes, surfaces and display
//!         modes are all stopped here before they reach a backend. If this layer weakens, the same
//!         settings pass on D3D12 and fail on Vulkan, or crash inside a driver: a non-portable
//!         failure. It also fixes that an uninitialized renderer and an unsupported backend report
//!         structured errors rather than throwing or crashing, and that a moved-from renderer stays
//!         safe. No GPU and no display are needed, so this contract runs on every build machine.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/renderer/Renderer.h>
#include <oui/renderer/ProjectionPipeline.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

namespace
{

int g_failures = 0;

//! 失敗しても打ち切らず数え上げる. 各条項は互いに独立なので、1回の実行で壊れた契約を
//! すべて並べたほうが原因を絞り込みやすい.
void expect( const bool condition_in, const char* const message_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++g_failures;
    }
}

} // namespace

int main()
{
    // Frame layoutはPacked／Padded rowと不正Pitchを区別する.
    // 期待値はすべて1920x1080 RGBA8から手計算で出る. 7680 = 1920 * 4 Byteが最小Row pitch、
    // 8294400 = 7680 * 1080が詰まったFrame、8640000 = 8000 * 1080がPadding付きFrameのSizeである.
    // ここが狂うとCPU側のBufferとGPU側のCopyでSizeが食い違い、はみ出しかDataの欠けになる.
    wse::oui::sRendererFrameDescription frame_description;
    frame_description.extent = { 1920U, 1080U };
    frame_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    expect( frame_description.bytesPerPixel() == 4U, "RGBA8 bytes per pixel" );
    expect( frame_description.minimumRowPitch() == 7680U, "RGBA8 minimum row pitch" );
    expect( frame_description.memorySize() == 8294400U, "RGBA8 packed frame size" );
    expect( wse::oui::validateRendererFrameDescription( frame_description ).ok(),
        "Valid frame description" );
    frame_description.row_pitch = 8000U;
    expect( frame_description.memorySize() == 8640000U, "Padded frame size" );
    frame_description.row_pitch = 100U;
    expect( !wse::oui::validateRendererFrameDescription( frame_description ).ok(),
        "Undersized row pitch is rejected" );
    // Descriptorだけでなく、実Dataの長さもDescriptorと一致していなければ受け付けない.
    // 2x2 RGBA8は16 Byteちょうどであり、15 Byteは1 Byte足りない読み出しになるため拒否される.
    wse::oui::sRendererFrame frame;
    frame.description.extent = { 2U, 2U };
    frame.description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    frame.data.resize( 15U );
    expect( !wse::oui::validateRendererFrame( frame ).ok(),
        "Frame data-size mismatch is rejected" );
    frame.data.resize( 16U );
    expect( wse::oui::validateRendererFrame( frame ).ok(), "Valid frame data" );

    // Texture Stateは対応Usageを必須とする.
    // 初期StateがRenderTargetなのにRenderTarget Usageが無いTextureは、D3D12でもVulkanでも
    // Resource生成後にBarrierが組めなくなる. その矛盾をDescriptorの段階で断つ.
    wse::oui::sTextureDescription texture_description;
    texture_description.extent = { 64U, 64U };
    texture_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    texture_description.usage = wse::oui::eTextureUsage::RenderTarget |
        wse::oui::eTextureUsage::TransferSource;
    texture_description.initial_state = wse::oui::eTextureState::RenderTarget;
    expect( wse::oui::validateTextureDescription( texture_description ).ok(),
        "Valid texture description" );
    texture_description.usage = wse::oui::eTextureUsage::Sampled;
    expect( !wse::oui::validateTextureDescription( texture_description ).ok(),
        "RenderTarget state without usage is rejected" );

    // Meshは有限Vertex、TopologyごとのIndex数およびIndex範囲を検証する.
    // 範囲外IndexはGPUのVertex Bufferをはみ出して読ませ、NaN VertexはRasterizerへ渡ると
    // Backendごとに違う絵になる. どちらもDrawへ届く前に止める.
    wse::oui::sMeshDescription mesh_description;
    mesh_description.vertices = {
          { -1.0F, -1.0F, 0.0F, 1.0F }
        , {  0.0F,  1.0F, 0.5F, 0.0F }
        , {  1.0F, -1.0F, 1.0F, 1.0F }
    };
    mesh_description.indices = { 0U, 1U, 2U };
    expect( wse::oui::validateMeshDescription( mesh_description ).ok(),
        "Valid triangle mesh" );
    mesh_description.indices[ 2U ] = 3U;
    expect( !wse::oui::validateMeshDescription( mesh_description ).ok(),
        "Out-of-range mesh index is rejected" );
    mesh_description.indices[ 2U ] = 2U;
    mesh_description.vertices[ 0U ].position_x =
        ( std::numeric_limits< float >::quiet_NaN )();
    expect( !wse::oui::validateMeshDescription( mesh_description ).ok(),
        "Non-finite mesh vertex is rejected" );

    // SurfaceとRender passはOpaque handleおよび境界値を検証する.
    // Surface種別ごとに要求が違う. Offscreenは表示先を持たないのでBuffer 1枚だけ、
    // WindowはFlip用に2枚以上とTitleを要求する. 種別と設定の組み合わせを取り違えたまま
    // Backendへ渡すと、Windows側はSwapchain生成失敗、Linux側はxdg-shell側の失敗になる.
    wse::oui::sSurfaceDescription surface_description;
    surface_description.extent = { 1280U, 720U };
    expect( wse::oui::validateSurfaceDescription( surface_description ).ok(),
        "Valid offscreen surface" );
    surface_description.buffer_count = 2U;
    expect( !wse::oui::validateSurfaceDescription( surface_description ).ok(),
        "Offscreen multi-buffer surface is rejected" );
    surface_description.type = wse::oui::eSurfaceType::Window;
    expect( wse::oui::validateSurfaceDescription( surface_description ).ok(),
        "Valid internally owned window surface" );
    surface_description.title.clear();
    expect( !wse::oui::validateSurfaceDescription( surface_description ).ok(),
        "Window surface requires a title" );

    // Refresh rateは有理数で持つ. 60000／1000という書き方はBackendが返す非整数Rateを丸めずに
    // 運ぶための形であり、分母0はRate比較そのものを壊すため必ず弾く.
    wse::oui::sDisplayMode display_mode;
    display_mode.extent = { 1920U, 1080U };
    display_mode.refresh_rate_numerator = 60000U;
    display_mode.refresh_rate_denominator = 1000U;
    display_mode.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    expect( wse::oui::validateDisplayMode( display_mode ).ok(),
        "Valid rational display mode" );
    display_mode.refresh_rate_denominator = 0U;
    expect( !wse::oui::validateDisplayMode( display_mode ).ok(),
        "Zero display refresh denominator is rejected" );
    display_mode.refresh_rate_denominator = 1000U;

    // Display descriptionは「現在のModeが必ずMode一覧に含まれる」ことを保つ.
    // 60／1と60000／1000はたすき掛けで同じRateと判定されるので、Backendが一覧と現在Modeを
    // 別の約分で返しても同一Modeとして解決できる. これが崩れると、実際には選べているModeを
    // 上位が「一覧に無い」と誤判定してFullscreen遷移を諦めることになる.
    wse::oui::sDisplayDescription display_description;
    display_description.id = "test:adapter:display";
    display_description.adapter_id = "test:adapter";
    display_description.adapter_name = "Test Adapter";
    display_description.display_name = "Test Display";
    display_description.desktop_extent = { 1920U, 1080U };
    display_description.current_mode = display_mode;
    display_description.modes.push_back( display_mode );
    display_description.rotation = wse::oui::eDisplayRotation::Identity;
    expect( wse::oui::validateDisplayDescription( display_description ).ok(),
        "Valid portable display description" );
    display_description.current_mode.refresh_rate_numerator = 60U;
    display_description.current_mode.refresh_rate_denominator = 1U;
    expect( wse::oui::validateDisplayDescription( display_description ).ok(),
        "Equivalent rational refresh rates identify the current mode" );
    display_description.rotation = static_cast< wse::oui::eDisplayRotation >( 255U );
    expect( !wse::oui::validateDisplayDescription( display_description ).ok(),
        "Invalid display rotation is rejected" );
    display_description.rotation = wse::oui::eDisplayRotation::Identity;
    display_description.modes.clear();
    expect( !wse::oui::validateDisplayDescription( display_description ).ok(),
        "Display description requires its current mode" );

    // Surface stateはWindow modeとDisplay指定が食い違わないことを保証する.
    // Display先を指すModeだけがDisplay IDを持ち、Windowedは持たない. 対応が崩れると
    // 「どのDisplayを全画面にしているか」が不定のままBackendへ渡る.
    wse::oui::sSurfaceState surface_state;
    surface_state.extent = { 640U, 480U };
    expect( wse::oui::validateSurfaceState( surface_state ).ok(),
        "Valid offscreen surface state" );
    surface_state.type = wse::oui::eSurfaceType::Window;
    surface_state.window_mode = wse::oui::eSurfaceWindowMode::Windowed;
    expect( wse::oui::validateSurfaceState( surface_state ).ok(),
        "Valid windowed surface state" );
    surface_state.window_mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
    expect( !wse::oui::validateSurfaceState( surface_state ).ok(),
        "Borderless surface state requires a display ID" );
    surface_state.display_id = "test:display";
    expect( wse::oui::validateSurfaceState( surface_state ).ok(),
        "Valid borderless-fullscreen surface state" );
    surface_state.window_mode = wse::oui::eSurfaceWindowMode::DisplayModeFullscreen;
    surface_state.display_mode = display_mode;
    expect( wse::oui::validateSurfaceState( surface_state ).ok(),
        "Valid display-mode-fullscreen surface state" );
    surface_state.type = wse::oui::eSurfaceType::DirectDisplay;
    expect( wse::oui::validateSurfaceState( surface_state ).ok(),
        "Valid direct-display surface state" );

    // 表示状態の変更要求も、返ってくるStateと同じ規則で検証される.
    // 要求側だけ緩いと、受理された要求から不正なStateが生まれてしまう.
    wse::oui::sSurfaceWindowModeRequest window_mode_request;
    expect( wse::oui::validateSurfaceWindowModeRequest( window_mode_request ).ok(),
        "Valid windowed mode request" );
    window_mode_request.mode = wse::oui::eSurfaceWindowMode::BorderlessFullscreen;
    expect( !wse::oui::validateSurfaceWindowModeRequest( window_mode_request ).ok(),
        "Borderless mode request requires a display ID" );
    window_mode_request.display_id = "test:display";
    expect( wse::oui::validateSurfaceWindowModeRequest( window_mode_request ).ok(),
        "Valid borderless mode request" );
    window_mode_request.mode = wse::oui::eSurfaceWindowMode::DisplayModeFullscreen;
    window_mode_request.display_mode = display_mode;
    expect( wse::oui::validateSurfaceWindowModeRequest( window_mode_request ).ok(),
        "Valid display-mode request" );

    // DirectDisplay Surfaceは、Compositorを介さずDisplayを占有する経路である.
    // ExtentとFormatがDisplay Modeと1つでもずれたまま通ると、Mode設定後に真っ黒な画面や
    // 復帰不能なDisplayを残しかねない. だからここで完全一致を要求する.
    wse::oui::sSurfaceDescription direct_surface;
    direct_surface.type = wse::oui::eSurfaceType::DirectDisplay;
    direct_surface.extent = display_mode.extent;
    direct_surface.format = display_mode.format;
    direct_surface.buffer_count = 2U;
    direct_surface.display_id = "test:display";
    direct_surface.display_mode = display_mode;
    expect( wse::oui::validateSurfaceDescription( direct_surface ).ok(),
        "Valid direct-display description" );

    // Render passの必須項目と、正規化された値域を確認する.
    // 既定構築のHandleは無効であり、Clear colorは0.0-1.0の外へ出られない.
    wse::oui::sRenderPassDescription render_pass;
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Render pass requires a texture handle" );
    render_pass.color_attachment = { 1U, 1U };
    render_pass.clear_color.red = 2.0F;
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Out-of-range clear color is rejected" );
    render_pass.clear_color.red = 1.0F;
    expect( wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Valid clear render pass" );
    render_pass.draw_commands.emplace_back(
        wse::oui::sMeshDrawCommand{ { 1U, 1U }, { 2U, 1U } } );
    expect( wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Valid portable mesh draw handles" );
    // 描画先を同じPassのSampling元にすると、GPUは読みと書きが競合したまま走る.
    // Backendによって結果が変わる未定義動作なので、Source側でもAlpha Map側でも禁じる.
    render_pass.draw_commands.front().source_texture = render_pass.color_attachment;
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Render attachment cannot sample itself" );
    render_pass.draw_commands.front().source_texture = { 2U, 1U };
    render_pass.draw_commands.front().alpha_texture = render_pass.color_attachment;
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Render attachment cannot be its own alpha map" );
    // Alpha Mapは省略可能だが、省略は完全なZero Handleに限る. { 0, 1 }のように
    // 片側だけ埋まったHandleは、書き損じたHandleを「無指定」と誤読させるため拒否する.
    render_pass.draw_commands.front().alpha_texture = { 0U, 1U };
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Partial optional alpha handle is rejected" );
    // 255はどのEnumeratorにも当たらない値であり、ABI越しに壊れた値が届いた場合を模す.
    // Enum、Opacity、Edge blend幅のいずれも、既定へ黙って落とさず失敗として返すことを求める.
    render_pass.draw_commands.front().alpha_texture = { 3U, 1U };
    render_pass.draw_commands.front().sampling_filter =
        static_cast< wse::oui::eTextureSamplingFilter >( 255U );
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Unknown sampling filter is rejected" );
    render_pass.draw_commands.front().sampling_filter =
        wse::oui::eTextureSamplingFilter::Linear;
    render_pass.draw_commands.front().opacity =
        ( std::numeric_limits< float >::quiet_NaN )();
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Non-finite draw opacity is rejected" );
    render_pass.draw_commands.front().opacity = 1.0F;
    // Edge blend幅はUV正規化なので0.0-1.0に収まる. 1.01Fは上限をわずかに超えただけの値で、
    // 上限Check自体が抜けていれば通ってしまう. 通れば境界の外をSamplingする絵になる.
    render_pass.draw_commands.front().edge_blend.left = 0.25F;
    render_pass.draw_commands.front().edge_blend.curve =
        wse::oui::eEdgeBlendCurve::Smoothstep;
    expect( wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Valid smoothstep edge blend" );
    render_pass.draw_commands.front().edge_blend.right = 1.01F;
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Out-of-range edge-blend width is rejected" );
    render_pass.draw_commands.front().edge_blend.right = 0.0F;
    render_pass.draw_commands.front().edge_blend.curve =
        static_cast< wse::oui::eEdgeBlendCurve >( 255U );
    expect( !wse::oui::validateRenderPassDescription( render_pass ).ok(),
        "Unknown edge-blend curve is rejected" );

    // Projection facadeはLayer必須とStraight-alpha Drawへの変換を検証する.
    wse::oui::sProjectionPassDescription projection_pass;
    projection_pass.color_attachment = { 1U, 1U };
    expect( !wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Projection pass requires a layer" );
    projection_pass.layers.emplace_back( wse::oui::sProjectionLayerDescription{
          { 1U, 1U }
        , { 2U, 1U }
        , { 3U, 1U }
        , wse::oui::eTextureSamplingFilter::Linear
        , 0.5F
        , { 0.25F, 0.0F, 0.0F, 0.25F, wse::oui::eEdgeBlendCurve::Smoothstep }
    } );
    expect( wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Valid projection layer" );
    // Supersample倍率は1か2しか無い. 2のときFacadeは2倍の中間Targetを自分で作るため、
    // Render areaが空だと大きさが決まらず、Extentが大きすぎるとその2倍がuint32を溢れる.
    // 0はどちらでもない値で、既定へ黙って戻さないことを見る.
    projection_pass.supersample_scale = 0U;
    expect( !wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Zero projection supersample scale is rejected" );
    projection_pass.supersample_scale = 2U;
    expect( !wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Supersampling requires an explicit output extent" );
    projection_pass.render_area.extent = { 640U, 480U };
    expect( wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Legacy two-times supersampling is valid" );
    projection_pass.render_area.extent.width =
        ( std::numeric_limits< std::uint32_t >::max )();
    expect( !wse::oui::validateProjectionPassDescription( projection_pass ).ok(),
        "Supersampling extent overflow is rejected" );
    // Facadeはnull Rendererを逆参照せず、InvalidArgumentとして返す.
    projection_pass.render_area.extent = { 640U, 480U };
    const auto null_projection_result =
        wse::oui::ProjectionPipeline::execute( nullptr, projection_pass );
    expect( !null_projection_result.succeeded() &&
            null_projection_result.error().code() ==
                wse::oui::eRendererErrorCode::InvalidArgument,
        "Projection facade rejects a null renderer" );

    // Facadeは未初期化と未対応Backendを構造化Errorで通知する.
    // ここから先のRendererは一度も初期化しない. 検査順序が「Handle、Descriptor、Lifecycle」の
    // 順であること、つまりどの入口も初期化前にBackendへ触らないことを見る.
    wse::oui::Renderer renderer;
    mesh_description.vertices[ 0U ].position_x = -1.0F;
    const auto invalid_mesh_update = renderer.updateMesh( {}, mesh_description );
    expect( !invalid_mesh_update.succeeded() &&
            invalid_mesh_update.error().code() ==
                wse::oui::eRendererErrorCode::InvalidArgument,
        "Mesh update rejects an invalid handle" );
    const auto uninitialized_mesh_update =
        renderer.updateMesh( { 1U, 1U }, mesh_description );
    expect( !uninitialized_mesh_update.succeeded() &&
            uninitialized_mesh_update.error().code() ==
                wse::oui::eRendererErrorCode::NotInitialized,
        "Mesh update preserves renderer lifecycle errors" );
    mesh_description.indices.clear();
    expect( !renderer.updateMesh( { 1U, 1U }, mesh_description ).succeeded(),
        "Mesh update validates replacement data before backend access" );
    // 問い合わせ系と状態変更系の入口が、未初期化のとき例外でも既定値でもなく
    // NotInitializedを返すことをまとめて確認する. 静かに空を返す入口が1つでもあると、
    // 上位はRendererが動いていると誤認したまま進んでしまう.
    mesh_description.indices = { 0U, 1U, 2U };
    const auto capabilities_result = renderer.getCapabilities();
    expect( !capabilities_result.succeeded(), "Uninitialized capability query fails" );
    expect( capabilities_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized query error code" );
    const auto displays_result = renderer.enumerateDisplays();
    expect( !displays_result.succeeded() &&
            displays_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized display enumeration fails visibly" );
    const auto surface_state_result = renderer.getSurfaceState( { 1U, 1U } );
    expect( !surface_state_result.succeeded() &&
            surface_state_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized surface-state query fails visibly" );
    const auto resize_result = renderer.resizeSurface( { 1U, 1U }, { 32U, 24U } );
    expect( !resize_result.succeeded() &&
            resize_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized surface resize fails visibly" );
    const auto window_mode_result = renderer.setSurfaceWindowMode(
        { 1U, 1U }, wse::oui::sSurfaceWindowModeRequest{} );
    expect( !window_mode_result.succeeded() &&
            window_mode_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized surface mode change fails visibly" );
    const auto surface_events_result = renderer.pollSurfaceEvents( { 1U, 1U } );
    expect( !surface_events_result.succeeded() &&
            surface_events_result.error().code() == wse::oui::eRendererErrorCode::NotInitialized,
        "Uninitialized surface event polling fails visibly" );
    const auto uninitialized_projection_result =
        wse::oui::ProjectionPipeline::execute( &renderer, projection_pass );
    expect( !uninitialized_projection_result.succeeded() &&
            uninitialized_projection_result.error().code() ==
                wse::oui::eRendererErrorCode::NotInitialized,
        "Projection facade preserves renderer lifecycle errors" );
    // uint32の最大値は「無限待ち」を意味する値として使われがちだが、この契約では禁じている.
    // 無限待ちを許すとGPU Hang時にTestもApplicationも戻らなくなるため、Validation Errorとして
    // 即座に返す. Lifecycle Errorより先に返る点も、この順序が意図であることを示す.
    const auto infinite_wait = renderer.waitFence(
          { 1U, 1U }
        , ( std::numeric_limits< std::uint32_t >::max )()
    );
    expect( !infinite_wait.succeeded(), "Infinite fence timeout is rejected" );
    expect( infinite_wait.error().category() == wse::oui::eRendererErrorCategory::Validation,
        "Infinite fence timeout is a validation error" );

    // 走っているPlatformに存在しない方のBackendを名指しする. WindowsではVulkan12、
    // それ以外ではDirect3D12が「必ず無い」Backendになる. 無いBackendを頼まれたRendererは
    // 別のBackendへ黙って乗り換えず、UnsupportedBackendで失敗し、初期化済みにもならない.
    wse::oui::sRendererConfiguration unsupported_configuration;
#if defined( _WIN32 )
    unsupported_configuration.backend = wse::oui::eRendererBackend::Vulkan12;
#else
    unsupported_configuration.backend = wse::oui::eRendererBackend::Direct3D12;
#endif
    const auto unsupported_result = renderer.initialize( unsupported_configuration );
    expect( !unsupported_result.succeeded(), "Unsupported platform backend fails" );
    expect( unsupported_result.error().code() == wse::oui::eRendererErrorCode::UnsupportedBackend,
        "Unsupported platform backend error code" );
    expect( !renderer.isInitialized(), "Unsupported backend does not fake initialization" );

    // 未対応Backendと未知Backendは別物である. 前者はUnsupported、後者はValidationとして
    // 分類されるので、上位は「将来対応しうる」のか「入力が壊れている」のかを区別できる.
    wse::oui::sRendererConfiguration invalid_configuration;
    invalid_configuration.backend = static_cast< wse::oui::eRendererBackend >( 255U );
    const auto invalid_result = renderer.initialize( invalid_configuration );
    expect( !invalid_result.succeeded(), "Unknown backend value fails" );
    expect( invalid_result.error().category() == wse::oui::eRendererErrorCategory::Validation,
        "Unknown backend is a validation error" );

    // Moveした後も、移動元Rendererは破棄と再問い合わせが安全な状態で残る.
    // 移動元がDangling Implを抱えると、Scope終端の破棄でCrashすることになる.
    wse::oui::Renderer moved( std::move( renderer ) );
    expect( !moved.isInitialized(), "Move preserves uninitialized state" );
    expect( !renderer.isInitialized(), "Moved-from renderer remains safe" );

    return g_failures == 0 ? 0 : 1;
}
