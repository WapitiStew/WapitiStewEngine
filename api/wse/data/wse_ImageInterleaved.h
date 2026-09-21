//*****************************************************************************************************************
//!
//! @file    wse_ImageInterleaved.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-06, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 外部が並べたInterleaved Bufferと Image_ を相互に変換するファイル。
//!     \~english  Converts between an externally laid-out interleaved buffer and Image_.
//!
//!
//! @details
//!     \~japanese
//!         Deviceが渡すFrameは行ごとの余白(Row Stride)を持つことがあるが、`Image_`は常に詰まった
//!      @n 配置である。本ファイルはその差を吸収し、Bufferの境界を検証してから詰めて複製する。
//!      @n 検証(`isInterleavedViewValid`)と構築(`makeImageFromInterleaved`)を分けているため、
//!      @n 呼び出し側が先に検証すれば、残る例外は記憶域確保の失敗だけになる。
//!      @n 16bit SampleのByte順はLittle Endianと定める。
//!     \~english
//!         A frame a device hands over may carry a per-row margin, while `Image_` is always packed.
//!      @n This file absorbs that difference: it checks the buffer bounds, then copies row by row.
//!      @n Validation (`isInterleavedViewValid`) is split from construction
//!      @n (`makeImageFromInterleaved`), so once a caller has validated, the only remaining
//!      @n exception is a failed allocation.
//!      @n The byte order of a sixteen-bit sample is defined as little endian.
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

#ifndef WONDERSTEWENGINE_DATA_IMAGEINTERLEAVED_H
#define WONDERSTEWENGINE_DATA_IMAGEINTERLEAVED_H

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
#include <cstring>
#include <limits>
#include <type_traits>


// \~japanese 16bit SampleをそのままCopyできるHostかどうか.
// \~english  Whether this host can copy a sixteen-bit sample verbatim.
#if defined( _WIN32 ) || defined( __LITTLE_ENDIAN__ ) \
 || ( defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
    #define WSE_INTERLEAVED_LITTLE_ENDIAN 1
#else
    #define WSE_INTERLEAVED_LITTLE_ENDIAN 0
#endif


namespace wse
{
    //!
    //! @struct sInterleavedView
    //! @brief
    //!     \~japanese 外部が所有するInterleaved Bufferの読み取りView。
    //!     \~english  Read-only view over an interleaved buffer someone else owns.
    //!
    //! @details
    //!     \~japanese
    //!         `row_stride`が0の場合は余白の無い行として扱う。`accessible_bytes`は`data`から
    //!      @n 安全に読める大きさであり、Bufferの終端を越えないことの検証に使用する。
    //!     \~english
    //!         A zero `row_stride` selects a row with no margin. `accessible_bytes` is how much can
    //!         be read from `data` safely and is used to prove the copy stays inside the buffer.
    //!
    struct sInterleavedView final
    {
        const std::uint8_t* data; //!< First byte of the first row.
        std::size_t         width;      //!< Width [pixel].
        std::size_t         height;      //!< Height [pixel].
        std::size_t         row_stride;      //!< Row size [byte]. Zero selects the packed row.
        std::size_t         accessible_bytes;      //!< Readable size from `data` [byte].

        //! @brief Construct all members with explicit defaults.
        sInterleavedView(
              const std::uint8_t * data_in = nullptr
            , std::size_t width_in = 0U
            , std::size_t height_in = 0U
            , std::size_t row_stride_in = 0U
            , std::size_t accessible_bytes_in = 0U
        )
            : data             ( data_in )
            , width            ( width_in )
            , height           ( height_in )
            , row_stride       ( row_stride_in )
            , accessible_bytes ( accessible_bytes_in )
        {
        }
    };

    //!
    //! @struct sInterleavedTarget
    //! @brief
    //!     \~japanese 外部が所有するInterleaved Bufferの書き込み先。
    //!     \~english  Writable destination in an interleaved buffer someone else owns.
    //!
    struct sInterleavedTarget final
    {
        std::uint8_t* data; //!< First byte of the first row.
        std::size_t   width;      //!< Width [pixel].
        std::size_t   height;      //!< Height [pixel].
        std::size_t   row_stride;      //!< Row size [byte]. Zero selects the packed row.
        std::size_t   accessible_bytes;      //!< Writable size from `data` [byte].

        //! @brief Construct all members with explicit defaults.
        sInterleavedTarget(
              std::uint8_t * data_in = nullptr
            , std::size_t width_in = 0U
            , std::size_t height_in = 0U
            , std::size_t row_stride_in = 0U
            , std::size_t accessible_bytes_in = 0U
        )
            : data             ( data_in )
            , width            ( width_in )
            , height           ( height_in )
            , row_stride       ( row_stride_in )
            , accessible_bytes ( accessible_bytes_in )
        {
        }
    };


    namespace detail
    {
        //! \~japanese 1 Pixelが占めるByte数. \~english Bytes one pixel occupies.
        template< ePixFormat _Pf >
        constexpr std::size_t interleavedPixelBytes() noexcept
        {
            return static_cast< std::size_t >( PixelSize< _Pf > )
                 * sizeof( PixelType< _Pf > );
        }

        //!
        //! \~japanese 幅・高さ・Strideの整合とBuffer終端を検証する.
        //! \~english  Checks the extent, the stride, and that the copy stays inside the buffer.
        //!
        template< ePixFormat _Pf >
        constexpr bool isInterleavedLayoutValid(
              const std::size_t width_in
            , const std::size_t height_in
            , const std::size_t row_stride_in
            , const std::size_t accessible_bytes_in ) noexcept
        {
            constexpr std::size_t pixel_bytes = interleavedPixelBytes< _Pf >();
            constexpr std::size_t maximum     = ( std::numeric_limits< std::size_t >::max )();

            if( width_in == 0U || height_in == 0U )                 { return false; }
            if( width_in > maximum / pixel_bytes )                  { return false; }

            const std::size_t packed_row_bytes = width_in * pixel_bytes;
            const std::size_t stride           = row_stride_in != 0U ? row_stride_in : packed_row_bytes;
            if( stride < packed_row_bytes )                         { return false; }
            if( height_in > maximum / stride )                      { return false; }

            const std::size_t span = ( height_in - 1U ) * stride;
            if( span > maximum - packed_row_bytes )                 { return false; }
            return accessible_bytes_in >= span + packed_row_bytes;
        }

        //! \~japanese 有効なRow Stride. \~english The effective row stride.
        template< ePixFormat _Pf >
        constexpr std::size_t interleavedRowStride(
              const std::size_t width_in
            , const std::size_t row_stride_in ) noexcept
        {
            return row_stride_in != 0U
                 ? row_stride_in
                 : width_in * interleavedPixelBytes< _Pf >();
        }
    }


    //!
    //! @brief
    //!     \~japanese 余白の無い1行が占めるByte数を返す。
    //!     \~english  Bytes one packed row occupies.
    //!
    //! @tparam _Pf Pixel format.
    //! @param [in] width_in Width [pixel].
    //! @return Packed row size [byte]. Zero when the multiplication would overflow.
    //!
    template< ePixFormat _Pf >
    constexpr std::size_t packedRowBytes( const std::size_t width_in ) noexcept
    {
        return width_in != 0U
            && width_in <= ( std::numeric_limits< std::size_t >::max )()
                         / detail::interleavedPixelBytes< _Pf >()
             ? width_in * detail::interleavedPixelBytes< _Pf >()
             : 0U;
    }

    //!
    //! @brief
    //!     \~japanese Viewが指定Formatとして読み取り可能かを検証する。記憶域を確保せず例外も投げない。
    //!     \~english  Whether the view can be read as the given format. Allocates nothing and never throws.
    //!
    //! @tparam _Pf Pixel format.
    //! @param [in] view_in View to check.
    //! @retval true  読み取り可能.
    //! @retval false Pointerが無い、大きさが0、Strideが不足、またはBuffer終端を越える.
    //!
    template< ePixFormat _Pf >
    bool isInterleavedViewValid( const sInterleavedView& view_in ) noexcept
    {
        if( view_in.data == nullptr ) { return false; }
        return detail::isInterleavedLayoutValid< _Pf >(
            view_in.width, view_in.height, view_in.row_stride, view_in.accessible_bytes );
    }

    //!
    //! @brief
    //!     \~japanese 書き込み先が指定Formatとして使用可能かを検証する。例外を投げない。
    //!     \~english  Whether the destination can be written as the given format. Never throws.
    //!
    //! @tparam _Pf Pixel format.
    //! @param [in] target_in Destination to check.
    //! @retval true  書き込み可能.
    //! @retval false Pointerが無い、大きさが0、Strideが不足、またはBuffer終端を越える.
    //!
    template< ePixFormat _Pf >
    bool isInterleavedTargetValid( const sInterleavedTarget& target_in ) noexcept
    {
        if( target_in.data == nullptr ) { return false; }
        return detail::isInterleavedLayoutValid< _Pf >(
            target_in.width, target_in.height, target_in.row_stride, target_in.accessible_bytes );
    }

    //!
    //! @brief
    //!     \~japanese Interleaved Bufferを詰まった `Image_` へ複製する。Row Strideは畳み込まれる。
    //!     \~english  Copies an interleaved buffer into a packed `Image_`, folding the row stride away.
    //!
    //! @tparam _Pf Pixel format.
    //! @param [in] view_in Source view.
    //! @return Packed image.
    //! @throw std::invalid_argument when the view does not satisfy isInterleavedViewValid.
    //! @note
    //!     \~japanese
    //!         先に`isInterleavedViewValid`で検証しておけば、残る例外は記憶域確保の失敗だけになる。
    //!     \~english
    //!         Validate with `isInterleavedViewValid` first and the only remaining exception is a
    //!         failed allocation.
    //!
    template< ePixFormat _Pf >
    Image_< _Pf > makeImageFromInterleaved( const sInterleavedView& view_in )
    {
        using PixelValue = PixelAlias< _Pf >;
        using SampleType = PixelType< _Pf >;
        constexpr std::size_t channels    = static_cast< std::size_t >( PixelSize< _Pf > );
        constexpr std::size_t sample_size = sizeof( SampleType );
        static_assert( sizeof( PixelValue ) == channels * sample_size
                     , "Pixel_ must stay a bare interleaved array for the row copy to hold." );

        if( !isInterleavedViewValid< _Pf >( view_in ) )
        {
            throw std::invalid_argument( "the interleaved view is not valid for the pixel format" );
        }

        const std::size_t stride = detail::interleavedRowStride< _Pf >(
            view_in.width, view_in.row_stride );
        const std::size_t row_bytes = view_in.width * channels * sample_size;

        Image_< _Pf > image( view_in.width, view_in.height );
        for( std::size_t y = 0U; y < view_in.height; ++y )
        {
            const std::uint8_t* const p_source = view_in.data + y * stride;
            PixelValue* const         p_row    = image[ y ];
#if WSE_INTERLEAVED_LITTLE_ENDIAN
            std::memcpy( p_row, p_source, row_bytes );
#else
            if( sample_size == 1U )
            {
                std::memcpy( p_row, p_source, row_bytes );
            }
            else
            {
                for( std::size_t x = 0U; x < view_in.width; ++x )
                {
                    for( std::size_t channel = 0U; channel < channels; ++channel )
                    {
                        const std::uint8_t* const p_sample =
                            p_source + ( x * channels + channel ) * sample_size;
                        p_row[ x ][ channel ] = static_cast< SampleType >(
                            static_cast< SampleType >( p_sample[ 0U ] )
                          | static_cast< SampleType >(
                                static_cast< SampleType >( p_sample[ 1U ] ) << 8U ) );
                    }
                }
            }
#endif
        }
        return image;
    }

    //!
    //! @brief
    //!     \~japanese `Image_` を書き込み先のRow Strideに合わせて書き出す。例外を投げない。
    //!     \~english  Writes an `Image_` out at the destination's row stride. Never throws.
    //!
    //! @tparam _Pf Pixel format.
    //! @param [out] p_target_out  Destination. The descriptor itself is not modified; the buffer
    //!                            it names is what this call writes.
    //! @param [in]  image_in      Source image.
    //! @retval true  書き出した.
    //! @retval false 書き込み先が不正、または大きさが一致しない。書き込み先は変更されない.
    //!
    template< ePixFormat _Pf >
    bool writeImageToInterleaved(
          const sInterleavedTarget* const p_target_out
        , const Image_< _Pf >&           image_in ) noexcept
    {
        const sInterleavedTarget& target_out = *p_target_out;

        using PixelValue = PixelAlias< _Pf >;
        using SampleType = PixelType< _Pf >;
        constexpr std::size_t channels    = static_cast< std::size_t >( PixelSize< _Pf > );
        constexpr std::size_t sample_size = sizeof( SampleType );

        if( !isInterleavedTargetValid< _Pf >( target_out ) )         { return false; }
        if( image_in.width()  != target_out.width )                  { return false; }
        if( image_in.height() != target_out.height )                 { return false; }

        const std::size_t stride = detail::interleavedRowStride< _Pf >(
            target_out.width, target_out.row_stride );
        const std::size_t row_bytes = target_out.width * channels * sample_size;

        for( std::size_t y = 0U; y < target_out.height; ++y )
        {
            std::uint8_t* const     p_destination = target_out.data + y * stride;
            const PixelValue* const p_row         = image_in[ y ];
#if WSE_INTERLEAVED_LITTLE_ENDIAN
            std::memcpy( p_destination, p_row, row_bytes );
#else
            if( sample_size == 1U )
            {
                std::memcpy( p_destination, p_row, row_bytes );
            }
            else
            {
                for( std::size_t x = 0U; x < target_out.width; ++x )
                {
                    for( std::size_t channel = 0U; channel < channels; ++channel )
                    {
                        std::uint8_t* const p_sample =
                            p_destination + ( x * channels + channel ) * sample_size;
                        const SampleType sample = p_row[ x ][ channel ];
                        p_sample[ 0U ] = static_cast< std::uint8_t >( sample & 0xFFU );
                        p_sample[ 1U ] = static_cast< std::uint8_t >( ( sample >> 8U ) & 0xFFU );
                    }
                }
            }
#endif
        }
        return true;
    }

    //!
    //! @brief
    //!     \~japanese Color ChannelのRGB／BGR順序を入れ替える。Alpha Channelはそのまま運ぶ。
    //!     \~english  Swaps the RGB/BGR colour order. The alpha channel is carried through.
    //!
    //! @tparam _dPf Destination pixel format.
    //! @tparam _sPf Source pixel format.
    //! @param [in] source_in Source image.
    //! @return Reordered image.
    //! @throw std::invalid_argument when the source is empty.
    //! @note
    //!     \~japanese
    //!         Channel数とBit深度は一致していなければならない。両者が同じFormatの場合は複製になる。
    //!     \~english
    //!         The channel count and the bit depth must match. Naming one format twice copies.
    //!
    template< ePixFormat _dPf, ePixFormat _sPf >
    Image_< _dPf > convertChannelOrder( const Image_< _sPf >& source_in )
    {
        static_assert( PixelSize< _dPf > == PixelSize< _sPf >
                     , "A channel-order conversion keeps the channel count." );
        static_assert( PixelBitDepth< _dPf > == PixelBitDepth< _sPf >
                     , "A channel-order conversion keeps the bit depth." );
        static_assert( PixelSize< _dPf > == 3U || PixelSize< _dPf > == 4U
                     , "Channel order only has meaning for a three- or four-channel colour image." );

        if( source_in.width() == 0U || source_in.height() == 0U )
        {
            throw std::invalid_argument( "the source image must not be empty" );
        }

        constexpr std::size_t channels = static_cast< std::size_t >( PixelSize< _dPf > );
        Image_< _dPf > result( source_in.width(), source_in.height() );
        for( std::size_t y = 0U; y < source_in.height(); ++y )
        {
            const PixelAlias< _sPf >* const p_source = source_in[ y ];
            PixelAlias< _dPf >* const       p_result = result[ y ];
            for( std::size_t x = 0U; x < source_in.width(); ++x )
            {
                p_result[ x ][ 0U ] = p_source[ x ][ 2U ];
                p_result[ x ][ 1U ] = p_source[ x ][ 1U ];
                p_result[ x ][ 2U ] = p_source[ x ][ 0U ];
                if( channels == 4U )
                {
                    p_result[ x ][ 3U ] = p_source[ x ][ 3U ];
                }
            }
        }
        return result;
    }

};


#endif //WONDERSTEWENGINE_DATA_IMAGEINTERLEAVED_H
