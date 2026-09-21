//*****************************************************************************************************************
//! 
//! @file    wse_Range2D.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! //! @brief
//!     \~japanese 2次元の範囲データを扱うRange2_テンプレート構造体の定義。
//!     \~english  Definition of the Range2_ template struct for handling 2D range data.
//!
//! @details
//!     \~japanese
//!         このファイルでは、2次元空間上の最小・最大座標を保持し、範囲内判定などのユーティリティ関数を提供する。
//!     \~english
//!         This file provides the Range2_ struct that holds the minimum and maximum coordinates in 2D space,
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

#ifndef WONDERSTEWENGINE_DATA_RANGE2D_H
#define WONDERSTEWENGINE_DATA_RANGE2D_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
#include <stdexcept>
#include "../../dynamic.h"
        #include "../depend/wse_STD.h"
        #include "../depend/wse_Typedef.h"
        #include "../depend/wse_Enum.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "wse_Point2D.h"
#include "wse_Range1D.h"
#include "wse_Size.h"





namespace wse
{
    //!
    //! 
    //! @struct Range2_
    //! 
    //! @brief
    //!     \~japanese 2次元の範囲（矩形）を扱う汎用構造体。
    //!     \~english  Generic struct for handling 2D range (rectangle) data.
    //!
    //! @details
    //!     \~japanese
        //!         また、任意の点が範囲内かどうか、外側かどうかを判定する関数を備える。
    //!     \~english
        //!         Includes methods to determine whether a point is inside or outside the range.
    //!
    //! @note
    //!     \~japanese メンバ変数は直接アクセスされるべきではなく、getter/setterを通して使用すること。
    //!     \~english  Member variables should not be accessed directly; use getter/setter methods.
    //!
    template<typename _Tp>
    struct WSE_API Range2_      final
    {
        typedef _Tp value_type;
        //----------------------------------
        // Member
        //----------------------------------
        private: _Tp m_min_x;    //!< \~english Minimum value on the X axis. \~japanese X軸の最小値.
        private: _Tp m_max_x;    //!< \~english Maximum value on the X axis. \~japanese X軸の最大値.
        private: _Tp m_min_y;    //!< \~english Minimum value on the Y axis. \~japanese Y軸の最小値.
        private: _Tp m_max_y;    //!< \~english Maximum value on the Y axis. \~japanese Y軸の最大値.



        //----------------------------------
        // Private Menber Method
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese メンバー値が正常か検証する。
        //!     \~english  Verifies that member values are valid.
        //!
        //! @note
        //!     \~japanese 異常な場合は例外を投げる。
        //!     \~english  Throws exception if values are invalid.
        //!
        private: void checkMemberSetting( void )
        {
            if( this->m_min_x > this->m_max_x ||
                this->m_min_y > this->m_max_y  )
            {
                throw std::invalid_argument( "the range minimum must not exceed the maximum" );
            }
        }


        //----------------------------------
        // Getter
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese データサイズをバイト単位で取得する。
        //!     \~english  Returns data size in bytes.
        //!
        //! @return Size in bytes.
        //!
        public: uint64_t byte ( void ) const { return sizeof( value_type ) * 4; }

        //!
        //! @brief
        //!     \~japanese 最小X値を取得する。
        //!     \~english  Returns minimum X.
        //!
        //! @return Minimum X value.
        //!
        public: _Tp min_x( void ) const { return this->m_min_x; }

        //!
        //! @brief
        //!     \~japanese 最大X値を取得する。
        //!     \~english  Returns maximum X.
        //!
        //! @return Maximum X value.
        //!
        public: _Tp max_x( void ) const { return this->m_max_x; }

        //!
        //! @brief
        //!     \~japanese 最小Y値を取得する。
        //!     \~english  Returns minimum Y.
        //!
        //! @return Minimum Y value.
        //!
        public: _Tp min_y( void ) const { return this->m_min_y; }

        //!
        //! @brief
        //!     \~japanese 最大Y値を取得する。
        //!     \~english  Returns maximum Y.
        //!
        //! @return Maximum Y value.
        //!
        public: _Tp max_y( void ) const { return this->m_max_y; }

        //!
        //! @brief
        //!     \~japanese 幅を取得する。
        //!     \~english  Returns width.
        //!
        //! @return Width.
        //!
        public: _Tp width ( void ) const { return this->max_x() - this->min_x(); }

        //!
        //! @brief
        //!     \~japanese 高さを取得する。
        //!     \~english  Returns height.
        //!
        //! @return Height.
        //!
        public: _Tp height( void ) const { return this->max_y() - this->min_y(); }

        //!
        //! @brief
        //!     \~japanese サイズを取得する。
        //!     \~english  Returns size object.
        //!
        //! @return Size object.
        //!
        public: wse::Size_<_Tp> size()  const { return  wse::Size_<_Tp>( this->width(), this->height() ); }


        //----------------------------------
        // Setter
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese X, Y各軸の最小・最大値を直接指定して設定する。
        //!     \~english  Sets the range by specifying min/max values for X and Y axes directly.
        //!
        //! @param [in] x_min_in  \~japanese X座標 最小値. \~english Minimum value of X.
        //! @param [in] x_max_in  \~japanese X座標 最大値. \~english Maximum value of X.
        //! @param [in] y_min_in  \~japanese Y座標 最小値. \~english Minimum value of Y.
        //! @param [in] y_max_in  \~japanese Y座標 最大値. \~english Maximum value of Y.
        //!
        public: void setup( const _Tp x_min_in, const _Tp x_max_in, const _Tp y_min_in, const _Tp y_max_in )
        {
            this->m_min_x = x_min_in;
            this->m_max_x = x_max_in;
            this->m_min_y = y_min_in;
            this->m_max_y = y_max_in;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese X座標の最小値を個別に設定する。
        //!     \~english  Sets the minimum value for X axis.
        //!
        //! @param [in] value_in \~japanese 設定する値. \~english Value to set.
        //!
        public: void setMinX( const _Tp value_in )
        {
            this->m_min_x = value_in;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese X座標の最大値を個別に設定する。
        //!     \~english  Sets the maximum value for X axis.
        //!
        //! @param [in] value_in \~japanese 設定する値. \~english Value to set.
        //!
        public: void setMaxX( const _Tp value_in )
        {
            this->m_max_x = value_in;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese Y座標の最小値を個別に設定する。
        //!     \~english  Sets the minimum value for Y axis.
        //!
        //! @param [in] value_in \~japanese 設定する値. \~english Value to set.
        //!
        public: void setMinY( const _Tp value_in )
        {
            this->m_min_y = value_in;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese Y座標の最大値を個別に設定する。
        //!     \~english  Sets the maximum value for Y axis.
        //!
        //! @param [in] value_in \~japanese 設定する値. \~english Value to set.
        //!
        public: void setMaxY( const _Tp value_in )
        {
            this->m_max_y = value_in;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 2点の座標から範囲を設定する。
        //!     \~english  Sets the range from two 2D points (min and max).
        //!
        //! @param [in] minimum_in \~japanese 最小座標. \~english Minimum coordinate point.
        //! @param [in] maximum_in \~japanese 最大座標. \~english Maximum coordinate point.
        //!
        public: void setup( const Point2_< _Tp >& minimum_in, const Point2_< _Tp >& maximum_in )
        {
            this->m_min_x = minimum_in.x;
            this->m_max_x = maximum_in.x;
            this->m_min_y = minimum_in.y;
            this->m_max_y = maximum_in.y;
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 始点座標とサイズから範囲を設定する。
        //!     \~english  Sets the range using a start coordinate and size.
        //!
        //! @param [in] start_in \~japanese 始点座標. \~english Start coordinate.
        //! @param [in] size_in  \~japanese 幅と高さ. \~english Width and height.
        //!
        public: void setup( const Point2_< _Tp >& start_in, const Size_< _Tp >& size_in )
        {
            this->m_min_x = start_in.x;
            this->m_min_y = start_in.y;
            this->m_max_x = start_in.x + size_in.width();
            this->m_max_y = start_in.y + size_in.height();
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 1次元のX/Y範囲から2次元範囲を設定する。
        //!     \~english  Sets the range using 1D ranges for X and Y.
        //!
        //! @param [in] x_range_in \~japanese X軸範囲. \~english Range of X axis.
        //! @param [in] y_range_in \~japanese Y軸範囲. \~english Range of Y axis.
        //!
        public: void setup(
                const Range1_< _Tp > x_range_in
            ,   const Range1_< _Tp > y_range_in
        )
        {
            this->m_min_x = x_range_in.minimum();
            this->m_max_x = x_range_in.maximum();
            this->m_min_y = y_range_in.minimum();
            this->m_max_y = y_range_in.maximum();
            this->checkMemberSetting();
        }


        //----------------------------------
        // Constractor / Destoractor
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。すべての座標を0で初期化する。
        //!     \~english  Default constructor. Initializes all coordinates to 0.
        //!
        public: explicit Range2_( void )
            : m_min_x ( static_cast< _Tp >( 0 ) )
            , m_max_x ( static_cast< _Tp >( 0 ) )
            , m_min_y ( static_cast< _Tp >( 0 ) )
            , m_max_y ( static_cast< _Tp >( 0 ) )
        {}

        //!
        //! @brief
        //!     \~japanese デストラクタ。
        //!     \~english  Destructor.
        //!
        public: ~Range2_( void ) {}

        //!
        //! @brief
        //!     \~japanese 最小/最大のX・Y座標を直接指定するコンストラクタ。
        //!     \~english  Constructor that directly specifies min/max X and Y coordinates.
        //!
        //! @param [in] in_x_min_in \~japanese X座標 最小値. \~english Minimum value of X.
        //! @param [in] in_x_max_in \~japanese X座標 最大値. \~english Maximum value of X.
        //! @param [in] in_y_min_in \~japanese Y座標 最小値. \~english Minimum value of Y.
        //! @param [in] in_y_max_in \~japanese Y座標 最大値. \~english Maximum value of Y.
        //!
        public: Range2_(
                const _Tp in_x_min_in
            ,   const _Tp in_x_max_in
            ,   const _Tp in_y_min_in
            ,   const _Tp in_y_max_in
        )
            : m_min_x ( in_x_min_in )
            , m_max_x ( in_x_max_in )
            , m_min_y ( in_y_min_in )
            , m_max_y ( in_y_max_in )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 2点の座標で矩形を定義するコンストラクタ。
        //!     \~english  Constructor that defines the range from two corner points.
        //!
        //! @param [in] minimum_in \~japanese 最小点. \~english Minimum point.
        //! @param [in] maximum_in \~japanese 最大点. \~english Maximum point.
        //!
        public: Range2_(
                const Point2_< _Tp >& minimum_in
            ,   const Point2_< _Tp >& maximum_in
        )
            : m_min_x ( minimum_in.x )
            , m_max_x ( maximum_in.x )
            , m_min_y ( minimum_in.y )
            , m_max_y ( maximum_in.y )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 始点座標とサイズで矩形を定義するコンストラクタ。
        //!     \~english  Constructor that defines the range using start point and size.
        //!
        //! @param [in] start_in \~japanese 始点. \~english Start point.
        //! @param [in] size_in  \~japanese 幅・高さ. \~english Width and height.
        //!
        public: Range2_(
                const Point2_< _Tp >& start_in
            ,   const Size_< _Tp >& size_in
        )
            : m_min_x ( start_in.x )
            , m_max_x ( start_in.x + size_in.width() )
            , m_min_y ( start_in.y )
            , m_max_y ( start_in.y + size_in.height() )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 1次元のX/Y範囲から2次元範囲を構築する。
        //!     \~english  Constructor from 1D X/Y ranges.
        //!
        //! @param [in] x_range_in \~japanese X軸の範囲. \~english Range for X axis.
        //! @param [in] y_range_in \~japanese Y軸の範囲. \~english Range for Y axis.
        //!
        public: Range2_(
                const Range1_< _Tp > x_range_in
            ,   const Range1_< _Tp > y_range_in
        )
            : m_min_x ( x_range_in.minimum() )
            , m_max_x ( x_range_in.maximum() )
            , m_min_y ( y_range_in.minimum() )
            , m_max_y ( y_range_in.maximum() )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 異なる型の数値を受け取るテンプレートコンストラクタ。
        //!     \~english  Templated constructor for different numeric types.
        //!
        //! @param [in] in_x_min_in \~japanese X座標 最小値. \~english Minimum X.
        //! @param [in] in_x_max_in \~japanese X座標 最大値. \~english Maximum X.
        //! @param [in] in_y_min_in \~japanese Y座標 最小値. \~english Minimum Y.
        //! @param [in] in_y_max_in \~japanese Y座標 最大値. \~english Maximum Y.
        //!
        public: template< typename _inTp > explicit
        Range2_(
                const _inTp in_x_min_in
            ,   const _inTp in_x_max_in
            ,   const _inTp in_y_min_in
            ,   const _inTp in_y_max_in
        )
            : m_min_x ( static_cast< _Tp >( in_x_min_in ) )
            , m_max_x ( static_cast< _Tp >( in_x_max_in ) )
            , m_min_y ( static_cast< _Tp >( in_y_min_in ) )
            , m_max_y ( static_cast< _Tp >( in_y_max_in ) )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 異なる型のPoint2_から構築するテンプレートコンストラクタ。
        //!     \~english  Templated constructor from Point2_ of different type.
        //!
        //! @param [in] minimum_in \~japanese 最小座標. \~english Minimum point.
        //! @param [in] maximum_in \~japanese 最大座標. \~english Maximum point.
        //!
        public: template< typename _inTp > explicit
        Range2_(
                const Point2_< _inTp > minimum_in
            ,   const Point2_< _inTp > maximum_in
        )
            : m_min_x ( static_cast<_Tp>( minimum_in.x ) )
            , m_max_x ( static_cast<_Tp>( maximum_in.x ) )
            , m_min_y ( static_cast<_Tp>( minimum_in.y ) )
            , m_max_y ( static_cast<_Tp>( maximum_in.y ) )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese 型変換付きコピーコンストラクタ。
        //!     \~english  Copy constructor with type conversion.
        //!
        //! @param [in] obj_in \~japanese コピー元のオブジェクト. \~english Source object.
        //!
        public: template< typename _inTp > explicit
        Range2_( const Range2_<_inTp>& obj_in )
            : m_min_x ( static_cast< _Tp >( obj_in.m_min_x ) )
            , m_max_x ( static_cast< _Tp >( obj_in.m_max_x ) )
            , m_min_y ( static_cast< _Tp >( obj_in.m_min_y ) )
            , m_max_y ( static_cast< _Tp >( obj_in.m_max_y ) )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief
        //!     \~japanese コピーコンストラクタ。
        //!     \~english  Copy constructor.
        //!
        //! @param [in] obj_in \~japanese コピー元のオブジェクト. \~english Source object.
        //!
        public: Range2_( const Range2_& obj_in )
            : m_min_x ( obj_in.m_min_x )
            , m_max_x ( obj_in.m_max_x )
            , m_min_y ( obj_in.m_min_y )
            , m_max_y ( obj_in.m_max_y )
        {}

        //!
        //! @brief
        //!     \~japanese ムーブコンストラクタ。
        //!     \~english  Move constructor.
        //!
        //! @param [in,out] obj_inout \~japanese ムーブ元のオブジェクト. \~english Object to move.
        //!
        public: Range2_( Range2_&& obj_inout ) noexcept
            : m_min_x ( std::move( obj_inout.m_min_x ) )
            , m_max_x ( std::move( obj_inout.m_max_x ) )
            , m_min_y ( std::move( obj_inout.m_min_y ) )
            , m_max_y ( std::move( obj_inout.m_max_y ) )
        {}

        //!
        //! @brief
        //!     \~japanese スワップ関数。2つのRange2_オブジェクトの内容を入れ替える。
        //!     \~english  Swap function to exchange contents of two Range2_ objects.
        //!
        //! @param [in,out] obj1_inout \~japanese 一方のオブジェクト. \~english First object.
        //! @param [in,out] obj2_inout \~japanese 他方のオブジェクト. \~english Second object.
        //!
        friend void swap( Range2_<_Tp>& obj1_inout, Range2_<_Tp>& obj2_inout )
        {
            std::swap( obj1_inout.m_min_x, obj2_inout.m_min_x );
            std::swap( obj1_inout.m_max_x, obj2_inout.m_max_x );
            std::swap( obj1_inout.m_min_y, obj2_inout.m_min_y );
            std::swap( obj1_inout.m_max_y, obj2_inout.m_max_y );
        }

        //!
        //! @brief
        //!     \~japanese コピー代入演算子。
        //!     \~english  Copy assignment operator.
        //!
        //! @param [in] obj \~japanese コピー元のオブジェクト. \~english Object to copy from.
        //! @return         コピー後の自身.
        //!
        public: Range2_< _Tp >& operator = ( const Range2_<_Tp>& obj )
        {
            Range2_< _Tp > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese ムーブ代入演算子。
        //!     \~english  Move assignment operator.
        //!
        //! @param [in] obj \~japanese ムーブ元のオブジェクト. \~english Object to move from.
        //! @return         ムーブ後の自身.
        //!
        public: Range2_< _Tp >& operator = ( Range2_<_Tp>&& obj ) noexcept
        {
            Range2_< _Tp > move_obj( std::move( obj ) );
            swap( *this, move_obj );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 等号オペレーター。全ての座標が一致しているかを判定する。
        //!     \~english  Equality operator. Checks if all coordinate values are equal.
        //!
        //! @param [in] data \~japanese 比較対象のオブジェクト. \~english Object to compare.
        //! @retval true     全ての座標が一致している場合.
        //! @retval false    いずれかの座標が異なる場合.
        //!
        public:
        bool operator == ( const Range2_< _Tp >& data ) noexcept
        {
            return ( this->m_min_x == data.min_x() ) && 
                   ( this->m_max_x == data.max_x() ) && 
                   ( this->m_min_y == data.min_y() ) && 
                   ( this->m_max_y == data.max_y() );
        }

        //!
        //! @brief
        //!     \~japanese 不等号オペレーター。いずれかの座標が異なるかを判定する。
        //!     \~english  Inequality operator. Checks if any of the coordinate values differ.
        //!
        //! @param [in] data \~japanese 比較対象のオブジェクト. \~english Object to compare.
        //! @retval true     一部の座標が異なる場合.
        //! @retval false    全ての座標が一致している場合.
        //!
        public:
        bool operator != ( const Range2_< _Tp >& data )
        {
            return !( *this == data );
        }

        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 指定された点が範囲内にあるかを判定する。
        //!     \~english  Checks whether the specified point lies within the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲内.
        //! @retval false 範囲外.
        //!
        public:
        bool isInner( const Point2_<_Tp>& value_in ) const
        {
            return ( ( ( m_min_x <= value_in.x ) && ( value_in.x <= m_max_x ) ) &&
                     ( ( m_min_y <= value_in.y ) && ( value_in.y <= m_max_y ) ) );
        }

        //!
        //! @brief
        //!     \~japanese X座標が範囲内にあるかを判定する。
        //!     \~english  Checks whether the X value is within the range.
        //!
        //! @param [in] value_in \~japanese 判定対象のX座標値. \~english X value to check.
        //! @retval true  範囲内.
        //! @retval false 範囲外.
        //!
        public:
        bool isInner_X( const _Tp value_in ) const
        {
            return ( ( m_min_x <= value_in ) && ( value_in <= m_max_x ) );
        }

        //!
        //! @brief
        //!     \~japanese 点のX座標が範囲内にあるかを判定する。
        //!     \~english  Checks whether the point's X coordinate is within the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲内.
        //! @retval false 範囲外.
        //!
        public:
        bool isInner_X( const Point2_<_Tp>& value_in ) const
        {
            return isInner_X( value_in.x );
        }

        //!
        //! @brief
        //!     \~japanese Y座標が範囲内にあるかを判定する。
        //!     \~english  Checks whether the Y value is within the range.
        //!
        //! @param [in] value_in \~japanese 判定対象のY座標値. \~english Y value to check.
        //! @retval true  範囲内.
        //! @retval false 範囲外.
        //!
        public:
        bool isInner_Y( const _Tp value_in ) const
        {
            return ( ( m_min_y <= value_in ) && ( value_in <= m_max_y ) );
        }

        //!
        //! @brief
        //!     \~japanese 点のY座標が範囲内にあるかを判定する。
        //!     \~english  Checks whether the point's Y coordinate is within the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲内.
        //! @retval false 範囲外.
        //!
        public:
        bool isInner_Y( const Point2_<_Tp>& value_in ) const
        {
            return isInner_Y( value_in.y );
        }

        //!
        //! @brief
        //!     \~japanese 点が範囲外にあるかを判定する。
        //!     \~english  Checks whether the specified point lies outside the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲外.
        //! @retval false 範囲内.
        //!
        public:
        bool isOver( const Point2_<_Tp>& value_in ) const
        {
            return !this->isInner( value_in );
        }

        //!
        //! @brief
        //!     \~japanese X座標が範囲外にあるかを判定する。
        //!     \~english  Checks whether the X value is outside the range.
        //!
        //! @param [in] value_in \~japanese 判定対象のX座標値. \~english X value to check.
        //! @retval true  範囲外.
        //! @retval false 範囲内.
        //!
        public:
        bool isOver_X( const _Tp value_in ) const
        {
            return !this->isInner_X( value_in );
        }

        //!
        //! @brief
        //!     \~japanese 点のX座標が範囲外にあるかを判定する。
        //!     \~english  Checks whether the point's X coordinate is outside the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲外.
        //! @retval false 範囲内.
        //!
        public:
        bool isOver_X( const Point2_<_Tp>& value_in ) const
        {
            return !this->isInner_X( value_in );
        }

        //!
        //! @brief
        //!     \~japanese Y座標が範囲外にあるかを判定する。
        //!     \~english  Checks whether the Y value is outside the range.
        //!
        //! @param [in] value_in \~japanese 判定対象のY座標値. \~english Y value to check.
        //! @retval true  範囲外.
        //! @retval false 範囲内.
        //!
        public:
        bool isOver_Y( const _Tp value_in ) const
        {
            return !this->isInner_Y( value_in );
        }

        //!
        //! @brief
        //!     \~japanese 点のY座標が範囲外にあるかを判定する。
        //!     \~english  Checks whether the point's Y coordinate is outside the range.
        //!
        //! @param [in] value_in \~japanese 判定対象の点. \~english Point to check.
        //! @retval true  範囲外.
        //! @retval false 範囲内.
        //!
        public:
        bool isOver_Y( const Point2_<_Tp>& value_in ) const
        {
            return !this->isInner_Y( value_in );
        }



        //! 
        //! @brief
        //!     \~japanese 文字列に変換する。
        //!     \~english  Convert the 3D range to a formatted string.
        //!
        //! @return String representation of the range.
        //!
        public : std::string str(  )const
        {
            const std::string os =  "X[ " + toString( this->min_x() ) + " <-> " + toString( this->max_x() ) + " ]"
                                 + " Y[ " + toString( this->min_y() ) + " <-> " + toString( this->max_y() ) + " ]";
            return os;
        }
    };

    //!
    //! @brief
    //!     \~japanese Range2_オブジェクトを文字列として出力するストリーム演算子。
    //!     \~english  Stream operator to output Range2_ object as a string.
    //!
    //! @param [in] os   \~japanese 出力ストリーム. \~english Output stream.
    //! @param [in] obj  \~japanese 出力対象オブジェクト. \~english Object to be streamed.
    //! @return 出力ストリーム（連結可能） / Output stream (chained).
    //!
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Range2_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }
    

    //!
    //! @brief
    //!     \~japanese Range2_の型別エイリアス定義。
    //!     \~english  Type alias definitions for Range2_ with specific types.
    //!
    typedef Range2_<  S8 >  S8_RECT;        //!< \~japanese int8型矩形.   \~english int8 rectangle.
    typedef Range2_<  U8 >  U8_RECT;        //!< \~japanese uint8型矩形.  \~english uint8 rectangle.
    typedef Range2_< S16 > S16_RECT;        //!< \~japanese int16型矩形.  \~english int16 rectangle.
    typedef Range2_< U16 > U16_RECT;        //!< \~japanese uint16型矩形. \~english uint16 rectangle.
    typedef Range2_< S32 > S32_RECT;        //!< \~japanese int32型矩形.  \~english int32 rectangle.
    typedef Range2_< U32 > U32_RECT;        //!< \~japanese uint32型矩形. \~english uint32 rectangle.
    typedef Range2_< S64 > S64_RECT;        //!< \~japanese int64型矩形.  \~english int64 rectangle.
    typedef Range2_< U64 > U64_RECT;        //!< \~japanese uint64型矩形. \~english uint64 rectangle.
    typedef Range2_< F32 > F32_RECT;        //!< \~japanese float32型矩形. \~english float32 rectangle.
    typedef Range2_< F64 > F64_RECT;        //!< \~japanese float64型矩形. \~english float64 rectangle.

    typedef Range2_<  S8 >  sint08_rect;    //!< \~japanese 別名sint08.  \~english Alias for sint08.
    typedef Range2_< S16 >  sint16_rect;    //!< \~japanese 別名sint16.  \~english Alias for sint16.
    typedef Range2_< S32 >  sint32_rect;    //!< \~japanese 別名sint32.  \~english Alias for sint32.
    typedef Range2_< S64 >  sint64_rect;    //!< \~japanese 別名sint64.  \~english Alias for sint64.
    typedef Range2_<  U8 >  uint08_rect;    //!< \~japanese 別名uint08.  \~english Alias for uint08.
    typedef Range2_< U16 >  uint16_rect;    //!< \~japanese 別名uint16.  \~english Alias for uint16.
    typedef Range2_< U32 >  uint32_rect;    //!< \~japanese 別名uint32.  \~english Alias for uint32.
    typedef Range2_< U64 >  uint64_rect;    //!< \~japanese 別名uint64.  \~english Alias for uint64.
    typedef Range2_< F32 >  float32_rect;   //!< \~japanese 別名float32. \~english Alias for float32.
    typedef Range2_< F64 >  float64_rect;   //!< \~japanese 別名float64. \~english Alias for float64.
    typedef Range2_< F64 >  double_rect;    //!< \~japanese 別名double.   \~english Alias for double.

};


#endif //WONDERSTEWENGINE_DATA_RANGE2D_H