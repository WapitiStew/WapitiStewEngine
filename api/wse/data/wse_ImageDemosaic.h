//*****************************************************************************************************************
//!
//! @file    wse_ImageDemosaic.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-02, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Bayer配列の単色画像をColor画像へ変換する処理を定義するファイル。
//!     \~english  Defines the conversion from a Bayer-mosaiced monochrome image to a colour image.
//!
//!
//! @details
//!     \~japanese
//!         本ファイルはDeviceを一切参照しない汎用の画像演算だけを提供する。
//!      @n 入力は1 Channelの単色画像で、各画素が`eBayerPattern`の並びに従ってR、G、Bのいずれかを持つ。
//!      @n 出力は3 Channelであり、Channel順序は呼び出し側が`eColorChannelOrder`で指定する。
//!      @n Bayer画像はSubsamplingされているため、回転や反転を適用すると配列の意味が壊れる。
//!      @n 向きの補正はColorへ変換した後に行うこと。
//!     \~english
//!         This file provides general image operations only and never refers to a device.
//!      @n The source is a one-channel monochrome image whose pixels carry R, G, or B according to
//!      @n the `eBayerPattern` layout. The result has three channels in the order the caller names
//!      @n with `eColorChannelOrder`.
//!      @n A Bayer image is subsampled, so rotating or mirroring it destroys the layout. Correct the
//!      @n orientation after converting to colour.
//!
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma once

#ifndef WONDERSTEWENGINE_DATA_IMAGEDEMOSAIC_H
#define WONDERSTEWENGINE_DATA_IMAGEDEMOSAIC_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "wse_Image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>


namespace wse
{
    //!
    //! @brief
    //!     \~japanese Bayer配列。名前は左上2x2 Blockの並びを表す。
    //!     \~english  Bayer layout; the name gives the order of the top-left two-by-two block.
    //!
    enum class eBayerPattern : std::uint8_t
    {
          Rggb = 0U  //!< \~japanese 左上からR, Gr / Gb, B.  \~english R, Gr / Gb, B from the top left.
        , Bggr       //!< \~japanese 左上からB, Gb / Gr, R.  \~english B, Gb / Gr, R from the top left.
        , Grbg       //!< \~japanese 左上からGr, R / B, Gb.  \~english Gr, R / B, Gb from the top left.
        , Gbrg       //!< \~japanese 左上からGb, B / R, Gr.  \~english Gb, B / R, Gr from the top left.
    };


    // \~japanese 出力Color画像のChannel順序を表す`eColorChannelOrder`は`wse_Enum.h`にある。
    // \~english  `eColorChannelOrder`, the channel order of the resulting colour image, lives in
    //            wse_Enum.h so that the pixel-format helpers can name it too.


    //!
    //! @brief
    //!     \~japanese Demosaicの方式。
    //!     \~english  Demosaic method.
    //!
    //! @details
    //!     \~japanese
    //!         `Block2x2`は2x2 Block内の4 Sampleをそのまま4画素へ複製する。実効解像度は半分になるが、
    //!      @n 補間を行わないため入力Sample値がそのまま残る。
    //!      @n `Bilinear`は各画素で欠けているChannelを近傍から平均して補う。輪郭がなめらかになる。
    //!     \~english
    //!         `Block2x2` copies the four samples of a two-by-two block to all four of its pixels. The
    //!         effective resolution halves, but no interpolation happens, so the sample values survive
    //!         unchanged.
    //!      @n `Bilinear` fills each pixel's missing channels by averaging its neighbours, which gives
    //!      @n smoother edges.
    //!
    enum class eDemosaicMethod : std::uint8_t
    {
          Block2x2 = 0U
        , Bilinear
    };


    namespace detail
    {
        //! \~japanese 2x2 Block内の位置(0:左上 1:右上 2:左下 3:右下)ごとの色を返す.
        //! \~english  Colour of each position in a two-by-two block: 0 top-left to 3 bottom-right.
        //! 0 = red, 1 = green, 2 = blue.
        inline std::array< std::uint8_t, 4U > bayerBlockColors( const eBayerPattern pattern_in )
        {
            switch( pattern_in )
            {
                case eBayerPattern::Rggb : return { 0U, 1U, 1U, 2U };
                case eBayerPattern::Bggr : return { 2U, 1U, 1U, 0U };
                case eBayerPattern::Grbg : return { 1U, 0U, 2U, 1U };
                case eBayerPattern::Gbrg : return { 1U, 2U, 0U, 1U };
                default                  : throw std::invalid_argument( "unknown Bayer pattern" );
            }
        }

        //! \~japanese 色Index(0:R 1:G 2:B)を出力Channelへ写す.
        //! \~english  Maps a colour index, red zero to blue two, onto an output channel.
        inline size_t colorChannel( const std::uint8_t color_in, const eColorChannelOrder order_in )
        {
            if( order_in == eColorChannelOrder::Rgb )
                return static_cast< size_t >( color_in );
            return static_cast< size_t >( 2U - color_in );
        }
    }


    //!
    //! @brief
    //!     \~japanese Bayer配列の単色画像をColor画像へ変換する。
    //!     \~english  Converts a Bayer-mosaiced monochrome image into a colour image.
    //!
    //! @details
    //!     \~japanese
    //!         入力は1 Channel、出力は3 Channelでなければならない。Channel型は共通である。
    //!      @n 幅と高さはいずれも2以上であることを要求する。Bayer Blockが成立しないためである。
    //!      @n 端の画素も必ず書き込む。未書き込みの行や列を残さない。
    //!     \~english
    //!         The source has one channel and the result has three, sharing one channel type.
    //!      @n Both the width and the height have to be at least two, because a Bayer block does not
    //!      @n otherwise exist.
    //!      @n Edge pixels are written like any other; no row or column is left untouched.
    //!
    //! @param[in] source_in   \~japanese Bayer単色画像. \~english Bayer monochrome source.
    //! @param[in] pattern_in  \~japanese Bayer配列.     \~english Bayer layout.
    //! @param[in] order_in    \~japanese 出力Channel順序. \~english Result channel order.
    //! @param[in] method_in   \~japanese Demosaic方式.  \~english Demosaic method.
    //!
    //! @return \~japanese Color画像. \~english The colour image.
    //!
    //! @retval 例外 \~japanese 入力が2x2未満の場合 std::invalid_argument を投げる。
    //!              \~english  Throws std::invalid_argument when the source is smaller than two by two.
    //!
    template< ePixFormat _SourcePf, ePixFormat _TargetPf >
    Image_< _TargetPf > demosaic(
          const Image_< _SourcePf >& source_in
        , const eBayerPattern        pattern_in
        , const eColorChannelOrder   order_in  = eColorChannelOrder::Rgb
        , const eDemosaicMethod      method_in = eDemosaicMethod::Bilinear )
    {
        static_assert( PixelTraits< _SourcePf >::DATA_NUM == 1U,
            "demosaic requires a one-channel source" );
        static_assert( PixelTraits< _TargetPf >::DATA_NUM == 3U,
            "demosaic produces a three-channel result" );

        const size_t width  = source_in.width();
        const size_t height = source_in.height();
        if( width < 2U || height < 2U )
        {
            throw std::invalid_argument( "demosaicing requires at least a 2x2 source" );
        }

        using TargetChannel = PixelType< _TargetPf >;
        const std::array< std::uint8_t, 4U > colors = detail::bayerBlockColors( pattern_in );
        Image_< _TargetPf > target( width, height );

        // 位置(x, y)が持つ色を返す。Bayerは2x2周期であるため座標の偶奇だけで決まる。
        const auto color_at = [ &colors ]( const size_t x_in, const size_t y_in ) -> std::uint8_t
        {
            return colors[ ( y_in % 2U ) * 2U + ( x_in % 2U ) ];
        };

        if( method_in == eDemosaicMethod::Block2x2 )
        {
            // 2x2 Blockの4 Sampleをそのまま4画素へ複製する。端が奇数で余る場合は直前のBlockを使う。
            for( size_t y = 0U; y < height; y += 2U )
            {
                for( size_t x = 0U; x < width; x += 2U )
                {
                    const size_t block_x = ( x + 1U < width )  ? x : ( x - 1U );
                    const size_t block_y = ( y + 1U < height ) ? y : ( y - 1U );

                    std::array< TargetChannel, 3U > block = { 0, 0, 0 };
                    std::array< std::uint32_t, 3U > count = { 0U, 0U, 0U };
                    for( size_t offset_y = 0U; offset_y < 2U; ++offset_y )
                    {
                        for( size_t offset_x = 0U; offset_x < 2U; ++offset_x )
                        {
                            const size_t sample_x = block_x + offset_x;
                            const size_t sample_y = block_y + offset_y;
                            const std::uint8_t color = color_at( sample_x, sample_y );
                            // 緑は1 Block内に2 Sampleある。最初の1つを採用する。
                            if( count[ color ] == 0U )
                            {
                                block[ color ] = static_cast< TargetChannel >(
                                    source_in[ sample_y ][ sample_x ][ 0U ] );
                            }
                            ++count[ color ];
                        }
                    }

                    for( size_t offset_y = 0U; offset_y < 2U && ( y + offset_y ) < height; ++offset_y )
                    {
                        for( size_t offset_x = 0U; offset_x < 2U && ( x + offset_x ) < width; ++offset_x )
                        {
                            for( std::uint8_t color = 0U; color < 3U; ++color )
                            {
                                target[ y + offset_y ][ x + offset_x ]
                                      [ detail::colorChannel( color, order_in ) ] = block[ color ];
                            }
                        }
                    }
                }
            }
            return target;
        }

        // Bilinear: 各画素で欠けている色を、その色を持つ近傍の平均で埋める。
        for( size_t y = 0U; y < height; ++y )
        {
            for( size_t x = 0U; x < width; ++x )
            {
                std::array< std::uint64_t, 3U > sum   = { 0U, 0U, 0U };
                std::array< std::uint32_t, 3U > count = { 0U, 0U, 0U };

                const size_t first_y = ( y == 0U ) ? 0U : ( y - 1U );
                const size_t last_y  = ( y + 1U < height ) ? ( y + 1U ) : y;
                const size_t first_x = ( x == 0U ) ? 0U : ( x - 1U );
                const size_t last_x  = ( x + 1U < width ) ? ( x + 1U ) : x;
                for( size_t sample_y = first_y; sample_y <= last_y; ++sample_y )
                {
                    for( size_t sample_x = first_x; sample_x <= last_x; ++sample_x )
                    {
                        const std::uint8_t color = color_at( sample_x, sample_y );
                        sum[ color ] += static_cast< std::uint64_t >(
                            source_in[ sample_y ][ sample_x ][ 0U ] );
                        ++count[ color ];
                    }
                }

                for( std::uint8_t color = 0U; color < 3U; ++color )
                {
                    // 3x3近傍には必ず3色すべてが現れるため、除数が0になることはない。
                    const std::uint64_t divisor = count[ color ];
                    target[ y ][ x ][ detail::colorChannel( color, order_in ) ] =
                        static_cast< TargetChannel >(
                            ( sum[ color ] + divisor / 2U ) / divisor );
                }
            }
        }
        return target;
    }


    //!
    //! @brief
    //!     \~japanese Color画像へ3x3の色行列を適用する。
    //!     \~english  Applies a three-by-three colour matrix to a colour image.
    //!
    //! @details
    //!     \~japanese
    //!         行列は出力Channelの並びで与える。`result[i] = sum(matrix[i][j] * source[j])`である。
    //!      @n 結果はChannel型の表現範囲へ飽和させる。負値は0になる。
    //!     \~english
    //!         The matrix is given in the result's channel order, so
    //!         `result[i] = sum(matrix[i][j] * source[j])`.
    //!      @n The result saturates to the channel type's range; a negative value becomes zero.
    //!
    //! @param[in] source_in \~japanese 入力Color画像. \~english Colour source.
    //! @param[in] matrix_in \~japanese 3x3の色行列.   \~english Three-by-three colour matrix.
    //!
    //! @return \~japanese 変換後のColor画像. \~english The converted colour image.
    //!
    template< ePixFormat _Pf >
    Image_< _Pf > applyColorMatrix(
          const Image_< _Pf >&                              source_in
        , const std::array< std::array< double, 3U >, 3U >& matrix_in )
    {
        static_assert( PixelTraits< _Pf >::DATA_NUM == 3U,
            "applyColorMatrix requires a three-channel image" );

        using Channel = PixelType< _Pf >;
        const double maximum = static_cast< double >(
            ( std::numeric_limits< Channel >::max )() );

        Image_< _Pf > target( source_in.width(), source_in.height() );
        for( size_t y = 0U; y < source_in.height(); ++y )
        {
            for( size_t x = 0U; x < source_in.width(); ++x )
            {
                for( size_t row = 0U; row < 3U; ++row )
                {
                    double value = 0.0;
                    for( size_t column = 0U; column < 3U; ++column )
                    {
                        value += matrix_in[ row ][ column ]
                               * static_cast< double >( source_in[ y ][ x ][ column ] );
                    }
                    value = ( value < 0.0 ) ? 0.0 : value;
                    value = ( value > maximum ) ? maximum : value;
                    target[ y ][ x ][ row ] = static_cast< Channel >( value + 0.5 );
                }
            }
        }
        return target;
    }
};

#endif // WONDERSTEWENGINE_DATA_IMAGEDEMOSAIC_H
