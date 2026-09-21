//*****************************************************************************************************************
//!
//! @file    wse_ImageTransform.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-02, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Image_ に対する向き補正と平均合成を定義するファイル。
//!     \~english  Defines orientation correction and average composition for Image_.
//!
//!
//! @details
//!     \~japanese
//!         本ファイルはデバイスを一切参照しない汎用の画像演算だけを提供する。
//!      @n 向き補正はチャネルの意味を解釈せず、ピクセルを値として移動するだけである。
//!      @n したがってRGBとBGRのように順序が異なる形式でも同じ実装で正しく動作する。
//!      @n 平均合成は チャネル型より広い整数で累積してから丸めるため、加算で桁が溢れない。
//!     \~english
//!         This file provides general image operations only and never refers to a device.
//!      @n Orientation moves whole pixels as values and never interprets a channel, so a format
//!      @n whose channel order differs, such as RGB against BGR, works with the same implementation.
//!      @n Average composition accumulates in an integer wider than the channel type before
//!      @n rounding, so adding samples cannot overflow.
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

#ifndef WONDERSTEWENGINE_DATA_IMAGETRANSFORM_H
#define WONDERSTEWENGINE_DATA_IMAGETRANSFORM_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <utility>
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "wse_Image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <cstdint>
#include <type_traits>
#include <vector>


namespace wse
{
    //!
    //! @brief
    //!     \~japanese 画像の向きを補正する操作。
    //!     \~english  Operation that corrects the orientation of an image.
    //!
    //! @details
    //!     \~japanese
    //!         取り付け向きが回転または反転しているDeviceの出力を、正しい向きへ戻すために使用する。
    //!      @n `Rotate90CW`と`Rotate90CCW`は幅と高さが入れ替わる。
    //!     \~english
    //!         Corrects the output of a device whose mounting is rotated or mirrored.
    //!      @n `Rotate90CW` and `Rotate90CCW` exchange the width and the height.
    //!
    enum class eImageOrientation : std::uint8_t
    {
          None            = 0U  //!< \~japanese 変換しない.       \~english No change.
        , Rotate90CW            //!< \~japanese 時計回りに90度.   \~english 90 degrees clockwise.
        , Rotate180             //!< \~japanese 180度.            \~english 180 degrees.
        , Rotate90CCW           //!< \~japanese 反時計回りに90度. \~english 90 degrees counter-clockwise.
        , FlipHorizontal        //!< \~japanese 左右反転.         \~english Mirrored left to right.
        , FlipVertical          //!< \~japanese 上下反転.         \~english Mirrored top to bottom.
    };


    //!
    //! @brief
    //!     \~japanese 向き補正後の幅と高さを求める。
    //!     \~english  Returns the width and the height after an orientation is applied.
    //!
    //! @param[out] p_width_out     \~japanese 出力の幅.   \~english Resulting width.
    //! @param[out] p_height_out    \~japanese 出力の高さ. \~english Resulting height.
    //! @param[in]  width_in        \~japanese 入力の幅.   \~english Source width.
    //! @param[in]  height_in       \~japanese 入力の高さ. \~english Source height.
    //! @param[in]  orientation_in  \~japanese 適用する操作. \~english Operation to apply.
    //!
    inline void orientedExtent(
          size_t* const           p_width_out
        , size_t* const           p_height_out
        , const size_t            width_in
        , const size_t            height_in
        , const eImageOrientation orientation_in )
    {
        size_t& width_out  = *p_width_out;
        size_t& height_out = *p_height_out;

        const bool transposed = ( orientation_in == eImageOrientation::Rotate90CW )
                             || ( orientation_in == eImageOrientation::Rotate90CCW );
        width_out  = transposed ? height_in : width_in;
        height_out = transposed ? width_in  : height_in;
    }


    //!
    //! @brief
    //!     \~japanese 画像の向きを補正した新しい画像を返す。
    //!     \~english  Returns a new image whose orientation has been corrected.
    //!
    //! @details
    //!     \~japanese
    //!         入力は変更しない。ピクセルを値として複製するだけなので、チャネル順序を問わない。
    //!     \~english
    //!         The source is not modified. Pixels are copied as values, so the channel order does
    //!         not matter.
    //!
    //! @param[in] source_in       \~japanese 入力画像.     \~english Source image.
    //! @param[in] orientation_in  \~japanese 適用する操作. \~english Operation to apply.
    //!
    //! @return \~japanese 補正後の画像. \~english The corrected image.
    //!
    //! @retval 例外 \~japanese 入力が空の場合 std::invalid_argument を投げる。
    //!              \~english  Throws std::invalid_argument when the source is empty.
    //!
    template< ePixFormat _Pf >
    Image_< _Pf > applyOrientation(
          const Image_< _Pf >&    source_in
        , const eImageOrientation orientation_in )
    {
        const size_t source_width  = source_in.width();
        const size_t source_height = source_in.height();
        if( source_width == 0U || source_height == 0U )
        {
            throw std::invalid_argument( "the source image must not be empty" );
        }

        size_t target_width  = 0U;
        size_t target_height = 0U;
        orientedExtent( &target_width, &target_height, source_width, source_height, orientation_in );

        Image_< _Pf > target( target_width, target_height );
        for( size_t y = 0U; y < source_height; ++y )
        {
            const PixelAlias< _Pf >* source_row = source_in[ y ];
            for( size_t x = 0U; x < source_width; ++x )
            {
                size_t target_x = 0U;
                size_t target_y = 0U;
                switch( orientation_in )
                {
                    case eImageOrientation::None :
                        target_x = x;
                        target_y = y;
                        break;
                    case eImageOrientation::Rotate90CW :
                        target_x = source_height - 1U - y;
                        target_y = x;
                        break;
                    case eImageOrientation::Rotate180 :
                        target_x = source_width  - 1U - x;
                        target_y = source_height - 1U - y;
                        break;
                    case eImageOrientation::Rotate90CCW :
                        target_x = y;
                        target_y = source_width - 1U - x;
                        break;
                    case eImageOrientation::FlipHorizontal :
                        target_x = source_width - 1U - x;
                        target_y = y;
                        break;
                    case eImageOrientation::FlipVertical :
                        target_x = x;
                        target_y = source_height - 1U - y;
                        break;
                    default :
                        throw std::invalid_argument( "unknown image orientation" );
                }
                target[ target_y ][ target_x ] = source_row[ x ];
            }
        }
        return target;
    }


    //!
    //! @class ImageAccumulator
    //!
    //! @brief
    //!     \~japanese 同一形状の画像を加算し、平均を取り出す累積器。
    //!     \~english  Accumulates images of one shape and produces their average.
    //!
    //! @details
    //!     \~japanese
    //!         保持するのは累積Bufferだけであり、加算した枚数に依存しない。
    //!      @n したがって64枚のような多数枚平均でも、画像1枚分の4倍のメモリしか使用しない。
    //!      @n 累積型は8bitおよび16bitチャネルではuint32_t、それより広いチャネルではuint64_tである。
    //!      @n 形状が異なる画像を加算しようとした場合は例外を投げ、累積内容は変更しない。
    //!     \~english
    //!         Only an accumulation buffer is held, independently of how many images were added, so
    //!         averaging 64 images costs four times one image rather than sixty-four.
    //!      @n The accumulator is uint32_t for 8-bit and 16-bit channels and uint64_t for wider ones.
    //!      @n Adding an image of a different shape throws and leaves the accumulation unchanged.
    //!
    template< ePixFormat _Pf >
    class ImageAccumulator final
    {
        //! @brief Construct all members with explicit defaults.
public:
        ImageAccumulator()
            : m_sum    ()
            , m_count  ( 0U )
            , m_width  ( 0U )
            , m_height ( 0U )
        {
        }
private:

        public : using ChannelType = PixelType< _Pf >;

        //! \~japanese 累積型. 8bitおよび16bitチャネルでは4Byteに抑える.
        //! \~english  Accumulator type; kept at four bytes for 8-bit and 16-bit channels.
        public : using AccumulatorType = typename std::conditional<
            ( sizeof( ChannelType ) <= 2U ), std::uint32_t, std::uint64_t >::type;

        //! \~japanese 累積した枚数. \~english Number of accumulated images.
        public : size_t count( void ) const noexcept { return this->m_count; }

        //! \~japanese 累積中の幅. \~english Width being accumulated.
        public : size_t width( void ) const noexcept { return this->m_width; }

        //! \~japanese 累積中の高さ. \~english Height being accumulated.
        public : size_t height( void ) const noexcept { return this->m_height; }

        //!
        //! @brief
        //!     \~japanese 画像を1枚加算する。
        //!     \~english  Adds one image.
        //!
        //! @param[in] source_in \~japanese 加算する画像. \~english Image to add.
        //!
        //! @throws std::invalid_argument \~japanese 入力が空、または形状が既存の累積と異なる場合に送出する。
        //!                       \~english  Thrown when the source is empty or its shape differs.
        //!
        public : void add( const Image_< _Pf >& source_in )
        {
            const size_t source_width  = source_in.width();
            const size_t source_height = source_in.height();
            if( source_width == 0U || source_height == 0U )
            {
                throw std::invalid_argument( "the source image must not be empty" );
            }

            if( this->m_count == 0U )
            {
                this->m_width  = source_width;
                this->m_height = source_height;
                this->m_sum.assign( source_width * source_height * CHANNEL_NUM, 0U );
            }
            else if( source_width != this->m_width || source_height != this->m_height )
            {
                throw std::invalid_argument( "every accumulated frame must share one size" );
            }

            size_t index = 0U;
            for( size_t y = 0U; y < source_height; ++y )
            {
                const PixelAlias< _Pf >* source_row = source_in[ y ];
                for( size_t x = 0U; x < source_width; ++x )
                {
                    for( size_t channel = 0U; channel < CHANNEL_NUM; ++channel )
                    {
                        this->m_sum[ index ] += static_cast< AccumulatorType >(
                            source_row[ x ][ channel ] );
                        ++index;
                    }
                }
            }
            ++this->m_count;
        }

        //!
        //! @brief
        //!     \~japanese 累積した画像の平均を返す。丸めは四捨五入である。
        //!     \~english  Returns the average of the accumulated images, rounded to nearest.
        //!
        //! @return \~japanese 平均画像. \~english The averaged image.
        //!
        //! @retval 例外 \~japanese 1枚も加算していない場合 std::logic_error を投げる。
        //!              \~english  Throws std::logic_error when nothing has been added.
        //!
        public : Image_< _Pf > average( void ) const
        {
            if( this->m_count == 0U )
            {
                throw std::logic_error( "no frame has been accumulated" );
            }

            const AccumulatorType divisor = static_cast< AccumulatorType >( this->m_count );
            const AccumulatorType bias    = divisor / 2U;

            Image_< _Pf > target( this->m_width, this->m_height );
            size_t index = 0U;
            for( size_t y = 0U; y < this->m_height; ++y )
            {
                PixelAlias< _Pf >* target_row = target[ y ];
                for( size_t x = 0U; x < this->m_width; ++x )
                {
                    for( size_t channel = 0U; channel < CHANNEL_NUM; ++channel )
                    {
                        target_row[ x ][ channel ] = static_cast< ChannelType >(
                            ( this->m_sum[ index ] + bias ) / divisor );
                        ++index;
                    }
                }
            }
            return target;
        }

        //! \~japanese 累積を破棄する. \~english Discards the accumulation.
        public : void reset( void ) noexcept
        {
            this->m_sum.clear();
            this->m_count  = 0U;
            this->m_width  = 0U;
            this->m_height = 0U;
        }

        private : static constexpr size_t CHANNEL_NUM =
            static_cast< size_t >( PixelTraits< _Pf >::DATA_NUM );

        private : std::vector< AccumulatorType > m_sum;
        private : size_t                       m_count;
        private : size_t                       m_width;
        private : size_t                       m_height;
    };


    //!
    //! @brief
    //!     \~japanese 同一形状の画像列の平均を返す。
    //!     \~english  Returns the average of a sequence of images that share one shape.
    //!
    //! @param[in] sources_in \~japanese 平均する画像列. \~english Images to average.
    //!
    //! @return \~japanese 平均画像. \~english The averaged image.
    //!
    //! @retval 例外 \~japanese 空の列、または形状が揃わない場合 std::invalid_argument を投げる。
    //!              \~english  Throws std::invalid_argument when the sequence is empty or shapes differ.
    //!
    template< ePixFormat _Pf >
    Image_< _Pf > averageImages( const std::vector< Image_< _Pf > >& sources_in )
    {
        ImageAccumulator< _Pf > accumulator;
        for( const Image_< _Pf >& source : sources_in )
        {
            accumulator.add( source );
        }
        return accumulator.average();
    }
};

#endif // WONDERSTEWENGINE_DATA_IMAGETRANSFORM_H
