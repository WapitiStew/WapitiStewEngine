//*****************************************************************************************************************
//! 
//! @file    wse_Point4D.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 4次元ベクトル・座標データの操作を提供する構造体とユーティリティを定義するファイル.
//!     \~english  Defines struct and utilities for handling 4D vector/coordinate operations.
//!
//! @details
//!     \~japanese
//!         本ファイルでは、4次元座標を表現するテンプレート構造体 `Point4_` を定義し、加減算、正規化、各種距離の算出、
//!     @n テンソル積・行列変換など、様々な数学的操作を提供する。
//!     @n 対応型のtypedefエイリアスも含まれる。
//!
//!     \~english
//!         This file defines a template struct `Point4_` for representing 4D coordinates,
//!     @n offering a wide range of mathematical operations such as addition, subtraction, normalization,
//!     @n distance computations, tensor product, and matrix transformations.
//!     @n Type aliases for various supported types are also included.
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

#ifndef WONDERSTEWENGINE_DATA_POINT4D_H
#define WONDERSTEWENGINE_DATA_POINT4D_H


// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#pragma warning(disable: 4820)  // メンバー変数の定義した時にアライメント調整用のスペースで発生する警告除去.
#endif

// include files
#include <stdexcept>

#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../utility/wse_Math.h"
#include "wse_Matrix.h"


namespace wse
{

    //!
    //! @struct Point4_
    //!
    //! @brief
    //!     \~japanese 4次元のベクトル・座標データを扱う汎用構造体。
    //!     \~english  Generic struct for representing 4D vector or coordinate data.
    //!
    //! @details
    //!     \~japanese
    //!         X, Y, Z, W 各成分を保持し、ベクトル演算、ノルム計算、正規化、距離測定、行列変換などの操作を提供する。
    //!     @n 型はテンプレート引数として与えられ、整数・浮動小数点のいずれにも対応する。
    //!
    //!     \~english
    //!         Holds components X, Y, Z, W and provides operations such as vector arithmetic, norm calculation,
    //!     @n normalization, distance measurement, and matrix transformation.
    //!     @n The type is given as a template parameter and supports both integer and floating-point types.
    //!
    //! @note
    //!     \~japanese テンプレート型として任意の数値型が使用可能。
    //!     \~english  Any numeric type can be used as template argument.
    //!
    template<typename _Tp>
    struct WSE_API Point4_      final
    {

        typedef _Tp value_type; //!< \~japanese 型エイリアス：内部で使用されるスカラー型。 \~english Type alias: scalar value type used internally.

        //----------------------------------
        // Member
        //----------------------------------
        public: _Tp x; //!< \~japanese X座標。 \~english X coordinate.
        public: _Tp y; //!< \~japanese Y座標。 \~english Y coordinate.
        public: _Tp z; //!< \~japanese Z座標。 \~english Z coordinate.
        public: _Tp w; //!< \~japanese W座標。 \~english W coordinate.

        //!
        //! @brief
        //!     \~japanese データのバイトサイズを返す。
        //!     \~english  Returns the byte size of the data.
        //!
        //! @return
        //!     \~english Total number of bytes.
        //!
        public: uint64_t byte( void )const { return sizeof( value_type ) * 4; }

        //!
        //! @brief  Default constructor.
        //!
        public: explicit Point4_( void ) 
            : x ( static_cast< _Tp >( 0 ) )
            , y ( static_cast< _Tp >( 0 ) )
            , z ( static_cast< _Tp >( 0 ) )
            , w ( static_cast< _Tp >( 0 ) )
        {
        }
        //!
        //! @brief デストラクタ.
        //! 
        public: ~Point4_( void ) {}

        //!
        //! @brief
        //!     \~japanese 指定された各座標値で初期化するコンストラクタ。
        //!     \~english  Constructor that initializes with specified coordinates.
        //!
        //! @param [in] x_in X coordinate.
        //! @param [in] y_in Y coordinate.
        //! @param [in] z_in Z coordinate.
        //! @param [in] w_in W coordinate.
        //!
        public: Point4_( const _Tp x_in, const _Tp y_in, const _Tp z_in, const _Tp w_in )
            : x ( x_in )
            , y ( y_in )
            , z ( z_in )
            , w ( w_in ) {}

        //!
        //! @brief
        //!     \~japanese 異なる型の Point4_ からキャスト付きで初期化するコンストラクタ。
        //!     \~english  Constructor with type cast from another Point4_ of different type.
        //!
        //! @param [in] x_in X coordinate.
        //! @param [in] y_in Y coordinate.
        //! @param [in] z_in Z coordinate.
        //! @param [in] w_in W coordinate.
        //!
        public: template< typename _inTp > explicit Point4_( const _inTp x_in, const _inTp y_in, const _inTp z_in, const _inTp w_in )
            : x ( static_cast< _Tp >( x_in ) )
            , y ( static_cast< _Tp >( y_in ) )
            , z ( static_cast< _Tp >( z_in ) )
            , w ( static_cast< _Tp >( w_in ) )
        {
        }
        //!
        //! @brief
        //!     \~japanese 異なる型の Point4_ からキャスト付きで初期化するコンストラクタ。
        //!     \~english  Constructor with type cast from another Point4_ of different type.
        //!
        //! @param [in] obj_in Input object.
        //!
        public: template< typename _inTp > explicit Point4_( const Point4_<_inTp>& obj_in ) 
            : x ( static_cast< _Tp >( obj_in.x ) )
            , y ( static_cast< _Tp >( obj_in.y ) )
            , z ( static_cast< _Tp >( obj_in.z ) )
            , w ( static_cast< _Tp >( obj_in.w ) ){}

        //!
        //! @brief  Copy constructor.
        //!
        //! @param [in] obj_in Source object.
        //!
        public: Point4_( const Point4_& obj_in )
            : x ( obj_in.x )
            , y ( obj_in.y )
            , z ( obj_in.z )
            , w ( obj_in.w ) {}

        //!
        //! @brief  Copy constructor.
        //!
        //! @param [in] obj_inout Source object.
        //!
        public:
        Point4_( Point4_&& obj_inout )noexcept
            : x ( std::move( obj_inout.x ) )
            , y ( std::move( obj_inout.y ) )
            , z ( std::move( obj_inout.z ) )
            , w ( std::move( obj_inout.w ) ) {}

        //!
        //! @brief
        //!     \~japanese 2つのPoint4_オブジェクトの内容を交換する。
        //!     \~english  Swaps the contents of two Point4_ objects.
        //!
        //! @param [in,out] obj1_inout First object.
        //! @param [in,out] obj2_inout Second object.
        //!
        friend void swap( Point4_<_Tp>& obj1_inout, Point4_<_Tp>& obj2_inout )
        {
            std::swap( obj1_inout.x, obj2_inout.x );
            std::swap( obj1_inout.y, obj2_inout.y );
            std::swap( obj1_inout.z, obj2_inout.z );
            std::swap( obj1_inout.w, obj2_inout.w );
        }

        //!
        //! @brief
        //!     \~japanese 代入演算子（コピー）: 他のPoint4_オブジェクトの内容をコピーする。
        //!     \~english  Copy assignment operator: copies contents from another Point4_ object.
        //!
        //! @param [in] obj Source object.
        //! @return Reference to this object.
        //!
        public:
        Point4_< _Tp >& operator = ( const Point4_<_Tp>& obj )
        {
            Point4_< _Tp > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 代入演算子（ムーブ）: 他のPoint4_オブジェクトの内容をムーブする。
        //!     \~english  Move assignment operator: moves contents from another Point4_ object.
        //!
        //! @param [in] obj Source object.
        //! @return Reference to this object.
        //!
        public:
        Point4_< _Tp >& operator = ( Point4_<_Tp>&& obj )noexcept
        {
            Point4_< _Tp > move_obj( std::move( obj ) );
            swap( *this, move_obj );
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 等号演算子：2つのPoint4_オブジェクトが等しいかを判定する。
        //!     \~english  Equality operator: checks if two Point4_ objects are equal.
        //!
        //! @param [in] data Object to compare.
        //! @retval true     All components are equal.
        //! @retval false    Any component differs.
        //!
        public:
        bool operator == ( const Point4_< _Tp >& data )
        {
            return ( this->x == data.x ) && ( this->y == data.y ) && ( this->z == data.z ) && ( this->w == data.w );
        }

        //!
        //! @brief
        //!     \~japanese 不等号演算子：2つのPoint4_オブジェクトが異なるかを判定する。
        //!     \~english  Inequality operator: checks if two Point4_ objects are not equal.
        //!
        //! @param [in] data Object to compare.
        //! @retval true     Any component differs.
        //! @retval false    All components are equal.
        //!
        public:
        bool operator != ( const Point4_< _Tp >& data )
        {
            return !( *this == data );
        }

        //!
        //! @brief
        //!     \~japanese 加算代入演算子：各成分を他のPoint4_オブジェクトと加算する。
        //!     \~english  Addition assignment operator: adds each component from another Point4_ object.
        //!
        //! @param [in] obj Object to add.
        //! @return Reference to this object.
        //!
        public:
        Point4_< _Tp >& operator += ( const Point4_<_Tp>& obj )
        {
            this->x += obj.x;
            this->y += obj.y;
            this->z += obj.z;
            this->w += obj.w;
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 減算代入演算子：各成分を他のPoint4_オブジェクトと減算する。
        //!     \~english  Subtraction assignment operator: subtracts each component from another Point4_ object.
        //!
        //! @param [in] obj Object to subtract.
        //! @return Reference to this object.
        //!
        public:
        Point4_< _Tp >& operator -= ( const Point4_<_Tp>& obj )
        {
            this->x -= obj.x;
            this->y -= obj.y;
            this->z -= obj.z;
            this->w -= obj.w;
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 乗算代入演算子：各成分をスカラー値と乗算する。
        //!     \~english  Multiplication assignment operator: multiplies each component by a scalar value.
        //!
        //! @param [in] scalar Scalar value to multiply.
        //! @return Reference to this object.
        //!
        public:
        Point4_< _Tp >& operator *= ( const _Tp scalar )
        {
            this->x *= scalar;
            this->y *= scalar;
            this->z *= scalar;
            this->w *= scalar;
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 除算代入演算子：各成分をスカラー値で除算する。
        //!     \~english  Division assignment operator: divides each component by a scalar value.
        //!
        //! @param [in] scalar Scalar value to divide by.
        //! @return Reference to this object.
        //!
        //! @note
        //!     \~japanese スカラーが0の場合は例外を投げる。
        //!     \~english  Throws exception if scalar is zero.
        //!
        public:
        Point4_< _Tp >& operator /= ( const _Tp scalar )
        {
            if( scalar == _Tp( 0 ) )
            {
                throw std::domain_error( "division by zero" );
            }

            this->x /= scalar;
            this->y /= scalar;
            this->z /= scalar;
            this->w /= scalar;
            return *this;
        }

        //--------------------------------------------------------------------------------
        // 上位概念のデータに変換する.
        //--------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese このベクトルを列ベクトルとして4x1行列に変換する。
        //!     \~english  Converts this vector into a 4x1 column matrix.
        //!
        //! @return Matrix containing [x, y, z, w] as a column.
        //!
        //! @note
        //!     Point           Matrix
        //!                       | X |
        //!      [ X, Y, Z ]  =>  | Y |
        //!                       | Z |
        //!     
        public: Matrix_< double > matrix( void ) const
        {
            const std::vector< double > elements = { 
                        static_cast<double>( this->x )
                    ,   static_cast<double>( this->y ) 
                    ,   static_cast<double>( this->z ) 
                    ,   static_cast<double>( this->w ) 
                };
            return Matrix_< double >( 4, 1, elements );
        }

        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 指定されたPoint4_との内積（ドット積）を計算する。
        //!     \~english  Calculates the dot product with another Point4_.
        //!
        //! @param [in] obj_in Target vector.
        //! @return Dot product value.
        //!
        public: _Tp dot( const Point4_<_Tp>& obj_in ) const
        {
            const Point4_<double> src_obj = Point4_<double>( obj_in );
            const Point4_<double> this_obj = Point4_<double>( *this );
            return static_cast< _Tp >( ( this_obj.x * src_obj.x )
                                     + ( this_obj.y * src_obj.y )
                                     + ( this_obj.z * src_obj.z )
                                     + ( this_obj.w * src_obj.w ) );
        }

        //!
        //! @brief
        //!     \~japanese 指定されたPoint4_とのテンソル積（外積）を計算する。
        //!     \~english  Calculates the tensor product (outer product) with another Point4_.
        //!
        //! @param [in] obj_in Target vector.
        //! @return Tensor product matrix.
        //!
        public: Matrix_<_Tp > tensor( const Point4_<_Tp>& obj_in ) const
        {
            const Matrix src_obj  = Point4_<double>(   obj_in ).matrix();
            const Matrix this_obj = Point4_<double>( *this ).matrix();

            Matrix ret( 4*4, 1, 0 );
            for( size_t this_row = 0; this_row < this_obj.rows(); ++this_row )
            {
                for( size_t src_row = 0; src_row < src_obj.rows(); ++src_row )
                {
                    ret[ ( this_row * this_obj.rows() ) + src_row ][0] = this_obj[this_row][0] * src_obj[ src_row ][0];
                }
            }

            std::vector<_Tp> converted;
            for( size_t i = 0; i < ret.size(); ++i )
            {
                converted.emplace_back( static_cast< _Tp >( ret[ i ][ 0 ] ) );
            }
            // 変換後のデータから Matrix_<_Tp> を生成
            return Matrix_<_Tp>( ret.rows(), ret.cols(), converted );

        }

        //!
        //! @brief
        //!     \~japanese このベクトルを正規化して返す。
        //!     \~english  Returns the normalized vector.
        //!
        //! @param [in] target_in Target norm value (default = 1.0).
        //! @return Normalized vector.
        //!
        //! @note
        //!     \~japanese 初期値のままなら正規化を行わずそのまま返す。
        //!     \~english  If the vector is zero, it returns itself without normalization.
        //!
        public: Point4_<_Tp> normalize( const double target_in = 1.0 ) const
        {
            if( this->x == 0 && this->y == 0 && this->z == 0 ) { return *this; }

            Point4_<double> obj = Point4_<double>( *this );
            const double coef = std::sqrt( ( target_in * target_in ) 
                                         / ( ( obj.x * obj.x ) + ( obj.y * obj.y ) + ( obj.z * obj.z ) ) );
            return Point4_<_Tp>( ( coef * obj.x ), ( coef * obj.y ), ( coef * obj.z ), ( coef * obj.w ) );
        }

        //!
        //! @brief
        //!     \~japanese マンハッタン距離（L1距離）を計算する。
        //!     \~english  Calculates Manhattan distance (L1 distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return Manhattan distance.
        //!
        public: _Tp manhattan_distance( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const
        {
            const Point4_<double> src_obj = Point4_<double>( obj_in );
            const Point4_<double> this_obj = Point4_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            const double diff_w = this_obj.z - src_obj.w;
            return static_cast< _Tp >( std::abs( diff_x ) + std::abs( diff_y ) + std::abs( diff_z ) + std::abs( diff_w ) );
        }

        //!
        //! @brief
        //!     \~japanese L1ノルム（マンハッタン距離）を計算する。
        //!     \~english  Calculates the L1 norm (Manhattan distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return L1 norm.
        //!
        //! @note
        //!     \~japanese manhattan_distance() と同義。
        //!     \~english  Equivalent to manhattan_distance().
        //!
        public: _Tp norm_one( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const { return manhattan_distance( obj_in ); }

        //!
        //! @brief
        //!     \~japanese ユークリッド距離（L2距離）を計算する。
        //!     \~english  Calculates Euclidean distance (L2 distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return Euclidean distance.
        //!
        public: _Tp euclidean_distance( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const
        {
            const Point4_<double> src_obj = Point4_<double>( obj_in );
            const Point4_<double> this_obj = Point4_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            const double diff_w = this_obj.z - src_obj.w;
            return static_cast< _Tp >( std::sqrt( ( diff_x * diff_x )
                                                + ( diff_y * diff_y )
                                                + ( diff_z * diff_z )
                                                + ( diff_w * diff_w ) ) );
        }

        //!
        //! @brief
        //!     \~japanese L2ノルム（ユークリッド距離）を計算する。
        //!     \~english  Calculates the L2 norm (Euclidean distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return L2 norm.
        //!
        //! @note
        //!     \~japanese euclidean_distance() と同義。
        //!     \~english  Equivalent to euclidean_distance().
        //!
        public: _Tp norm_two( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const { return euclidean_distance( obj_in ); }

        //!
        //! @brief
        //!     \~japanese チェビシェフ距離（L∞距離）を計算する。
        //!     \~english  Calculates Chebyshev distance (L∞ distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return Chebyshev distance.
        //!
        public: _Tp chebyshev_distance( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const
        {
            const Point4_<double> src_obj = Point4_<double>( obj_in );
            const Point4_<double> this_obj = Point4_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            const double diff_w = this_obj.w - src_obj.w;
            return static_cast< _Tp >( Maximum( Maximum( Maximum( std::abs( diff_x ), std::abs( diff_y ) ), std::abs( diff_z ) ), std::abs( diff_w ) ) );
        }

        //!
        //! @brief
        //!     \~japanese L∞ノルム（チェビシェフ距離）を計算する。
        //!     \~english  Calculates the L∞ norm (Chebyshev distance).
        //!
        //! @param [in] obj_in Target vector.
        //! @return L∞ norm.
        //!
        //! @note
        //!     \~japanese chebyshev_distance() と同義。
        //!     \~english  Equivalent to chebyshev_distance().
        //!
        public: _Tp norm_infinity( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const { return chebyshev_distance( obj_in ); }
        
        //!
        //! @brief
        //!     \~japanese マハラノビス距離を計算する。
        //!     \~english  Calculates the Mahalanobis distance.
        //!
        //! @param [in] data_in Vector of Point4_ elements as dataset.
        //! @return Mahalanobis distance.
        //!
        //! @note
        //!     \~japanese 共分散行列を用いて計算するため、対象データが十分にある必要がある。
        //!     \~english  Requires sufficient data to compute covariance matrix.
        //!
        public: double mahalanobis_distance( const std::vector< Point4_<_Tp> >& data_in ) const
        {
            // データを行列化.
            std::vector< Matrix > data_matrix_array;
            for( const Point4_ < _Tp>& pt : data_in )
            {
                const Matrix pt_mat = Point4_< float64_t >( pt ).matrix();
                data_matrix_array.emplace_back( pt_mat );
            }

            // 共分散行列.
            Matrix mean;
            const Matrix covariance_matrix = Matrix::Covariance( &mean, data_matrix_array );

            // 共分散行列の逆行列
            // 退化した分布（特異な共分散）では距離が定義できない. Data前提違反として報告する.
            const wse::CoreResult< Matrix > inv_covariance = covariance_matrix.tryInverse();
            if( !inv_covariance.succeeded() )
            {
                throw std::domain_error( "the covariance matrix of the distribution is singular" );
            }
            const Matrix inv_covariance_matrix = inv_covariance.value();

            // マハラノビス距離の計算
            const Point4_< float64_t > this_point( *this );
            const Point4_< float64_t > diff(
                  this_point.x - mean[ 0 ][ 0 ]
                , this_point.y - mean[ 1 ][ 0 ]
                , this_point.z - mean[ 2 ][ 0 ]
                , this_point.w - mean[ 3 ][ 0 ]
            );
            const Matrix diff_matrix = diff.matrix();
            const Matrix diff_matrix_trans = diff_matrix.transpose();
            const Matrix multiply_matrix_vector = diff_matrix_trans * inv_covariance_matrix * diff_matrix;
            return std::sqrt( static_cast< double >( multiply_matrix_vector[ 0 ][ 0 ] ) );
        }
        
        //!
        //! @brief
        //!     \~japanese 通常の距離（ユークリッド距離）を求める。
        //!     \~english  Returns the default distance (Euclidean).
        //!
        //! @param [in] obj_in Target vector.
        //! @return Distance value.
        //!
        //! @note
        //!     \~japanese euclidean_distance() のエイリアス。
        //!     \~english  Alias of euclidean_distance().
        //!
        public: _Tp distance( const Point4_<_Tp>& obj_in = Point4_<_Tp>() ) const
        {
            return euclidean_distance( obj_in );
        }

        //!
        //! @brief
        //!     \~japanese ベクトルの文字列表現を生成する。
        //!     \~english  Generates a string representation of the vector.
        //!
        //! @return String in the format "[ x, y, z, w ]".
        //!
        public : std::string str( )const
        {
            const std::string os = "[ " + toString( this->x ) + ", " + toString( this->y ) + ", " + toString( this->z ) + ", " + toString( this->w ) + " ]";
            return os;
        }
    };
    
    //!
    //! @brief
    //!     \~japanese ベクトルの文字列表現を生成する。
    //!     \~english  Generates a string representation of the vector.
    //!
    //! @param [in] os  outputstream
    //! @param [in] obj object.
    //! 
    //! @return String in the format "[ x, y, z, w ]".
    //!
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Point4_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }

    //!
    //! @brief
    //!     \~japanese Point4_の加算演算子。
    //!     \~english  Addition operator for Point4_.
    //!
    //! @param [in] obj1 Left-hand side.
    //! @param [in] obj2 Right-hand side.
    //! @return Result of obj1 + obj2.
    //!
    template<typename _Tp>
    inline static Point4_< _Tp > operator + ( const Point4_<_Tp>& obj1, const Point4_<_Tp>& obj2 )
    {
        Point4_< _Tp >tmp( obj1 );
        tmp += obj2;
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese Point4_の減算演算子。
    //!     \~english  Subtraction operator for Point4_.
    //!
    //! @param [in] obj1 Left-hand side.
    //! @param [in] obj2 Right-hand side.
    //! @return Result of obj1 - obj2.
    //!
    template<typename _Tp>
    inline static Point4_< _Tp > operator - ( const Point4_<_Tp>& obj1, const Point4_<_Tp>& obj2 )
    {
        Point4_< _Tp >tmp( obj1 );
        tmp -= obj2;
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese Point4_とスカラーの乗算演算子。
    //!     \~english  Multiplication operator between Point4_ and scalar.
    //!
    //! @param [in] obj1 Point vector.
    //! @param [in] scalar Scalar value.
    //! @return Result of obj1 * scalar.
    //!
    template< typename _Tp1, typename _Tp2 >
    inline static Point4_< _Tp1 > operator * ( const Point4_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point4_< _Tp1 >tmp( obj1 );
        tmp *= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese スカラーとPoint4_の乗算演算子。
    //!     \~english  Multiplication operator between scalar and Point4_.
    //!
    //! @param [in] scalar Scalar value.
    //! @param [in] obj1 Point vector.
    //! @return Result of scalar * obj1.
    //!
    template< typename _Tp1, typename _Tp2 >
    inline static Point4_< _Tp1 > operator * ( const _Tp2& scalar, const Point4_<_Tp1>& obj1 )
    {
        Point4_< _Tp1 >tmp( obj1 );
        tmp *= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese Point4_とスカラーの除算演算子。
    //!     \~english  Division operator between Point4_ and scalar.
    //!
    //! @param [in] obj1 Point vector.
    //! @param [in] scalar Scalar value.
    //! @return Result of obj1 / scalar.
    //!
    template< typename _Tp1, typename _Tp2 >
    inline static Point4_< _Tp1 > operator / ( const Point4_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point4_< _Tp1 >tmp( obj1 );
        tmp /= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    // Type aliases (no comments required per user instruction)
    typedef Point4_<  S8 > S8_XYZW;
    typedef Point4_<  U8 > U8_XYZW;
    typedef Point4_< S16 >S16_XYZW;
    typedef Point4_< U16 >U16_XYZW;
    typedef Point4_< S32 >S32_XYZW;
    typedef Point4_< U32 >U32_XYZW;
    typedef Point4_< S64 >S64_XYZW;
    typedef Point4_< U64 >U64_XYZW;
    typedef Point4_< F32 >F32_XYZW;
    typedef Point4_< F64 >F64_XYZW;

    typedef Point4_<  S8 >  sint08_xyzw;
    typedef Point4_< S16 >  sint16_xyzw;
    typedef Point4_< S32 >  sint32_xyzw;
    typedef Point4_< S64 >  sint64_xyzw;
    typedef Point4_<  U8 >  uint08_xyzw;
    typedef Point4_< U16 >  uint16_xyzw;
    typedef Point4_< U32 >  uint32_xyzw;
    typedef Point4_< U64 >  uint64_xyzw;
    typedef Point4_< F32 > float32_xyzw;
    typedef Point4_< F64 > float64_xyzw;
    typedef Point4_< F64 >  double_xyzw;

};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_DATA_POINT4D_H
