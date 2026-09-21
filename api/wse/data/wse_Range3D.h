//*****************************************************************************************************************
//! 
//! @file    wse_Range3D.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 3次元の範囲データを扱うRange3_テンプレート構造体の定義。
//!     \~english  Definition of the Range3_ template struct for handling 3D range data.
//!
//! @details
//!     \~japanese
//!         このファイルでは、3次元空間上の最小・最大座標を保持し、範囲内判定などのユーティリティ関数を提供する。
//!         範囲外チェックや文字列変換などの機能も含まれている。
//!     \~english
//!         This file provides the Range3_ struct that holds the minimum and maximum coordinates in 3D space,
//!         and includes utilities such as range checks and string conversion.
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

#ifndef WONDERSTEWENGINE_DATA_RANGE3D_H
#define WONDERSTEWENGINE_DATA_RANGE3D_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../depend/wse_Enum.h"
#include "wse_Point3D.h"
#include "wse_Range1D.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


namespace wse
{
    //! 
    //! @struct Range3_
    //! 
    //! @brief
    //!     \~japanese 3次元の範囲を保持するための構造体。
    //!     \~english  Struct for holding a 3D range.
    //! 
    //! @details
    //!     \~japanese
    //!         各軸（X, Y, Z）の最小値・最大値を保持し、範囲の設定や内外判定などのユーティリティを提供する。
    //!         Point3_型やRange1_型などを引数に取るsetup関数も提供されている。
    //!     \~english
    //!         Stores the minimum and maximum values for each axis (X, Y, Z), and provides utilities for
    //!         setting the range and checking inclusion. Setup functions support Point3_ and Range1_ types.
    //! 
    //! @note
    //!     \~japanese テンプレートパラメータはfloatやintなどの数値型を想定している。
    //!     \~english  The template parameter is expected to be a numeric type like float or int.
    //!
    template<typename _Tp>
    struct WSE_API Range3_      final
    {
        typedef _Tp value_type;
        //----------------------------------
        // Member
        //----------------------------------
        private: _Tp m_min_x  ; //!< \~english Minimum limit on X axis. \~japanese X軸の最小値.
        private: _Tp m_max_x  ; //!< \~english Maximum limit on X axis. \~japanese X軸の最大値.
        private: _Tp m_min_y  ; //!< \~english Minimum limit on Y axis. \~japanese Y軸の最小値.
        private: _Tp m_max_y  ; //!< \~english Maximum limit on Y axis. \~japanese Y軸の最大値.
        private: _Tp m_min_z  ; //!< \~english Minimum limit on Z axis. \~japanese Z軸の最小値.
        private: _Tp m_max_z  ; //!< \~english Maximum limit on Z axis. \~japanese Z軸の最大値.

        //! 
        //! @brief
        //!     \~japanese 2つのRange3_オブジェクトの値を入れ替える。
        //!     \~english  Swap the values between two Range3_ objects.
        //!
        //! @param [in,out] obj1_inout First object to swap.
        //! @param [in,out] obj2_inout Second object to swap.
        //!
        friend void swap( Range3_<_Tp>& obj1_inout, Range3_<_Tp>& obj2_inout )
        {
            std::swap( obj1_inout.m_min_x, obj2_inout.m_min_x );
            std::swap( obj1_inout.m_max_x, obj2_inout.m_max_x );
            std::swap( obj1_inout.m_min_y, obj2_inout.m_min_y );
            std::swap( obj1_inout.m_max_y, obj2_inout.m_max_y );
            std::swap( obj1_inout.m_min_z, obj2_inout.m_min_z );
            std::swap( obj1_inout.m_max_z, obj2_inout.m_max_z );
        }


        //----------------------------------
        // Private Menber Method
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese 内部値の整合性チェックを行い、不正な場合は例外を投げる。
        //!     \~english  Checks member values and throws if invalid.
        //!
        private: void checkMemberSetting( void )
        {
            if( this->m_min_x > this->m_max_x ||
                this->m_min_y > this->m_max_y ||
                this->m_min_z > this->m_max_z )
            {
                throw std::invalid_argument( "the range minimum must not exceed the maximum" );
            }
        }

        //----------------------------------
        // Getter
        //----------------------------------
        //! 
        //! @brief
        //!     \~japanese データサイズ（バイト）を取得する。
        //!     \~english  Get the data size in bytes.
        //!
        //! @return Size in bytes.
        //!
        public: uint64_t byte ( void ) const { return sizeof( value_type ) * 3; }

        //! 
        //! @brief
        //!     \~japanese 最小X座標値を取得する。
        //!     \~english  Get the minimum X coordinate.
        //!
        //! @return Minimum X value.
        //!
        public: const _Tp min_x( void ) const { return this->m_min_x; }

        //! 
        //! @brief
        //!     \~japanese 最大X座標値を取得する。
        //!     \~english  Get the maximum X coordinate.
        //!
        //! @return Maximum X value.
        //!
        public: const _Tp max_x( void ) const { return this->m_max_x; }

        //! 
        //! @brief
        //!     \~japanese 最小Y座標値を取得する。
        //!     \~english  Get the minimum Y coordinate.
        //!
        //! @return Minimum Y value.
        //!
        public: const _Tp min_y( void ) const { return this->m_min_y; }

        //! 
        //! @brief
        //!     \~japanese 最大Y座標値を取得する。
        //!     \~english  Get the maximum Y coordinate.
        //!
        //! @return Maximum Y value.
        //!
        public: const _Tp max_y( void ) const { return this->m_max_y; }

        //! 
        //! @brief
        //!     \~japanese 最小Z座標値を取得する。
        //!     \~english  Get the minimum Z coordinate.
        //!
        //! @return Minimum Z value.
        //!
        public: const _Tp min_z( void ) const { return this->m_min_z; }

        //! 
        //! @brief
        //!     \~japanese 最大Z座標値を取得する。
        //!     \~english  Get the maximum Z coordinate.
        //!
        //! @return Maximum Z value.
        //!
        public: const _Tp max_z( void ) const { return this->m_max_z; }

        //----------------------------------
        // Setter
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese 範囲を各軸の最小最大値で設定する。
        //!     \~english  Set the range using minimum and maximum values for each axis.
        //!
        //! @param[in] x_min_in Minimum value of X axis.
        //! @param[in] x_max_in Maximum value of X axis.
        //! @param[in] y_min_in Minimum value of Y axis.
        //! @param[in] y_max_in Maximum value of Y axis.
        //! @param[in] z_min_in Minimum value of Z axis.
        //! @param[in] z_max_in Maximum value of Z axis.
        //!
        public: void setup( const _Tp x_min_in, const _Tp x_max_in, const _Tp y_min_in, const _Tp y_max_in, const _Tp z_min_in, const _Tp z_max_in )
        { 
            this->m_min_x = x_min_in;
            this->m_max_x = x_max_in;
            this->m_min_y = y_min_in;
            this->m_max_y = y_max_in;
            this->m_min_z = z_min_in;
            this->m_max_z = z_max_in;
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 最小・最大点のPoint3_で範囲を設定する。
        //!     \~english  Set the range using minimum and maximum Point3_ values.
        //!
        //! @param[in] minimum_in Minimum coordinate.
        //! @param[in] maximum_in Maximum coordinate.
        //!
        public: void setup( const Point3_< _Tp >& minimum_in, const Point3_< _Tp >& maximum_in )
        {
            this->m_min_x = minimum_in.x;
            this->m_max_x = maximum_in.x;
            this->m_min_y = minimum_in.y;
            this->m_max_y = maximum_in.y;
            this->m_min_z = minimum_in.z;
            this->m_max_z = maximum_in.z;
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 各軸の1次元範囲で設定する。
        //!     \~english  Set the 3D range using 1D ranges for each axis.
        //!
        //! @param[in] x_range_in X axis range.
        //! @param[in] y_range_in Y axis range.
        //! @param[in] z_range_in Z axis range.
        //!
        public: void setup(
                const Range1_< _Tp > x_range_in
            ,   const Range1_< _Tp > y_range_in
            ,   const Range1_< _Tp > z_range_in
        )
        {
            this->m_min_x = x_range_in.minimum();
            this->m_max_x = x_range_in.maximum();
            this->m_min_y = y_range_in.minimum();
            this->m_max_y = y_range_in.maximum();
            this->m_min_z = z_range_in.minimum();
            this->m_max_z = z_range_in.maximum();
            this->checkMemberSetting();
        }


        //----------------------------------
        // Constractor / Destoractor
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。
        //!     \~english  Default constructor.
        //!
        public: explicit Range3_( void ) 
            : m_min_x ( static_cast< _Tp >( 0 ) )
            , m_max_x ( static_cast< _Tp >( 0 ) )
            , m_min_y ( static_cast< _Tp >( 0 ) )
            , m_max_y ( static_cast< _Tp >( 0 ) )
            , m_min_z ( static_cast< _Tp >( 0 ) )
            , m_max_z ( static_cast< _Tp >( 0 ) )
        {
        }
        //! 
        //! @brief
        //!     \~japanese デストラクタ。
        //!     \~english  Destructor.
        //!
        public: ~Range3_( void ) {}

        //! 
        //! @brief
        //!     \~japanese 各座標範囲を指定して初期化するコンストラクタ。
        //!     \~english  Constructor that initializes with each coordinate range.
        //!
        //! @param [in] in_x_min_in Minimum X value.
        //! @param [in] in_x_max_in Maximum X value.
        //! @param [in] in_y_min_in Minimum Y value.
        //! @param [in] in_y_max_in Maximum Y value.
        //! @param [in] in_z_min_in Minimum Z value.
        //! @param [in] in_z_max_in Maximum Z value.
        //!
        public: Range3_( 
                const _Tp in_x_min_in
            ,   const _Tp in_x_max_in
            ,   const _Tp in_y_min_in
            ,   const _Tp in_y_max_in
            ,   const _Tp in_z_min_in
            ,   const _Tp in_z_max_in
        )
            : m_min_x ( in_x_min_in )
            , m_max_x ( in_x_max_in )
            , m_min_y ( in_y_min_in )
            , m_max_y ( in_y_max_in )
            , m_min_z ( in_z_min_in )
            , m_max_z ( in_z_max_in )
        { 
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese Point3_で初期化するコンストラクタ。
        //!     \~english  Constructor with Point3_ min/max.
        //!
        //! @param [in] minimum_in Minimum Point3_.
        //! @param [in] maximum_in Maximum Point3_.
        //!
        public: Range3_( 
                const Point3_< _Tp > minimum_in
            ,   const Point3_< _Tp > maximum_in
        )
            : m_min_x ( minimum_in.x )
            , m_max_x ( maximum_in.x )
            , m_min_y ( minimum_in.y )
            , m_max_y ( maximum_in.y )
            , m_min_z ( minimum_in.z )
            , m_max_z ( maximum_in.z )
        {
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese Range1_で初期化するコンストラクタ。
        //!     \~english  Constructor with Range1_ x/y/z.
        //!
        //! @param [in] x_range_in X range.
        //! @param [in] y_range_in Y range.
        //! @param [in] z_range_in Z range.
        //!
        public: Range3_( 
                const Range1_< _Tp > x_range_in
            ,   const Range1_< _Tp > y_range_in
            ,   const Range1_< _Tp > z_range_in
        )
            : m_min_x ( x_range_in.minimum() )
            , m_max_x ( x_range_in.maximum() )
            , m_min_y ( y_range_in.minimum() )
            , m_max_y ( y_range_in.maximum() )
            , m_min_z ( z_range_in.minimum() )
            , m_max_z ( z_range_in.maximum() )
        {
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 型キャスト用コンストラクタ。
        //!     \~english  Constructor for type casting.
        //!
        //! @param [in] obj_in Source object to cast from.
        //!
        public: template< typename _inTp > explicit
            Range3_( const Range3_<_inTp>&obj_in )
            : m_min_x ( static_cast< _Tp >( obj_in.m_min_x ) )
            , m_max_x ( static_cast< _Tp >( obj_in.m_max_x ) )
            , m_min_y ( static_cast< _Tp >( obj_in.m_min_y ) )
            , m_max_y ( static_cast< _Tp >( obj_in.m_max_y ) )
            , m_min_z ( static_cast< _Tp >( obj_in.m_minimum.z ) )
            , m_max_z ( static_cast< _Tp >( obj_in.m_maximum.z ) )
        {
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 型キャスト用コンストラクタ（Point3_型）。
        //!     \~english  Constructor for Point3_ type casting.
        //!
        //! @param [in] minimum_in Minimum Point3_.
        //! @param [in] maximum_in Maximum Point3_.
        //!
        public: template< typename _inTp > explicit 
        Range3_(
                const Point3_< _inTp > minimum_in
            ,   const Point3_< _inTp > maximum_in
        )
            : m_min_x ( static_cast<_Tp>( minimum_in.x ) )
            , m_max_x ( static_cast<_Tp>( maximum_in.x ) )
            , m_min_y ( static_cast<_Tp>( minimum_in.y ) )
            , m_max_y ( static_cast<_Tp>( maximum_in.y ) )
            , m_min_z ( static_cast<_Tp>( minimum_in.z ) )
            , m_max_z ( static_cast<_Tp>( maximum_in.z ) )
        {
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 型キャスト用コンストラクタ（数値）。
        //!     \~english  Constructor for numeric casting.
        //!
        //! @param [in] in_x_min_in Minimum X value.
        //! @param [in] in_x_max_in Maximum X value.
        //! @param [in] in_y_min_in Minimum Y value.
        //! @param [in] in_y_max_in Maximum Y value.
        //! @param [in] in_z_min_in Minimum Z value.
        //! @param [in] in_z_max_in Maximum Z value.
        //!
        public: template< typename _inTp > explicit 
        Range3_( 
                const _inTp in_x_min_in
            ,   const _inTp in_x_max_in
            ,   const _inTp in_y_min_in
            ,   const _inTp in_y_max_in
            ,   const _inTp in_z_min_in
            ,   const _inTp in_z_max_in
        ) 
            : m_min_x ( static_cast< _Tp >( in_x_min_in ) )
            , m_max_x ( static_cast< _Tp >( in_x_max_in ) )
            , m_min_y ( static_cast< _Tp >( in_y_min_in ) )
            , m_max_y ( static_cast< _Tp >( in_y_max_in ) )
            , m_min_z ( static_cast< _Tp >( in_z_min_in ) )
            , m_max_z ( static_cast< _Tp >( in_z_max_in ) )
        {
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese コピーコンストラクタ。
        //!     \~english  Copy constructor.
        //!
        //! @param [in] obj_in Source object.
        //!
        public: Range3_( const Range3_& obj_in )
            : m_min_x ( obj_in.m_min_x )
            , m_max_x ( obj_in.m_max_x )
            , m_min_y ( obj_in.m_min_y )
            , m_max_y ( obj_in.m_max_y )
            , m_min_z ( obj_in.m_min_z )
            , m_max_z ( obj_in.m_max_z )
        {
        }
        //! 
        //! @brief
        //!     \~japanese ムーブコンストラクタ。
        //!     \~english  Move constructor.
        //!
        //! @param [in] obj_inout Source object.
        //!
        public:
        Range3_( Range3_&& obj_inout )noexcept
            : m_min_x ( std::move( obj_inout.m_min_x ) )
            , m_max_x ( std::move( obj_inout.m_max_x ) )
            , m_min_y ( std::move( obj_inout.m_min_y ) )
            , m_max_y ( std::move( obj_inout.m_max_y ) )
            , m_min_z ( std::move( obj_inout.m_min_z ) )
            , m_max_z ( std::move( obj_inout.m_max_z ) )
        {
        }
        //! 
        //! @brief
        //!     \~japanese コピー代入演算子。
        //!     \~english  Copy assignment operator.
        //!
        //! @param [in] obj Source object.
        //! @return Reference to self.
        //!
        public:
        Range3_< _Tp >& operator = ( const Range3_<_Tp>& obj )
        {
            Range3_< _Tp > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //! 
        //! @brief
        //!     \~japanese ムーブ代入演算子。
        //!     \~english  Move assignment operator.
        //!
        //! @param [in] obj Source object.
        //! @return Reference to self.
        //!
        public:
        Range3_< _Tp >& operator = ( Range3_<_Tp>&& obj )
        {
            Range3_< _Tp > move_obj( std::move( obj ) );
            swap( *this, move_obj );
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
        bool operator == ( const Range3_< _Tp >& data )
        {
            return ( this->m_min_x == data.min_x() ) && 
                   ( this->m_max_x == data.max_x() ) && 
                   ( this->m_min_y == data.min_y() ) && 
                   ( this->m_max_y == data.max_y() ) && 
                   ( this->m_min_z == data.min_z() ) && 
                   ( this->m_max_z == data.max_z() ) ;
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
        bool operator != ( const Range3_< _Tp >& data )
        {
            return !( *this == data );
        }


        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //! 
        //! @brief
        //!     \~japanese 範囲内かを判定。
        //!     \~english  Check if point is within range.
        //!
        //! @param [in] value_in Point3_ value to test.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        public:
        bool isInner( const Point3_<_Tp>& value_in ) const
        { 
            return ( ( ( this->m_min_x <= value_in.x ) && ( value_in.x <= this->m_max_x ) ) &&
                     ( ( this->m_min_y <= value_in.y ) && ( value_in.y <= this->m_max_y   ) ) &&
                     ( ( this->m_min_z <= value_in.z ) && ( value_in.z <= this->m_max_z ) ) );
        }
        //! 
        //! @brief
        //!     \~japanese X座標が範囲内かを判定する。
        //!     \~english  Check whether X value is within range.
        //!
        //! @param [in] value_in X value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_X( const _Tp value_in ) const { return ( ( m_min_x <= value_in ) && ( value_in <= m_max_x ) ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のX座標が範囲内かを判定する。
        //!     \~english  Check whether X component of Point3_ is within range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_X( const Point3_<_Tp>& value_in ) const { return isInner_X( value_in.x ); }

        //! 
        //! @brief
        //!     \~japanese Y座標が範囲内かを判定する。
        //!     \~english  Check whether Y value is within range.
        //!
        //! @param [in] value_in Y value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_Y( const _Tp value_in ) const { return ( ( m_min_y <= value_in ) && ( value_in <= m_max_y ) ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のY座標が範囲内かを判定する。
        //!     \~english  Check whether Y component of Point3_ is within range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_Y( const Point3_<_Tp>& value_in ) const { return isInner_Y( value_in.y ); }

        //! 
        //! @brief
        //!     \~japanese Z座標が範囲内かを判定する。
        //!     \~english  Check whether Z value is within range.
        //!
        //! @param [in] value_in Z value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_Z( const _Tp value_in ) const { return ( ( m_min_z <= value_in ) && ( value_in <= m_max_z ) ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のZ座標が範囲内かを判定する。
        //!     \~english  Check whether Z component of Point3_ is within range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Within range.
        //! @retval false Out of range.
        //!
        bool isInner_Z( const Point3_<_Tp>& value_in ) const { return isInner_Z( value_in.z ); }

        //! 
        //! @brief
        //!     \~japanese 範囲外かを判定する。
        //!     \~english  Check whether the point is outside the range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver( const Point3_<_Tp>& value_in ) const { return !this->isInner( value_in ); }

        //! 
        //! @brief
        //!     \~japanese X座標が範囲外かを判定する。
        //!     \~english  Check whether X value is out of range.
        //!
        //! @param [in] value_in X value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_X( const _Tp value_in ) const { return !this->isInner_X( value_in ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のX座標が範囲外かを判定する。
        //!     \~english  Check whether X component of Point3_ is out of range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_X( const Point3_<_Tp>& value_in ) const { return !this->isInner_X( value_in ); }

        //! 
        //! @brief
        //!     \~japanese Y座標が範囲外かを判定する。
        //!     \~english  Check whether Y value is out of range.
        //!
        //! @param [in] value_in Y value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_Y( const _Tp value_in ) const { return !this->isInner_Y( value_in ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のY座標が範囲外かを判定する。
        //!     \~english  Check whether Y component of Point3_ is out of range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_Y( const Point3_<_Tp>& value_in ) const { return !this->isInner_Y( value_in ); }

        //! 
        //! @brief
        //!     \~japanese Z座標が範囲外かを判定する。
        //!     \~english  Check whether Z value is out of range.
        //!
        //! @param [in] value_in Z value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_Z( const _Tp value_in ) const { return !this->isInner_Z( value_in ); }

        //! 
        //! @brief
        //!     \~japanese Point3_のZ座標が範囲外かを判定する。
        //!     \~english  Check whether Z component of Point3_ is out of range.
        //!
        //! @param [in] value_in Point3_ value.
        //! @retval true  Out of range.
        //! @retval false Within range.
        //!
        bool isOver_Z( const Point3_<_Tp>& value_in ) const { return !this->isInner_Z( value_in ); }

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
                                 + " Y[ " + toString( this->min_y() ) + " <-> " + toString( this->max_y() ) + " ]"
                                 + " Z[ " + toString( this->min_z() ) + " <-> " + toString( this->max_z() ) + " ]";
            return os;
        }
    };

    //! 
    //! @brief
    //!     \~japanese Range3_オブジェクトを文字列として出力する。
    //!     \~english  Output the Range3_ object as a string.
    //! 
    //! @param [in] os   Outout stream
    //! @param [in] obj  Range3_object.
    //! @return stream
    //!
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Range3_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }

    typedef Range3_<  S8 > S8_CUBOID;        //!< \~english Alias for signed 8-bit 3D range. \~japanese int8_t の3次元範囲型.
    typedef Range3_<  U8 > U8_CUBOID;        //!< \~english Alias for unsigned 8-bit 3D range. \~japanese uint8_t の3次元範囲型.
    typedef Range3_< S16 > S16_CUBOID;       //!< \~english Alias for signed 16-bit 3D range. \~japanese int16_t の3次元範囲型.
    typedef Range3_< U16 > U16_CUBOID;       //!< \~english Alias for unsigned 16-bit 3D range. \~japanese uint16_t の3次元範囲型.
    typedef Range3_< S32 > S32_CUBOID;       //!< \~english Alias for signed 32-bit 3D range. \~japanese int32_t の3次元範囲型.
    typedef Range3_< U32 > U32_CUBOID;       //!< \~english Alias for unsigned 32-bit 3D range. \~japanese uint32_t の3次元範囲型.
    typedef Range3_< S64 > S64_CUBOID;       //!< \~english Alias for signed 64-bit 3D range. \~japanese int64_t の3次元範囲型.
    typedef Range3_< U64 > U64_CUBOID;       //!< \~english Alias for unsigned 64-bit 3D range. \~japanese uint64_t の3次元範囲型.
    typedef Range3_< F32 > F32_CUBOID;       //!< \~english Alias for float 3D range.         \~japanese float の3次元範囲型.
    typedef Range3_< F64 > F64_CUBOID;       //!< \~english Alias for double 3D range.        \~japanese double の3次元範囲型.

    typedef Range3_<  S8 >  sint08_cuboid;   //!< \~english Alias for int8 3D range. \~japanese int8_t の3次元範囲型.
    typedef Range3_< S16 >  sint16_cuboid;   //!< \~english Alias for int16 3D range. \~japanese int16_t の3次元範囲型.
    typedef Range3_< S32 >  sint32_cuboid;   //!< \~english Alias for int32 3D range. \~japanese int32_t の3次元範囲型.
    typedef Range3_< S64 >  sint64_cuboid;   //!< \~english Alias for int64 3D range. \~japanese int64_t の3次元範囲型.
    typedef Range3_<  U8 >  uint08_cuboid;   //!< \~english Alias for uint8 3D range. \~japanese uint8_t の3次元範囲型.
    typedef Range3_< U16 >  uint16_cuboid;   //!< \~english Alias for uint16 3D range. \~japanese uint16_t の3次元範囲型.
    typedef Range3_< U32 >  uint32_cuboid;   //!< \~english Alias for uint32 3D range. \~japanese uint32_t の3次元範囲型.
    typedef Range3_< U64 >  uint64_cuboid;   //!< \~english Alias for uint64 3D range. \~japanese uint64_t の3次元範囲型.
    typedef Range3_< F32 > float32_cuboid;   //!< \~english Alias for float 3D range.  \~japanese float の3次元範囲型.
    typedef Range3_< F64 > float64_cuboid;   //!< \~english Alias for double 3D range. \~japanese double の3次元範囲型.
    typedef Range3_< F64 >  double_cuboid;   //!< \~english Alias for double 3D range. \~japanese double の3次元範囲型.


};


#endif //WONDERSTEWENGINE_DATA_RANGE3D_H