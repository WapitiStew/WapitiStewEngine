//*****************************************************************************************************************
//!
//! @file    oui_d3d12_offscreen_golden.cpp
//! @brief   \~japanese Portable Renderer API経由のD3D12 Offscreen出力をGoldenと比較する.
//!                    Windows D3D12におけるRenderer全体の基準Testである. 再Initialize契約、
//!                    Texture／Surfaceの所有と失効、部分領域Clear、State遷移、Fence待機、
//!                    Packed Readback、そしてPixel値そのものを1本で押さえる. これが落ちる製品は
//!                    投影映像の色や位置が変わっているか、Resource所有の境界が崩れている.
//! @brief   \~english  Compares D3D12 offscreen output through the portable renderer API with a Golden image.
//!                    This is the reference test for the whole Windows D3D12 renderer: repeated
//!                    initialization, texture/surface ownership and invalidation, sub-region
//!                    clears, state transitions, fence waits, packed readback, and the pixel values
//!                    themselves. A failure means either the projected image moved or changed
//!                    color, or the resource-ownership boundary broke.
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
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{

// 10 x 10はGoldenをText形式のまま人が読める大きさで、4 x 4 Quadrantを4つ置いても外周1 Pixelの
// 黒Borderが残る. Borderが残ることでRender areaのはみ出しやOff-by-oneが目視で分かる.
// TIMEOUT_MSはCMakeLists側のCTest TIMEOUT 30 sと同値で、Fenceが本当に止まればCTestが先に打ち切る.
// EXPECTED_HASHはこのTestが生成した値ではなく、doc/design/{en,ja}/OuiRenderer.mdへ記録済みの
// WARP基準値である. 更新にはDocumentの基準値も同時に書き換える必要がある.
constexpr std::uint32_t WIDTH  = 10U;
constexpr std::uint32_t HEIGHT = 10U;
constexpr std::size_t CHANNELS = 4U;
constexpr std::uint32_t TIMEOUT_MS = 30000U;
constexpr std::uint64_t EXPECTED_HASH = 0xde58c7e12a2fca45ULL;

int g_failures = 0;

void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

// PPMの次のTokenを取り出し、'#'で始まる行はComment行として行末まで読み飛ばす.
// Golden fileへ由来や再生成手順をCommentで書き足しても壊れないようにするための処理である.
bool nextPpmToken( std::istream* const p_stream_inout, std::string* const p_token_out )
{
    if( p_stream_inout == nullptr || p_token_out == nullptr )
    {
        return false;
    }
    while( *p_stream_inout >> *p_token_out )
    {
        if( !p_token_out->empty() && p_token_out->front() == '#' )
        {
            p_stream_inout->ignore(
                ( std::numeric_limits< std::streamsize >::max )(), '\n' );
            continue;
        }
        return true;
    }
    return false;
}

// Tokenを0..255の整数へ厳密に変換する. 部分一致を弾くのは"12abc"や"1.5"を12として
// 受け入れないためで、範囲を255までに絞るのは8 bit Channel値としての妥当性検査である.
// 副作用として幅と高さも255までしか解釈できない. 現在のGoldenは10 x 10なので足りている.
// stoulのExceptionはここで不正Tokenへ畳んで戻り値falseにする. 呼び出し側は必ずfail()を
// 通すので、Golden fileの破損が握り潰されることはない.
bool parseUnsigned( std::uint32_t* const p_value_out, const std::string& token_in )
{
    if( p_value_out == nullptr )
    {
        return false;
    }
    try
    {
        std::size_t parsed = 0U;
        const unsigned long converted = std::stoul( token_in, &parsed, 10 );
        if( parsed != token_in.size() || converted > 255UL )
        {
            return false;
        }
        *p_value_out = static_cast< std::uint32_t >( converted );
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

// Goldenを読み込む. 実体はRepositoryの test/golden/oui_d3d12_quadrants.ppm で、Pathは
// CMakeListsがTest引数として渡す. 人が読み書きできるようBinary P6ではなくText P3に限定する.
// 大きさとMax値、そしてPixel数の過不足まで検査するのは、Goldenが静かにずれた状態で
// 比較が通ってしまうのを防ぐためである. Golden側が壊れていれば描画結果の良し悪しに
// 関わらずここで落ちる.
bool loadGoldenPpm( std::vector< std::uint8_t >* const p_rgb_out, const char* const path_in )
{
    if( p_rgb_out == nullptr || path_in == nullptr )
    {
        fail( "Golden PPM arguments are invalid." );
        return false;
    }
    std::ifstream stream( path_in );
    if( !stream )
    {
        fail( "Golden PPM could not be opened." );
        return false;
    }

    std::string token;
    if( !nextPpmToken( &stream, &token ) || token != "P3" )
    {
        fail( "Golden image must use the P3 PPM format." );
        return false;
    }

    std::uint32_t width   = 0U;
    std::uint32_t height  = 0U;
    std::uint32_t maximum = 0U;
    if( !nextPpmToken( &stream, &token ) || !parseUnsigned( &width, token ) ||
        !nextPpmToken( &stream, &token ) || !parseUnsigned( &height, token ) ||
        !nextPpmToken( &stream, &token ) || !parseUnsigned( &maximum, token ) ||
        width != WIDTH || height != HEIGHT || maximum != 255U )
    {
        fail( "Golden PPM dimensions or range are invalid." );
        return false;
    }

    p_rgb_out->clear();
    p_rgb_out->reserve( static_cast< std::size_t >( WIDTH ) * HEIGHT * 3U );
    for( std::size_t index = 0U;
         index < static_cast< std::size_t >( WIDTH ) * HEIGHT * 3U;
         ++index )
    {
        std::uint32_t channel = 0U;
        if( !nextPpmToken( &stream, &token ) || !parseUnsigned( &channel, token ) )
        {
            fail( "Golden PPM pixel data is incomplete." );
            return false;
        }
        p_rgb_out->emplace_back( static_cast< std::uint8_t >( channel ) );
    }
    if( nextPpmToken( &stream, &token ) )
    {
        fail( "Golden PPM contains unexpected trailing pixel data." );
        return false;
    }
    return true;
}

// 1領域をClearするRender passを送信し、Fence完了まで待つ.
// 毎回待つのは描画順を確定させるためである. 全面の黒Clearと後続の4 Quadrantが重なるので、
// 送信だけして待たないと実行順に結果が左右され、Goldenが決定論でなくなる.
bool clearRegion(
          wse::oui::Renderer* const             p_renderer_inout
    , const wse::oui::sTextureHandle            texture_in
    , const wse::oui::sRendererRegion2D&        region_in
    , const wse::oui::sRendererColor&           color_in
    , const wse::oui::eTextureState             final_state_in
)
{
    wse::oui::sRenderPassDescription render_pass;
    render_pass.color_attachment = texture_in;
    render_pass.render_area      = region_in;
    render_pass.load_operation   = wse::oui::eAttachmentLoadOperation::Clear;
    render_pass.store_operation  = wse::oui::eAttachmentStoreOperation::Store;
    render_pass.clear_color      = color_in;
    render_pass.final_state      = final_state_in;

    const auto submit_result = p_renderer_inout->executeRenderPass( render_pass );
    if( !submit_result.succeeded() )
    {
        fail( "Portable render-pass submission failed: " + submit_result.error().message() );
        return false;
    }
    const auto wait_result = p_renderer_inout->waitFence( submit_result.value(), TIMEOUT_MS );
    if( !wait_result.succeeded() )
    {
        fail( "Portable renderer fence wait failed: " + wait_result.error().message() );
        return false;
    }
    return true;
}

} // namespace

int main( const int argument_count_in, const char* const arguments_in[] )
{
    // Golden PathはCMakeListsがTest登録時に渡す唯一の引数である. 既定Pathへ落とさず必須に
    // しているので、引数が抜けたTest登録はGolden比較を飛ばさずここで落ちる.
    if( argument_count_in != 2 )
    {
        fail( "Expected the Golden PPM path." );
        return 1;
    }

    std::vector< std::uint8_t > golden;
    if( !loadGoldenPpm( &golden, arguments_in[ 1 ] ) )
    {
        return 1;
    }

    // Deterministic software-adapter Rendererを初期化する.
    // WARPを指定するのはPixel値をGPU Vendorから独立させるためで、代わりに実Driverの挙動、
    // 実Displayへの出力、Hardware固有のFormat対応はこのTestの範囲外になる.
    // 続くCapability照会はBackendが本当にD3D12でOffscreenを扱えることの確認であり、
    // ここを通らない環境ではSkipせずに失敗させる.
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend              = wse::oui::eRendererBackend::Direct3D12;
    configuration.use_software_adapter = true;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
    {
        fail( "Portable D3D12 renderer initialization failed: " +
            initialize_result.error().message() );
        return 1;
    }

    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        capabilities_result.value().backend != wse::oui::eRendererBackend::Direct3D12 ||
        !capabilities_result.value().supports_offscreen )
    {
        fail( "Portable D3D12 renderer capabilities are invalid." );
        return 1;
    }

    // 再Initialize契約. 同一設定なら成功して既存Deviceを保つ一方、設定が1つでも違えば
    // AlreadyInitializedで拒否する. 黙って設定を無視するとApplicationは要求していない
    // Adapterや検証設定のまま動き続けることになるため、成否だけでなくError codeも見る.
    const auto repeated_initialize = renderer.initialize( configuration );
    wse::oui::sRendererConfiguration different_configuration = configuration;
    different_configuration.enable_validation = true;
    const auto different_initialize = renderer.initialize( different_configuration );
    if( !repeated_initialize.succeeded() || different_initialize.succeeded() ||
        different_initialize.error().code() != wse::oui::eRendererErrorCode::AlreadyInitialized )
    {
        fail( "Portable renderer repeated-initialization contract failed." );
        return 1;
    }

    // Standalone Texture lifecycleとDirect Display未対応Errorを検証する.
    // Surfaceに属さないTextureは呼び出し側の所有なので、生成した側が解放できなければならない.
    // 2回目の解放はResourceNotFoundで拒否する必要がある. 成功してしまえば二重解放を許す実装で、
    // 別のResourceへHandleが再利用された後なら無関係なResourceを巻き込む.
    wse::oui::sTextureDescription standalone_description;
    standalone_description.extent = { 2U, 2U };
    standalone_description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    standalone_description.usage = wse::oui::eTextureUsage::RenderTarget |
        wse::oui::eTextureUsage::TransferSource;
    standalone_description.initial_state = wse::oui::eTextureState::RenderTarget;
    const auto standalone_result = renderer.createTexture( standalone_description );
    if( !standalone_result.succeeded() ||
        !renderer.destroyTexture( standalone_result.value() ).succeeded() )
    {
        fail( "Standalone texture lifecycle failed." );
        return 1;
    }
    const auto repeated_texture_release = renderer.destroyTexture( standalone_result.value() );
    if( repeated_texture_release.succeeded() ||
        repeated_texture_release.error().code() != wse::oui::eRendererErrorCode::ResourceNotFound )
    {
        fail( "Stale texture handle did not fail safely." );
        return 1;
    }

    // DirectDisplayはdisplay_idとdisplay_modeが必須である. どちらも与えないこの記述が
    // InvalidDescriptionで弾かれることを見る. Backendへ届く前のValidationを押さえる検査なので、
    // Displayが1台も無いHostでも同じ結果になり、実Display接続には依存しない.
    wse::oui::sSurfaceDescription direct_display_description;
    direct_display_description.type         = wse::oui::eSurfaceType::DirectDisplay;
    direct_display_description.extent       = { WIDTH, HEIGHT };
    direct_display_description.format       = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    direct_display_description.buffer_count = 2U;
    const auto direct_display_result = renderer.createSurface( direct_display_description );
    if( direct_display_result.succeeded() ||
        direct_display_result.error().code() !=
            wse::oui::eRendererErrorCode::InvalidDescription )
    {
        fail( "Incomplete direct-display description did not fail validation." );
        return 1;
    }

    // Offscreen Surfaceと所有Textureを生成する.
    // buffer_count 1はOffscreenにFlipが要らないためで、Window Surfaceの2以上とは要件が違う.
    // getSurfaceTextureが返すHandleはSurfaceの所有物であり、この区別は後段の解放検査で効いてくる.
    wse::oui::sSurfaceDescription surface_description;
    surface_description.type         = wse::oui::eSurfaceType::Offscreen;
    surface_description.extent       = { WIDTH, HEIGHT };
    surface_description.format       = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    surface_description.buffer_count = 1U;
    const auto surface_result = renderer.createSurface( surface_description );
    if( !surface_result.succeeded() )
    {
        fail( "Portable offscreen surface creation failed: " + surface_result.error().message() );
        return 1;
    }
    const auto texture_result = renderer.getSurfaceTexture( surface_result.value() );
    if( !texture_result.succeeded() )
    {
        fail( "Portable offscreen texture lookup failed: " + texture_result.error().message() );
        return 1;
    }

    // Black backgroundと4 Quadrantを5 Render passで描画する.
    // 空Regionは「Attachment全体」を意味する. 続く4つは(1,1)(5,1)(1,5)(5,5)を起点とする4 x 4で、
    // 4領域は隙間なく隣接しつつ、外周1 Pixelの黒Borderだけを残す配置になっている.
    // Render areaが無視されればそのBorderが塗り潰されて落ちるので、部分Clearが本当に効いて
    // いることの検査になる.
    // 最後の1回だけfinal_stateをCopySourceにし、それ以外はRenderTargetのまま次のPassへ渡す.
    // State遷移の連結が壊れればWARPのValidationか読み出し内容で露見する.
    const wse::oui::sRendererRegion2D complete = {};
    const wse::oui::sRendererRegion2D red_region   = { 1U, 1U, { 4U, 4U } };
    const wse::oui::sRendererRegion2D green_region = { 5U, 1U, { 4U, 4U } };
    const wse::oui::sRendererRegion2D blue_region  = { 1U, 5U, { 4U, 4U } };
    const wse::oui::sRendererRegion2D white_region = { 5U, 5U, { 4U, 4U } };
    const wse::oui::sRendererColor black = { 0.0F, 0.0F, 0.0F, 1.0F };
    const wse::oui::sRendererColor red   = { 1.0F, 0.0F, 0.0F, 1.0F };
    const wse::oui::sRendererColor green = { 0.0F, 1.0F, 0.0F, 1.0F };
    const wse::oui::sRendererColor blue  = { 0.0F, 0.0F, 1.0F, 1.0F };
    const wse::oui::sRendererColor white = { 1.0F, 1.0F, 1.0F, 1.0F };
    if( !clearRegion( &renderer, texture_result.value(), complete, black,
            wse::oui::eTextureState::RenderTarget ) ||
        !clearRegion( &renderer, texture_result.value(), red_region, red,
            wse::oui::eTextureState::RenderTarget ) ||
        !clearRegion( &renderer, texture_result.value(), green_region, green,
            wse::oui::eTextureState::RenderTarget ) ||
        !clearRegion( &renderer, texture_result.value(), blue_region, blue,
            wse::oui::eTextureState::RenderTarget ) ||
        !clearRegion( &renderer, texture_result.value(), white_region, white,
            wse::oui::eTextureState::CopySource ) )
    {
        return 1;
    }

    const auto frame_result = renderer.readTexture( texture_result.value(), TIMEOUT_MS );
    if( !frame_result.succeeded() )
    {
        fail( "Portable texture readback failed: " + frame_result.error().message() );
        return 1;
    }
    const wse::oui::sRendererFrame& frame = frame_result.value();
    if( frame.description.extent.width != WIDTH ||
        frame.description.extent.height != HEIGHT ||
        frame.description.format != wse::oui::eRendererPixelFormat::Rgba8Unorm ||
        frame.data.size() != static_cast< std::size_t >( WIDTH ) * HEIGHT * CHANNELS )
    {
        fail( "Portable readback frame description is invalid." );
        return 1;
    }

    // Packed RGBAをGoldenとHashへ比較する.
    std::uint64_t hash = 14695981039346656037ULL;
    std::size_t mismatch_count = 0U;
    for( std::size_t pixel = 0U;
         pixel < static_cast< std::size_t >( WIDTH ) * HEIGHT;
         ++pixel )
    {
        const std::size_t actual_index = pixel * CHANNELS;
        const std::size_t golden_index = pixel * 3U;
        for( std::size_t channel = 0U; channel < CHANNELS; ++channel )
        {
            hash ^= frame.data[ actual_index + channel ];
            hash *= 1099511628211ULL;
        }
        if( frame.data[ actual_index ] != golden[ golden_index ] ||
            frame.data[ actual_index + 1U ] != golden[ golden_index + 1U ] ||
            frame.data[ actual_index + 2U ] != golden[ golden_index + 2U ] ||
            frame.data[ actual_index + 3U ] != 255U )
        {
            ++mismatch_count;
        }
    }
    if( mismatch_count != 0U )
    {
        fail( std::to_string( mismatch_count ) + " pixels differ from the Golden image." );
    }
    if( hash != EXPECTED_HASH )
    {
        fail( "RGBA hash differs from the documented D3D12 baseline." );
    }

    // Surface所有Textureの誤解放を拒否し、Surface経由で解放する.
    const auto owned_texture_release = renderer.destroyTexture( texture_result.value() );
    if( owned_texture_release.succeeded() ||
        owned_texture_release.error().code() != wse::oui::eRendererErrorCode::ResourceInUse )
    {
        fail( "Surface-owned texture release did not fail safely." );
    }
    const auto surface_release = renderer.destroySurface( surface_result.value() );
    if( !surface_release.succeeded() )
    {
        fail( "Offscreen surface release failed." );
    }
    renderer.shutdown();

    const auto reinitialize_result = renderer.initialize( configuration );
    if( !reinitialize_result.succeeded() )
    {
        fail( "Portable renderer could not initialize after shutdown." );
    }
    renderer.shutdown();

    std::cout << "D3D12 portable renderer RGBA8 FNV-1a: 0x"
              << std::hex << hash << std::dec << '\n';
    return g_failures == 0 ? 0 : 1;
}
