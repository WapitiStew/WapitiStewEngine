//*****************************************************************************************************************
//! 
//! @file    wse_Size.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese サイズ構造体テンプレートの定義。
//!     \~english  Template definition for size structure.
//!
//!
//! @details
//!     \~japanese このファイルでは、2次元空間上のサイズ（幅・高さ）を扱う汎用テンプレート構造体 Size_ を定義しています。
//!     \~english  This file defines a generic 2D size structure template Size_, supporting width and height.
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

#ifndef WONDERSTEWENGINE_DATA_SIZE_H
#define WONDERSTEWENGINE_DATA_SIZE_H


// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#pragma warning(disable: 4820)  // メンバー変数の定義した時にアライメント調整用のスペースで発生する警告除去.
#endif

// include files
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../depend/wse_Enum.h"
#include "../utility/wse_StringTool.h"




namespace wse
{

    //! 
    //! @class Size_
    //! 
    //! @brief
    //!     \~japanese 汎用2次元サイズ構造体テンプレート。
    //!     \~english  Generic 2D size structure template.
    //! 
    //! @details
    //!     \~japanese このクラスは幅と高さの2次元サイズを保持・操作する。
        //!     \~english  This class represents and manipulates 2D width and height values.
        //!
    //! @note
    //!     \~japanese テンプレート引数により様々な数値型で使用できる。
    //!     \~english  Can be used with various numeric types via template argument.
    //!
    template<typename _Tp>
    struct WSE_API Size_      final
    {
        typedef _Tp value_type; //!< \~english Value type alias. \~japanese 値の型のエイリアス.

        //----------------------------------
        // Member
        //----------------------------------
        private:_Tp m_width ; //!< \~english Width.  \~japanese 幅.
        private:_Tp m_height; //!< \~english Height. \~japanese 高さ.


        //----------------------------------
        // Getter
        //----------------------------------
        //! 
        //! @brief
        //!     \~japanese バイトサイズを返す。
        //!     \~english  Returns size in bytes.
        //! 
        //! @return Byte size.
        //! 
        public: uint64_t byte( void ) { return sizeof( value_type ) * 2; }

        //! 
        //! @brief
        //!     \~japanese 幅を返す。
        //!     \~english  Returns width.
        //! 
        //! @return Width.
        //! 
        public: _Tp width ( void ) const { return m_width ; }

        //! 
        //! @brief
        //!     \~japanese 高さを返す。
        //!     \~english  Returns height.
        //! 
        //! @return Height.
        //! 
        public: _Tp height( void ) const { return m_height; }

        //----------------------------------
        // Setter
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese 幅と高さを設定する。
        //!     \~english  Sets width and height.
        //! 
        //! @param [in] width_in  Width value.
        //! @param [in] height_in Height value.
        //! 
        public : void setup( const _Tp width_in, const _Tp height_in )
        {
            this->m_width  = width_in ;
            this->m_height = height_in;
        }

        //----------------------------------
        // Costractor / Destoractor
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。
        //!     \~english  Default constructor.
        //! 
        public: explicit Size_( void )
            : m_width  ( static_cast< _Tp >( 0 ) )
            , m_height ( static_cast< _Tp >( 0 ) ){}

        //! 
        //! @brief
        //!     \~japanese デストラクタ。
        //!     \~english  Destructor.
        //! 
        public: virtual ~Size_( void ){}

        //! 
        //! @brief
        //!     \~japanese 幅と高さを指定するコンストラクタ。
        //!     \~english  Constructor with width and height.
        //! 
        //! @param [in] width_in     Width value.
        //! @param [in] height_in    Height value.
        //! 
        public: Size_( const _Tp width_in, const _Tp height_in )
            : m_width  ( width_in )
            , m_height ( height_in ){}

        //! 
        //! @brief
        //!     \~japanese キャスト付きテンプレートコンストラクタ。
        //!     \~english  Templated constructor with type casting.
        //! 
        //! @param [in] width_in     Width value.
        //! @param [in] height_in    Height value.
        //! 
        public: template< typename _inTp > explicit Size_( const _inTp width_in, const _inTp height_in ) 
            : m_width  ( static_cast<_Tp>( width_in ) )
            , m_height ( static_cast<_Tp>( height_in ) ){}

        //! 
        //! @brief
        //!     \~japanese 別型Size_からのキャストコンストラクタ。
        //!     \~english  Cast constructor from another Size_ of different type.
        //! 
        //! @param [in] obj_in Source object.
        //! 
        public: template< typename _inTp > explicit Size_( const Size_<_inTp>& obj_in ) 
            : m_width  ( static_cast<_Tp>(obj_in.width() ) )
            , m_height ( static_cast<_Tp>(obj_in.height() ) ){}

        //! 
        //! @brief Copy constructor.
        //! 
        //! @param [in] obj_in Source object.
        //! 
        public: Size_( const Size_& obj_in )
            : m_width  ( obj_in.m_width )
            , m_height ( obj_in.m_height ){}

        //! 
        //! @brief Move constructor.
        //! 
        //! @param [in] obj_inout Source object.
        //! 
        public:
        Size_( Size_&& obj_inout )noexcept
            : m_width  ( std::move(obj_inout.m_width) )
            , m_height ( std::move(obj_inout.m_height) ){}

        //! 
        //! @brief
        //!     \~japanese スワップ関数。
        //!     \~english  Swap function.
        //! 
        //! @param [in,out] obj1_inout First object.
        //! @param [in,out] obj2_inout Second object.
        //! 
        friend void swap( Size_<_Tp>& obj1_inout, Size_<_Tp>& obj2_inout )
        {
            std::swap(obj1_inout.m_width, obj2_inout.m_width);
            std::swap(obj1_inout.m_height, obj2_inout.m_height);
        }

        //! 
        //! @brief Copy assignment operator.
        //! 
        //! @param [in] obj Source object.
        //! 
        public:
        Size_< _Tp >& operator = ( const Size_<_Tp>& obj )
        {
            Size_< _Tp > copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }

        //! 
        //! @brief Move assignment operator.
        //! 
        //! @param [in] obj Source object.
        //! 
        public:
        Size_< _Tp >& operator = ( Size_<_Tp>&& obj )noexcept
        {
            Size_< _Tp > move_obj(std::move(obj));
            swap(*this, move_obj);
            return *this;
        }

        //! 
        //! @brief
        //!     \~japanese 等価演算子。
        //!     \~english  Equality operator.
        //! 
        //! @param [in] data Comparison target.
        //! @retval true  Equal.
        //! @retval false Not equal.
        //! 
        public:
        bool operator == ( const Size_< _Tp >& data )
        {
            return ( this->m_width == data.m_width ) && ( this->m_height == data.m_height );
        }

        //! 
        //! @brief
        //!     \~japanese 非等価演算子。
        //!     \~english  Inequality operator.
        //! 
        //! @param [in] data Comparison target.
        //! @retval true  Not equal.
        //! @retval false Equal.
        //! 
        public:
        bool operator != ( const Size_< _Tp >& data )
        {
            return !( *this == data );
        }



        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //! 
        //! @brief
        //!     \~japanese 面積を返す。
        //!     \~english  Returns area.
        //! 
        //! @return Area.
        //! 
        public: 
        double area( void ) const
        {
            return static_cast< double >( this->m_width * this->m_height );
        }

        //! 
        //! @brief
        //!     \~japanese アスペクト比を返す。
        //!     \~english  Returns aspect ratio (width / height).
        //! 
        //! @return Aspect ratio.
        //! 
        double aspect( void ) const
        {
            return static_cast< double >( this->m_width ) / static_cast< double >( this->m_height );
        }

        //! 
        //! @brief
        //!     \~japanese 空かどうかを判定する。
        //!     \~english  Checks if empty.
        //! 
        //! @retval true  Width or height is zero.
        //! @retval false Non-zero dimensions.
        //! 
        bool empty( void ) const
        {
            return ( ( this->m_width == _Tp( 0 ) ) || ( this->m_height == _Tp(0) ) );
        }

        




        //! 
        //! @brief
        //!     \~japanese 文字列に変換する。
        //!     \~english  Returns string representation.
        //! 
        //! @return String.
        //! 
        public : std::string str(  )const
        {
            const std::string os = "[ " + toString( this->width() ) + " x " + toString( this->height() ) + " ]";
            return os;
        }
    };

    //! 
    //! @brief
    //!     \~japanese Size_ オブジェクトを文字列として出力する。
    //!     \~english  Output the Size_ object as a string.
    //! 
    //! @param [in] os   Output stream.
    //! @param [in] obj  Size_ object.
    //! @return Output stream with formatted size string.
    //! 
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Size_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }

    //! 
    //! @brief
    //!     \~japanese 2つのサイズが等しいかを比較する。
    //!     \~english  Compare if two sizes are equal.
    //! 
    //! @param [in] obj1 First Size_ object.
    //! @param [in] obj2 Second Size_ object.
    //! @retval true     Both width and height are equal.
    //! @retval false    Either width or height differ.
    //! 
    template<typename _Tp>
    inline static bool operator == ( const Size_<_Tp>& obj1, const Size_<_Tp>& obj2 )
    {
        return ( obj1.width() == obj2.width() ) && ( obj1.height() == obj2.height() );
    }




    

    typedef Size_<  S8 > S8_SIZE;           //!< \~english Size using S8.            \~japanese S8型サイズ.
    typedef Size_<  U8 > U8_SIZE;           //!< \~english Size using U8.            \~japanese U8型サイズ.
    typedef Size_< S16 > S16_SIZE;          //!< \~english Size using S16.           \~japanese S16型サイズ.
    typedef Size_< U16 > U16_SIZE;          //!< \~english Size using U16.           \~japanese U16型サイズ.
    typedef Size_< S32 > S32_SIZE;          //!< \~english Size using S32.           \~japanese S32型サイズ.
    typedef Size_< U32 > U32_SIZE;          //!< \~english Size using U32.           \~japanese U32型サイズ.
    typedef Size_< S64 > S64_SIZE;          //!< \~english Size using S64.           \~japanese S64型サイズ.
    typedef Size_< U64 > U64_SIZE;          //!< \~english Size using U64.           \~japanese U64型サイズ.
    typedef Size_< F32 > F32_SIZE;          //!< \~english Size using F32.           \~japanese F32型サイズ.
    typedef Size_< F64 > F64_SIZE;          //!< \~english Size using F64.           \~japanese F64型サイズ.

    typedef Size_<  S8 >  int08_size;       //!< \~english Size using int8.          \~japanese int8サイズ.
    typedef Size_< S16 >  int16_size;       //!< \~english Size using int16.         \~japanese int16サイズ.
    typedef Size_< S32 >  int32_size;       //!< \~english Size using int32.         \~japanese int32サイズ.
    typedef Size_< S64 >  int64_size;       //!< \~english Size using int64.         \~japanese int64サイズ.
    typedef Size_<  S8 > sint08_size;       //!< \~english Size using sint8.         \~japanese sint8サイズ.
    typedef Size_< S16 > sint16_size;       //!< \~english Size using sint16.        \~japanese sint16サイズ.
    typedef Size_< S32 > sint32_size;       //!< \~english Size using sint32.        \~japanese sint32サイズ.
    typedef Size_< S64 > sint64_size;       //!< \~english Size using sint64.        \~japanese sint64サイズ.
    typedef Size_<  U8 > uint08_size;       //!< \~english Size using uint8.         \~japanese uint8サイズ.
    typedef Size_< U16 > uint16_size;       //!< \~english Size using uint16.        \~japanese uint16サイズ.
    typedef Size_< U32 > uint32_size;       //!< \~english Size using uint32.        \~japanese uint32サイズ.
    typedef Size_< U64 > uint64_size;       //!< \~english Size using uint64.        \~japanese uint64サイズ.
    typedef Size_< F32 > float32_size;      //!< \~english Size using float32.       \~japanese float32サイズ.
    typedef Size_< F64 > float64_size;      //!< \~english Size using float64.       \~japanese float64サイズ.
    typedef Size_< F64 >  double_size;      //!< \~english Size using double.        \~japanese doubleサイズ.

    typedef Size_< int >     Size2i;        //!< \~english Size using int.           \~japanese int型サイズ.
    typedef Size_< int64_t > Size2l;        //!< \~english Size using int64_t.       \~japanese int64_t型サイズ.
    typedef Size_< float >   Size2f;        //!< \~english Size using float.         \~japanese float型サイズ.
    typedef Size_< double >  Size2d;        //!< \~english Size using double.        \~japanese double型サイズ.
    typedef Size2i           Size;          //!< \~english Default size alias.   

};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


#endif //WONDERSTEWENGINE_DATA_SIZE_H