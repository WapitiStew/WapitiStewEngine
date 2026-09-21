//*****************************************************************************************************************
//!
//! @file    wse_image_interleaved_contract.cpp
//! @brief   \~japanese Interleaved Bufferと Image_ の相互変換の契約を検証する.
//! @brief   \~english  Verifies the contract between an interleaved buffer and Image_.
//!
//! @details
//!     \~japanese
//!         ここはDeviceが所有するFrame Bufferの生Pointerを受け取る境界であり、破れたときの被害が
//!      @n 最も大きい場所である. 境界計算を誤ればBuffer終端の外を読むし、Row Strideを畳み損ねれば
//!      @n 余白のGomiがそのまま画素として流れ込む. 検証(`isInterleavedViewValid`)がfalseを返す道と
//!      @n 構築(`makeImageFromInterleaved`)が例外を投げる道が食い違えば、呼出側は事前検証しても
//!      @n 安全にならない. 16bit SampleのByte順(Little Endian)もここで固定する.
//!     \~english
//!         This is the boundary that takes a raw pointer into a frame buffer a device owns, so it is
//!      @n where a broken contract costs the most. A bad bound reads past the end of that buffer, and
//!      @n a stride that is not folded away lets padding bytes arrive as pixel samples. If the
//!      @n validating path (`isInterleavedViewValid`) and the constructing path
//!      @n (`makeImageFromInterleaved`) ever disagree, checking first no longer makes a caller safe.
//!      @n The little-endian byte order of a sixteen-bit sample is pinned here too.
//!
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/stew.h>

#include <stdexcept>

#include <cstdint>
#include <iostream>
#include <string>
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

constexpr std::size_t WIDTH   = 4U;
constexpr std::size_t HEIGHT  = 3U;
//! 余白付きの行。packedな12 Byteに5 Byte足してある。
//! 5は3の倍数でも2の冪でもないので、Strideを幅や画素の大きさから割り出そうとする実装はここで破綻する.
constexpr std::size_t PADDED_STRIDE = WIDTH * 3U + 5U;

//! 余白の値は複製されてはならないので、目印になる値で埋める。
std::vector< std::uint8_t > makePaddedRgbBuffer()
{
    std::vector< std::uint8_t > buffer( PADDED_STRIDE * HEIGHT, 0xEEU );
    for( std::size_t y = 0U; y < HEIGHT; ++y )
    {
        for( std::size_t x = 0U; x < WIDTH; ++x )
        {
            const std::size_t offset = y * PADDED_STRIDE + x * 3U;
            buffer[ offset + 0U ] = static_cast< std::uint8_t >( y * 10U + x );
            buffer[ offset + 1U ] = static_cast< std::uint8_t >( y * 10U + x + 1U );
            buffer[ offset + 2U ] = static_cast< std::uint8_t >( y * 10U + x + 2U );
        }
    }
    return buffer;
}

} // namespace

int main()
{
    // packedな1行の大きさ.
    expect( wse::packedRowBytes< wse::ePixFormat::CH3D8  >( WIDTH ) == WIDTH * 3U
          , "A packed eight-bit RGB row is three bytes per pixel" );
    expect( wse::packedRowBytes< wse::ePixFormat::CH4D16 >( WIDTH ) == WIDTH * 8U
          , "A packed sixteen-bit RGBA row is eight bytes per pixel" );
    // 幅0で0が返るのは、乗算が溢れた場合と同じ値である. 呼出側はこの0を「packedな行が作れない」
    // 唯一の印として扱えばよく、別のError経路を持たなくて済む.
    expect( wse::packedRowBytes< wse::ePixFormat::CH3D8  >( 0U ) == 0U
          , "A zero width has no packed row" );

    const std::vector< std::uint8_t > buffer = makePaddedRgbBuffer();

    // 余白付きBufferの検証と取り込み.
    {
        wse::sInterleavedView view;
        view.data             = buffer.data();
        view.width            = WIDTH;
        view.height           = HEIGHT;
        view.row_stride       = PADDED_STRIDE;
        view.accessible_bytes = buffer.size();
        expect( wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( view )
              , "A padded view is valid" );

        const wse::img3c08_t image =
            wse::makeImageFromInterleaved< wse::ePixFormat::CH3D8 >( view );
        expect( image.width() == WIDTH && image.height() == HEIGHT
              , "The image takes the extent of its view" );

        // 比較側のOffsetもPADDED_STRIDEで計算する. Buffer全体をそのまま複製した実装であれば、
        // 2行目以降が5 Byteずつずれてここで露見する.
        bool identical = true;
        for( std::size_t y = 0U; y < HEIGHT; ++y )
        {
            for( std::size_t x = 0U; x < WIDTH; ++x )
            {
                const std::size_t offset = y * PADDED_STRIDE + x * 3U;
                for( std::size_t channel = 0U; channel < 3U; ++channel )
                {
                    identical = identical &&
                        image[ y ][ x ][ channel ] == buffer[ offset + channel ];
                }
            }
        }
        expect( identical, "The row stride is folded away and no padding byte is copied" );
    }

    // Strideが0のときは詰まった行として扱う.
    {
        std::vector< std::uint8_t > packed( WIDTH * HEIGHT * 3U );
        for( std::size_t index = 0U; index < packed.size(); ++index )
        {
            packed[ index ] = static_cast< std::uint8_t >( index );
        }
        wse::sInterleavedView view;
        view.data             = packed.data();
        view.width            = WIDTH;
        view.height           = HEIGHT;
        view.row_stride       = 0U;
        view.accessible_bytes = packed.size();
        expect( wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( view )
              , "A zero stride selects the packed row" );
        const wse::img3c08_t image =
            wse::makeImageFromInterleaved< wse::ePixFormat::CH3D8 >( view );
        // 2行目の先頭を見るのは、Strideの解決を誤れば必ずずれる最初の位置だからである.
        // 1行目だけならStrideが何であっても一致してしまう.
        expect( image[ 1U ][ 0U ][ 0U ] == packed[ WIDTH * 3U ]
              , "A zero stride reads consecutive rows" );
    }

    // 拒否条件。いずれも例外ではなくfalseで返る.
    {
        wse::sInterleavedView valid;
        valid.data             = buffer.data();
        valid.width            = WIDTH;
        valid.height           = HEIGHT;
        valid.row_stride       = PADDED_STRIDE;
        valid.accessible_bytes = buffer.size();

        // 例外ではなくfalseで返すと決めたのは、毎Frame来るViewを篩う道であり、記憶域確保も
        // 例外の巻き戻しCostも掛けられないためである. 以下は有効なViewを1項目ずつ崩し、
        // 検証がその項目を実際に見ていることを示す.
        wse::sInterleavedView null_data = valid;
        null_data.data = nullptr;
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( null_data )
              , "A view without a pointer is rejected" );

        wse::sInterleavedView zero_width = valid;
        zero_width.width = 0U;
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( zero_width )
              , "A zero width is rejected" );

        wse::sInterleavedView zero_height = valid;
        zero_height.height = 0U;
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( zero_height )
              , "A zero height is rejected" );

        // 境界はちょうど「packedな1行以上」であり、1 Byte足りないだけで拒否される.
        // 申告されたStrideをそのまま信じると、この不足が行ごとに累積して最後にBufferの外へ出る.
        wse::sInterleavedView short_stride = valid;
        short_stride.row_stride = WIDTH * 3U - 1U;
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( short_stride )
              , "A stride shorter than one packed row is rejected" );

        // 最終行の余白までは要らない。必要なのは (height - 1) * stride + packedな1行 である。
        const std::size_t required_bytes = ( HEIGHT - 1U ) * PADDED_STRIDE + WIDTH * 3U;
        wse::sInterleavedView trailing_padding_absent = valid;
        trailing_padding_absent.accessible_bytes = required_bytes;
        expect( wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( trailing_padding_absent )
              , "The padding after the last row does not have to be readable" );

        wse::sInterleavedView short_buffer = valid;
        short_buffer.accessible_bytes = required_bytes - 1U;
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH3D8 >( short_buffer )
              , "A buffer that ends before the last row is rejected" );

        // 同じBytesでも、1 Pixelが大きいFormatとしては足りない。
        expect( !wse::isInterleavedViewValid< wse::ePixFormat::CH4D16 >( valid )
              , "The bound is checked against the requested format" );

        // 不正なViewの取り込みは構造化された例外になる。
        // Codeまで見るのは、検証の失敗と記憶域確保の失敗を呼出側が区別できなければならないためで、
        // 併せて検証と構築が同じ境界で判断していること、つまり事前検証が意味を持つことを示す.
        bool rejected = false;
        try
        {
            const wse::img3c08_t unused =
                wse::makeImageFromInterleaved< wse::ePixFormat::CH3D8 >( short_buffer );
            static_cast< void >( unused );
        }
        catch( const std::invalid_argument& )
        {
            rejected = true;
        }
        expect( rejected, "Building from an invalid view reports std::invalid_argument" );
    }

    // 書き出しは余白を残したまま画素だけを更新する.
    {
        wse::sInterleavedView view;
        view.data             = buffer.data();
        view.width            = WIDTH;
        view.height           = HEIGHT;
        view.row_stride       = PADDED_STRIDE;
        view.accessible_bytes = buffer.size();
        const wse::img3c08_t image =
            wse::makeImageFromInterleaved< wse::ePixFormat::CH3D8 >( view );

        // 書き込み先は0x11で埋めておく. 余白に0x11が残っていれば、書き出しが行ごとにpackedな
        // 1行分だけを触り、Strideの隙間を跨いで塗り潰していないことが分かる. 隙間まで書く実装は、
        // Bufferの持ち主がそこに置いた値を黙って壊す.
        std::vector< std::uint8_t > destination( PADDED_STRIDE * HEIGHT, 0x11U );
        wse::sInterleavedTarget target;
        target.data             = destination.data();
        target.width            = WIDTH;
        target.height           = HEIGHT;
        target.row_stride       = PADDED_STRIDE;
        target.accessible_bytes = destination.size();
        expect( wse::isInterleavedTargetValid< wse::ePixFormat::CH3D8 >( target )
              , "A padded destination is valid" );
        expect( wse::writeImageToInterleaved( &target, image )
              , "Writing an image back out succeeds" );

        bool identical = true;
        for( std::size_t y = 0U; y < HEIGHT; ++y )
        {
            for( std::size_t x = 0U; x < WIDTH * 3U; ++x )
            {
                identical = identical &&
                    destination[ y * PADDED_STRIDE + x ] == buffer[ y * PADDED_STRIDE + x ];
            }
            // 余白は書き換えられない。
            identical = identical && destination[ y * PADDED_STRIDE + WIDTH * 3U ] == 0x11U;
        }
        expect( identical, "A round trip restores every pixel and leaves the padding alone" );

        // 大きさが違う書き込み先は、書き換えずに拒否する.
        // accessible_bytesは据え置きなので、高さを1増やしたこのTargetは境界検証の側でも落ちる.
        // 見ているのは戻り値だけで、書き込み先が無傷であることまでは確かめていない.
        wse::sInterleavedTarget mismatched = target;
        mismatched.height = HEIGHT + 1U;
        expect( !wse::writeImageToInterleaved( &mismatched, image )
              , "A destination whose extent differs is refused" );
    }

    // 16bitはLittle EndianのByte順で運ぶ.
    {
        // 先頭のByteが下位である. 0x34,0x12は0x1234であって0x3412ではない. Little Endian Hostでは
        // 取り込みが素通しの複製になるため、この並びを外すとBig Endian Hostでだけ壊れる差になる.
        // 2行目は1,2,3と読める値にしてあり、Strideを跨いだ行の先頭を識別できる.
        std::vector< std::uint8_t > samples = {
              0x34U, 0x12U, 0x78U, 0x56U, 0xBCU, 0x9AU
            , 0x01U, 0x00U, 0x02U, 0x00U, 0x03U, 0x00U
        };
        wse::sInterleavedView view;
        view.data             = samples.data();
        view.width            = 1U;
        view.height           = 2U;
        view.row_stride       = 6U;
        view.accessible_bytes = samples.size();
        const wse::img3c16_t image =
            wse::makeImageFromInterleaved< wse::ePixFormat::CH3D16 >( view );
        expect( image[ 0U ][ 0U ][ 0U ] == 0x1234U &&
                image[ 0U ][ 0U ][ 1U ] == 0x5678U &&
                image[ 0U ][ 0U ][ 2U ] == 0x9ABCU
              , "A sixteen-bit sample is read little endian" );
        expect( image[ 1U ][ 0U ][ 0U ] == 1U
              , "The second row starts at its own stride" );
    }

    // 4 Channelの取り込みは Alpha を落とさない.
    {
        // BGRA4D8はCH4D8にFlagを立てた値なので、Channel数とSample幅の解決がFlagを落としてから
        // 行われなければ1 Pixelの大きさを取り違える. 取り込み側は順序を解釈せず、来たBytesを
        // そのまま並べるだけで、Channel 3のAlphaも位置を変えない.
        std::vector< std::uint8_t > bgra = { 10U, 20U, 30U, 40U };
        wse::sInterleavedView view;
        view.data             = bgra.data();
        view.width            = 1U;
        view.height           = 1U;
        view.row_stride       = 0U;
        view.accessible_bytes = bgra.size();
        const wse::img4c08_bgra_t image =
            wse::makeImageFromInterleaved< wse::ePixFormat::BGRA4D8 >( view );
        expect( image[ 0U ][ 0U ][ 0U ] == 10U && image[ 0U ][ 0U ][ 1U ] == 20U &&
                image[ 0U ][ 0U ][ 2U ] == 30U && image[ 0U ][ 0U ][ 3U ] == 40U
              , "A four-channel view keeps its alpha channel" );
    }

    return g_failures == 0 ? 0 : 1;
}
