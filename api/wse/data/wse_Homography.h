//*****************************************************************************************************************
//! 
//! @file    wse_Homography.h
//! @brief   射影変換用の行列クラス.
//! @author  
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//! 
//!
//! @brief
//!     \~japanese ホモグラフィ行列を扱うクラスを定義するヘッダファイル.
//!     \~english  Header file that defines a class for handling homography matrix.
//!
//! 
//! @details
//!     \~japanese
//!         本ファイルは、2D間のホモグラフィ変換を扱う `wse::Homography` クラスを定義する。
//!         点群間の射影変換行列を計算し、floatまたはdouble精度の座標で対応可能。
//!
//!     \~english
//!         This file defines the `wse::Homography` class for handling 2D homography transformations.
//!         It computes a projection matrix from pairs of corresponding points with float or double precision.
//!
//! 
//! @note
//!     \~japanese Matrixを継承し3x3の変換行列として使用される。
//!     \~english  Inherits from Matrix and is used as a 3x3 transformation matrix.
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

#ifndef WONDERSTEWENGINE_DATA_MATRIX_HOMOGRAPHY_H
#define WONDERSTEWENGINE_DATA_MATRIX_HOMOGRAPHY_H

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#endif
// include
#include <stdexcept>
#include "../depend/wse_STD.h"

#include "wse_Matrix.h"
#include "wse_Point2D.h"
#include "wse_TiePoint.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4514)  // 参照されていないインライン関数は削除の警告.
#endif
namespace wse
{
    
    //!
    //! @class Homography
    //!
    //! @brief
    //!     \~japanese ホモグラフィ変換行列を管理・適用するクラス.
    //!     \~english  Class for managing and applying homography transformation matrices.
    //!
    //! @details
    //!     \~japanese
    //!         本クラスは3x3のホモグラフィ行列を内部に保持し、対応点から行列を計算・適用する機能を提供する。
    //!         Point2_<T>、f64_xy_xy、f32_xy_xy など多様な型からsetup可能。
    //!         変換行列はMatrixクラスを継承して管理され、transform関数により任意点に変換を適用できる。
    //!
    //!     \~english
    //!         This class maintains a 3x3 homography matrix and provides functions to compute and apply it.
    //!         It supports multiple point types such as Point2_<T>, f64_xy_xy, f32_xy_xy for input.
    //!         The matrix is stored via inheritance from Matrix, and transform can apply the result to any point.
    //!
    //! @note
    //!     \~japanese Matrixクラスを継承し3x3の行列を扱う。入力点数により例外が発生する可能性がある。
    //!     \~english  Inherits from Matrix and handles a 3x3 matrix. May throw exceptions if input size mismatches.
    //!
    class WSE_API Homography : public Matrix
    {

        // エイリアスで基底クラスを定義
        using Base = Matrix;
        public: using Base::Base;
        //! \~english Public unchecked row access. \~japanese 公開された検査なしの行アクセス。
        public: using Base::operator[];


        //!
        //! @brief
        //!     \~japanese 2D座標の対応点からホモグラフィを設定する（double型）.
        //!     \~english  Set homography using 2D corresponding points (double precision).
        //!
        //! @param [in] src_in  Source points.
        //! @param [in] dst_in  Destination points.
        //!
        public: void setup( const std::vector< float64_xy >& src_in, const std::vector< float64_xy >& dst_in );

        //!
        //! @brief
        //!     \~japanese 2D座標の対応点からホモグラフィを設定する（float型）.
        //!     \~english  Set homography using 2D corresponding points (float precision).
        //!
        //! @param [in] src_in  Source points.
        //! @param [in] dst_in  Destination points.
        //!
        public: void setup( const std::vector< float32_xy >& src_in, const std::vector< float32_xy >& dst_in )
        {
            if( src_in.size() != dst_in.size() ){ throw std::invalid_argument( "Homography::setup requires matching point lists" ); }

            std::vector< float64_xy > src_tmp;
            std::vector< float64_xy > dst_tmp;
            for( size_t i = 0; i < src_in.size(); ++i )
            {
                src_tmp.emplace_back( float64_xy( src_in[i] ) );
                dst_tmp.emplace_back( float64_xy( dst_in[i] ) );
            }
            setup( src_tmp, dst_tmp );
        }

        //!
        //! @brief
        //!     \~japanese 対応点ペアからホモグラフィを設定する（double型構造）.
        //!     \~english  Set homography using point pair structure (double precision).
        //!
        //! @param [in] pts_in  Vector of point pairs (src/dst).
        //!
        public: void setup( const std::vector< f64_xy_xy >& pts_in )
        {
            std::vector< float64_xy > src_tmp;
            std::vector< float64_xy > dst_tmp;
            for( size_t i = 0; i < pts_in.size(); ++i )
            {
                src_tmp.emplace_back( float64_xy( pts_in[i].src ) );
                dst_tmp.emplace_back( float64_xy( pts_in[i].dst ) );
            }
            setup( src_tmp, dst_tmp );
        }

        //!
        //! @brief
        //!     \~japanese 対応点ペアからホモグラフィを設定する（float型構造）.
        //!     \~english  Set homography using point pair structure (float precision).
        //!
        //! @param [in] pts_in  Vector of point pairs (src/dst).
        //!
        public: void setup( const std::vector< f32_xy_xy >& pts_in )
        {
            std::vector< float64_xy > src_tmp;
            std::vector< float64_xy > dst_tmp;
            for( size_t i = 0; i < pts_in.size(); ++i )
            {
                src_tmp.emplace_back( float64_xy( pts_in[i].src ) );
                dst_tmp.emplace_back( float64_xy( pts_in[i].dst ) );
            }
            setup( src_tmp, dst_tmp );
        }

        //---------------------------------------------
        // コンストラクタ
        //---------------------------------------------

        //!
        //! @brief Default constructor.
        //!
        public : Homography( void )
            : Matrix ( 3, 3 )
        {
        }
        //!
        //! @brief Constructor from double point vector.
        //!
        //! @param [in] src_in  Source points.
        //! @param [in] dst_in  Destination points.
        //!
        public : Homography( const std::vector< Point2_< F64 > >&src_in, const std::vector< Point2_< F64 > >& dst_in )
            : Matrix ( 3, 3 )
        {
            setup( src_in, dst_in );
        }

        //!
        //! @brief Constructor from float point vector.
        //!
        //! @param [in] src_in  Source points.
        //! @param [in] dst_in  Destination points.
        //!
        public : Homography( const std::vector< Point2_< F32 > >& src_in, const std::vector< Point2_< F32 > >& dst_in )
            : Matrix ( 3, 3 )
        {
            setup( src_in, dst_in );
        }

        //!
        //! @brief Constructor from point pair (double).
        //!
        //! @param [in] pts_in  Vector of point pairs.
        //!
        public : Homography( const std::vector< f64_xy_xy >&pts_in )
            : Matrix ( 3, 3 )
        {
            setup( pts_in );
        }

        //!
        //! @brief Constructor from point pair (float).
        //!
        //! @param [in] pts_in  Vector of point pairs.
        //!
        public: Homography( const std::vector< f32_xy_xy >& pts_in )
            : Matrix ( 3, 3 )
        {
            setup( pts_in );
        }

        //---------------------------------------
        // 関数.
        //---------------------------------------

        //!
        //! @brief
        //!     \~japanese 入力座標にホモグラフィ変換を適用する.
        //!     \~english  Apply homography transformation to input coordinate.
        //!
        //! @param [in] pt_in  Input coordinate.
        //! @return Transformed coordinate.
        //!
        public : float64_xy transform( const float64_xy& pt_in )const;

    };

};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_DATA_MATRIX_H