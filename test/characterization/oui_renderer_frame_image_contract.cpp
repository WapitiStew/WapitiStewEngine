//*****************************************************************************************************************
//!
//! @file    oui_renderer_frame_image_contract.cpp
//! @brief   \~japanese Renderer FrameとCore Imageの相互変換の契約を検証する.
//! @brief   \~english  Verifies the contract between a renderer frame and a Core image.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @details
//!     \~japanese
//!         RendererのFrameとCoreのImageは、WSEの中でPixelが行き来する唯一の境界である。
//!      @n この契約が壊れると、GPUから読み出した絵をCoreの画像処理へ渡した時点で、Channel順序の
//!      @n 入れ替わり、Alphaの脱落、Row pitch余白の混入、16bitのByte順反転といった形で静かに絵が
//!      @n 変質する。いずれも例外を出さないため、下流のGolden Testまで発覚しない種類の壊れ方である。
//!      @n 併せて、写せないFormatと長さの合わないFrameが例外ではなく構造化Errorで返ることを固定する。
//!      @n GPUを使わないCPU専用の契約であり、Backendの有無に関係なく必ず実行される。
//!     \~english
//!         A renderer frame and a Core image form the only boundary across which pixels travel
//!         inside WSE. If this contract breaks, a picture read back from the GPU quietly changes as
//!         soon as it enters Core image processing: swapped channel order, a dropped alpha, row
//!         padding folded into the pixels, or a byte-swapped sixteen-bit sample. None of those throw,
//!         so they surface only in a downstream golden test. It also fixes that an unmappable format
//!         and a short frame come back as structured errors rather than exceptions. The contract is
//!         CPU-only, so it runs whether or not a backend is present.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/renderer/RendererFrameOps.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

int g_failures = 0;

void expect( const bool condition_in, const std::string& description_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << description_in << '\n';
        ++g_failures;
    }
}

//! 各Sampleが位置から一意に決まるFrameを作る。row_pitchに余白を持たせられる。
//! 値は 1 + y * 10 + x + channel * 40 で、y、x、channelのどれが入れ替わっても別の値になる.
//! つまりChannel順序の取り違えも行の詰め違いも、単純な比較で見分けられる.
//! 書き込むのは各Sampleの先頭1 Byteだけなので、この生成器が意味を持つのは8bit Formatに限る.
//! 16bit Formatの往復は下のBlockで手書きのByte列を使う.
wse::oui::sRendererFrame makeFrame(
      const std::uint32_t                width_in
    , const std::uint32_t                height_in
    , const wse::oui::eRendererPixelFormat format_in
    , const std::size_t                  channels_in
    , const std::size_t                  sample_bytes_in
    , const std::size_t                  pitch_padding_in = 0U )
{
    wse::oui::sRendererFrame frame;
    frame.description.extent = { width_in, height_in };
    frame.description.format = format_in;
    frame.description.row_pitch =
        static_cast< std::size_t >( width_in ) * channels_in * sample_bytes_in + pitch_padding_in;
    frame.data.assign( frame.description.row_pitch * height_in, 0U );
    for( std::uint32_t y = 0U; y < height_in; ++y )
    {
        for( std::uint32_t x = 0U; x < width_in; ++x )
        {
            for( std::size_t channel = 0U; channel < channels_in; ++channel )
            {
                const std::size_t offset = y * frame.description.row_pitch
                    + ( x * channels_in + channel ) * sample_bytes_in;
                frame.data[ offset ] =
                    static_cast< std::uint8_t >( 1U + y * 10U + x + channel * 40U );
            }
        }
    }
    return frame;
}

} // namespace

int main()
{
    using namespace wse::oui;

    // Renderer FormatとCore Formatの対応表.
    {
        const std::initializer_list<
            std::pair< eRendererPixelFormat, wse::ePixFormat > > mapped = {
              { eRendererPixelFormat::R8Unorm    , wse::ePixFormat::CH1D8   }
            , { eRendererPixelFormat::Rgba8Unorm , wse::ePixFormat::CH4D8   }
            , { eRendererPixelFormat::Bgra8Unorm , wse::ePixFormat::BGRA4D8 }
            , { eRendererPixelFormat::Rgba16Unorm, wse::ePixFormat::CH4D16  }
        };
        for( const auto& entry : mapped )
        {
            wse::ePixFormat core = wse::ePixFormat::CH1D8;
            expect( coreFormatOf( &core, entry.first ) && core == entry.second
                  , "Every convertible renderer format maps onto its Core format" );
        }

        // Alphaを持つFormatは4 Channelのまま写る。
        wse::ePixFormat rgba = wse::ePixFormat::CH1D8;
        wse::ePixFormat bgra = wse::ePixFormat::CH1D8;
        expect( coreFormatOf( &rgba, eRendererPixelFormat::Rgba8Unorm ) &&
                wse::getDataNum( rgba ) == 4U
              , "Rgba8Unorm keeps four channels so its alpha is not dropped" );
        expect( coreFormatOf( &bgra, eRendererPixelFormat::Bgra8Unorm ) &&
                wse::getDataNum( bgra ) == 4U &&
                wse::getChannelOrder( bgra ) == wse::eColorChannelOrder::Bgr
              , "Bgra8Unorm keeps four channels and its channel order" );

        // 半精度浮動小数点はCoreの整数Pixelでは表現できない。
        wse::ePixFormat unsupported = wse::ePixFormat::CH1D8;
        expect( !coreFormatOf( &unsupported, eRendererPixelFormat::Rgba16Float )
              , "Rgba16Float has no Core pixel format" );
        expect( !coreFormatOf( &unsupported, eRendererPixelFormat::Unknown )
              , "An unknown format has no Core pixel format" );
    }

    // 余白付きFrameの取り込みと書き戻し.
    {
        const sRendererFrame source =
            makeFrame( 4U, 3U, eRendererPixelFormat::Rgba8Unorm, 4U, 1U, 7U );
        wse::img4c08_t image;
        expect( toImage( &image, source ).succeeded()
              , "A padded Rgba8Unorm frame converts into an image" );
        expect( image.width() == 4U && image.height() == 3U
              , "The image takes the frame extent" );

        bool identical = true;
        for( std::uint32_t y = 0U; y < 3U; ++y )
        {
            for( std::uint32_t x = 0U; x < 4U; ++x )
            {
                for( std::size_t channel = 0U; channel < 4U; ++channel )
                {
                    const std::size_t offset =
                        y * source.description.row_pitch + x * 4U + channel;
                    identical = identical &&
                        image[ y ][ x ][ channel ] == source.data[ offset ];
                }
            }
        }
        expect( identical, "The row pitch is folded away and every sample survives" );

        const RendererResult< sRendererFrame > written = toRendererFrame( image );
        expect( written.succeeded(), "An image converts back into a renderer frame" );
        expect( written.value().description.format == eRendererPixelFormat::Rgba8Unorm
              , "The image type selects the renderer format" );
        expect( written.value().description.row_pitch == 0U
              , "A converted frame leaves the packed pitch to the backend" );
        expect( written.value().description.extent.width == 4U &&
                written.value().description.extent.height == 3U
              , "A converted frame keeps its extent" );
        expect( validateRendererFrame( written.value() ).ok()
              , "A converted frame satisfies the renderer frame contract" );
    }

    // Channel順序は型で守られる.
    {
        const sRendererFrame bgra_frame =
            makeFrame( 2U, 2U, eRendererPixelFormat::Bgra8Unorm, 4U, 1U );
        wse::img4c08_bgra_t bgra_image;
        expect( toImage( &bgra_image, bgra_frame ).succeeded()
              , "A Bgra8Unorm frame converts into a BGRA image" );
        expect( bgra_image[ 0U ][ 0U ][ 3U ] == bgra_frame.data[ 3U ]
              , "A BGRA frame keeps its alpha channel" );

        wse::img4c08_t wrong_order;
        const RendererStatus refused = toImage( &wrong_order, bgra_frame );
        expect( !refused.succeeded() &&
                refused.error().category() == eRendererErrorCategory::Unsupported &&
                refused.error().code() == eRendererErrorCode::UnsupportedFormat
              , "A BGRA frame does not silently become an RGBA image" );
        expect( wrong_order.width() == 0U
              , "A refused conversion leaves the destination untouched" );
    }

    // 16bitはLittle EndianのByte順で往復する.
    {
        sRendererFrame frame;
        frame.description.extent    = { 1U, 1U };
        frame.description.format    = eRendererPixelFormat::Rgba16Unorm;
        frame.description.row_pitch = 8U;
        frame.data = { 0x34U, 0x12U, 0x78U, 0x56U, 0xBCU, 0x9AU, 0xF0U, 0xDEU };
        wse::img4c16_t image;
        expect( toImage( &image, frame ).succeeded()
              , "An Rgba16Unorm frame converts into an image" );
        expect( image[ 0U ][ 0U ][ 0U ] == 0x1234U && image[ 0U ][ 0U ][ 3U ] == 0xDEF0U
              , "A sixteen-bit sample is read little endian and keeps its alpha" );
        expect( toRendererFrame( image ).value().data == frame.data
              , "A sixteen-bit round trip restores the original bytes" );
    }

    // 半精度Frameと不整合Frameは、例外ではなく構造化Errorで返る.
    {
        sRendererFrame half;
        half.description.extent    = { 2U, 2U };
        half.description.format    = eRendererPixelFormat::Rgba16Float;
        half.description.row_pitch = 16U;
        half.data.assign( 32U, 0U );
        wse::img4c16_t image;
        const RendererStatus refused = toImage( &image, half );
        expect( !refused.succeeded() &&
                refused.error().code() == eRendererErrorCode::UnsupportedFormat
              , "A half-precision frame is refused with a stable code" );

        sRendererFrame truncated =
            makeFrame( 4U, 3U, eRendererPixelFormat::Rgba8Unorm, 4U, 1U );
        truncated.data.pop_back();
        wse::img4c08_t truncated_image;
        const RendererStatus rejected = toImage( &truncated_image, truncated );
        expect( !rejected.succeeded() &&
                rejected.error().category() == eRendererErrorCategory::Validation
              , "A frame whose data is short is refused without throwing" );

        const wse::img4c08_t empty;
        const RendererResult< sRendererFrame > from_empty = toRendererFrame( empty );
        expect( !from_empty.succeeded() &&
                from_empty.error().code() == eRendererErrorCode::InvalidArgument
              , "An empty image has no renderer frame" );
    }

    return g_failures == 0 ? 0 : 1;
}
