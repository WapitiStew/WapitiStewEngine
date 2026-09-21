//*****************************************************************************************************************
//! 
//! @file    wse_Image.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Image.h は画像データを扱うクラスと関連トレイトを定義するファイル。
//!     \~english  Image.h defines classes and traits for handling image data.
//!
//! 
//! @details
//!     \~japanese
//!         このファイルでは、ピクセルトレイト PixelTraits、
//!         および Map を継承した Image_ クラスを提供する。Image_ クラスはピクセルフォーマットに依存して
//!         データを管理する。OpenCV連携は`<cv/OpenCvAdapter.h>`が担う。
//!     \~english
//!         This file provides the pixel traits (PixelTraits),
//!         and the Image_ class inheriting from Map. The Image_ class manages data based on pixel format
//!         and the image container; OpenCV interop lives in `<cv/OpenCvAdapter.h>`.
//!
//! 
//! @note
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

#ifndef WONDERSTEWENGINE_DATA_IMAGE_H
#define WONDERSTEWENGINE_DATA_IMAGE_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "../depend/wse_Typedef.h"
#include "../utility/wse_PixelFormat.h"
#if defined(_MSC_VER)
#pragma warning(pop)    
#endif

#include "wse_Map.h"
#include "wse_Pixel.h"

#include <cstring>
#include <limits>



namespace wse
{
    // 基本形（未定義版）
    template< ePixFormat _Pf, typename Enable = void>
    struct PixelTraits;

    //! 
    //! @struct PixelTraits
    //!
    //! @brief
    //!     \~japanese ビット深度 8 ビット以下のピクセルフォーマットに対応するトレイト。
    //!     \~english  Trait for pixel formats with bit depth ≤ 8 bits.
    //!
    //! @details
    //!     \~japanese
    //!         ePixFormat が CH1D8, CH2D8, CH3D8, CH4D8 およびそのChannel順序違いの場合、
    //!         内部型を uint8_t とし、DATA_NUM と BIT_DEPTH を定義する。
    //!     \~english
    //!         For ePixFormat values CH1D8, CH2D8, CH3D8, CH4D8 and their channel-order variants,
    //!         defines internal type as uint8_t and sets DATA_NUM and BIT_DEPTH.
    //!
    template< ePixFormat  _Pf >
    class PixelTraits < _Pf, typename std::enable_if<
        ( _Pf == ePixFormat::CH1D8 ||
          _Pf == ePixFormat::CH2D8 ||
          _Pf == ePixFormat::CH3D8 ||
          _Pf == ePixFormat::CH4D8 ||
          _Pf == ePixFormat::BGR3D8 ||
          _Pf == ePixFormat::BGRA4D8 )
    >::type> {
        public : typedef uint8_t _Tp;                                         //!< \~english Underlying type is uint8_t. \~japanese 内部型は uint8_t。
        public : static constexpr uint8_t DATA_NUM  = getDataNum ( _Pf );     //!< \~english Number of channels. \~japanese チャネル数。
        public : static constexpr uint8_t BIT_DEPTH = getBitDepth( _Pf );     //!< \~english Bit depth per channel. \~japanese チャネルあたりのビット深度。
    };

    //! 
    //! @struct PixelTraits
    //!
    //! @brief
    //!     \~japanese ビット深度 8 以上～16 ビット以下のピクセルフォーマットに対応するトレイト。
    //!     \~english  Trait for pixel formats with bit depth > 8 bits and ≤ 16 bits.
    //!
    //! @details
    //!     \~japanese
    //!         ePixFormat が CH?D10, CH?D12, CH?D14, CH?D16 およびそのChannel順序違いの場合、
    //!         内部型を uint16_t とし、DATA_NUM と BIT_DEPTH を定義する。
    //!     \~english
    //!         For ePixFormat values CH?D10, CH?D12, CH?D14, CH?D16 and their channel-order variants,
    //!         defines internal type as uint16_t and sets DATA_NUM and BIT_DEPTH.
    //!
    template< ePixFormat  _Pf >
    struct PixelTraits < _Pf, typename std::enable_if<
        ( _Pf == ePixFormat::CH1D10 || _Pf == ePixFormat::CH1D12 || _Pf == ePixFormat::CH1D14 || _Pf == ePixFormat::CH1D16 ||
          _Pf == ePixFormat::CH2D10 || _Pf == ePixFormat::CH2D12 || _Pf == ePixFormat::CH2D14 || _Pf == ePixFormat::CH2D16 ||
          _Pf == ePixFormat::CH3D10 || _Pf == ePixFormat::CH3D12 || _Pf == ePixFormat::CH3D14 || _Pf == ePixFormat::CH3D16 ||
          _Pf == ePixFormat::CH4D10 || _Pf == ePixFormat::CH4D12 || _Pf == ePixFormat::CH4D14 || _Pf == ePixFormat::CH4D16 ||
          _Pf == ePixFormat::BGR3D16 || _Pf == ePixFormat::BGRA4D16 )
    >::type> {
        public : typedef uint16_t _Tp;                                        //!< \~english Underlying type is uint16_t. \~japanese 内部型は uint16_t。
        public : static constexpr uint8_t DATA_NUM  = getDataNum ( _Pf );     //!< \~english Number of channels. \~japanese チャネル数。
        public : static constexpr uint8_t BIT_DEPTH = getBitDepth( _Pf );     //!< \~english Bit depth per channel. \~japanese チャネルあたりのビット深度。
    };

    //! 
    //! @struct PixelTraits
    //!
    //! @brief
    //!     \~japanese ビット深度 16 以上～32 ビット以下のピクセルフォーマットに対応するトレイト。
    //!     \~english  Trait for pixel formats with bit depth > 16 bits and ≤ 32 bits.
    //!
    //! @details
    //!     \~japanese
    //!         ePixFormat が CH?D32 のいずれかの場合、
    //!         内部型を uint32_t とし、DATA_NUM と BIT_DEPTH を定義する。
    //!     \~english
    //!         For ePixFormat values CH?D32,
    //!         defines internal type as uint32_t and sets DATA_NUM and BIT_DEPTH.
    //!
    template< ePixFormat  _Pf >
    struct PixelTraits < _Pf, typename std::enable_if<
        ( _Pf == ePixFormat::CH1D32 ||
          _Pf == ePixFormat::CH2D32 ||
          _Pf == ePixFormat::CH3D32 ||
          _Pf == ePixFormat::CH4D32  )
    >::type> {
        public : typedef uint32_t _Tp;                                        //!< \~english Underlying type is uint32_t. \~japanese 内部型は uint32_t。
        public : static constexpr uint8_t DATA_NUM  = getDataNum ( _Pf );     //!< \~english Number of channels. \~japanese チャネル数。
        public : static constexpr uint8_t BIT_DEPTH = getBitDepth( _Pf );     //!< \~english Bit depth per channel. \~japanese チャネルあたりのビット深度。
    };

    //! 
    //! @struct PixelTraits
    //!
    //! @brief
    //!     \~japanese ビット深度 32 以上～64 ビット以下のピクセルフォーマットに対応するトレイト。
    //!     \~english  Trait for pixel formats with bit depth > 32 bits and ≤ 64 bits.
    //!
    //! @details
    //!     \~japanese
    //!         ePixFormat が CH?D64 のいずれかの場合、
    //!         内部型を uint64_t とし、DATA_NUM と BIT_DEPTH を定義する。
    //!     \~english
    //!         For ePixFormat values CH?D64,
    //!         defines internal type as uint64_t and sets DATA_NUM and BIT_DEPTH.
    //!
    template< ePixFormat  _Pf >
    struct PixelTraits < _Pf, typename std::enable_if<
        ( _Pf == ePixFormat::CH1D64 ||  
          _Pf == ePixFormat::CH2D64 || 
          _Pf == ePixFormat::CH3D64 || 
          _Pf == ePixFormat::CH4D64  )
    >::type> {
        public : typedef uint64_t _Tp;                                        //!< \~english Underlying type is uint64_t. \~japanese 内部型は uint64_t。
        public : static constexpr uint8_t DATA_NUM  = getDataNum ( _Pf );     //!< \~english Number of channels. \~japanese チャネル数。
        public : static constexpr uint8_t BIT_DEPTH = getBitDepth( _Pf );     //!< \~english Bit depth per channel. \~japanese チャネルあたりのビット深度。
    };
    template< ePixFormat _Pf >
    using PixelAlias = Pixel_< typename PixelTraits<_Pf>::_Tp
                             , PixelTraits<_Pf>::DATA_NUM
                             , PixelTraits<_Pf>::BIT_DEPTH >;
    //! 
    //! @brief
    //!     \~japanese 指定フォーマットのピクセルの内部型を取得するエイリアス。
    //!     \~english  Alias to get the internal type for a given pixel format.
    //!
    template< ePixFormat Pf > using PixelType       = typename PixelTraits< Pf >::_Tp;

    //! 
    //! @brief
    //!     \~japanese 指定フォーマットのピクセルあたりデータ数を取得する定数。
    //!     \~english  Constant to get number of data elements per pixel for a given format.
    //!
    template< ePixFormat Pf > constexpr std::uint8_t PixelSize       = PixelTraits< Pf >::DATA_NUM;

    //! 
    //! @brief
    //!     \~japanese 指定フォーマットのピクセルあたりビット深度を取得する定数。
    //!     \~english  Constant to get bit depth per pixel for a given format.
    //!
    template< ePixFormat Pf > constexpr std::uint8_t PixelBitDepth   = PixelTraits< Pf >::BIT_DEPTH;


    //!
    //! @class Image_
    //!
    //! @brief
    //!     \~japanese ePixFormat に基づく画像データクラス。
    //!     \~english  Image data class based on ePixFormat.
    //!
    //! @details
    //!     \~japanese
    //!         PixelAlias<_Pf> を要素型とする Map を継承し、幅・高さを管理する。
    //!         データバイト数、ピクセル数、ビット深度、ピクセルフォーマット取得、画像メモリサイズ計算機能を提供する。
    //!         OpenCV連携は`<cv/OpenCvAdapter.h>`が担う。
    //!
    //!     \~english
    //!         Inherits from Map with PixelAlias<_Pf> as element type, managing width and height.
    //!         Provides data byte size, pixel count, bit depth, pixel format retrieval, and image memory size calculation.
    //!         OpenCV interop lives in `<cv/OpenCvAdapter.h>`.
    //!
    //! @note
            //!
    template< ePixFormat _Pf >
    class WSE_API Image_  : public Map < PixelAlias<_Pf> >
    {
        public : static ePixFormat pixel_format( void ){ return _Pf; } //!< \~english Get pixel format. \~japanese ピクセルフォーマットを取得。


        //-------------------------------------------------------------------------
        // using. Constractor / Destractor./Operator. / Accesor / Getter / Setter 
        //-------------------------------------------------------------------------
        // Aylias.
        using Base = Map< PixelAlias<_Pf> >;
        public : using Base::Base;
        //! \~english Public unchecked row access. \~japanese 公開された検査なしの行アクセス。
        public: using Base::operator[];

     //! 
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。
        //!     \~english  Default constructor.
        //!
        public : Image_()
            : Map< PixelAlias<_Pf> > () {}

        //! 
        //! @brief
        //!     \~japanese デストラクタ。
        //!     \~english  Destructor.
        //!
        public : ~Image_( void ) 
        {
            this->m_data = std::vector< PixelAlias<_Pf> >();
        }

        //! 
        //! @brief
        //!     \~japanese コピーコンストラクタ。
        //!     \~english  Copy constructor.
        //!
        public: Image_( const Image_ & obj_in )
            : Base ( obj_in ) {};

        //! 
        //! @brief
        //!     \~japanese ムーブコンストラクタ。
        //!     \~english  Move constructor.
        //!
        public: Image_( Image_&& obj_inout ) noexcept
            : Base ( std::move( obj_inout ) ) {};

        //! 
        //! @brief
        //!     \~japanese コピー代入演算子。
        //!     \~english  Copy assignment operator.
        //!
        public: Image_& operator = ( const Image_& obj )
        {
            if (this != &obj)
            {
                Base::operator=(obj);
            }
            return *this;
        }

        //! 
        //! @brief
        //!     \~japanese ムーブ代入演算子。
        //!     \~english  Move assignment operator.
        //!
        public: Image_& operator = ( Image_&& obj ) noexcept
        {
            if (this != &obj)
            {
                Base::operator=( std::move( obj ) );
            }
            return *this;
        }

        //-------------------------------------------------------------------------
        // Getter
        //-------------------------------------------------------------------------
        //!
        //! @brief
        //!     \~japanese 1データ当たりのバイト数を取得する。
        //!     \~english  Returns the number of bytes per data unit.
        //!
        //! @return Number of bytes per data unit.
        //!
        public: size_t data_byte( void ) const { return PixelAlias<_Pf>::type_data_byte(); }

        //!
        //! @brief
        //!     \~japanese ピクセル内のデータ数を取得する。
        //!     \~english  Returns the number of data elements per pixel.
        //!
        //! @return Number of elements in a pixel.
        //!
        public: size_t pixel_size( void ) const { return PixelAlias<_Pf>::type_pixel_size(); }

        //!
        //! @brief
        //!     \~japanese ピクセルあたりのバイト数を取得する。
        //!     \~english  Returns the number of bytes per pixel.
        //!
        //! @return Number of bytes per pixel.
        //!
        public: size_t pixel_byte( void ) const { return PixelAlias<_Pf>::type_pixel_byte(); }

        //!
        //! @brief
        //!     \~japanese ピクセルのビット深度を取得する。
        //!     \~english  Returns the bit depth of a pixel.
        //!
        //! @return Bit depth of the pixel.
        //!
        public: size_t bit_depth( void ) const { return PixelAlias<_Pf>::type_bit_depth(); }

        //!
        //! @brief
        //!     \~japanese ピクセルフォーマットを取得する。
        //!     \~english  Returns the pixel format.
        //!
        //! @return Pixel format.
        //!
        public: ePixFormat pix_format( void ) const { return _Pf; }

        //!
        //! @brief
        //!     \~japanese 画像データのメモリーサイズを取得する。
        //!     \~english  Returns the memory size used for image data.
        //!
        //! @return Memory size for image data.
        //!
        //! @note
        //!     \~japanese 他のメンバーのメモリーについては考慮しない。
        //!     \~english  Does not include memory used by other members.
        //!
        public: size_t image_memory_size( void ) const
        {
            return this->pixel_byte() * this->m_data.size();
        }




        //!
        //! @brief
        //!     \~japanese 画像内容を文字列として整形出力する。
        //!     \~english  Formats and outputs the image contents as a string.
        //!
        //! @return 整形された画像内容文字列 / Formatted string of image contents.
        //!
        std::string str( void )const
        {
            std::string os = END_LINE();
            for( size_t y = 0; y < this->m_height; ++y )
            {
                os += "| ";
                for( size_t x = 0; x < this->m_width; ++x )
                {
                    const size_t index = static_cast< size_t >( ( y * this->m_width ) + x );
                    os += this->m_data[ index ].str();
                }
                os += "|" + END_LINE();
            }
            os += END_LINE();
            return os;
        }

    };

    //! 
    //! @brief
    //!     \~japanese Image_ から std::ostream への出力演算子。
    //!     \~english  Output operator for Image_ to std::ostream.
    //!
    //! @param[out] os   出力ストリーム / Output stream
    //! @param[in]  obj  Image_ オブジェクト / Image_ object
    //! @return 出力ストリーム / Output stream
    //!
    template< ePixFormat _Pf >
    std::ostream& operator << ( std::ostream& os, const Image_< _Pf >& obj )
    {
        os << obj.str();
        return os;
    }

    typedef Image_< ePixFormat::CH1D8   > mono_t;
    typedef Image_< ePixFormat::CH1D8   > img1_t;
    typedef Image_< ePixFormat::CH1D8   > img1c8_t;
    typedef Image_< ePixFormat::CH1D8   > img1c08_t;
    typedef Image_< ePixFormat::CH1D10  > img1c10_t;
    typedef Image_< ePixFormat::CH1D12  > img1c12_t;
    typedef Image_< ePixFormat::CH1D14  > img1c14_t;
    typedef Image_< ePixFormat::CH1D16  > img1c16_t;
    typedef Image_< ePixFormat::CH1D32  > img1c32_t;
    typedef Image_< ePixFormat::CH1D64  > img1c64_t;
    
    typedef Image_< ePixFormat::CH2D8   > img2_t;
    typedef Image_< ePixFormat::CH2D8   > img2c8_t;
    typedef Image_< ePixFormat::CH2D8   > img2c08_t;
    typedef Image_< ePixFormat::CH2D10  > img2c10_t;
    typedef Image_< ePixFormat::CH2D12  > img2c12_t;
    typedef Image_< ePixFormat::CH2D14  > img2c14_t;
    typedef Image_< ePixFormat::CH2D16  > img2c16_t;
    typedef Image_< ePixFormat::CH2D32  > img2c32_t ;
    typedef Image_< ePixFormat::CH2D64  > img2c64_t ;


    typedef Image_< ePixFormat::CH3D8   > img_t      ;
    typedef Image_< ePixFormat::CH3D8   > img3_t     ;
    typedef Image_< ePixFormat::CH3D8   > img3c8_t  ;
    typedef Image_< ePixFormat::CH3D8   > img3c08_t ;
    typedef Image_< ePixFormat::CH3D10  > img3c10_t ;
    typedef Image_< ePixFormat::CH3D12  > img3c12_t ;
    typedef Image_< ePixFormat::CH3D14  > img3c14_t ;
    typedef Image_< ePixFormat::CH3D16  > img3c16_t ;
    typedef Image_< ePixFormat::CH3D32  > img3c32_t ;
    typedef Image_< ePixFormat::CH3D64  > img3c64_t ;

    typedef Image_< ePixFormat::CH4D8   > img4_t     ;
    typedef Image_< ePixFormat::CH4D8   > img4c8_t  ;
    typedef Image_< ePixFormat::CH4D8   > img4c08_t ;
    typedef Image_< ePixFormat::CH4D10  > img4c10_t ;
    typedef Image_< ePixFormat::CH4D12  > img4c12_t ;
    typedef Image_< ePixFormat::CH4D14  > img4c14_t ;
    typedef Image_< ePixFormat::CH4D16  > img4c16_t ;
    typedef Image_< ePixFormat::CH4D32  > img4c32_t ;
    typedef Image_< ePixFormat::CH4D64  > img4c64_t ;


    // \~japanese Channel 0がBである画像。Pixelの記憶レイアウトは対応するRGB版と同一だが、
    //            型が異なるためAPI境界でChannel順序を取り違えない。
    // \~english  Images whose channel zero is blue. The pixel storage matches the RGB counterpart,
    //            but the distinct type keeps a channel order from being mistaken at an API boundary.
    typedef Image_< ePixFormat::BGR3D8   > img3bgr_t      ;
    typedef Image_< ePixFormat::BGR3D8   > img3c08_bgr_t  ;
    typedef Image_< ePixFormat::BGR3D16  > img3c16_bgr_t  ;
    typedef Image_< ePixFormat::BGRA4D8  > img4bgra_t     ;
    typedef Image_< ePixFormat::BGRA4D8  > img4c08_bgra_t ;
    typedef Image_< ePixFormat::BGRA4D16 > img4c16_bgra_t ;


};


#endif //WONDERSTEWENGINE_DATA_COLOR_H
