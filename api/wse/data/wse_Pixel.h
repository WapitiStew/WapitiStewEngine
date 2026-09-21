//*****************************************************************************************************************
//! 
//! @file    wse_Pixel.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Color.h は Pixel_ テンプレートと基本的なカラー型を定義するファイル。  
//!     \~english  Color.h defines the Pixel_ template and basic color types.
//!
//!
//! @details
//!     \~japanese
//!         このファイルでは、Pixel_ テンプレートを用いて画像のピクセルデータを表現する。
//!      @n データバイト数、ピクセルサイズ、ビット深度の取得、要素アクセス、型変換などの機能を提供する。
//!      @n 標準出力用のストリーム演算子も実装する。OpenCV連携は`<cv/OpenCvAdapter.h>`が担う。
//!      @n さらに、color8_t などの基本的なカラーフォーマットを typedef で定義している。
//!     \~english
//!         This file represents image pixel data using the Pixel_ template.
//!      @n Provides functions to retrieve data byte size, pixel size, bit depth, element access, and type conversions.
//!      @n Implements stream operators for standard output; OpenCV interop lives in `<cv/OpenCvAdapter.h>`.
//!      @n Additionally, defines basic color formats such as color8_t via typedefs.
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

#ifndef WONDERSTEWENGINE_DATA_COLOR_H
#define WONDERSTEWENGINE_DATA_COLOR_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../utility/wse_StringTool.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif




namespace wse
{
    //! 
    //! @class Pixel_
    //! 
    //! @brief
    //!     \~japanese NUM 要素を持つピクセルデータを管理するテンプレート構造体。
    //!     \~english  Template struct for managing pixel data with NUM elements.
    //!
    //! @details
    //!     \~japanese
    //!         このテンプレートクラスは、_Tp 型の値を NUM 個保持するピクセルデータを管理する。
    //!         データバイト数、ピクセルサイズ、ビット深度取得や、要素アクセス、型変換などの機能を提供する。
    //!     \~english
    //!         This template struct manages pixel data consisting of NUM values of type _Tp.
    //!         Provides functions to retrieve data byte size, pixel size, bit depth, element access, and type conversions.
    //!
    //! @note
    //!     \~japanese ビット深度チェックや範囲チェックを含むセーフティチェック機能を提供する。
    //!     \~english  Includes safety checks for bit depth and range when setting values.
    //!
    template< typename _Tp, uint8_t NUM, uint8_t DEPTH
            , typename = std::enable_if_t<std::is_integral< _Tp >::value  && ( NUM > 0 ) && ( DEPTH > 0 ) > >
    struct WSE_API Pixel_
    {
        typedef _Tp     value_type;
        //---------------------------------------
        // メンバー.
        //---------------------------------------
        public: _Tp data[ NUM ];    //!< \~english Pixel data array. \~japanese ピクセルデータ配列。



        //---------------------------------------
        // ゲッター.
        //---------------------------------------

        //! 
        //! @brief
        //!     \~japanese 1データ当たりのバイト数を取得する。
        //!     \~english  Get the byte size per data element.
        //!
        //! @return Byte count.
        //! 
        public: size_t data_byte( void ) const { return sizeof( _Tp ); }

        //! 
        //! @brief
        //!     \~japanese ピクセル内データ数を取得する。
        //!     \~english  Get the number of data elements per pixel.
        //!
        //! @return Number of data elements.
        //! 
        public: size_t pixel_size( void )const { return static_cast< size_t >( NUM ); }

        //! 
        //! @brief
        //!     \~japanese 1ピクセルあたりのバイト数を取得する。
        //!     \~english  Get the byte size per pixel.
        //!
        //! @return Byte count.
        //! 
        public: size_t pixel_byte( void )const { return static_cast< size_t >( NUM * sizeof( value_type ) ); }

        //! 
        //! @brief
        //!     \~japanese ビット深度を取得する。
        //!     \~english  Get the bit depth.
        //!
        //! @return Bit depth.
        //!
        public: size_t bit_depth ( void )const { return static_cast< size_t >( DEPTH ); }

        //! 
        //! @brief
        //!     \~japanese 指定したインデックスのデータを取得する。
        //!     \~english  Get the data at the specified index.
        //!
        //! @param [in] index_in  Index of the element.
        //! @return Value at the specified index.
        //!
        public: template< typename _Tpi >
        _Tp get( const _Tpi index_in )
        {
            //要素番号チェック.
            if( static_cast< uint8_t >( index_in ) >= NUM ) { throw std::out_of_range( "the channel index is outside the pixel" ); }
            return this->data[ static_cast< uint8_t >( index_in ) ];
        }

        //! 
        //! @brief
        //!     \~japanese 1データ当たりのバイト数を型情報から取得する。
        //!     \~english  Get the byte size per data element (static).
        //!
        //! @return Byte count.
        //! 
        public: static size_t type_data_byte( void ) { return sizeof( _Tp ); }

        //! 
        //! @brief
        //!     \~japanese ビット深度を型情報から取得する。
        //!     \~english  Get the bit depth (static).
        //!
        //! @return Bit depth.
        //!
        public: static size_t type_bit_depth ( void ) { return static_cast< size_t >( DEPTH ); }

        //! 
        //! @brief
        //!     \~japanese ピクセル内データ数を型情報から取得する。
        //!     \~english  Get the number of data elements per pixel (static).
        //!
        //! @return Number of data elements.
        //! 
        public: static size_t type_pixel_size ( void ) { return static_cast< size_t >( NUM ); }

        //! 
        //! @brief
        //!     \~japanese 1ピクセルあたりのバイト数を型情報から取得する。
        //!     \~english  Get the byte size per pixel (static).
        //!
        //! @return Byte count.
        //! 
        public: static size_t type_pixel_byte( void ) { return type_data_byte() * type_pixel_size(); }



        //---------------------------------------
        // アクセッサ.
        //---------------------------------------

        //! 
        //! @brief
        //!     \~japanese インデックスで指定した要素にアクセスする。
        //!     \~english  Access the element at the specified index.
        //!
        //! @param [in] i_in  Index of the element.
        //! @return Reference to the element.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline _Tp& operator[]( const T i_in ) noexcept
        {
            return this->data[ static_cast<size_t>( i_in ) ];
        }

        //! 
        //! @brief
        //!     \~japanese インデックスで指定した要素にアクセスする（const）。
        //!     \~english  Access the element at the specified index (const).
        //!
        //! @param [in] i_in  Index of the element.
        //! @return Const reference to the element.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline const _Tp& operator[]( const T i_in ) const noexcept
        {
            return this->data[ static_cast<size_t>( i_in ) ];
        }

        //! 
        //! @brief
        //!     \~japanese インデックスと値のセーフティチェックを行う。
        //!     \~english  Perform safety checks on index_in and value_in.
        //!
        //! @param [in] index_in  Index to check.
        //! @param [in] value_in  Value to check for bit depth.
        //!
        private: void checkSafetyArgs( const uint8_t index_in, const _Tp value_in )
        {
            //要素番号チェック.
            if( index_in >= NUM ) { throw std::out_of_range( "the channel index is outside the pixel" ); }

            //ビット長チェック.
            int bit_depth = 0;
            if ( value_in == 0) 
            {
                // 0は「0」として1桁とみなす
                bit_depth = 1;
            } 
            else 
            {
                // numを右シフトしながらビット数をカウント
                _Tp tmp = value_in;
                while ( tmp > 0)
                {
                    ++bit_depth;
                    tmp >>= 1; // 1ビット右シフト
                }
            }
            if( bit_depth == 0 || bit_depth > DEPTH )
            {
                throw std::out_of_range( "the value does not fit in the pixel bit depth" );
            }
        }

        //---------------------------------------
        // セッター.
        //---------------------------------------

        //! 
        //! @brief
        //!     \~japanese 指定したインデックスに値を設定する。
        //!     \~english  Set the value at the specified index.
        //!
        //! @param [in] index_in  Index of the element.
        //! @param [in] value_in  Value to set.
        //!
        public: template< typename _Tpi >
        void set( const _Tpi index_in, const _Tp value_in )
        { 
            checkSafetyArgs( static_cast< uint8_t >( index_in ), value_in );
            this->data[ index_in ] = value_in;
        }

        //---------------------------------------
        // Constractor / Destractor.
        //---------------------------------------


        //---------------------------------------
        // Operator.
        //---------------------------------------
        
        //! 
        //! @brief
        //!     \~japanese 全要素が等しいか判定する等号演算子。
        //!     \~english  Equality operator to check if all elements are equal.
        //!
        //! @param [in] obj  Object to compare.
        //! @retval true     All elements are equal.
        //! @retval false    At least one element differs.
        //!
        public: bool operator == ( const Pixel_< _Tp, NUM, DEPTH >& obj )
        {
            for( uint8_t i = 0; i < NUM; ++i )
            {
                if( this->data[ i ] != obj[i] ){ return false; }
            }
            return true;
        }

        //! 
        //! @brief
        //!     \~japanese 要素を比較し、等しくないか判定する不等号演算子。
        //!     \~english  Inequality operator to check if any element differs.
        //!
        //! @param [in] obj  Object to compare.
        //! @retval true     At least one element differs.
        //! @retval false    All elements are equal.
        //!
        public: bool operator != ( const Pixel_< _Tp, NUM, DEPTH >& obj )
        {
            return !( ( *this ) == obj );
        }

        
        //! 
        //! @brief
        //!     \~japanese ピクセルデータを文字列形式 "(v1, v2, ...)" で返す。
        //!     \~english  Get string representation of pixel data in the form "(v1, v2, ...)".
        //!
        //! @return String representation of pixel data.
        //!
        public : std::string str()const 
        {
            std::string os =  "( ";
            for( size_t i = 0; i < this->pixel_size(); ++i )
            {
                os += toString( data[i] );
                if( i < this->pixel_size() - 1 )
                {
                    os += ", ";
                }
                else
                {
                    os += " )";
                }
            }
            return os;
        }
    };

    //! 
    //! @brief
    //!     \~japanese ピクセルデータを標準出力ストリームに文字列形式で出力する演算子。
    //!     \~english  Operator to output pixel data to standard output stream as a string.
    //!
    //! @param [in] os   Output stream.
    //! @param [in] obj  Pixel object.
    //! @return Reference to output stream.
    //!
    template< typename _Tp, uint8_t NUM, uint8_t DEPTH
            , typename = std::enable_if_t<std::is_integral< _Tp >::value  && ( NUM > 0 ) && ( DEPTH > 0 ) > >
    inline static std::ostream& operator << ( std::ostream& os, const Pixel_< _Tp, NUM, DEPTH >& obj )
    {
        os << obj.str();
        return os;
    }




    typedef Pixel_<  U8, 3,  8 > color8_t ;   //!< \~english 24-bit RGB color type (8 bits per channel). \~japanese 各チャネル8ビットのRGBカラーデータ型。
    typedef Pixel_<  U8, 3,  8 > color08_t;   //!< \~english 24-bit RGB color type (8 bits per channel). \~japanese 各チャネル8ビットのRGBカラーデータ型。
    typedef Pixel_< U16, 3, 10 > color10_t;   //!< \~english 30-bit RGB color type (10 bits per channel). \~japanese 各チャネル10ビットのRGBカラーデータ型。
    typedef Pixel_< U16, 3, 12 > color12_t;   //!< \~english 36-bit RGB color type (12 bits per channel). \~japanese 各チャネル12ビットのRGBカラーデータ型。
    typedef Pixel_< U16, 3, 14 > color14_t;   //!< \~english 42-bit RGB color type (14 bits per channel). \~japanese 各チャネル14ビットのRGBカラーデータ型。
    typedef Pixel_< U16, 3, 16 > color16_t;   //!< \~english 48-bit RGB color type (16 bits per channel). \~japanese 各チャネル16ビットのRGBカラーデータ型。
    typedef Pixel_< U32, 3, 32 > color32_t;   //!< \~english 96-bit RGB color type (32 bits per channel). \~japanese 各チャネル32ビットのRGBカラーデータ型。
    typedef Pixel_< U64, 3, 64 > color64_t;   //!< \~english 192-bit RGB color type (64 bits per channel). \~japanese 各チャネル64ビットのRGBカラーデータ型。
    typedef Pixel_<  U8, 4,  8 > acolor8_t;   //!< \~english 32-bit ARGB color type (8 bits per channel). \~japanese 各チャネル8ビットのARGBカラーデータ型。
    typedef Pixel_<  U8, 4,  8 > acolor08_t;  //!< \~english 32-bit ARGB color type (8 bits per channel). \~japanese 各チャネル8ビットのARGBカラーデータ型。
    typedef Pixel_< U16, 4, 10 > acolor10_t;  //!< \~english 40-bit ARGB color type (10 bits per channel). \~japanese 各チャネル10ビットのARGBカラーデータ型。
    typedef Pixel_< U16, 4, 12 > acolor12_t;  //!< \~english 48-bit ARGB color type (12 bits per channel). \~japanese 各チャネル12ビットのARGBカラーデータ型。
    typedef Pixel_< U16, 4, 14 > acolor14_t;  //!< \~english 56-bit ARGB color type (14 bits per channel). \~japanese 各チャネル14ビットのARGBカラーデータ型。
    typedef Pixel_< U16, 4, 16 > acolor16_t;  //!< \~english 64-bit ARGB color type (16 bits per channel). \~japanese 各チャネル16ビットのARGBカラーデータ型。
    typedef Pixel_< U32, 4, 32 > acolor32_t;  //!< \~english 128-bit ARGB color type (32 bits per channel). \~japanese 各チャネル32ビットのARGBカラーデータ型。
    typedef Pixel_< U64, 4, 64 > acolor64_t;  //!< \~english 256-bit ARGB color type (64 bits per channel). \~japanese 各チャネル64ビットのARGBカラーデータ型。
    typedef Pixel_<  U8, 1,  8 > mono8_t;    //!< \~english 8-bit monochrome color type. \~japanese 8ビットモノクロカラーデータ型。
    typedef Pixel_<  U8, 1,  8 > mono08_t;   //!< \~english 8-bit monochrome color type. \~japanese 8ビットモノクロカラーデータ型。
    typedef Pixel_< U16, 1, 10 > mono10_t;   //!< \~english 10-bit monochrome color type. \~japanese 10ビットモノクロカラーデータ型。
    typedef Pixel_< U16, 1, 12 > mono12_t;   //!< \~english 12-bit monochrome color type. \~japanese 12ビットモノクロカラーデータ型。
    typedef Pixel_< U16, 1, 16 > mono16_t;   //!< \~english 16-bit monochrome color type. \~japanese 16ビットモノクロカラーデータ型。
    typedef Pixel_< U32, 1, 32 > mono32_t;   //!< \~english 32-bit monochrome color type. \~japanese 32ビットモノクロカラーデータ型。
    typedef Pixel_< U64, 1, 64 > mono64_t;   //!< \~english 64-bit monochrome color type. \~japanese 64ビットモノクロカラーデータ型。



    static_assert( std::is_trivially_copyable< color08_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color10_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color12_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color14_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color16_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color12_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< color14_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor08_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor10_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor12_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor14_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor16_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor32_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< acolor64_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono08_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono10_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono12_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono16_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono32_t >::value, "Pixel must be trivially copyable." );
    static_assert( std::is_trivially_copyable< mono64_t >::value, "Pixel must be trivially copyable." );
};


#endif //WONDERSTEWENGINE_DATA_COLOR_H