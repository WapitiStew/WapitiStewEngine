//*****************************************************************************************************************
//! 
//! @file    wse_Point2D.h
//! @brief   2次元座標,ベクトルを扱うデータクラス.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @brief
//!     \~japanese 2次元座標・ベクトルデータ構造 Point2_ を提供するヘッダファイル。
//!     \~english  Header file providing 2D coordinate/vector structure Point2_.
//!
//!
//! @details
//!     \~japanese
//!         Point2_ は X, Y の各成分を持ち、加算・減算・ノルム・距離計算などを行う。
//!     @n OpenCV連携は`<cv/OpenCvAdapter.h>`が担う。
//!
//!     \~english
//!         Point3_ holds X, Y components and supports arithmetic, norm, and distance computations.
//!     @n OpenCV interop lives in `<cv/OpenCvAdapter.h>`.
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

#ifndef WONDERSTEWENGINE_DATA_POINT2D_H
#define WONDERSTEWENGINE_DATA_POINT2D_H


// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#pragma warning(disable: 4820)  // メンバー変数の定義した時にアライメント調整用のスペースで発生する警告除去.
#endif

// include files
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../utility/wse_Math.h"
#include <stdexcept>

#include "../../dynamic.h"
#include "wse_Matrix.h"




namespace wse
{

    //!
    //! @struct Point2_
    //!
    //! @brief
    //!     \~japanese テンプレートによる2次元ベクトル構造体。
    //!     \~english  2D vector structure using template.
    //!
    //! @details
    //!     \~japanese
    //!         各成分X, Y, Zを保持し、代入・比較・四則演算・距離・正規化などの処理を提供する。
    //!     @n OpenCV連携は`<cv/OpenCvAdapter.h>`が担う。
    //!
    //!     \~english
    //!         Stores X, Y, Z components and provides assignment, comparison, arithmetic,
    //!     @n distance, and normalization functions; OpenCV interop lives in `<cv/OpenCvAdapter.h>`.
    //! 
    template<typename _Tp>
    struct WSE_API Point2_      final
    {

        public: typedef _Tp value_type;
        //----------------------------------
        // Member
        //----------------------------------
        public:_Tp x; //!< \~japanese X座標値. \~english X coordinate value.
        public:_Tp y; //!< \~japanese Y座標値. \~english Y coordinate value.

        //!
        //! @brief
        //!     \~japanese 各成分の合計バイトサイズを返す。
        //!     \~english Returns total byte size of all components.
        //!
        //! @return Total byte size.
        //!
        public: uint64_t byte(void) const { return sizeof( value_type ) * 2; }

        //!
        //! @brief
        //!     \~japanese 各成分の合計バイトサイズを返す。
        //!     \~english Returns total byte size of all components.
        //!
        //! @return Total byte size.
        //!
        public: static uint64_t BYTE(void) { return sizeof( value_type ) * 2; }

        //!
        //! @brief  Default constructor.
        //!
        public: explicit Point2_( void )
            : x ( static_cast< _Tp >( 0 ) )
            , y ( static_cast< _Tp >( 0 ) ){}

        //!
        //! @brief  Destructor.
        //!
        public: ~Point2_( void ){}

        //!
        //! @brief  Constructor with parameters.
        //!
        //! @param [in ] x_in    X coordinate.
        //! @param [in ] y_in    Y coordinate.
        //!
        public: Point2_( const _Tp x_in, const _Tp y_in )
            : x ( x_in )
            , y ( y_in ){}

        //! 
        //! @brief  Templated constructor with parameters.
        //!
        //! @param [in ] x_in    X coordinate.
        //! @param [in ] y_in    Y coordinate.
        //! 
        public: template< typename _inTp > explicit Point2_( const _inTp x_in, const _inTp y_in )
            : x ( static_cast<_Tp>(x_in) )
            , y ( static_cast<_Tp>(y_in) ){}

        //! 
        //! @brief  Templated constructor with parameters.
        //!
        //! @param [in ] x_in    X coordinate.
        //! @param [in ] y_in    Y coordinate.
        //! 
        public: template< typename _inTp1, typename _inTp2 > 
        explicit Point2_( const _inTp1 x_in, const _inTp2 y_in ) 
            : x ( static_cast<_Tp>(x_in) )
            , y ( static_cast<_Tp>(y_in) )
        {
        }
        //! 
        //! @brief  Templated cast constructor.
        //!
        //! @param [in ] obj_in Source object.
        //! 
        public: template< typename _inTp > explicit Point2_( const Point2_<_inTp>& obj_in )
            : x ( static_cast<_Tp>(obj_in.x) )
            , y ( static_cast<_Tp>(obj_in.y) ){}



        //! 
        //! @brief  Copy constructor.
        //!
        //! @param [in ] obj_in Source object.
        //! 
        public: Point2_( const Point2_& obj_in )
            : x ( obj_in.x )
            , y ( obj_in.y ){}

        //! 
        //! @brief  Move constructor.
        //!
        //! @param [in ] obj_inout Source object.
        //! 
        public:
        Point2_( Point2_&& obj_inout )noexcept
            : x ( std::move(obj_inout.x) )
            , y ( std::move(obj_inout.y) ){}

        //! 
        //! @brief
        //!     \~japanese スワップ処理。
        //!     \~english Swap two objects.
        //!
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //! 
        friend void swap( Point2_<_Tp>& obj1_inout, Point2_<_Tp>& obj2_inout )
        {
            std::swap(obj1_inout.x, obj2_inout.x);
            std::swap(obj1_inout.y, obj2_inout.y);
        }

        //! 
        //! @brief  Copy assignment operator.
        //!
        //! @param [in ] obj Source object.
        //! 
        public:
        Point2_< _Tp >& operator = ( const Point2_<_Tp>& obj )
        {
            Point2_< _Tp > copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }


        //! 
        //! @brief  Move assignment operator.
        //!
        //! @param [in ] obj Source object.
        //! 
        public:
        Point2_< _Tp >& operator = ( Point2_<_Tp>&& obj )noexcept
        {
            Point2_< _Tp > move_obj(std::move(obj));
            swap(*this, move_obj);
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
        bool operator == ( const Point2_< _Tp >& data )
        {
            return ( this->x == data.x ) && ( this->y == data.y );
        }

        //! 
        //! @brief  Inequality operator.
        //!
        //! @param [in ] data Comparison target.
        //! @retval true      Any component is different.
        //! @retval false     All components are equal.
        //! 
        public:
        bool operator != ( const Point2_< _Tp >& data )
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
        Point2_< _Tp >& operator += ( const Point2_<_Tp>& obj )
        {
            this->x += obj.x;
            this->y += obj.y;
            return *this;
        }

        //! 
        //! @brief  Subtraction operator.
        //!
        //! @param [in ] obj Operand.
        //! @return Reference to this object after subtraction.
        //! 
        public:
        Point2_< _Tp >& operator -= ( const Point2_<_Tp>& obj )
        {
            this->x -= obj.x;
            this->y -= obj.y;
            return *this;
        }

        //! 
        //! @brief  Multiplication operator.
        //!
        //! @param [in ] scalar Scalar value.
        //! @return Reference to this object after scaling.
        //! 
        public:
        Point2_< _Tp >& operator *= ( const _Tp scalar )
        {
            this->x *= scalar;
            this->y *= scalar;
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
        Point2_< _Tp >& operator /= ( const _Tp scalar )
        {
            if ( scalar == _Tp( 0 ) )
            {
                throw std::domain_error( "division by zero" );
            }

            this->x /= scalar;
            this->y /= scalar;
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
        //!     Point         Matrix
        //!      [ X, Y ]  =>  | X |
        //!                    | Y |
        //! 
        public: Matrix_< double > matrix( void ) const
        {
            const std::vector< double > elements = { static_cast<double>( this->x ), static_cast<double>( this->y ) };
            return Matrix_<double>( 2, 1, elements );
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
        public: _Tp dot( const Point2_<_Tp>& obj_in ) const
        {
            const Point2_<double> src_obj = Point2_<double>( obj_in );
            const Point2_<double> this_obj = Point2_<double>( *this );
            return static_cast< _Tp >( ( this_obj.x * src_obj.x)
                                     + ( this_obj.y * src_obj.y) );
        }

        //!
        //! @brief 
        //!     \~japanese 外積(クロス積)を算出する。 
        //!     \~english  Calculate cross product.
        //! 
        //! @param [in] obj_in Target vector.
        //! @return Cross product vector.
        //! 
        public: _Tp cross( const Point2_<_Tp>& obj_in ) const
        {
            Point2_<double> src_obj = Point2_<double>(obj_in);
            const Point2_<double> this_obj = Point2_<double>( *this );
            return static_cast< _Tp >( (this_obj.x * src_obj.y )
                                     - (this_obj.y * src_obj.x ) );
        }

        //!
        //! @brief 
        //!     \~japanese 正規化ベクトルを取得する。
        //!     \~english  Normalize this vector.
        //! 
        //! @param [in] target_in Normalized vector's length.
        //! @return Normalized vector.
        //! 
        public: Point2_<_Tp> normalize( const double target_in = 1.0 ) const
        {
            if( this->x == 0 && this->y == 0 ){ return *this; }

            Point2_<double> obj = Point2_<double>( *this );
            const double coef = std::sqrt( ( target_in * target_in ) 
                              / ( ( obj.x * obj.x ) + ( obj.y * obj.y ) ) );

            return Point2_<_Tp>( ( coef * obj.x ), ( coef * obj.y ) );
        }

        //!
        //! @brief 
        //!     \~japanese マンハッタン距離を算出する。
        //!     \~english  Calculate Manhattan distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Manhattan distance.
        //! 
        public: _Tp manhattan_distance( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const
        {
            const Point2_<double> src_obj  = Point2_<double>( obj_in );
            const Point2_<double> this_obj = Point2_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            return static_cast< _Tp >( std::abs( diff_x ) + std::abs( diff_y ) );
        }

        //!
        //! @brief 
        //!     \~japanese L1ノルムを算出する。
        //!     \~english  Compute L1 norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L1 norm.
        //! 
        public: _Tp norm_one( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const { return manhattan_distance( obj_in ); }


        //!
        //! @brief 
        //!     \~japanese ユークリッド距離を算出する。
        //!     \~english  Calculate Euclidean distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Euclidean distance.
        //! 
        public: _Tp euclidean_distance( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const
        {
            const Point2_<double> src_obj  = Point2_<double>( obj_in );
            const Point2_<double> this_obj = Point2_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            return static_cast< _Tp >( std::sqrt( ( diff_x * diff_x )
                                                + ( diff_y * diff_y ) ) );
        }

        //!
        //! @brief 
        //!     \~japanese L2ノルムを算出する。
        //!     \~english  Compute L2 norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L2 norm.
        //! 
        public: _Tp norm_two( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const { return euclidean_distance( obj_in ); }


        //!
        //! @brief 
        //!     \~japanese チェビシェフ距離を算出する。
        //!     \~english  Calculate Chebyshev distance.
        //! 
        //! @param [in] obj_in Target point.
        //! @return Chebyshev distance.
        //! 
        public: _Tp chebyshev_distance( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const
        {
            const Point2_<double> src_obj = Point2_<double>( obj_in );
            const Point2_<double> this_obj = Point2_<double>( *this );
            const double diff_x = this_obj.x - src_obj.x;
            const double diff_y = this_obj.y - src_obj.y;
            return static_cast< _Tp >( Maximum( std::abs( diff_x ), std::abs( diff_y ) ) );
        }

        //!
        //! @brief 
        //!     \~japanese L∞ノルムを算出する。
        //!     \~english  Compute L∞ norm.
        //! 
        //! @param [in] obj_in Target point.
        //! @return L∞ norm.
        //! 
        public: _Tp norm_infinity( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const { return chebyshev_distance( obj_in ); }

        //!
        //! @brief 
        //!     \~japanese マハラノビス距離を算出する。
        //!     \~english  Calculate Mahalanobis distance.
        //! 
        //! @param [in] data_in Vector of points (distribution).
        //! @return Mahalanobis distance.
        //! 
        public: double mahalanobis_distance( const std::vector< Point2_<_Tp> >& data_in ) const
        {
            // データを行列化.
            std::vector< Matrix > data_matrix_array;
            for( const Point2_ < _Tp>& pt : data_in )
            {
                const Matrix pt_mat = Point2_< float64_t >( pt ).matrix();
                data_matrix_array.emplace_back( pt_mat );
            }
            // 共分散行列.
            Matrix mean;
            const Matrix covariance_matrix = Matrix::Covariance( &mean, data_matrix_array );
            return 0;
//
//            // 共分散行列の逆行列
//            const Matrix inv_covariance_matrix = covariance_matrix.inverse();
//
//            // マハラノビス距離の計算
//            const Point2_< float64_t > this_point( *this );
//            const Point2_< float64_t > diff = this_point - Point2_< float64_t >( mean[ 0 ][ 0 ], mean[ 1 ][ 0 ] );
//            const Matrix diff_matrix = diff.matrix();
//            const Matrix diff_matrix_trans = diff_matrix.transpose();
//            const Matrix multiply_matrix_vector = diff_matrix_trans * inv_covariance_matrix * diff_matrix;
//            return static_cast< _Tp >( std::sqrt( multiply_matrix_vector[0][0] ) );
        }

        //!
        //! @brief 
        //!     \~japanese 距離（ユークリッド）を取得する。
        //!     \~english  Get distance (Euclidean).
        //! 
        //! @param [in] obj_in Target point.
        //! @return Distance.
        //! 
        public : _Tp distance( const Point2_<_Tp>& obj_in = Point2_<_Tp>() ) const
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
        public : std::string str( )const
        {
            const std::string os = "[ " + toString( this->x ) + ", " + toString( this->y ) + " ]";
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
    inline static Point2_< _Tp > operator + ( const Point2_<_Tp>& obj1, const Point2_<_Tp>& obj2 )
    {
        Point2_< _Tp >tmp( obj1 );
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
    inline static Point2_< _Tp > operator - ( const Point2_<_Tp>& obj1, const Point2_<_Tp>& obj2 )
    {
        Point2_< _Tp >tmp( obj1 );
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
    inline static Point2_< _Tp1 > operator * ( const Point2_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point2_< _Tp1 >tmp( obj1 );
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
    inline static Point2_< _Tp1 > operator * ( const _Tp2& scalar, const Point2_<_Tp1>& obj1 )
    {
        Point2_< _Tp1 >tmp( obj1 );
        tmp *= static_cast< _Tp1>(scalar );
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
    inline static Point2_< _Tp1 > operator / ( const Point2_<_Tp1>& obj1, const _Tp2& scalar )
    {
        Point2_< _Tp1 >tmp( obj1 );
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
    inline static std::ostream& operator << ( std::ostream& os, const Point2_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }




    typedef Point2_<  S8 > S8_XY;
    typedef Point2_<  U8 > U8_XY;
    typedef Point2_< S16 >S16_XY;
    typedef Point2_< U16 >U16_XY;
    typedef Point2_< S32 >S32_XY;
    typedef Point2_< U32 >U32_XY;
    typedef Point2_< S64 >S64_XY;
    typedef Point2_< U64 >U64_XY;
    typedef Point2_< F32 >F32_XY;
    typedef Point2_< F64 >F64_XY;

    typedef Point2_<  S8 >  sint08_xy;
    typedef Point2_< S16 >  sint16_xy;
    typedef Point2_< S32 >  sint32_xy;
    typedef Point2_< S64 >  sint64_xy;
    typedef Point2_<  U8 >  uint08_xy;
    typedef Point2_< U16 >  uint16_xy;
    typedef Point2_< U32 >  uint32_xy;
    typedef Point2_< U64 >  uint64_xy;
    typedef Point2_< F32 > float32_xy;
    typedef Point2_< F64 > float64_xy;
    typedef Point2_< F64 >  double_xy;

};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


#endif //WONDERSTEWENGINE_DATA_POINT2D_H