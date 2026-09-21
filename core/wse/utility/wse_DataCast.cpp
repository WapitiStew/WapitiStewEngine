//*****************************************************************************************************************
//! 
//! @file    wse_DataCast.cpp
//! @brief   \~japanese Pixel format間のImage変換（Channel数・Bit深さ）の実装と全組み合わせの明示的実体化.
//! @brief   \~english  Implements image conversion between pixel formats (channel count, bit depth) and
//!                     explicitly instantiates every combination.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @details  
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
#include <stdexcept>
#include "../../../api/wse/utility/wse_DataCast.h"
#include "../../../api/wse/utility/wse_Log.h"


/*!
@class  Timer
@author  WapitiStew

タイマークラス.
*/

namespace  wse
{
    static uint64_t average3( const uint64_t value0_in, const uint64_t value1_in, const uint64_t value2_in )
    {
        return ( value0_in / 3 ) + ( value1_in / 3 ) + ( value2_in / 3 )
             + ( ( value0_in % 3 ) + ( value1_in % 3 ) + ( value2_in % 3 ) ) / 3;
    }
    
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4293)  // '>>': シフト数が負の値であるか、大きすぎます。定義されていない動作です
#endif
    // 同じ意味・同じ値域の間の変換。要素をそのまま写すだけで、値のScalingは行わない.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > copyImage( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        for( size_t y = 0; y< dst.height(); ++y )
        {
            for( size_t x = 0; x < dst.width(); ++x )
            {
                for( size_t c = 0; c < dst.pixel_size(); ++c )
                {
                    dst[ y ][ x ][ c ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ c ] );
                }
            }
        }
        return dst;
    }


    // Bit深さを上げる変換。最大値同士（2^n - 1）の比で掛けるので、白は白のまま写る。
    // 単なるShiftだと旧最大値が新最大値に届かず、画面がわずかに暗くなる.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > expandImage( const Image_< _sPf >&src_in )
    {
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );

        Image_< _dPf > dst( src_in.width(), src_in.height() );
        for( size_t y = 0; y< dst.height(); ++y )
        {
            for( size_t x = 0; x < dst.width(); ++x )
            {
                for( size_t c = 0; c < dst.pixel_size(); ++c )
                {
                    dst[ y ][ x ][ c ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ c ] ) );
                }
            }
        }
        return dst;
    }
        
    // Bit深さを下げる変換。右Shiftで上位Bitだけを残す。拡大と違い比の掛け算をしないのは、
    // 切り捨ての単調性が保たれ、値域を超える中間値が生まれないため.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > reduceImage( const Image_< _sPf >&src_in )
    {
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );

        Image_< _dPf > dst( src_in.width(), src_in.height() );
        for( size_t y = 0; y< dst.height(); ++y )
        {
            for( size_t x = 0; x < dst.width(); ++x )
            {
                for( size_t c = 0; c < dst.pixel_size(); ++c )
                {
                    dst[ y ][ x ][ c ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ c ] >> offset );
                }
            }
        }
        return dst;
    }

    // 1Channel→3Channel。輝度を三色へ複製する（Grayscaleの見た目を保つ）。Bit深さの差は
    // 複製と同時に拡大／縮小で吸収する.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > mono2color( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        if constexpr( src_bit_depth == dst_bit_depth )
        {
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                }
            }
        }
        return dst;
    }

    // 1Channel→4Channel。三色へ複製し、Alphaは常に不透明（最大値）で埋める.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > mono2acolor( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        PixelType< _dPf > alpha = static_cast< PixelType< _dPf > >( std::pow( 2.0, ( 8 * sizeof( PixelType< _dPf > ) ) ) - 1.0 );
        if constexpr( src_bit_depth == dst_bit_depth )
        {

            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 3 ] = alpha;//0;
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 3 ] = alpha;//0;//alpha;
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 3 ] = alpha;//0;//alpha;
                }
            }
        }
        return dst;
    }

        
    // 3Channel→1Channel。三色の単純平均で輝度化する。知覚重み（BT.601等）を使わないのは、
    // この変換がSensor dataの整形用であって表示用の色空間変換ではないため.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > color2mono( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        if constexpr( src_bit_depth == dst_bit_depth )
        {

            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _dPf > value = static_cast< PixelType< _dPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = value;
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _sPf > value = static_cast< PixelType< _sPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( value >> offset );
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _sPf > value = static_cast< PixelType< _sPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( value ) );
                }
            }
        }
        return dst;
    }
        
    // 3Channel→4Channel。色をそのまま写し、Alphaは常に不透明で埋める.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > color2acolor( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;
            
        PixelType< _dPf > alpha = static_cast< PixelType< _dPf > >( std::pow( 2.0, ( 8 * sizeof( PixelType< _dPf > ) ) ) - 1.0 );
        if constexpr( src_bit_depth == dst_bit_depth )
        {

            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 1 ] );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 2 ] );
                    dst[ y ][ x ][ 3 ] = alpha;//0;//alpha;
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 1 ] >> offset );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 2 ] >> offset );
                    dst[ y ][ x ][ 3 ] = alpha;//0;//alpha;
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 1 ] ) );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 2 ] ) );
                    dst[ y ][ x ][ 3 ] = alpha;//0;//alpha;
                }
            }
        }
        return dst;
    }
        
    // 4Channel→1Channel。Alphaは捨て、三色の単純平均で輝度化する.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > acolor2mono( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        if constexpr( src_bit_depth == dst_bit_depth )
        {

            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _dPf > value = static_cast< PixelType< _dPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = value;
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _sPf > value = static_cast< PixelType< _sPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( value >> offset );
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _sPf > value = static_cast< PixelType< _sPf > >( 
                                                        ( static_cast< uint64_t >( src_in[ y ][ x ][ 0 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 1 ] )
                                                        + static_cast< uint64_t >( src_in[ y ][ x ][ 2 ] ) )
                                                        / 3
                                            );
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( value ) );
                }
            }
        }
        return dst;
    }
        
    // 4Channel→3Channel。Alphaを捨てて色だけを写す。合成はせず、透過情報は失われる.
    template<  ePixFormat _dPf, ePixFormat _sPf >
    static Image_< _dPf > acolor2color( const Image_< _sPf >&src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;
            
        if constexpr( src_bit_depth == dst_bit_depth )
        {

            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 1 ] );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 2 ] );
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 1 ] >> offset );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 2 ] >> offset );
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    dst[ y ][ x ][ 0 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    dst[ y ][ x ][ 1 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 1 ] ) );
                    dst[ y ][ x ][ 2 ] = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 2 ] ) );
                }
            }
        }
        return dst;
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127)  // '>>': シフト数が負の値であるか、大きすぎます。定義されていない動作です
#endif
    // 公開入口。Channel数の組でどの補助関数へ振るかをCompile時に決め、同Channel同深さは
    // 純粋Copyになる。以降の数百行はこのTemplateを全Format組み合わせで実体化し、DLLの
    // 輸出面を固定するための一覧である.
    template< ePixFormat _dPf, ePixFormat _sPf > Image_< _dPf > castData::image( const Image_< _sPf >& src_in )
    {
        constexpr size_t src_pixel_size = PixelSize< _sPf >;
        constexpr size_t dst_pixel_size = PixelSize< _dPf >;
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        // Mono
        if constexpr( src_pixel_size == 1 )
        {
                 if constexpr( dst_pixel_size == 3 ){ return mono2color < _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 4 ){ return mono2acolor< _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 1 )
            {
                     if constexpr( dst_bit_depth < src_bit_depth ){ return reduceImage< _dPf, _sPf >(src_in); }
                else if constexpr( dst_bit_depth > src_bit_depth ){ return expandImage< _dPf, _sPf >(src_in); }
                else
                {
                    return copyImage< _dPf, _sPf >( src_in );
                }
            
            }
            else // if( dst_pixel_size == 2 )
            {
                throw std::invalid_argument( "the pixel-size combination is not supported by this cast" );
            }
        }
            
        // Color
        else if constexpr( src_pixel_size == 3 )
        {
                 if constexpr( dst_pixel_size == 1 ){ return color2mono  < _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 4 ){ return color2acolor< _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 3 )
            {
                     if constexpr( dst_bit_depth < src_bit_depth ){ return reduceImage< _dPf, _sPf >(src_in); }
                else if constexpr( dst_bit_depth > src_bit_depth ){ return expandImage< _dPf, _sPf >(src_in); }
                else
                {
                    return copyImage< _dPf, _sPf >( src_in );
                }
            
            }
            else // if( dst_pixel_size == 2 )
            {
                throw std::invalid_argument( "the pixel-size combination is not supported by this cast" );
            }
        }
            
        // Alphaed Color
        else if constexpr( src_pixel_size == 4 )
        {
                 if constexpr( dst_pixel_size == 1 ){ return acolor2mono < _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 3 ){ return acolor2color< _dPf, _sPf >( src_in ); }
            else if constexpr( dst_pixel_size == 4 )
            {
                     if constexpr( dst_bit_depth < src_bit_depth ){ return reduceImage< _dPf, _sPf >(src_in); }
                else if constexpr( dst_bit_depth > src_bit_depth ){ return expandImage< _dPf, _sPf >(src_in); }
                else
                {
                    return copyImage< _dPf, _sPf >( src_in );
                }
            
            }
            else // if( dst_pixel_size == 2 )
            {
                throw std::invalid_argument( "the pixel-size combination is not supported by this cast" );
            }
        }
        else // if( src_pixel_size == 2 )
        {
            throw std::invalid_argument( "the pixel-size combination is not supported by this cast" );
        }
    }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c08_t castData::image < ePixFormat::CH3D08, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c10_t castData::image < ePixFormat::CH3D10, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c12_t castData::image < ePixFormat::CH3D12, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c14_t castData::image < ePixFormat::CH3D14, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c16_t castData::image < ePixFormat::CH3D16, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c32_t castData::image < ePixFormat::CH3D32, ePixFormat::CH4D64 >( const img4c64_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D08 >( const img1c08_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D10 >( const img1c10_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D12 >( const img1c12_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D14 >( const img1c14_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D16 >( const img1c16_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D32 >( const img1c32_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH1D64 >( const img1c64_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D08 >( const img3c08_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D10 >( const img3c10_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D12 >( const img3c12_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D14 >( const img3c14_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D16 >( const img3c16_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D32 >( const img3c32_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH3D64 >( const img3c64_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D08 >( const img4c08_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D10 >( const img4c10_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D12 >( const img4c12_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D14 >( const img4c14_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D16 >( const img4c16_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D32 >( const img4c32_t& );
    template WSE_API img3c64_t castData::image < ePixFormat::CH3D64, ePixFormat::CH4D64 >( const img4c64_t& );

    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c08_t castData::image < ePixFormat::CH1D08, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c10_t castData::image < ePixFormat::CH1D10, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c12_t castData::image < ePixFormat::CH1D12, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c14_t castData::image < ePixFormat::CH1D14, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c16_t castData::image < ePixFormat::CH1D16, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c32_t castData::image < ePixFormat::CH1D32, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img1c64_t castData::image < ePixFormat::CH1D64, ePixFormat::CH4D64 > ( const img4c64_t& );

    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c08_t castData::image < ePixFormat::CH4D08, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c10_t castData::image < ePixFormat::CH4D10, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c12_t castData::image < ePixFormat::CH4D12, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c14_t castData::image < ePixFormat::CH4D14, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c16_t castData::image < ePixFormat::CH4D16, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c32_t castData::image < ePixFormat::CH4D32, ePixFormat::CH4D64 > ( const img4c64_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D08 > ( const img1c08_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D10 > ( const img1c10_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D12 > ( const img1c12_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D14 > ( const img1c14_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D16 > ( const img1c16_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D32 > ( const img1c32_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH1D64 > ( const img1c64_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D08 > ( const img3c08_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D10 > ( const img3c10_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D12 > ( const img3c12_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D14 > ( const img3c14_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D16 > ( const img3c16_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D32 > ( const img3c32_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH3D64 > ( const img3c64_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D08 > ( const img4c08_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D10 > ( const img4c10_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D12 > ( const img4c12_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D14 > ( const img4c14_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D16 > ( const img4c16_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D32 > ( const img4c32_t& );
    template WSE_API img4c64_t castData::image < ePixFormat::CH4D64, ePixFormat::CH4D64 > ( const img4c64_t& );



#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4293)  // '>>': シフト数が負の値であるか、大きすぎます。定義されていない動作です
#pragma warning(disable: 4333)  // '>>': シフト数が負の値であるか、大きすぎます。定義されていない動作です
#endif
    template< ePixFormat _dPf, ePixFormat _sPf >
    static  Image_< _dPf > convert4chAlphamap_mono( const Image_< _sPf >& src_in )
    {

        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        if constexpr( src_bit_depth == dst_bit_depth )
        {
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    PixelType< _dPf > value = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    PixelType< _dPf > value = static_cast< PixelType< _dPf > >( src_in[ y ][ x ][ 0 ] >> offset );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    PixelType< _dPf > value = static_cast< PixelType< _dPf > >( expand * static_cast< double >( src_in[ y ][ x ][ 0 ] ) );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }

        return dst;

    }

    template< ePixFormat _dPf, ePixFormat _sPf >
    static  Image_< _dPf > convert4chAlphamap_color( const Image_< _sPf >& src_in )
    {
        Image_< _dPf > dst( src_in.width(), src_in.height() );
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        if constexpr( src_bit_depth == dst_bit_depth )
        {
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const PixelType< _dPf > value = static_cast< PixelType< _dPf > >( average3(
                          static_cast<uint64_t>( src_in[ y ][ x ][ 0 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 1 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 2 ] )
                    ) );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }
            
        else if constexpr( src_bit_depth > dst_bit_depth )
        {
            const PixelType< _sPf > offset = static_cast< PixelType< _sPf > >( src_bit_depth - dst_bit_depth );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const uint64_t average = average3(
                          static_cast<uint64_t>( src_in[ y ][ x ][ 0 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 1 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 2 ] )
                    );
                    const PixelType< _dPf > value = static_cast< PixelType< _dPf > >( average >> offset );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }

        else //if( src_bit_depth < dst_bit_depth )
        {
            const double expand = ( ( std::pow( 2.0, static_cast< double >( dst_bit_depth ) ) - 1.0 )
                                  / ( std::pow( 2.0, static_cast< double >( src_bit_depth ) ) - 1.0 ) );
            for( size_t y = 0; y< dst.height(); ++y )
            {
                for( size_t x = 0; x < dst.width(); ++x )
                {
                    const uint64_t average = average3(
                          static_cast<uint64_t>( src_in[ y ][ x ][ 0 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 1 ] )
                        , static_cast<uint64_t>( src_in[ y ][ x ][ 2 ] )
                    );
                    const PixelType< _dPf > value = static_cast< PixelType< _dPf > >( expand * static_cast< double >( average ) );
                    for( size_t c = 0; c < dst.pixel_size(); ++c )
                    {
                        dst[ y ][ x ][ c ] = value;
                    }
                }
            }
        }
        return dst;
    }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
 
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127)  // '>>': シフト数が負の値であるか、大きすぎます。定義されていない動作です
#endif
    template< ePixFormat _dPf, ePixFormat _sPf > Image_< _dPf > castData::to4chAlphamap( const Image_< _sPf >& src_in )
    {
        // 想定しているフォーマット以外ではコンパイルエラーにする.
        // OpenCVは基本BGRで画像を扱うので、画像はBGR方式で扱う物とする.
        static_assert( ( _dPf == ePixFormat::CH4D08 ) || 
                       ( _dPf == ePixFormat::CH4D10 ) || 
                       ( _dPf == ePixFormat::CH4D12 ) || 
                       ( _dPf == ePixFormat::CH4D14 ) || 
                       ( _dPf == ePixFormat::CH4D16 ) || 
                       ( _dPf == ePixFormat::CH4D32 ) || 
                       ( _dPf == ePixFormat::CH4D64 ) || 

                       ( _sPf == ePixFormat::CH1D08 ) || 
                       ( _sPf == ePixFormat::CH1D10 ) || 
                       ( _sPf == ePixFormat::CH1D12 ) || 
                       ( _sPf == ePixFormat::CH1D14 ) || 
                       ( _sPf == ePixFormat::CH1D16 ) || 
                       ( _sPf == ePixFormat::CH3D08 ) || 
                       ( _sPf == ePixFormat::CH3D10 ) || 
                       ( _sPf == ePixFormat::CH3D12 ) || 
                       ( _sPf == ePixFormat::CH3D14 ) || 
                       ( _sPf == ePixFormat::CH3D16 ) || 
                       ( _sPf == ePixFormat::CH4D08 ) || 
                       ( _sPf == ePixFormat::CH4D10 ) ||
                       ( _sPf == ePixFormat::CH4D12 ) ||
                       ( _sPf == ePixFormat::CH4D14 ) ||
                       ( _sPf == ePixFormat::CH4D16 ) 
                       , "Unexpected enumerator value" );

        constexpr size_t src_pixel_size = PixelSize< _sPf >;
        constexpr size_t src_bit_depth  = PixelBitDepth< _sPf >;
        constexpr size_t dst_bit_depth  = PixelBitDepth< _dPf >;

        // Mono
        if constexpr( src_pixel_size == 1 )
        {
            return convert4chAlphamap_mono< _dPf, _sPf >( src_in );
        }

        // Color
        else if constexpr( src_pixel_size == 3 )
        {
            return convert4chAlphamap_color< _dPf, _sPf >( src_in );
        }

        // Alphaed Color
        else if constexpr( src_pixel_size == 4 )
        {
                 if constexpr( dst_bit_depth < src_bit_depth ){ return reduceImage< _dPf, _sPf >(src_in); }
            else if constexpr( dst_bit_depth > src_bit_depth ){ return expandImage< _dPf, _sPf >(src_in); }
            else
            {
                return copyImage< _dPf, _sPf >( src_in );
            }
        }
        else // if( src_pixel_size == 2 )
        {
            throw std::invalid_argument( "the pixel-size combination is not supported by this cast" );
        }
    }
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c64_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c32_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c16_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c14_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c12_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c10_t castData::to4chAlphamap ( const img4c64_t& );

    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c08_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c10_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c12_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c14_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c16_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c32_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img1c64_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c08_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c10_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c12_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c14_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c16_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c32_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img3c64_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c08_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c10_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c12_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c14_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c16_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c32_t& );
    template WSE_API img4c08_t castData::to4chAlphamap ( const img4c64_t& );



#if defined(_MSC_VER)
#pragma warning(pop)
#endif







};
