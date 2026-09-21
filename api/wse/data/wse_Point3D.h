//*****************************************************************************************************************
//! 
//! @file    wse_Point3D.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 3次元座標・ベクトルデータ構造 Point3_ を提供するヘッダファイル。
//!     \~english  Header file providing 3D coordinate/vector structure Point3_.
//!
//!
//! @details
//!     \~japanese
//!         Point3_ は X, Y, Z の各成分を持ち、加算・減算・ノルム・距離計算などを行う。
//!
//!     \~english
//!         Point3_ holds X, Y, Z components and supports arithmetic, norm, and distance computations.
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

#ifndef WONDERSTEWENGINE_DATA_POINT3D_H
#define WONDERSTEWENGINE_DATA_POINT3D_H


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
    //! @struct Point3_
    //!
    //! @brief
    //!     \~japanese テンプレートによる3次元ベクトル構造体。
    //!     \~english  3D vector structure using template.
    //!
    //! @details
    //!     \~japanese
    //!         各成分X, Y, Zを保持し、代入・比較・四則演算・距離・正規化などの処理を提供する。
        //!
    //!     \~english
    //!         Stores X, Y, Z components and provides assignment, comparison, arithmetic,
        //! 
    template<typename _Tp>
    struct WSE_API Point3_      final
    {

        typedef _Tp value_type;
        //----------------------------------
        // Member
        //----------------------------------
        public:_Tp x; //!< \~japanese X座標値. \~english X coordinate value.
        public:_Tp y; //!< \~japanese Y座標値. \~english Y coordinate value.
        public:_Tp z; //!< \~japanese Z座標値. \~english Z coordinate value.

        //!
        //! @brief
        //!     \~japanese 各成分の合計バイトサイズを返す。
        //!     \~english Returns total byte size of all components.
        //!
        //! @return Total byte size.
        //!
        public: uint64_t byte( void )const { return sizeof( value_type ) * 3; }

        //!
        //! @brief
        //!     \~japanese 各成分の合計バイトサイズを返す。
        //!     \~english Returns total byte size of all components.
        //!
        //! @return Total byte size.
        //!
        public: static uint64_t BYTE( void ){ return sizeof( value_type ) * 3; }

        //!
        //! @brief  Default constructor.
        //!
        public: explicit Point3_( void )
            : x ( static_cast< _Tp >( 0 ) )
            , y ( static_cast< _Tp >( 0 ) )
            , z ( static_cast< _Tp >( 0 ) ) {}

        //!
        //! @brief  Destructor.
        //!
        public: ~Point3_( void ) {}

        //!
        //! @brief  Constructor with parameters.
        //!
        //! @param [in ] x_in    X coordinate.
        //! @param [in ] y_in    Y coordinate.
        //! @param [in ] z_in    Z coordinate.
        //!
        public: Point3_( const _Tp x_in, const _Tp y_in, const _Tp z_in )
            : x ( x_in )
            , y ( y_in )
            , z ( z_in ) {}

        //!
        //! @brief  Templated constructor with parameters.
        //!
        //! @param [in ] x_in    X coordinate.
        //! @param [in ] y_in    Y coordinate.
        //! @param [in ] z_in    Z coordinate.
        //!
        public: template< typename _inTp > explicit Point3_( const _inTp x_in, const _inTp y_in, const _inTp z_in )
            : x ( static_cast< _Tp >( x_in ) )
            , y ( static_cast< _Tp >( y_in ) )
            , z ( static_cast< _Tp >( z_in ) ){}

        //!
        //! @brief  Templated cast constructor.
        //!
        //! @param [in ] obj_in Source object.
        //!
        public: template< typename _inTp > explicit Point3_( const Point3_<_inTp>& obj_in ) 
            : x ( static_cast< _Tp >( obj_in.x ) )
            , y ( static_cast< _Tp >( obj_in.y ) )
            , z ( static_cast< _Tp >( obj_in.z ) ){}

        //!
        //! @brief  Copy constructor.
        //!
        //! @param [in ] obj_in Source object.
        //!
        public: Point3_( const Point3_& obj_in )
            : x ( obj_in.x )
            , y ( obj_in.y )
            , z ( obj_in.z ) {}

        //!
        //! @brief  Move constructor.
        //!
        //! @param [in ] obj_inout Source object.
        //!
        public:
        Point3_( Point3_&& obj_inout )noexcept
            : x ( std::move( obj_inout.x ) )
            , y ( std::move( obj_inout.y ) )
            , z ( std::move( obj_inout.z ) ) {}
        //!
        //! @brief
        //!     \~japanese スワップ処理。
        //!     \~english Swap two objects.
        //!
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //!
        friend void swap( Point3_<_Tp>& obj1_inout, Point3_<_Tp>& obj2_inout )
        {
            std::swap( obj1_inout.x, obj2_inout.x );
            std::swap( obj1_inout.y, obj2_inout.y );
            std::swap( obj1_inout.z, obj2_inout.z );
        }

        //!
        //! @brief  Copy assignment operator.
        //!
        //! @param [in ] obj Source object.
        //!
        public:
        Point3_< _Tp >& operator = ( const Point3_<_Tp>& obj )
        {
            Point3_< _Tp > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //!
        //! @brief  Move assignment operator.
        //!
        //! @param [in ] obj Source object.
        //!
        public:
        Point3_< _Tp >& operator = ( Point3_<_Tp>&& obj )noexcept
        {
            Point3_< _Tp > move_obj( std::move( obj ) );
            swap( *this, move_obj );
            return *this;
        }

        //!
        //! @brief  Equality operator.
        //!
        //! @param [in ] data Comparison target.
        //! @retval true      All components are equal.
        //! @retval false     Any component is different.
        //!
        public:
        bool operator == ( const Point3_< _Tp >& data )
        {
            return ( this->x == data.x ) && ( this->y == data.y ) && ( this->z == data.z );
        }

        //!
        //! @brief  Inequality operator.
        //!
        //! @param [in ] data Comparison target.
        //! @retval true      Any component is different.
        //! @retval false     All components are equal.
        //!
        public:
        bool operator != ( const Point3_< _Tp >& data )
        {
            return !( *this == data );
        }

        //!
        //! @brief  Addition operator.
        //!
        //! @param [in ] obj Operand.
        //! @return Reference to this object after addition.
        //!
        public:
        Point3_< _Tp >& operator += ( const Point3_<_Tp>& obj )
        {
            this->x += obj.x;
            this->y += obj.y;
            this->z += obj.z;
            return *this;
        }

        //!
        //! @brief  Subtraction operator.
        //!
        //! @param [in ] obj Operand.
        //! @return Reference to this object after subtraction.
        //!
        public:
        Point3_< _Tp >& operator -= ( const Point3_<_Tp>& obj )
        {
            this->x -= obj.x;
            this->y -= obj.y;
            this->z -= obj.z;
            return *this;
        }

        //!
        //! @brief  Multiplication operator.
        //!
        //! @param [in ] scalar Scalar value.
        //! @return Reference to this object after scaling.
        //!
        public:
        Point3_< _Tp >& operator *= ( const _Tp scalar )
        {
            this->x *= scalar;
            this->y *= scalar;
            this->z *= scalar;
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 除算オペレーター。
        //!     \~english  Division operator.
        //!
        //! @param [in ] scalar Scalar value.
        //! @return Reference to this object after division.
        //!
        //! @note
        //!     \~japanese 0除算では例外を投げる。
        //!     \~english Throws exception on division by zero.
        //!
        public:
        Point3_< _Tp >& operator /= ( const _Tp scalar )
        {
            if( scalar == _Tp( 0 ) )
            {
                throw std::domain_error( "division by zero" );
            }

            this->x /= scalar;
            this->y /= scalar;
            this->z /= scalar;
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
                };
            return Matrix_<double>( 3, 1, elements );
        }

        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------
        //! 
        //! @brief 
        //!     \~japanese 内積(ドット積)を算出する。 
        //!     \~english  Calculate dot product.
        //! 
        //! @param [in] obj_in Target vector.
        //! @return Dot product value.
        //! 
        public: _Tp dot( const Point3_<_Tp>& obj_in ) const
        {
            const Point3_<double> src_obj = Point3_<double>( obj_in );
            const Point3_<double> this_obj = Point3_<double>( *this );
            return static_cast< _Tp >( ( this_obj.x * src_obj.x )
                                     + ( this_obj.y * src_obj.y )
                                     + ( this_obj.z * src_obj.z ) );
        }

        //! 
        //! @brief 
        //!     \~japanese 外積(クロス積)を算出する。 
        //!     \~english  Calculate cross product.
        //! 
        //! @param [in] obj_in Target vector.
        //! @return Cross product vector.
        //! 
        public: Point3_<_Tp > cross( const Point3_<_Tp>& obj_in ) const
        {
            Point3_<double> src_obj = Point3_<double>( obj_in );
            const Point3_<double> this_obj = Point3_<double>( *this );
            return Point3_<_Tp >( ( this_obj.y * src_obj.z ) - ( this_obj.z * src_obj.y )
                                , ( this_obj.z * src_obj.x ) - ( this_obj.x * src_obj.z )
                                , ( this_obj.x * src_obj.y ) - ( this_obj.y * src_obj.x ) );
        }

        //! 
        //! @brief 
        //!     \~japanese 正規化ベクトルを取得する。
        //!     \~english  Normalize this vector.
        //! 
        //! @param [in] target_in Normalized vector's length.
        //! @return Normalized vector.
        //! 
        public: Point3_<_Tp> normalize( const double target_in = 1.0 ) const
        {
            if( this->x == 0 && this->y == 0 && this->z == 0 ) { return *this; }

            Point3_<double> obj = Point3_<double>( *this );
            const double coef = std::sqrt( ( target_in * target_in ) 
                                         / ( ( obj.x * obj.x ) + ( obj.y * obj.y ) + ( obj.z * obj.z ) ) );
            return Point3_<_Tp>( ( coef * obj.x ), ( coef * obj.y ), ( coef * obj.z ) );
        }

        //! 
        //! @brief 
        //!     \~japanese マンハッタン距離を算出する。
        //!     \~english  Calculate Manhattan distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Manhattan distance.
        //! 
        public: _Tp manhattan_distance( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const
        {
            const Point3_<double> src_obj = Point3_<double>( obj_in );
            const Point3_<double> this_obj = Point3_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            return static_cast< _Tp >( std::abs( diff_x ) + std::abs( diff_y ) + std::abs( diff_z ) );
        }

        //! 
        //! @brief 
        //!     \~japanese L1ノルムを算出する。
        //!     \~english  Compute L1 norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L1 norm.
        //! 
        public: _Tp norm_one( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const { return manhattan_distance( obj_in ); }

        //! 
        //! @brief 
        //!     \~japanese ユークリッド距離を算出する。
        //!     \~english  Calculate Euclidean distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Euclidean distance.
        //! 
        public: _Tp euclidean_distance( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const
        {
            const Point3_<double> src_obj = Point3_<double>( obj_in );
            const Point3_<double> this_obj = Point3_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            return static_cast< _Tp >( std::sqrt( ( diff_x * diff_x )
                                                + ( diff_y * diff_y )
                                                + ( diff_z * diff_z ) ) );
        }

        //! 
        //! @brief 
        //!     \~japanese L2ノルムを算出する。
        //!     \~english  Compute L2 norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L2 norm.
        //! 
        public: _Tp norm_two( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const { return euclidean_distance( obj_in ); }

        //! 
        //! @brief 
        //!     \~japanese チェビシェフ距離を算出する。
        //!     \~english  Calculate Chebyshev distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Chebyshev distance.
        //! 
        public: _Tp chebyshev_distance( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const
        {
            const Point3_<double> src_obj = Point3_<double>( obj_in );
            const Point3_<double> this_obj = Point3_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            const double diff_z = this_obj.z - src_obj.z;
            return static_cast< _Tp >( Maximum( Maximum( std::abs( diff_x ), std::abs( diff_y ) ), std::abs( diff_z ) ) );
        }

        //! 
        //! @brief 
        //!     \~japanese L∞ノルムを算出する。
        //!     \~english  Compute L∞ norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L∞ norm.
        //! 
        public: _Tp norm_infinity( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const { return chebyshev_distance( obj_in ); }

        //! 
        //! @brief 
        //!     \~japanese マハラノビス距離を算出する。
        //!     \~english  Calculate Mahalanobis distance.
        //! 
        //! @param [in] data_in Vector of points (distribution).
        //! @return Mahalanobis distance.
        //! 
        public: double mahalanobis_distance( const std::vector< Point3_<_Tp> >& data_in ) const
        {
            std::vector< Matrix > data_matrix_array;
            for( const Point3_ < _Tp>& pt : data_in )
            {
                const Matrix pt_mat = Point3_< float64_t >( pt ).matrix();
                data_matrix_array.emplace_back( pt_mat );
            }

            Matrix mean;
            const Matrix covariance_matrix = Matrix::Covariance( &mean, data_matrix_array );

            // 退化した分布（特異な共分散）では距離が定義できない. Data前提違反として報告する.
            const wse::CoreResult< Matrix > inv_covariance = covariance_matrix.tryInverse();
            if( !inv_covariance.succeeded() )
            {
                throw std::domain_error( "the covariance matrix of the distribution is singular" );
            }
            const Matrix inv_covariance_matrix = inv_covariance.value();

            const Point3_< float64_t > this_point( *this );
            const Point3_< float64_t > diff(
                  this_point.x - mean[ 0 ][ 0 ]
                , this_point.y - mean[ 1 ][ 0 ]
                , this_point.z - mean[ 2 ][ 0 ]
            );
            const Matrix diff_matrix = diff.matrix();
            const Matrix diff_matrix_trans = diff_matrix.transpose();
            const Matrix multiply_matrix_vector = diff_matrix_trans * inv_covariance_matrix * diff_matrix;
            return std::sqrt( static_cast< double >( multiply_matrix_vector[ 0 ][ 0 ] ) );
        }

        //! 
        //! @brief 
        //!     \~japanese 距離（ユークリッド）を取得する。
        //!     \~english  Get distance (Euclidean).
        //! 
        //! @param [in] obj_in Target point.
        //! @return Distance.
        //! 
        public: _Tp distance( const Point3_<_Tp>& obj_in = Point3_<_Tp>() ) const
        {
            return euclidean_distance( obj_in );
        }

        //! 
        //! @brief 
        //!     \~japanese 文字列化する。
        //!     \~english  Convert to string.
        //! 
        //! @return Formatted string.
        //! 
        public : std::string str( void )const
        {
            const std::string os = "[ " + toString( this->x ) + ", " + toString( this->y ) + ", " + toString( this->z ) + " ]";
            return os;
        }



    };
    //! 
    //! @brief 
    //!     \~japanese ベクトルの加算を行う。
    //!     \~english  Add two vectors.
    //! 
    //! @param [in] obj1 First operand.
    //! @param [in] obj2 Second operand.
    //! @return Result of addition.
    //! 
    template<typename _Tp>
    inline static Point3_< _Tp > operator + ( const Point3_<_Tp>& obj1, const Point3_<_Tp>& obj2 )
    {
        Point3_< _Tp >tmp( obj1 );
        tmp += obj2;
        return tmp;
    }

    //! 
    //! @brief 
    //!     \~japanese ベクトルの減算を行う。
    //!     \~english  Subtract two vectors.
    //! 
    //! @param [in] obj1 First operand.
    //! @param [in] obj2 Second operand.
    //! @return Result of subtraction.
    //! 
    template<typename _Tp>
    inline static Point3_< _Tp > operator - ( const Point3_<_Tp>& obj1, const Point3_<_Tp>& obj2 )
    {
        Point3_< _Tp >tmp( obj1 );
        tmp -= obj2;
        return tmp;
    }

    //! 
    //! @brief 
    //!     \~japanese ベクトルとスカラーの乗算を行う。
    //!     \~english  Multiply vector by scalar.
    //! 
    //! @param [in] obj1 Vector.
    //! @param [in] scalar Scalar value.
    //! @return Result of multiplication.
    //! 
    template< typename _Tp1, typename _Tp2 >
    inline static Point3_< _Tp1 > operator * ( const Point3_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point3_< _Tp1 >tmp( obj1 );
        tmp *= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    //! 
    //! @brief 
    //!     \~japanese スカラーとベクトルの乗算を行う。
    //!     \~english  Multiply scalar by vector.
    //! 
    //! @param [in] scalar Scalar value.
    //! @param [in] obj1 Vector.
    //! @return Result of multiplication.
    //! 
    template< typename _Tp1, typename _Tp2 >
    inline static Point3_< _Tp1 > operator * ( const _Tp2& scalar, const Point3_<_Tp1>& obj1 )
    {
        Point3_< _Tp1 >tmp( obj1 );
        tmp *= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    //! 
    //! @brief 
    //!     \~japanese ベクトルとスカラーの除算を行う。
    //!     \~english  Divide vector by scalar.
    //! 
    //! @param [in] obj1 Vector.
    //! @param [in] scalar Scalar value.
    //! @return Result of division.
    //! 
    template< typename _Tp1, typename _Tp2 >
    inline static Point3_< _Tp1 > operator / ( const Point3_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point3_< _Tp1 >tmp( obj1 );
        tmp /= static_cast< _Tp1 >( scalar );
        return tmp;
    }

    //! 
    //! @brief 
    //!     \~japanese 出力ストリーム演算子。文字列として表示する。
    //!     \~english  Output stream operator for printing.
    //! 
    //! @param [in,out] os Output stream.
    //! @param [in] obj Point3_ object.
    //! @return Reference to the stream.
    //! 
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Point3_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }



    typedef Point3_<  S8 > S8_XYZ;
    typedef Point3_<  U8 > U8_XYZ;
    typedef Point3_< S16 >S16_XYZ;
    typedef Point3_< U16 >U16_XYZ;
    typedef Point3_< S32 >S32_XYZ;
    typedef Point3_< U32 >U32_XYZ;
    typedef Point3_< S64 >S64_XYZ;
    typedef Point3_< U64 >U64_XYZ;
    typedef Point3_< F32 >F32_XYZ;
    typedef Point3_< F64 >F64_XYZ;

    typedef Point3_<  S8 >  sint08_xyz;
    typedef Point3_< S16 >  sint16_xyz;
    typedef Point3_< S32 >  sint32_xyz;
    typedef Point3_< S64 >  sint64_xyz;
    typedef Point3_<  U8 >  uint08_xyz;
    typedef Point3_< U16 >  uint16_xyz;
    typedef Point3_< U32 >  uint32_xyz;
    typedef Point3_< U64 >  uint64_xyz;
    typedef Point3_< F32 > float32_xyz;
    typedef Point3_< F64 > float64_xyz;
    typedef Point3_< F64 >  double_xyz;

};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif //WONDERSTEWENGINE_DATA_POINT3D_H
