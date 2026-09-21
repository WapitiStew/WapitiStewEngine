//*****************************************************************************************************************
//!
//! @file    oui_d3d12_window_surface.cpp
//! @brief   \~japanese Portable D3D12 Window Surfaceの描画／Present／Lifecycleを検証する.
//!                    Rendererが自前でWin32 WindowとSwap chainを所有し、Back bufferをOffscreen
//!                    Textureと同じRender pass／Fence／Readback経路で扱えること、Present後に
//!                    Current bufferが交代すること、Surface解放後のHandleが安全に失敗することを
//!                    固定する. ここが壊れると、表示Applicationが2 Frame目以降を同じBufferへ
//!                    描き続けるか、Window終了時に無効Handleを触って落ちる.
//! @brief   \~english  Verifies portable D3D12 window-surface drawing, present, and lifecycle.
//!                    It pins that the renderer owns its Win32 window and swap chain, that a back
//!                    buffer takes the same render-pass/fence/readback path as an offscreen
//!                    texture, that the current buffer advances after a present, and that a handle
//!                    outliving its surface fails safely. A regression means a windowed application
//!                    keeps drawing into one buffer, or touches a dead handle on shutdown.
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

// 16 x 12はWindowとSwap chainの生成に必要な最小限の大きさで、Readbackも768 Byteで済む.
// 1 Rowは16 x 4 = 64 ByteしかなくD3D12のReadback Row整列とは一致しないので、Packed pitchへ
// 詰め直す経路も併せて通る.
// TIMEOUT_MSはCMakeLists側のCTest TIMEOUT 30 sと同値である. Fenceが本当に止まった場合は
// Fence待ちの失敗ではなくCTestのTimeoutとして現れる.
constexpr std::uint32_t WIDTH      = 16U;
constexpr std::uint32_t HEIGHT     = 12U;
constexpr std::uint32_t TIMEOUT_MS = 30000U;

int g_failures = 0;

void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

bool waitFence(
          wse::oui::Renderer* const p_renderer_inout
    , const wse::oui::sFenceHandle  fence_in
)
{
    const auto result = p_renderer_inout->waitFence( fence_in, TIMEOUT_MS );
    if( !result.succeeded() )
    {
        fail( "Window fence wait failed: " + result.error().message() );
        return false;
    }
    return true;
}

} // namespace

int main()
{
    // WARPで非表示Window Surfaceを生成し、実Display依存を避ける.
    // WARPを選ぶのでPixel値は決定論になるが、実GPU Driverの表示経路、Monitorへの走査出力、
    // Compositorとのやり取りは検証範囲から外れる. このTestが見るのはSwap chainの状態遷移
    // までであり、映像が実際に画面へ出たかは見ていない.
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
    if( !capabilities_result.succeeded() || !capabilities_result.value().supports_window )
    {
        fail( "D3D12 window capability is unavailable." );
        return 1;
    }

    // buffer_count 2はFlipで交代するBufferが実在するための下限で、後段のBackbuffer進行検査は
    // この値に依存する. vertical_syncを切るのはVBlank待ちでCIのTest時間が延びるのを避けるため、
    // visibleを落とすのは対話Desktop Sessionが無いCIでも走らせるためである.
    // 生成直後のprocessSurfaceEventsがtrueを返すことは、非表示でもWindowが生存しMessageを
    // 処理できる状態にあることを意味する. ここでfalseならWindowが即座に閉じている.
    wse::oui::sSurfaceDescription surface_description;
    surface_description.type          = wse::oui::eSurfaceType::Window;
    surface_description.extent        = { WIDTH, HEIGHT };
    surface_description.format        = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    surface_description.buffer_count  = 2U;
    surface_description.vertical_sync = false;
    surface_description.visible       = false;
    surface_description.title         = "WSE portable window test";
    const auto surface_result = renderer.createSurface( surface_description );
    if( !surface_result.succeeded() )
    {
        fail( "Window surface creation failed: " + surface_result.error().message() );
        return 1;
    }
    const auto event_result = renderer.processSurfaceEvents( surface_result.value() );
    if( !event_result.succeeded() || !event_result.value() )
    {
        fail( "Window event processing failed." );
        return 1;
    }

    // Current back bufferをClearし、Readback可能Stateで色を検証する.
    // 期待Byteの64／128／191は0.25／0.5／0.75 x 255を四捨五入した値である. Linear値がそのまま
    // UNORMへ量子化されることを示し、sRGB変換やGamma補正が挟まれば全く別の値になって落ちる.
    // Alpha 255はClear色のalpha 1.0が保存されることを見る.
    const auto first_texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !first_texture_result.succeeded() )
    {
        fail( "Window back-buffer lookup failed." );
        return 1;
    }
    wse::oui::sRenderPassDescription clear_pass;
    clear_pass.color_attachment = first_texture_result.value();
    clear_pass.clear_color      = { 0.25F, 0.5F, 0.75F, 1.0F };
    clear_pass.final_state      = wse::oui::eTextureState::CopySource;
    const auto clear_result = renderer.executeRenderPass( clear_pass );
    if( !clear_result.succeeded() || !waitFence( &renderer, clear_result.value() ) )
    {
        fail( "Window clear render pass failed." );
        return 1;
    }
    const auto frame_result = renderer.readTexture( first_texture_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() || frame_result.value().data.size() != WIDTH * HEIGHT * 4U )
    {
        fail( "Window back-buffer readback failed." );
        return 1;
    }
    for( std::size_t offset = 0U; offset < frame_result.value().data.size(); offset += 4U )
    {
        const auto& data = frame_result.value().data;
        if( data[ offset ] != 64U || data[ offset + 1U ] != 128U ||
            data[ offset + 2U ] != 191U || data[ offset + 3U ] != 255U )
        {
            fail( "Window back-buffer clear color is invalid." );
            break;
        }
    }

    // Load passでPresentへ戻し、Flip後にCurrent bufferが交代することを検証する.
    // Readbackのために一度CopySourceへ落としたBack bufferは、そのままではPresentできない.
    // load_operation LoadはClearし直さずに描画済みの内容を保ったままStateだけを移す指定で、
    // これが効かないと表示Applicationは毎Frame内容を捨てることになる.
    wse::oui::sRenderPassDescription present_transition;
    present_transition.color_attachment = first_texture_result.value();
    present_transition.load_operation    = wse::oui::eAttachmentLoadOperation::Load;
    present_transition.final_state       = wse::oui::eTextureState::Present;
    const auto transition_result = renderer.executeRenderPass( present_transition );
    if( !transition_result.succeeded() )
    {
        fail( "Window Present transition failed: " + transition_result.error().message() );
        return 1;
    }
    const auto present_result = renderer.presentSurface( surface_result.value() );
    if( !present_result.succeeded() || !waitFence( &renderer, present_result.value() ) )
    {
        fail( "Window Present failed." );
        return 1;
    }
    const auto second_texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !second_texture_result.succeeded() ||
        second_texture_result.value().value == first_texture_result.value().value )
    {
        fail( "Window swap-chain buffer did not advance." );
    }

    // Surface解放後の同じHandleでのPresentは必ず失敗する. 成功してしまうと、Windowを閉じた
    // 後もPresentを呼び続けるApplicationが解放済みSwap chainを触ることになる.
    if( !renderer.destroySurface( surface_result.value() ).succeeded() ||
        renderer.presentSurface( surface_result.value() ).succeeded() )
    {
        fail( "Window surface stale-handle lifecycle failed." );
    }
    // An offscreen attachment must remain ineligible for Present even though
    // both Window and DirectDisplay swap-chain attachments are accepted.
    wse::oui::sSurfaceDescription offscreen_description;
    offscreen_description.type = wse::oui::eSurfaceType::Offscreen;
    offscreen_description.extent = { WIDTH, HEIGHT };
    const auto offscreen = renderer.createSurface( offscreen_description );
    if( !offscreen.succeeded() )
    {
        fail( "Offscreen negative-test surface creation failed." );
    }
    else
    {
        const auto texture = renderer.getSurfaceTexture( offscreen.value() );
        if( !texture.succeeded() ) fail( "Offscreen attachment lookup failed." );
        else
        {
            present_transition.color_attachment = texture.value();
            const auto rejected = renderer.executeRenderPass( present_transition );
            if( rejected.succeeded() ||
                rejected.error().code() != wse::oui::eRendererErrorCode::UnsupportedOperation )
                fail( "Offscreen Present transition must be rejected." );
        }
        if( !renderer.destroySurface( offscreen.value() ).succeeded() )
            fail( "Offscreen negative-test cleanup failed." );
    }
    renderer.shutdown();
    return g_failures == 0 ? 0 : 1;
}
