//*****************************************************************************************************************
//! 
//! @file    wse_Map.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Map.h は 2D データを一次元ベクトルで保持する Map テンプレートクラスを定義するファイル。
//!     \~english  Map.h defines the Map template class that stores 2D data in a one-dimensional vector.
//!
//! 
//! @details
//!     \~japanese
//!         このファイルでは、幅と高さを指定して要素数を管理する Map クラスを提供する。
//!         要素アクセス、データ取得・設定、初期化、行・列サイズ取得、文字列化などの機能を備える。
//!     \~english
//!         This file provides the Map class, which manages elements based on specified width and height.
//!         It includes element access, data retrieval/set, initialization, size getters, and string conversion functionality.
//!
//! 
//! @note
//!     \~japanese 無効なインデックスアクセス時やサイズ不一致時には標準Exceptionを投げる。
//!     検査付きアクセスには at() / row() を使用する。operator[] は検査なしの高速経路である。
//!     \~english  Throws a standard exception when an invalid index is accessed or sizes do not match.
//!     Use at() / row() for checked access; operator[] is the unchecked fast path.
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

#ifndef WONDERSTEWENGINE_DATA_MAP_H
#define WONDERSTEWENGINE_DATA_MAP_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  //
#endif
        #include <limits>
        #include <stdexcept>
        #include "../depend/wse_STD.h"
        #include "../depend/wse_Enum.h"
        #include "../depend/wse_Constant.h"
        #include "../utility/wse_StringTool.h"
        #include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)
#endif
namespace wse
{
    //!
    //! @class Map
    //!
    //! @brief
    //!     \~japanese 2D データを一次元ベクトルで保持する汎用マップクラス。
    //!     \~english  Generic map class that stores 2D data in a one-dimensional vector.
    //!
    //! @details
    //!     \~japanese
    //!         Map クラスは幅(width)と高さ(height)を持ち、内部で std::vector<_Tp> を用いてデータを管理する。
    //!         要素アクセス演算子、データ取得・設定メソッド、初期化メソッド、行・列数取得メソッド、
    //!         コピー・ムーブ・スワップ操作、文字列化機能を提供する。
    //!     \~english
    //!         The Map class holds width and height, managing data internally using std::vector<_Tp>.
    //!         It provides element access operators, data get/set methods, initialization, row/column size getters,
    //!         copy/move/swap operations, and string conversion functionality.
    //!
    template< typename _Tp >
    class WSE_API Map
    {

        typedef _Tp     value_type;
        //---------------------------------------
        // メンバー.
        //---------------------------------------
        protected : std::vector< _Tp > m_data;   //!< \~english Underlying data storage vector.  \~japanese 要素を格納するベクトル。
        protected : size_t             m_width ; //!< \~english Number of columns (width).       \~japanese 列数（幅）。
        protected : size_t             m_height; //!< \~english Number of rows (height).         \~japanese 行数（高さ）。

        //!
        //! @brief
        //!     \~japanese 幅×高さの要素数をオーバーフロー検査付きで計算する（内部使用）。
        //!     \~english  Compute width × height element count with overflow checking (internal use).
        //!
        //! @param [in] width_in   Width (columns).
        //! @param [in] height_in  Height (rows).
        //! @return Element count width_in * height_in.
        //!
        //! @note
        //!     \~japanese 積が size_t で表現できない場合は std::invalid_argument を投げる。
        //!     ラップした積が data サイズ検証を偶然通過し、不整合な Map が構築されるのを防ぐ。
        //!     \~english  Throws std::invalid_argument when the product is not
        //!     representable in size_t, so a wrapped product can never slip through the data-size
        //!     validation and build an inconsistent Map.
        //!
        private: static size_t checkedCount( const size_t width_in, const size_t height_in )
        {
            if( ( height_in != 0 ) && ( width_in > ( ( std::numeric_limits< size_t >::max )( ) / height_in ) ) )
            {
                throw std::invalid_argument( "width x height is not representable in size_t" );
            }
            return width_in * height_in;
        }

        //!
        //! @brief
        //!     \~japanese マップ同士を交換するスワップ関数。
        //!     \~english  Swap function to exchange two Map objects.
        //!
        //! @param [in,out] obj1_inout  First Map object.
        //! @param [in,out] obj2_inout  Second Map object.
        //!
        friend void swap( Map& obj1_inout, Map& obj2_inout )
        {
            std::swap( obj1_inout.m_data, obj2_inout.m_data );
            std::swap( obj1_inout.m_width , obj2_inout.m_width  );
            std::swap( obj1_inout.m_height, obj2_inout.m_height );
        }


        //---------------------------------------
        // アクセッサ.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese 内部データベクトルをコピーして返す。
        //!     \~english  Return a copy of the internal data vector.
        //!
        //! @return Copy of data vector.
        //!
        public: std::vector< _Tp >  data ( void ) const { return  this->m_data;         }

        //!
        //! @brief
        //!     \~japanese const な内部データベクトルへのポインタを返す。
        //!     \~english  Return const pointer to the internal data vector.
        //!
        //! @return Const pointer to data vector.
        //!
        public: const std::vector< _Tp >* vector ( void ) const { return &this->m_data;         }

        //!
        //! @brief
        //!     \~japanese 先頭要素への参照を返す。
        //!     \~english  Return reference to the first element.
        //!
        //! @return Reference to first element.
        //!
        //! @note
        //!     \~japanese 空の Map では std::out_of_range を投げる。
        //!     \~english  Throws std::out_of_range when the Map is empty.
        //!
        public: _Tp& front ( void )
        {
            if( this->m_data.empty() ) { throw std::out_of_range( "the index is outside the Map shape" ); }
            return this->m_data.front();
        }

        //!
        //! @brief
        //!     \~japanese const な先頭要素への参照を返す。
        //!     \~english  Return const reference to the first element.
        //!
        //! @return Const reference to first element.
        //!
        //! @note
        //!     \~japanese 空の Map では std::out_of_range を投げる。
        //!     \~english  Throws std::out_of_range when the Map is empty.
        //!
        public: const _Tp& front   ( void ) const
        {
            if( this->m_data.empty() ) { throw std::out_of_range( "the index is outside the Map shape" ); }
            return this->m_data.front();
        }

        //!
        //! @brief
        //!     \~japanese 幅と高さを記憶するヘッダーサイズをバイト数で返す。
        //!     \~english  Return header size (width + height) in bytes.
        //!
        //! @return Header size in bytes.
        //!
        public: size_t header_size ( void ) const { return sizeof( m_width ) + sizeof( m_height ); }

        //!
        //! @brief
        //!     \~japanese データベクトルが使用するメモリサイズをバイト数で返す。
        //!     \~english  Return memory size used by data vector in bytes.
        //!
        //! @return Memory size in bytes.
        //!
        public: size_t memory_size ( void ) const { return sizeof( _Tp ) * this->m_data.size(); }

        //!
        //! @brief
        //!     \~japanese 行 y の先頭要素へのポインタを返す要素アクセス演算子。
        //!     \~english  Element access operator returning pointer to first element of row y.
        //!
        //! @param [in] y_in  Row index.
        //! @return Pointer to the first element of the row.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline _Tp* operator[]( const T y_in ) noexcept
        {
            return &this->m_data[ static_cast<size_t>( y_in ) * this->m_width ];
        }

        //!
        //! @brief
        //!     \~japanese const な行 y の先頭要素へのポインタを返す要素アクセス演算子。
        //!     \~english  Const element access operator returning pointer to first element of row y.
        //!
        //! @param [in] y_in  Row index.
        //! @return Const pointer to the first element of the row.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline const _Tp* operator[]( const T y_in ) const noexcept
        {
            return &this->m_data[ static_cast<size_t>( y_in ) * this->m_width ];
        }

        //!
        //! @brief
        //!     \~japanese 座標 (x, y) の要素への参照を検査付きで返す。
        //!     \~english  Return a bounds-checked reference to the element at (x, y).
        //!
        //! @param [in] x_in  Column index.
        //! @param [in] y_in  Row index.
        //! @return Reference to the element.
        //!
        //! @note
        //!     \~japanese x_in >= width() または y_in >= height() の場合は std::out_of_range を
        //!     投げる。空の Map も同様に拒否される。
        //!     \~english  Throws std::out_of_range when x_in >= width() or
        //!     y_in >= height(); an empty Map is rejected the same way.
        //!
        public: _Tp& at( const size_t x_in, const size_t y_in )
        {
            if( ( x_in >= this->m_width ) || ( y_in >= this->m_height ) )
            {
                throw std::out_of_range( "the index is outside the Map shape" );
            }
            return this->m_data[ ( y_in * this->m_width ) + x_in ];
        }

        //!
        //! @brief
        //!     \~japanese 座標 (x, y) の要素への const 参照を検査付きで返す。
        //!     \~english  Return a bounds-checked const reference to the element at (x, y).
        //!
        //! @param [in] x_in  Column index.
        //! @param [in] y_in  Row index.
        //! @return Const reference to the element.
        //!
        //! @note
        //!     \~japanese x_in >= width() または y_in >= height() の場合は std::out_of_range を
        //!     投げる。空の Map も同様に拒否される。
        //!     \~english  Throws std::out_of_range when x_in >= width() or
        //!     y_in >= height(); an empty Map is rejected the same way.
        //!
        public: const _Tp& at( const size_t x_in, const size_t y_in ) const
        {
            if( ( x_in >= this->m_width ) || ( y_in >= this->m_height ) )
            {
                throw std::out_of_range( "the index is outside the Map shape" );
            }
            return this->m_data[ ( y_in * this->m_width ) + x_in ];
        }

        //!
        //! @brief
        //!     \~japanese 行 y の先頭要素へのポインタを検査付きで返す。
        //!     \~english  Return a bounds-checked pointer to the first element of row y.
        //!
        //! @param [in] y_in  Row index.
        //! @return Pointer to width() consecutive elements of the row.
        //!
        //! @note
        //!     \~japanese y_in >= height() または width() == 0 の場合は std::out_of_range を
        //!     投げる。有効な戻り値は width() 要素分だけ参照してよい。
        //!     \~english  Throws std::out_of_range when y_in >= height() or
        //!     width() == 0. A valid return may be dereferenced for width() elements only.
        //!
        public: _Tp* row( const size_t y_in )
        {
            if( ( y_in >= this->m_height ) || ( this->m_width == 0 ) )
            {
                throw std::out_of_range( "the index is outside the Map shape" );
            }
            return this->m_data.data() + ( y_in * this->m_width );
        }

        //!
        //! @brief
        //!     \~japanese 行 y の先頭要素への const ポインタを検査付きで返す。
        //!     \~english  Return a bounds-checked const pointer to the first element of row y.
        //!
        //! @param [in] y_in  Row index.
        //! @return Const pointer to width() consecutive elements of the row.
        //!
        //! @note
        //!     \~japanese y_in >= height() または width() == 0 の場合は std::out_of_range を
        //!     投げる。有効な戻り値は width() 要素分だけ参照してよい。
        //!     \~english  Throws std::out_of_range when y_in >= height() or
        //!     width() == 0. A valid return may be dereferenced for width() elements only.
        //!
        public: const _Tp* row( const size_t y_in ) const
        {
            if( ( y_in >= this->m_height ) || ( this->m_width == 0 ) )
            {
                throw std::out_of_range( "the index is outside the Map shape" );
            }
            return this->m_data.data() + ( y_in * this->m_width );
        }

        //!
        //! @brief
        //!     \~japanese 全要素が連続して並ぶ先頭へのポインタを返す。
        //!     \~english  Return a pointer to the contiguous element storage.
        //!
        //! @return Pointer to size() consecutive elements; may be null for an empty Map.
        //!
        //! @note
        //!     \~japanese 要素値の読み書きだけが可能で、Map の形状 (width / height / 要素数) は
        //!     この経路から変更できない。vector() の安全な置き換え先である。
        //!     \~english  Grants element value access only; the Map shape (width / height / element
        //!     count) cannot be changed through this path. This is the safe replacement for vector().
        //!
        public: _Tp* elements( void ) noexcept { return this->m_data.data(); }

        //!
        //! @brief
        //!     \~japanese 全要素が連続して並ぶ先頭への const ポインタを返す。
        //!     \~english  Return a const pointer to the contiguous element storage.
        //!
        //! @return Const pointer to size() consecutive elements; may be null for an empty Map.
        //!
        public: const _Tp* elements( void ) const noexcept { return this->m_data.data(); }


        //---------------------------------------
        // ゲッター.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese 要素数（幅×高さ）を返す。
        //!     \~english  Return number of elements (width × height).
        //!
        //! @return Total element count.
        //!
        public: size_t size  ( void ) const { return this->m_data.size(); }

        //!
        //! @brief
        //!     \~japanese 幅（列数）を返す。
        //!     \~english  Return width (number of columns).
        //!
        //! @return Width (columns).
        //!
        public: size_t width ( void ) const { return this->m_width;  }

        //!
        //! @brief
        //!     \~japanese 高さ（行数）を返す。
        //!     \~english  Return height (number of rows).
        //!
        //! @return Height (rows).
        //!
        public: size_t height( void ) const { return this->m_height; }

        //!
        //! @brief
        //!     \~japanese 指定インデックスの要素を取得する。
        //!     \~english  Get element at the specified index.
        //!
        //! @param [in] index_in  Element index.
        //! @return Value at the specified index.
        //!
        //! @note
        //!     \~japanese 範囲外の場合は std::out_of_range を投げる。
        //!     \~english  Throws std::out_of_range when the index is out of range.
        //!
        public: template< typename _Tpi >
        _Tp get( const _Tpi index_in )
        {
            // 要素番号チェック.
            if( static_cast< size_t >( index_in ) >= this->m_data.size() ) { throw std::out_of_range( "the index is outside the Map shape" ); }
            return this->m_data[ static_cast< size_t >( index_in ) ];
        }

        //---------------------------------------
        // セッター.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese 指定インデックスへ値を設定する。
        //!     \~english  Set value at the specified index.
        //!
        //! @param [in] index_in  Element index.
        //! @param [in] value_in  Value to set.
        //! 
        //! @throw
        //!     \~japanese  範囲外の場合 std::out_of_range を投げる。
        //!     \~english   Throws std::out_of_range if out of range.
        //!
        public: template< typename _Tpi >
        void set( const _Tpi index_in, const _Tp value_in )
        {
            if( static_cast< size_t > ( index_in ) >= this->m_data.size() ) { throw std::out_of_range( "the index is outside the Map shape" ); }
            this->m_data[ static_cast< size_t >( index_in ) ] = value_in;
        }

        //!
        //! @brief
        //!     \~japanese 幅および高さを指定してデータを初期化する。
        //!     \~english  Initialize data with specified width and height.
        //!
        //! @param [in] width_in   Width (columns).
        //! @param [in] height_in  Height (rows).
        //!
        //! @note
        //!     \~japanese width_in × height_in が size_t を超える場合は std::invalid_argument を
        //!     投げる。
        //!     \~english  Throws std::invalid_argument when width_in × height_in
        //!     overflows size_t.
        //!
        public :
        void init( const size_t width_in, const size_t height_in )
        {
            this->m_data = std::vector< _Tp >( checkedCount( width_in, height_in ) );
            this->m_width  = width_in ;
            this->m_height = height_in;
        }



        //---------------------------------------
        // Constractor / Destractor.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。
        //!     \~english  Default constructor.
        //!
        public: Map( void ) 
            : m_data   ( std::vector<_Tp>() )
            , m_width  ( 0 )
            , m_height ( 0 )
        {
        }
        //!
        //! @brief
        //!     \~japanese 幅と高さを指定して空のマップを生成するコンストラクタ。
        //!     \~english  Constructor that creates an empty map with specified width and height.
        //!
        //! @param [in] width_in   Width (columns).
        //! @param [in] height_in  Height (rows).
        //!
        public : Map( const size_t width_in, const size_t height_in )
            : m_data   ()
            , m_width  ( width_in )
            , m_height ( height_in )
        {
            this->m_data = std::vector< _Tp >( checkedCount( width_in, height_in ) );
        }

        //!
        //! @brief
        //!     \~japanese 幅・高さおよび初期値を指定してマップを生成するコンストラクタ。
        //!     \~english  Constructor that creates a map with specified width, height, and initial value.
        //!
        //! @param [in] width_in   Width (columns).
        //! @param [in] height_in  Height (rows).
        //! @param [in] value_in   Initial value for all elements.
        //!
        public: Map( const size_t width_in, const size_t height_in, const _Tp value_in )
            : m_data   ()
            , m_width  ( width_in )
            , m_height ( height_in )
        {
            this->m_data = std::vector< _Tp >( checkedCount( width_in, height_in ), value_in );
        }

        //!
        //! @brief
        //!     \~japanese 幅・高さおよびデータベクトルを指定してマップを生成するコンストラクタ。
        //!     \~english  Constructor that creates a map with specified width, height, and data vector.
        //!
        //! @param [in] width_in   Width (columns).
        //! @param [in] height_in  Height (rows).
        //! @param [in] data_in    Vector of element values.
        //! 
        //! @note
        //!     \~japanese  width_in*height_in と data_in.size() が異なる場合 std::invalid_argument を投げる。
        //!     積が size_t を超える場合も同様に std::invalid_argument を投げる。
        //!     \~english   Throws std::invalid_argument if width_in*height_in and data_in.size() are different,
        //!     and when the product overflows size_t.
        //!
        public : Map( const size_t width_in, const size_t height_in, const std::vector< _Tp >&data_in )
            : m_data   ( data_in )
            , m_width  ( width_in )
            , m_height ( height_in )
        {
            if( checkedCount( width_in, height_in ) != data_in.size() )
            {
               throw std::invalid_argument( "data_in.size() must equal width_in x height_in" );
            }
        }

        //!
        //! @brief
        //!     \~japanese デストラクタ。
        //!     \~english  Destructor.
        //!
        public: ~Map( void ){}

        //!
        //! @brief
        //!     \~japanese コピーコンストラクタ。
        //!     \~english  Copy constructor.
        //!
        //! @param [in] obj_in  Source Map object.
        //!
        public: Map( const Map & obj_in   )
            : m_data   ( obj_in.m_data )
            , m_width  ( obj_in.m_width )
            , m_height ( obj_in.m_height )
        {
        }
        //!
        //! @brief
        //!     \~japanese ムーブコンストラクタ。
        //!     \~english  Move constructor.
        //!
        //! @param [in] obj_inout  Source Map object.
        //!
        public:
        Map( Map&& obj_inout ) noexcept
            : m_data   ( std::move( obj_inout.m_data   ) )
            , m_width  ( std::move( obj_inout.m_width  ) )
            , m_height ( std::move( obj_inout.m_height ) )
        {
        }
        //---------------------------------------
        // Operator.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese コピー代入演算子。
        //!     \~english  Copy assignment operator.
        //!
        //! @param [in] obj  Source Map object.
        //! @return Reference to this object.
        //!
        public: Map& operator = ( const Map& obj )
        {
            Map< _Tp > tmp( obj );
            swap( *this, tmp );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese ムーブ代入演算子。
        //!     \~english  Move assignment operator.
        //!
        //! @param [in] obj  Source Map object.
        //! @return Reference to this object.
        //!
        public: Map& operator = ( Map&& obj ) noexcept
        {
            Map< _Tp > tmp( std::move( obj ) );
            swap( *this, tmp );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 要素型が数値型の場合、マップを行列状文字列で返す。
        //!     \~english  If element type is arithmetic, return map as formatted matrix string.
        //!
        //! @return Formatted string representing the map.
        //!
        public :
        template<typename U = _Tp>
        typename std::enable_if<std::is_arithmetic<U>::value, std::string>::type
        str( void ) const 
        {
            std::string os = END_LINE();
            for( size_t y = 0; y < height(); ++y )
            {
                os += "| ";
                for( size_t x = 0; x < width(); ++x )
                {
                    os += toString( this->m_data[ ( y * width() ) + x ] );
                    if( x < width() - 1 )
                    { 
                        os += ", "; 
                    }
                    else
                    {
                        os += " ";
                    }
                }
                os += "|" + END_LINE();
            }
            os += END_LINE();
            return os;
        }
        

        //!
        //! @brief
        //!     \~japanese 要素型が数値型でない場合、幅と高さを返す簡易文字列化。
        //!     \~english  If element type is non-arithmetic, return simple string with width and height.
        //!
        //! @return String "W: width H: height".
        //!
        public :
        template<typename U = _Tp>
        typename std::enable_if<!std::is_arithmetic<U>::value, std::string>::type
        str( void ) const 
        {
            std::string os = "W: " + std::to_string( this->width() ) + " H: " + std::to_string( this->height() );
            return os;
        }

    };

    //!
    //! @brief
    //!     \~japanese マップをストリームに出力する演算子。
    //!     \~english  Operator to output map to stream.
    //!
    //! @param [in] os   Output stream.
    //! @param [in] obj  Map object.
    //! @return Reference to output stream.
    //!
    template< typename _Tp >
    std::ostream& operator << ( std::ostream& os, const Map< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }

    typedef Map<  S8 >  S8_MAP;      //!< \~english Map of signed 8-bit integers.    \~japanese S8 ビット整数のマップ.
    typedef Map<  U8 >  U8_MAP;      //!< \~english Map of unsigned 8-bit integers.  \~japanese U8 ビット整数のマップ.
    typedef Map< S16 > S16_MAP;      //!< \~english Map of signed 16-bit integers.   \~japanese S16 ビット整数のマップ.
    typedef Map< U16 > U16_MAP;      //!< \~english Map of unsigned 16-bit integers. \~japanese U16 ビット整数のマップ.
    typedef Map< S32 > S32_MAP;      //!< \~english Map of signed 32-bit integers.   \~japanese S32 ビット整数のマップ.
    typedef Map< U32 > U32_MAP;      //!< \~english Map of unsigned 32-bit integers. \~japanese U32 ビット整数のマップ.
    typedef Map< S64 > S64_MAP;      //!< \~english Map of signed 64-bit integers.   \~japanese S64 ビット整数のマップ.
    typedef Map< U64 > U64_MAP;      //!< \~english Map of unsigned 64-bit integers. \~japanese U64 ビット整数のマップ.
    typedef Map< F32 > F32_MAP;      //!< \~english Map of 32-bit floats.            \~japanese 32 ビット浮動小数点のマップ.
    typedef Map< F64 > F64_MAP;      //!< \~english Map of 64-bit floats.            \~japanese 64 ビット浮動小数点のマップ.

    typedef Map<  S8 >  sint08_map;  //!< \~english Map of signed 8-bit coordinates.    \~japanese sint08_xy 座標のマップ.
    typedef Map< S16 >  sint16_map;  //!< \~english Map of signed 16-bit coordinates.   \~japanese sint16_xy 座標のマップ.
    typedef Map< S32 >  sint32_map;  //!< \~english Map of signed 32-bit coordinates.   \~japanese sint32_xy 座標のマップ.
    typedef Map< S64 >  sint64_map;  //!< \~english Map of signed 64-bit coordinates.   \~japanese sint64_xy 座標のマップ.
    typedef Map<  U8 >  uint08_map;  //!< \~english Map of unsigned 8-bit coordinates.  \~japanese uint08_xy 座標のマップ.
    typedef Map< U16 >  uint16_map;  //!< \~english Map of unsigned 16-bit coordinates. \~japanese uint16_xy 座標のマップ.
    typedef Map< U32 >  uint32_map;  //!< \~english Map of unsigned 32-bit coordinates. \~japanese uint32_xy 座標のマップ.
    typedef Map< U64 >  uint64_map;  //!< \~english Map of unsigned 64-bit coordinates. \~japanese uint64_xy 座標のマップ.
    typedef Map< F32 > float32_map;  //!< \~english Map of 32-bit float coordinates.    \~japanese float32_xy 座標のマップ.
    typedef Map< F64 > float64_map;  //!< \~english Map of 64-bit float coordinates.    \~japanese float64_xy 座標のマップ.
    typedef Map< F64 >  double_map;  //!< \~english Map of double coordinates.          \~japanese double_xy 座標のマップ.


};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_DATA_MAP_H
