//*****************************************************************************************************************
//! 
//! @file    wse_Matrix.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Matrix.h は浮動小数点型の行列クラスと関連オペレーターを定義するファイル。
//!     \~english  Matrix.h defines a floating-point matrix class and related operators.
//!
//! @details
//!     \~japanese
//!         このファイルでは、Map を基底クラスとする Matrix_ テンプレートを提供する。
//!         行列の行数・列数取得、等号・不等号・算術演算オペレーターの実装、  
//!         正方行列・対角行列・単位行列・回転行列・共分散行列の生成メソッドを備える。  
//!     \~english
//!         This file provides the Matrix_ template based on Map as its base class.  
//!         It includes methods to get the number of rows and columns, implementations of equality, inequality, and arithmetic operators,  
//!         and static methods to generate square matrices, diagonal matrices, identity matrices, rotation matrices, and covariance matrices.  
//!
//! @note
//!     \~japanese 行列計算中に不正な次元が指定された場合は std::invalid_argument を投げる。  
//!     \~english  Throws std::invalid_argument when invalid dimensions are specified during matrix operations.  
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

#ifndef WONDERSTEWENGINE_DATA_MATRIX_H
#define WONDERSTEWENGINE_DATA_MATRIX_H

// Warning Disable
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#pragma warning(disable: 4710)  // インラインか無視.
#pragma warning(disable: 4820)  // メンバー変数の定義した時にアライメント調整用のスペースで発生する警告除去.
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

// include
#include <stdexcept>

#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../depend/wse_Constant.h"
#include "../error/CoreError.h"
#include "../utility/wse_StringTool.h"

#include "wse_Map.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4251)
#endif
namespace wse
{
    //!
    //! @class Matrix_
    //!
    //! @brief
    //!     \~japanese 浮動小数点型の行列を表すテンプレートクラス。
    //!     \~english  Template class representing a floating-point matrix.
    //!
    //! @details
    //!     \~japanese
    //!         Map を基底クラスとして継承し、行列データを格納する。  
    //!         行数・列数の取得、要素アクセス、等号・不等号・加算・減算・スカラー乗算・スカラー除算演算子を実装。  
    //!         正方行列、対角行列、単位行列、2D/3D 回転行列、共分散行列を生成する静的メソッドを提供する。  
    //!     \~english
    //!         Inherits from Map to store matrix data.  
    //!         Implements methods to get rows and columns, element access, equality/inequality, addition, subtraction, scalar multiplication, and scalar division operators.  
    //!         Provides static methods to generate square matrices, diagonal matrices, identity matrices, 2D/3D rotation matrices, and covariance matrices.  
    //!
    template<typename _Tp, typename  Enable = std::enable_if<std::is_floating_point<_Tp>::value> >
    struct WSE_API Matrix_ : public Map < _Tp >
    {
        //---------------------------------------
        // using. Constractor / Destractor./Operator. / Accesor / Getter / Setter 
        //---------------------------------------
        // エイリアスで基底クラスを定義
        using Base = Map< _Tp >;
        public: using Base::Base;
        //! \~english Public unchecked row access. \~japanese 公開された検査なしの行アクセス。
        public: using Base::operator[];



        //---------------------------------------
        // メンバー.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese 行数を取得する。
        //!     \~english  Get the number of rows.
        //!
        //! @return Number of rows.
        //!
        public : size_t rows    ( void ) const { return this->height(); }

        //!
        //! @brief
        //!     \~japanese 列数を取得する。
        //!     \~english  Get the number of columns.
        //!
        //! @return Number of columns.
        //!
        public : size_t cols    ( void ) const { return this->width(); }


        //---------------------------------------
        // Constractor / Destractor.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese デフォルトコンストラクタ。
        //!     \~english  Default constructor.
        //!
        public: explicit Matrix_( void )
            : Map< _Tp > ()
        {
        }
        //!
        //! @brief
        //!     \~japanese 行数と列数を指定して行列を生成するコンストラクタ。
        //!     \~english  Constructor that creates a matrix with specified rows and columns.
        //!
        //! @param [in] row_in  Number of rows.
        //! @param [in] col_in  Number of columns.
        //!
        //! @note
        //!     \~japanese Map は幅(X)、高さ(Y) の順で設定するが、  
        //!     行列は行(X)、列(Y) の順で設定する。  
        //!     \~english  Map uses width (X), height (Y) order, while matrix uses row (X), column (Y) order.
        //!
        public : Matrix_( const size_t row_in, const size_t col_in )
            : Map< _Tp > ( col_in, row_in )
        {
        }
        //!
        //! @brief
        //!     \~japanese 行数、列数および要素初期値を指定して行列を生成するコンストラクタ。
        //!     \~english  Constructor that creates a matrix with specified rows, columns, and initial value.
        //!
        //! @param [in] row_in    Number of rows.
        //! @param [in] col_in    Number of columns.
        //! @param [in] value_in  Initial value for all elements.
        //!
        //! @note
        //!     \~japanese Map は幅(X)、高さ(Y) の順で設定するが、  
        //!     行列は行(X)、列(Y) の順で設定する。  
        //!     \~english  Map uses width (X), height (Y) order, while matrix uses row (X), column (Y) order.
        //!
        public: Matrix_( const size_t row_in, const size_t col_in, const _Tp value_in )
            : Map< _Tp > ( col_in, row_in, value_in )
        {
        }
        //!
        //! @brief
        //!     \~japanese 行数、列数および要素データを指定して行列を生成するコンストラクタ。
        //!     \~english  Constructor that creates a matrix with specified rows, columns, and element data.
        //!
        //! @param [in] row_in   Number of rows.
        //! @param [in] col_in   Number of columns.
        //! @param [in] data_in  Vector containing initial element values.
        //!
        //! @note
        //!     \~japanese Map は幅(X)、高さ(Y) の順で設定するが、  
        //!     行列は行(X)、列(Y) の順で設定する。  
        //!     \~english  Map uses width (X), height (Y) order, while matrix uses row (X), column (Y) order.
        //!
        public : Matrix_( const size_t row_in, const size_t col_in, const std::vector< _Tp >& data_in )
            : Map< _Tp > ( col_in, row_in, data_in )
        { }



        //-----------------------------------------------------------------------------------
        // オペレーター.
        //-----------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 行列同士の等号比較を行う。
        //!     \~english  Compare two matrices for equality.
        //!
        //! @param [in] obj  Object to compare.
        //! @retval true     Both matrices have the same dimensions and identical elements.
        //! @retval false    Dimensions differ or at least one element differs.
        //!
        public: bool operator == ( const Matrix_< _Tp, Enable >& obj )
        {
            if( ( this->cols() != obj.cols() ) || ( this->rows() != obj.rows() ) ) { return false;}

            const std::vector< _Tp > element = obj.data();
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                if( this->m_data[ index ] != element[ index ] ) { return false; }
            }
            return true;
        }

        //!
        //! @brief
        //!     \~japanese 行列同士の不等号比較を行う。
        //!     \~english  Compare two matrices for inequality.
        //!
        //! @param [in] obj  Object to compare.
        //! @retval true     Dimensions differ or at least one element differs.
        //! @retval false    Both matrices have the same dimensions and identical elements.
        //!
        public: bool operator != ( const Matrix_< _Tp, Enable >& obj )          //!< [in ] ムーブ元.
        {
            if( ( this->cols() != obj.cols() ) || ( this->rows() != obj.rows() ) ) { return true; }

            const std::vector< _Tp > element = obj.data();
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                if( this->m_data[ index ] != element[ index ] ) { return true; }
            }
            return false;
        }

        //!
        //! @brief
        //!     \~japanese 同一次元の行列同士を加算する代入演算子。
        //!     \~english  Add another matrix of the same dimensions (in-place).
        //!
        //! @param [in] obj  Object to add.
        //! @return Reference to this matrix.
        //!
        //! @note
        //!     \~english  Throws std::invalid_argument if dimensions do not match.
        //!
        public: Matrix_< _Tp >& operator += ( const Matrix_< _Tp, Enable >& obj )
        {
            if( ( this->cols() != obj.cols() ) || ( this->rows() != obj.rows() ) )
            {
                throw std::invalid_argument( "Matrix operator+= requires matching dimensions" );
            }
            const std::vector< _Tp > element = obj.data();
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                this->m_data[ index ] += element[ index ];
            }
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese 同一次元の行列同士を減算する代入演算子。
        //!     \~english  Subtract another matrix of the same dimensions (in-place).
        //!
        //! @param [in] obj  Object to subtract.
        //! @return Reference to this matrix.
        //!
        //! @note
        //!     \~english  Throws std::invalid_argument if dimensions do not match.
        //!
        public: Matrix_< _Tp, Enable >& operator -= ( const Matrix_< _Tp, Enable >& obj )
        {
            if( ( this->cols() != obj.cols() ) || ( this->rows() != obj.rows() ) )
            {
                throw std::invalid_argument( "Matrix operator-= requires matching dimensions" );
            }
            const std::vector< _Tp > element = obj.data();
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                this->m_data[ index ] -= element[ index ];
            }
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese スカラー倍を行う代入演算子。
        //!     \~english  Multiply all elements by a scalar (in-place).
        //!
        //! @param [in] scalar  Scalar value.
        //! @return Reference to this matrix.
        //!
        public: Matrix_< _Tp, Enable >& operator *= ( const _Tp scalar )
        {
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                this->m_data[ index ] *= scalar;
            }
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese スカラー除算を行う代入演算子。
        //!     \~english  Divide all elements by a scalar (in-place).
        //!
        //! @param [in] scalar  Scalar value.
        //! @return Reference to this matrix.
        //!
        //! @note
        //!     \~english  Throws std::domain_error if scalar is zero.
        //!
        public: Matrix_< _Tp, Enable >& operator /= ( const _Tp scalar )
        {
            if( scalar == _Tp( 0 ) )
            {
                throw std::domain_error( "Matrix operator/= by zero" );
            }
            for( size_t index = 0; index < this->m_data.size(); ++index )
            {
                this->m_data[ index ] /= scalar;
            }
            return *this;
        }

        //-----------------------------------------------------------------------------------
        // Specific Matrix Generator.
        //-----------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 指定次元の零行列（正方行列）を生成する。
        //!     \~english  Generate a zero square matrix of given dimension.
        //!
        //! @param [in] dimention_in  Dimension of the square matrix.
        //! @return Zero square matrix (_Tp 型).
        //!
        public: static Matrix_< _Tp, Enable > Square( const size_t dimention_in )
        {
            Matrix_<_Tp> mat( dimention_in, dimention_in, 0 );
            return mat;
        }

        //!
        //! @brief
        //!     \~japanese ベクトル diag_in の要素を対角に持つ対角行列を生成する。
        //!     \~english  Generate a diagonal matrix using elements of vector diag_in.
        //!
        //! @param [in] diag_in  Vector of diagonal elements.
        //! @return Diagonal matrix.
        //!
        public: static Matrix_<_Tp, Enable> Diagonal( const std::vector< _Tp >& diag_in )
        {
            Matrix_<_Tp> mat = Matrix_<_Tp>::Square( diag_in.size() );
            for( size_t i = 0; i < diag_in.size(); ++i )
            {
                mat[ i ][ i ] = static_cast< _Tp >( diag_in[i] );
            }
            return mat;
        }

        //!
        //! @brief
        //!     \~japanese 要素 scalar_in を対角に持つ対角行列を生成する。
        //!     \~english  Generate a diagonal matrix with the scalar_in on the diagonal.
        //!
        //! @param [in] scalar_in     Scalar value for diagonal elements.
        //! @param [in] dimention_in  Dimension of the square matrix.
        //! @return Diagonal matrix with scalar_in on diagonal.
        //!
        public: static Matrix_<_Tp, Enable> Scalar( const _Tp scalar_in, const size_t dimention_in )
        {
            const std::vector<_Tp> diag( dimention_in, static_cast< _Tp >( scalar_in ) );
            return Matrix_<_Tp, Enable>::Diagonal( diag );
        }

        //!
        //! @brief
        //!     \~japanese 単位行列を生成する。
        //!     \~english  Generate an identity matrix.
        //!
        //! @param [in] dimention_in  Dimension of the identity matrix.
        //! @return Identity matrix.
        //!
        public: static Matrix_<_Tp, Enable> Identify( const size_t dimention_in )
        {
            Matrix_<_Tp, Enable> mat = Matrix_<_Tp>::Square( dimention_in );
            return Matrix_<_Tp, Enable>::Scalar( static_cast< _Tp >( 1 ), dimention_in );
        }

        //!
        //! @brief
        //!     \~japanese 2次元空間の回転行列を生成する。
        //!     \~english  Generate a 2D rotation matrix.
        //!
        //! @param [in] radian_in  Rotation angle in radians.
        //! @return 2D rotation matrix.
        //!
        public: static Matrix_<_Tp, Enable> Rotation2D( const _Tp radian_in );

        //!
        //! @brief
        //!     \~japanese 3次元空間の回転行列を生成する。
        //!     \~english  Generate a 3D rotation matrix.
        //!
        //! @param [in] radian_in     Rotation angle in radians.
        //! @param [in] direction_in  Rotation direction (eDOR).
        //! @return 3D rotation matrix.
        //!
        public: static Matrix_<_Tp, Enable> Rotation3D( const _Tp radian_in, const eDOR direction_in );

        //!
        //! @brief
        //!     \~japanese 入力データから共分散行列を計算し、平均値を p_mean_out に返す。
        //!     \~english  Compute the covariance matrix from input data_in and return the mean in p_mean_out.
        //!
        //! @param[out] p_mean_out  Pointer to store mean matrix.
        //! @param [in] data_in   Vector of input data matrices.
        //! @return Covariance matrix.
        //!
        //! @note
        //!     \~japanese 標本共分散 (N-1 で除算) を計算する。data_in は同一次元の列ベクトル2個以上、
        //!     p_mean_out は非 null であること。満たさない場合は std::invalid_argument を投げる。
        //!     \~english  Computes the sample covariance (divided by N-1). data_in must hold two or more
        //!     column vectors of identical dimension and p_mean_out must not be null;
        //!     otherwise throws std::invalid_argument.
        //!
        public: static Matrix_<_Tp, Enable> Covariance( Matrix_< _Tp, Enable >* p_mean_out, const std::vector< Matrix_< _Tp, Enable > >& data_in );

        //!
        //! @brief
        //!     \~japanese 入力ベクトル data_in から共分散行列を計算し、平均値を p_mean_out に返す。
        //!     \~english  Compute the covariance matrix from input vector data_in and return the mean in p_mean_out.
        //!
        //! @param[out] p_mean_out  Pointer to store mean value.
        //! @param [in] data_in   Vector of input data values.
        //! @return Covariance matrix.
        //!
        //! @note
        //!     \~japanese 標本共分散 (N-1 で除算) を計算する。data_in は2要素以上、p_mean_out は非 null で
        //!     あること。満たさない場合は std::invalid_argument を投げる。
        //!     \~english  Computes the sample covariance (divided by N-1). data_in must hold two or more
        //!     values and p_mean_out must not be null; otherwise throws std::invalid_argument.
        //!
        public: static Matrix_<_Tp, Enable> Covariance( _Tp* p_mean_out, const std::vector< _Tp >& data_in );



        //-----------------------------------------------------------------------------------
        // Specific Function.
        //-----------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 正方行列かどうかを判定する（内部使用）。
        //!     \~english  Check whether the matrix is square (internal use).
        //!
        //! @return true if square matrix, false otherwise.
        //!
        private: inline bool isSquare( void )const { return this->cols() == this->rows(); }


        //!
        //! @brief
        //!     \~japanese 次元数（行数＝列数）を取得する（正方行列でない場合は例外を投げる）。
        //!     \~english  Get the dimension (rows = columns) for square matrix (throws if not square).
        //!
        //! @return Dimension of the square matrix.
        //!
        private: size_t getDimention( void )const 
        {
            if( !this->isSquare() ) { throw std::invalid_argument( "the operation requires a square matrix" ); }
            return this->cols(); 
        }

        //!
        //! @brief
        //!     \~japanese 行列式を計算する。
        //!     \~english  Compute the determinant of the matrix.
        //!
        //! @note
        //!     \~japanese 3次以上は部分ピボット選択付きガウス消去 (O(n^3)) で計算する。
        //!     \~english  For dimension 3 or higher, computed by Gaussian elimination with
        //!     partial pivoting (O(n^3)).
        //!
        //! @return Determinant value.
        //!
        public: _Tp determinant( void ) const;

        //!
        //! @brief
        //!     \~japanese 転置行列を計算する。
        //!     \~english  Compute the transpose of the matrix.
        //!
        //! @return Transposed matrix.
        //!
        public: Matrix_< _Tp, Enable > transpose( void ) const;

        //!
        //! @brief
        //!     \~japanese 逆行列の計算を試みる。
        //!     \~english  Attempt to compute the inverse of the matrix.
        //!
        //! @note
        //!     \~japanese 部分ピボット選択付きガウス・ジョルダン法による計算を行う。特異行列、および
        //!     浮動小数点の丸め誤差の範囲で特異とみなされる行列は `SingularMatrix` の失敗
        //!     `CoreResult` を返す。正常な入力でも起こり得る失敗のためExceptionにしない。
        //!     正方行列でない場合だけ、使い方違反として `std::invalid_argument` を投げる。
        //!     \~english  Uses Gauss-Jordan elimination with partial pivoting. A singular matrix, or a
        //!     matrix singular within floating-point rounding tolerance, returns a failed
        //!     `CoreResult` with `SingularMatrix` because it can occur on well-formed input.
        //!     Only a non-square matrix is a usage violation and throws `std::invalid_argument`.
        //!
        //! @return \~japanese 逆行列を運ぶ`CoreResult`. \~english The `CoreResult` carrying the inverse.
        //!
        public: CoreResult< Matrix_< _Tp, Enable > > tryInverse( void ) const;


    };

    typedef Matrix_< F32 > Matrix_f;       //!< \~english 32-bit float matrix. \~japanese 32ビット浮動小数点行列。
    typedef Matrix_< F64 > Matrix_d;       //!< \~english 64-bit float matrix. \~japanese 64ビット浮動小数点行列。
    typedef Matrix_< F64 > Matrix;         //!< \~english Default double matrix. \~japanese デフォルトは double 行列。

    typedef Matrix_< F32 > float32_matrix; //!< \~english 32-bit float matrix. \~japanese 32ビット浮動小数点行列。
    typedef Matrix_< F64 > float64_matrix; //!< \~english 64-bit float matrix. \~japanese 64ビット浮動小数点行列。


    //typedef template< typename _Tp > std::array< Matrix_< _Tp >, 3 > PLU;

    //!
    //! @brief
    //!     \~japanese 加算オペレーター。obj1 + obj2 の結果を返す。
    //!     \~english  Addition operator returning obj1 + obj2.
    //!
    //! @param [in] obj1  First matrix.
    //! @param [in] obj2  Second matrix.
    //! @return Resulting matrix.
    //!
    template<typename _Tp>
    static Matrix_< _Tp > operator + ( const Matrix_< _Tp >& obj1, const Matrix_< _Tp >& obj2 )
    {
        Matrix_< _Tp > tmp( obj1 );
        tmp += obj2;
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese 減算オペレーター。obj1 - obj2 の結果を返す。
    //!     \~english  Subtraction operator returning obj1 - obj2.
    //!
    //! @param [in] obj1  First matrix.
    //! @param [in] obj2  Second matrix.
    //! @return Resulting matrix.
    //!
    template<typename _Tp>
    static Matrix_< _Tp > operator - ( const Matrix_< _Tp >& obj1, const Matrix_< _Tp >& obj2 )
    {
        Matrix_< _Tp > tmp( obj1 );
        tmp -= obj2;
        return tmp;
    }
    
    //!
    //! @brief
    //!     \~japanese 行列乗算オペレーター。obj1 * obj2 の結果を返す。
    //!     \~english  Matrix multiplication operator returning obj1 * obj2.
    //!
    //! @param [in] obj1  Left-hand matrix.
    //! @param [in] obj2  Right-hand matrix.
    //! @return Resulting matrix.
    //!
    //! @note
    //!     \~english  Throws std::invalid_argument if obj1.cols() != obj2.rows().
    //!
    template<typename _Tp>
    static Matrix_< _Tp > operator * ( const Matrix_< _Tp >& obj1, const Matrix_< _Tp >& obj2 )
    {
        if( obj1.cols() != obj2.rows() ) { throw std::invalid_argument( "Matrix multiplication requires obj1.cols() == obj2.rows()" ); }

        Matrix_< _Tp > mat( obj1.rows(), obj2.cols() );
        // 行列の乗算を実行
        for( size_t i = 0; i < obj1.rows(); i++ )
        {
            for( size_t j = 0; j < obj2.cols(); j++ )
            {
                for( size_t k = 0; k < obj1.cols(); k++ )
                {
                    mat[ i ][ j ] += obj1[ i ][ k ] * obj2[ k ][ j ];
                }
            }
        }
        return mat;
    }

    //!
    //! @brief
    //!     \~japanese 行列とスカラーの乗算オペレーター。obj1 * scalar の結果を返す。
    //!     \~english  Matrix-scalar multiplication operator returning obj1 * scalar.
    //!
    //! @param [in] obj1    Matrix object.
    //! @param [in] scalar  Scalar value.
    //! @return Resulting matrix.
    //!
    template<typename _Tp>
    inline static Matrix_< _Tp > operator * ( const Matrix_< _Tp >& obj1, const _Tp scalar )
    {
        Matrix_< _Tp > tmp( obj1 );
        tmp *= scalar;
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese スカラーと行列の乗算オペレーター。scalar * obj の結果を返す。
    //!     \~english  Scalar-matrix multiplication operator returning scalar * obj.
    //!
    //! @param [in] scalar  Scalar value.
    //! @param [in] obj     Matrix object.
    //! @return Resulting matrix.
    //!
    template<typename _Tp>
    inline static Matrix_< _Tp > operator * ( const _Tp scalar, const Matrix_< _Tp >& obj )
    {
        Matrix_< _Tp > tmp( obj );
        tmp *= scalar;
        return tmp;
    }

    //!
    //! @brief
    //!     \~japanese 行列とスカラーの除算オペレーター。obj1 / scalar の結果を返す。
    //!     \~english  Matrix-scalar division operator returning obj1 / scalar.
    //!
    //! @param [in] obj1    Matrix object.
    //! @param [in] scalar  Scalar value.
    //! @return Resulting matrix.
    //!
    template<typename _Tp>
    inline static Matrix_< _Tp > operator / ( const Matrix_< _Tp >& obj1, const _Tp scalar )
    {
        Matrix_< _Tp > tmp( obj1 );
        tmp /= scalar;
        return tmp;
    }

};


#if defined(_MSC_VER)
#pragma warning(pop)
#endif


#endif //WONDERSTEWENGINE_DATA_MATRIX_H