//*****************************************************************************************************************
//! 
//! @file    ColorMatrix.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2022/03/11   Create New WapitiStew
//!
//! 
//! @brief
//!   \~japanese 色変換行列クラス `ColorMatrix` の定義
//!   \~english  Definition of color transformation matrix class `ColorMatrix`
//!
//! 
//! @details
//!   \~japanese
//!     本ファイルでは、3x3の色変換行列を扱うクラス `wse::tmr::ColorMatrix` を定義する。  
//!     RGB <-> XYZ 変換、行列の要素アクセス、設定、文字列化などの基本操作を提供する。  
//!     行列型は `float64_matrix` を使用し、演算や逆行列処理も対応。  
//!
//!     - 行列要素は `matrix()` / `get()` で取得、`set()` で設定可能  
//!     - `rgb2xyz()` / `xyz2rgb()` により順方向・逆方向の変換が可能  
//!     - 変換には3次元ベクトルを受け取り、テンプレートによる柔軟な型変換にも対応  
//!
//!   \~english
//!     This file defines `wse::tmr::ColorMatrix`, a class that manages a 3x3 color transformation matrix.  
//!     Provides basic operations such as RGB <-> XYZ conversion, matrix element access, setting, and string output.  
//!     Uses `float64_matrix` type for internal representation and supports arithmetic and inverse operations.  
//!
//!     - Matrix elements can be retrieved via `matrix()` or `get()`, and modified with `set()`  
//!     - `rgb2xyz()` / `xyz2rgb()` provide forward and inverse color space conversions  
//!     - Supports template overloads for flexible input types  
//!
//! 
//! @note
//!   \~japanese このクラスは色情報の線形変換に特化しており、Gamma補正など非線形変換は含まない。
//!   \~english  This class is specialized for linear color transformations; nonlinear operations like gamma correction are not included.
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
#ifndef WONDERSTEWENGINE_TURTLECAMERALIB_DATA_COLORMATRIX_H
#define WONDERSTEWENGINE_TURTLECAMERALIB_DATA_COLORMATRIX_H

// Dynamic Library define
#include <stdexcept>
#include "../../dynamic.h"

// C/C++
#include "../depend/STD.h"

// Common data
#include "../../wse/stew.h"

namespace wse
{
namespace tmr
{
    //! 
    //! @brief 
    //!     \~japanese 色変換行列クラス（RGB⇔XYZ変換）
    //!     \~english  Color transformation matrix class (RGB⇔XYZ conversion)
    //!
    //! @details
    //!     \~japanese
    //!         本クラスは、RGBとXYZの色空間間の相互変換を行うための3x3の色変換行列を管理する。
    //!         内部では `float64_matrix` 型を使用し、double精度での変換演算を提供する。
    //!         行列全体の設定・取得、個別要素のアクセスに加え、RGB⇔XYZ間の相互変換をサポートする。
    //!         テンプレート関数により、整数／浮動小数点いずれの形式の入力にも対応。
    //!
    //!     \~english
    //!         This class manages a 3x3 color transformation matrix for RGB ⇔ XYZ conversion.
    //!         Internally, it uses the `float64_matrix` type and provides high-precision (double) conversion operations.
    //!         It supports full matrix set/get operations, element-wise access, and bidirectional RGB⇔XYZ transformations.
    //!         Template functions allow input of either integer or floating-point types.
    //!
    //! @note
    //!     \~japanese
    //!         - 初期状態ではゼロ行列に初期化される。
    //!         - XYZ→RGB変換は、行列が可逆である必要がある。
    //!
    //!     \~english
    //!         - The matrix is initialized to a zero matrix by default.
    //!         - XYZ→RGB conversion requires the matrix to be invertible.
    //!
    //! @par
    //!     \~japanese 
    //!         主な機能:
    //!             - 色変換行列の設定／取得: set(), matrix()
    //!             - 個別要素アクセス: get(row, col)
    //!             - RGB → XYZ変換: rgb2xyz()
    //!             - XYZ → RGB変換: xyz2rgb()
    //!             - 行列の文字列化: str()
    //! 
    //!     \~english  
    //!         Key Features:
    //!             - Set/Get color matrix: set(), matrix()
    //!             - Element-wise access: get(row, col)
    //!             - RGB → XYZ transformation: rgb2xyz()
    //!             - XYZ → RGB transformation: xyz2rgb()
    //!             - Stringify matrix: str()
    //!
    //! @code
    //!     \~japanese
    //!     // 変換行列を設定し、RGB値をXYZに変換する例
    //!     \~english
    //!     // Example: Set transformation matrix and convert RGB to XYZ
    //!     
    //!     Parameters:
    //!     ColorMatrix matrix;
    //!     matrix.set( my_matrix );
    //!     double_xyz xyz = matrix.rgb2xyz( 0.5, 0.5, 0.5 );
    //! @endcode
    //! 
    class WSE_API ColorMatrix 
    {
        // 定数.
        public : static constexpr int8_t MATRIX_ROWS = 3            ;                   //!< 行列サイズ.
        public : static constexpr int8_t MATRIX_COLS = 3            ;                   //!< 行列サイズ.
        public : static constexpr int8_t MATRIX_SIZE = MATRIX_ROWS * MATRIX_COLS;       //!< 行列サイズ.


        //------------------------------------------------------------------------------------------
        // Member variables
        //------------------------------------------------------------------------------------------

        private : float64_matrix m_matrix   ;       //!< Matrix data

        //!
        //! @brief 
        //!     \~japanese  スワップ
        //!     \~english   swap
        //! 
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //! 
        friend void swap( ColorMatrix &obj1_inout, ColorMatrix &obj2_inout )
        {
            std::swap( obj1_inout.m_matrix   , obj2_inout.m_matrix   );
        }


        //------------------------------------------------------------------------------------------
        // Getter
        //------------------------------------------------------------------------------------------

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 色変換行列を取得する.
        //!     \~english   [Getter] Gets Color transformation matrix
        //! 
        //! @return Color transformation matrix
        //! 
        public : float64_matrix matrix ( void ) const { return this->m_matrix   ; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 色変換行列の要素を取得する.
        //!     \~english   [Getter] Gets a element of Color transformation matrix
        //! 
        //! @param [in] row_in  Matrix Row Index (0-2)
        //! @param [in] col_in  Matrix Colmun Index (0-2)
        //! 
        //! @return Element
        //! 
        public : double get(
                const int32_t row_in
            ,   const int32_t col_in
        )const
        {
            return this->m_matrix[row_in][col_in];
        }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 色変換行列を設定する.
        //!     \~english   [Setter] Sets a Color transformation matrix
        //! 
        //! @param [in] matrix_in  Matrix
        //! 
        public : void set(
                const float64_matrix   &matrix_in     //!< [in] データ.
        )
        {
            this->m_matrix  = matrix_in ;
        }


        //------------------------------------------------------------------------------------------
        // Constractor / Destractor
        //------------------------------------------------------------------------------------------

        //! 
        //! @brief  Default constoractor.
        //! 
        public : explicit
        ColorMatrix( void )
            : m_matrix ( MATRIX_ROWS, MATRIX_COLS, 0 )
        {
        }
        //! 
        //! @brief  Destructor.
        //! 
        public : virtual
        ~ColorMatrix( void )
        {
        }
        //! 
        //! @brief  Constoractor.
        //! 
        //! @param [in] matrix_in  Matrix
        //! 
        public : explicit
        ColorMatrix( 
                const float64_matrix &matrix_in     //!< θ -> Pix
        )
            : m_matrix ( matrix_in )
        {
        }
        //! 
        //! @brief Copy constoractor.
        //! @param [in] obj_in Copy src object.
        //! 
        ColorMatrix(const ColorMatrix &obj_in)
            : m_matrix ( obj_in.m_matrix )
        {
        }

        //! 
        //! @brief Move constoractor.
        //! @param [in,out] obj_inout Muve src object.
        //! 
        ColorMatrix(ColorMatrix &&obj_inout) noexcept
            : m_matrix ( std::move( obj_inout.m_matrix   ) )
        {
        }

        //! 
        //! @brief Copy operator.
        //! 
        //! @param [in ] obj Copy object
        //! 
        ColorMatrix &operator=(const ColorMatrix &obj)
        {
            if( this != &obj )
            {
                ColorMatrix tmp( obj );
                swap( *this, tmp );
            }
            return *this;
        }

        //! 
        //! @brief Move operator.
        //! 
        //! @param [in ] obj Move object
        //! 
        ColorMatrix &operator=(ColorMatrix &&obj) noexcept
        {
            if( this != &obj )
            {
                ColorMatrix tmp( std::move( obj ) );
                swap( *this, tmp );
            }
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese  [Setter] RGB->XYZ 変換.
        //!     \~english   [Setter] Transform RGB to XYZ.
        //! 
        //! @param [in] rgb_in     RGB
        //! 
        //! @return XYZ
        //! 
        public : 
        double_xyz rgb2xyz(                //!< @return XYZ
                const std::array< double, 3 >&  rgb_in    //!< [in ] RGB.
        )const
        {
            const float64_matrix rgb_mat( 3, 1, { rgb_in[0], rgb_in[1], rgb_in[2] } );
            const float64_matrix xyz_mat = this->m_matrix * rgb_mat;
            return double_xyz( xyz_mat[0][0], xyz_mat[1][0], xyz_mat[2][0] );
        }

        //!
        //! @brief
        //!     \~japanese  [Setter] RGB->XYZ 変換.
        //!     \~english   [Setter] Transform RGB to XYZ.
        //! 
        //! @param [in] red_in    R
        //! @param [in] green_in  G
        //! @param [in] blue_in   B
        //! 
        //! @return XYZ
        //! 
        public : template< typename TYPE >
        double_xyz rgb2xyz(                //!< @return XYZ
                const TYPE   red_in    //!< [in ] RGB.
            ,   const TYPE   green_in    //!< [in ] RGB.
            ,   const TYPE   blue_in    //!< [in ] RGB.
        )const
        {
            const std::array< double, 3 > rgb = { static_cast< double >( red_in ), static_cast< double >( green_in ), static_cast< double >( blue_in ) };
            return rgb2xyz( rgb );
        }

        //!
        //! @brief
        //!     \~japanese  [Setter] XYZ->RGB 変換.
        //!     \~english   [Setter] Transform XYZ to RGB.
        //! 
        //! @param [in] xyz_in    XYZ
        //! 
        //! @return XYZ
        //! 
        public : 
        std::array< double, 3 > xyz2rgb(                //!< @return XYZ
                const double_xyz   &xyz_in    //!< [in ] RGB.
        )const
        {
            const float64_matrix xyz_mat( 3, 1, { xyz_in.x, xyz_in.y, xyz_in.z } );
            // 設定済みColor Matrixが特異なら変換は定義できない. 設定Dataの誤りとして報告する.
            const wse::CoreResult< float64_matrix > inverse = this->m_matrix.tryInverse();
            if( !inverse.succeeded() )
            {
                throw std::domain_error( "the configured color matrix is singular" );
            }
            const float64_matrix rgb_mat = inverse.value() * xyz_mat;
            const std::array< double, 3 > rgb = { rgb_mat[0][0], rgb_mat[1][0], rgb_mat[2][0] };
            return rgb;
        }
        
        //!
        //! @brief
        //!     \~japanese  [Setter] XYZ->RGB 変換.
        //!     \~english   [Setter] Transform XYZ to RGB.
        //! 
        //! @param [in] x_in  X
        //! @param [in] y_in  Y
        //! @param [in] z_in  Z
        //! 
        //! @return XYZ
        //! 
        public : template< typename TYPE >
        std::array< double, 3 > xyz2rgb(
                const TYPE   x_in
            ,   const TYPE   y_in
            ,   const TYPE   z_in
        )const
        {
            const double_xyz xyz ( static_cast< double >( x_in ), static_cast< double >( y_in ), static_cast< double >( z_in ) );
            return xyz2rgb( xyz );
        }

        //!
        //! @brief
        //!     \~japanese  [Setter] 色変換行列の文字列化.
        //!     \~english   [Setter] Color matrix Stringification.
        //! 
        //! @return String
        //! 
        public: std::string str (  )const
        {
            return this->m_matrix.str();
        }

    };

};
};

#endif //TURTLE_CAMERA_LIBRALY_INTERFACE_EXTURNAL_FUNCTION_H
