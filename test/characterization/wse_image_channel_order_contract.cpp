//*****************************************************************************************************************
//!
//! @file    wse_image_channel_order_contract.cpp
//! @brief   \~japanese Channel順序を表すPixel Formatの契約を検証する.
//! @brief   \~english  Verifies the contract of the channel-order pixel formats.
//!
//! @details
//!     \~japanese
//!         BGR系Formatは対応するCHxDyyに`COLOR_ORDER_BGR_FLAG`を立てただけの値で、記憶レイアウトを
//!      @n 共有したまま型だけが分かれる. この契約が崩れると三つが同時に壊れる. 列挙子の値が動けば
//!      @n 保存済みDataとC ABIの互換が失われ、型が同一になればBGRのBufferをRGB用のCodeへ渡す
//!      @n 取り違えをCompilerが止められなくなり、順序変換が非対称になればRound Tripで色が入れ替わる.
//!     \~english
//!         A channel-order format is its CHxDyy counterpart with `COLOR_ORDER_BGR_FLAG` set: the two
//!      @n share a storage layout while staying distinct types. Three things break together if this
//!      @n contract slips. Moving an enumerator value breaks stored data and the C ABI; collapsing
//!      @n the two types lets a BGR buffer reach code that reads RGB without a diagnostic; and an
//!      @n asymmetric reorder swaps colours across a round trip.
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
#include <type_traits>

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

// 既存の列挙子の値は一切動かせない。ABIと保存済みDataの互換がここに掛かっている。
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH1D8  ) ==  0U, "CH1D8 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH1D64 ) ==  6U, "CH1D64 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH2D8  ) == 10U, "CH2D8 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH2D64 ) == 16U, "CH2D64 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH3D8  ) == 20U, "CH3D8 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH3D16 ) == 24U, "CH3D16 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH3D64 ) == 26U, "CH3D64 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH4D8  ) == 30U, "CH4D8 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH4D16 ) == 34U, "CH4D16 moved" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::CH4D64 ) == 36U, "CH4D64 moved" );

// 順序付きFormatは対応するCHxDyyにFlagを立てた値である。
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::BGR3D8   ) == 0x1014U, "BGR3D8 value" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::BGR3D16  ) == 0x1018U, "BGR3D16 value" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::BGRA4D8  ) == 0x101EU, "BGRA4D8 value" );
static_assert( static_cast< std::uint16_t >( wse::ePixFormat::BGRA4D16 ) == 0x1022U, "BGRA4D16 value" );

// RGB別名は値を変えない。
static_assert( wse::ePixFormat::RGB3D8   == wse::ePixFormat::CH3D8 , "RGB3D8 alias" );
static_assert( wse::ePixFormat::RGBA4D8  == wse::ePixFormat::CH4D8 , "RGBA4D8 alias" );
static_assert( wse::ePixFormat::RGBA4D16 == wse::ePixFormat::CH4D16, "RGBA4D16 alias" );

// Channel数とBit深度は正規化して解決される。
static_assert( wse::getDataNum ( wse::ePixFormat::BGR3D8   ) == 3U, "BGR3D8 channels" );
static_assert( wse::getBitDepth( wse::ePixFormat::BGR3D8   ) == 8U, "BGR3D8 depth" );
static_assert( wse::getDataNum ( wse::ePixFormat::BGRA4D8  ) == 4U, "BGRA4D8 channels" );
static_assert( wse::getBitDepth( wse::ePixFormat::BGRA4D16 ) == 16U, "BGRA4D16 depth" );

// 問い合わせ側の対称性。Flagを落とせばCanonicalへ戻り、Flagを持たないFormatは既定のRgbと答える.
// Mono(CH1D8)にもRgbを返すのは、順序の概念が無いFormatでも問い合わせが失敗しないと決めたためである.
static_assert( wse::canonicalFormat( wse::ePixFormat::BGR3D8   ) == wse::ePixFormat::CH3D8 , "BGR3D8 canonical" );
static_assert( wse::canonicalFormat( wse::ePixFormat::BGRA4D16 ) == wse::ePixFormat::CH4D16, "BGRA4D16 canonical" );
static_assert( wse::canonicalFormat( wse::ePixFormat::CH3D8    ) == wse::ePixFormat::CH3D8 , "canonical is idempotent" );
static_assert( wse::getChannelOrder( wse::ePixFormat::BGR3D8 ) == wse::eColorChannelOrder::Bgr, "BGR order" );
static_assert( wse::getChannelOrder( wse::ePixFormat::CH3D8  ) == wse::eColorChannelOrder::Rgb, "RGB order" );
static_assert( wse::getChannelOrder( wse::ePixFormat::CH1D8  ) == wse::eColorChannelOrder::Rgb, "mono reports Rgb" );
static_assert(  wse::isColorOrderVariant( wse::ePixFormat::BGRA4D8 ), "BGRA4D8 is a variant" );
static_assert( !wse::isColorOrderVariant( wse::ePixFormat::CH4D8   ), "CH4D8 is canonical" );

// 設計の要。記憶レイアウトは共有し、型は分かれる。
static_assert( std::is_same< wse::PixelAlias< wse::ePixFormat::BGR3D8 >
                           , wse::PixelAlias< wse::ePixFormat::CH3D8  > >::value
             , "A channel-order variant shares its pixel storage." );
static_assert( std::is_same< wse::PixelAlias< wse::ePixFormat::BGRA4D16 >
                           , wse::PixelAlias< wse::ePixFormat::CH4D16   > >::value
             , "A sixteen-bit variant shares its pixel storage." );
static_assert( !std::is_same< wse::img3c08_bgr_t, wse::img3c08_t >::value
             , "A channel-order variant is a distinct type at an API boundary." );
static_assert( !std::is_same< wse::img4c08_bgra_t, wse::img4c08_t >::value
             , "A four-channel variant is a distinct type at an API boundary." );

} // namespace

int main()
{
    // Unknown formats still fail after color-order aliases are canonicalized.
    for( const auto invalid : { static_cast<wse::ePixFormat>( 0x0fffU ),
                               static_cast<wse::ePixFormat>( 0xffffU ) } )
    {
        bool channels_rejected = false;
        bool depth_rejected = false;
        try { (void)wse::getDataNum( invalid ); }
        catch( const std::invalid_argument& ) { channels_rejected = true; }
        try { (void)wse::getBitDepth( invalid ); }
        catch( const std::invalid_argument& ) { depth_rejected = true; }
        expect( channels_rejected && depth_rejected, "Unknown pixel formats are rejected" );
    }

    // 3 Channelの順序入れ替えは往復して元へ戻る.
    {
        wse::img3c08_t rgb( 2U, 2U );
        for( std::size_t y = 0U; y < 2U; ++y )
        {
            for( std::size_t x = 0U; x < 2U; ++x )
            {
                const std::uint8_t base = static_cast< std::uint8_t >( ( y * 2U + x ) * 10U );
                rgb[ y ][ x ][ 0U ] = base;
                rgb[ y ][ x ][ 1U ] = static_cast< std::uint8_t >( base + 1U );
                rgb[ y ][ x ][ 2U ] = static_cast< std::uint8_t >( base + 2U );
            }
        }
        // Pixelごとに違う値を敷いてあるので、Channelの入れ替えとPixelの移動を取り違えれば
        // 次の比較で露見する. 全画素同値のTest画像ではこの二つを区別できない.
        const wse::img3c08_bgr_t bgr =
            wse::convertChannelOrder< wse::ePixFormat::BGR3D8, wse::ePixFormat::CH3D8 >( rgb );
        expect( bgr.width() == 2U && bgr.height() == 2U, "Reordered image keeps its extent" );
        expect( bgr[ 0U ][ 0U ][ 0U ] == rgb[ 0U ][ 0U ][ 2U ] &&
                bgr[ 0U ][ 0U ][ 1U ] == rgb[ 0U ][ 0U ][ 1U ] &&
                bgr[ 0U ][ 0U ][ 2U ] == rgb[ 0U ][ 0U ][ 0U ]
              , "Three-channel order swaps channel zero with channel two" );

        // 逆向きの変換が厳密な逆写像であること。Deviceから受けたBufferを表示側のFormatへ移して
        // 戻す経路で、Sampleが1つも欠けず丸められもしないことをここが担保する.
        const wse::img3c08_t restored =
            wse::convertChannelOrder< wse::ePixFormat::CH3D8, wse::ePixFormat::BGR3D8 >( bgr );
        bool identical = true;
        for( std::size_t y = 0U; y < 2U; ++y )
        {
            for( std::size_t x = 0U; x < 2U; ++x )
            {
                for( std::size_t channel = 0U; channel < 3U; ++channel )
                {
                    identical = identical &&
                        restored[ y ][ x ][ channel ] == rgb[ y ][ x ][ channel ];
                }
            }
        }
        expect( identical, "A channel-order round trip restores every sample" );
    }

    // 4 Channelでは Alpha が保持される.
    {
        wse::img4c08_bgra_t bgra( 1U, 1U );
        bgra[ 0U ][ 0U ][ 0U ] = 10U;  // blue
        bgra[ 0U ][ 0U ][ 1U ] = 20U;  // green
        bgra[ 0U ][ 0U ][ 2U ] = 30U;  // red
        bgra[ 0U ][ 0U ][ 3U ] = 40U;  // alpha
        // 入れ替えの対象はChannel 0..2に限られ、Channel 3は位置も値も動かない. 4 Channelの変換を
        // 単純な逆順として実装するとAlphaがChannel 0へ回り込むので、4 Sampleを別々の値にしてある.
        const wse::img4c08_t rgba =
            wse::convertChannelOrder< wse::ePixFormat::CH4D8, wse::ePixFormat::BGRA4D8 >( bgra );
        expect( rgba[ 0U ][ 0U ][ 0U ] == 30U &&
                rgba[ 0U ][ 0U ][ 1U ] == 20U &&
                rgba[ 0U ][ 0U ][ 2U ] == 10U
              , "Four-channel order swaps only the colour channels" );
        expect( rgba[ 0U ][ 0U ][ 3U ] == 40U
              , "A channel-order conversion carries the alpha channel through" );
    }

    // 空の画像は構造化された例外で拒否される.
    {
        // 戻り値がImage_そのものなので、この変換はBoolで拒否を伝えられない. 確かめているのはCodeが
        // std::invalid_argumentであることで、記憶域確保の失敗 (std::bad_alloc) と区別できる.
        bool rejected = false;
        try
        {
            const wse::img3c08_t empty;
            const wse::img3c08_bgr_t unused =
                wse::convertChannelOrder< wse::ePixFormat::BGR3D8, wse::ePixFormat::CH3D8 >( empty );
            static_cast< void >( unused );
        }
        catch( const std::invalid_argument& )
        {
            rejected = true;
        }
        expect( rejected, "An empty source is rejected with std::invalid_argument" );
    }

    // 向き補正はChannel順序を解釈しないため、両Formatで同じBytesを出す.
    {
        wse::img3c08_t rgb( 2U, 3U );
        wse::img3c08_bgr_t bgr( 2U, 3U );
        for( std::size_t y = 0U; y < 3U; ++y )
        {
            for( std::size_t x = 0U; x < 2U; ++x )
            {
                for( std::size_t channel = 0U; channel < 3U; ++channel )
                {
                    const std::uint8_t value =
                        static_cast< std::uint8_t >( ( y * 2U + x ) * 3U + channel );
                    rgb[ y ][ x ][ channel ] = value;
                    bgr[ y ][ x ][ channel ] = value;
                }
            }
        }
        // 幅2×高さ3と非正方にしてあるのは、Rotate90CWが幅と高さを入れ替えるためで、正方形では
        // Indexの転置間違いが隠れてしまう. 両Formatへ同じBytesを入れ、出力もByte単位で一致させる.
        // ここが示すのは向き補正がFormat非依存であることで、回転そのものの正しさは別のTestが持つ.
        const wse::img3c08_t rotated_rgb =
            wse::applyOrientation( rgb, wse::eImageOrientation::Rotate90CW );
        const wse::img3c08_bgr_t rotated_bgr =
            wse::applyOrientation( bgr, wse::eImageOrientation::Rotate90CW );
        expect( rotated_rgb.width() == rotated_bgr.width() &&
                rotated_rgb.height() == rotated_bgr.height()
              , "Orientation gives both formats the same extent" );
        bool identical = true;
        for( std::size_t y = 0U; y < rotated_rgb.height(); ++y )
        {
            for( std::size_t x = 0U; x < rotated_rgb.width(); ++x )
            {
                for( std::size_t channel = 0U; channel < 3U; ++channel )
                {
                    identical = identical &&
                        rotated_rgb[ y ][ x ][ channel ] == rotated_bgr[ y ][ x ][ channel ];
                }
            }
        }
        expect( identical, "Orientation moves whole pixels and never interprets a channel" );
    }

    return g_failures == 0 ? 0 : 1;
}
