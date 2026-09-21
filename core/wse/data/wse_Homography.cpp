//*****************************************************************************************************************
//! 
//! @file    wse_Homography.cpp
//! @brief   \~japanese 対応点からのHomography推定とその適用の実装.
//! @brief   \~english  Implements homography estimation from correspondences and its application.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @details  
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
#include <wse/data/wse_Homography.h>
#include <wse/data/wse_Point2D.h>
#include <wse/data/wse_Point3D.h>

// External Library
// OpenCV


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre スイッチが .
#endif
namespace wse
{
    static constexpr size_t MATRIX_SIZE = 9;
    static constexpr size_t HOMOGRAPHY_MATRIX_ROWS = 3;
    static constexpr size_t HOMOGRAPHY_MATRIX_COLS = 3;


    /*!
    @brief		x,y指定を一次元のIDに変換する.
    @author 	WapitiStew
    @details
        対応付けテーブルは、実態は一次元だが使う人は2次元的に使用するため
        2次元的に要素を指定されたときに実態の要素番号に変換する.
    */
    static size_t getIndex( const size_t x_in,   const size_t y_in )
    { 
        return ( ( HOMOGRAPHY_MATRIX_COLS * y_in ) + x_in ); 
    }

            
    /*!
    @brief		単位正方形からアファイン平面上(DST座標系)の4点への射影変換を求める..
    @author 	WapitiStew

        */
    static std::vector< double > CalculateHomographyUnitSquareToQuadrangle(                //!< @return アファイン平面上(DST座標系)の4点への射影変換行列.
        const std::vector< float64_xy>& quadrangle_in										//!< [in ] DST座標へのの4点.
    )
    {
            
        //!@todo Point をconstでマイナスしたい.
        // 第0頂点を原点とした場合の第1～第3頂点の位置.
        float64_xy offset = quadrangle_in[ 0 ];
        float64_xy vector_1 = quadrangle_in[ 1 ] - offset;
        float64_xy vector_2 = quadrangle_in[ 2 ] - offset;
        float64_xy vector_3 = quadrangle_in[3] - offset;


        // 三角形分割した面積を求める.
        const double area_0 = vector_1.x * vector_3.y - vector_1.y * vector_3.x;		// 頂点0を挟む三角形の面積.
        const double area_1 = vector_1.x * vector_2.y - vector_1.y * vector_2.x;		// 頂点1を挟む三角形の面積.
        const double area_3 = vector_2.x * vector_3.y - vector_2.y * vector_3.x;		// 頂点3を挟む三角形の面積.
        const double area_2 = area_3+ area_1 - area_0;									// 頂点2を挟む三角形の面積.
            
        // 射影変換行列の係数を求める.
        const double  coef_x = area_0 - area_1;
        const double  coef_y = area_0 - area_3;
        const double& coef_z = area_2;

        std::vector< double > homography( MATRIX_SIZE );
        homography[ getIndex( 0, 2 ) ] = coef_x;
        homography[ getIndex( 1, 2 ) ] = coef_y;
        homography[ getIndex( 2, 2 ) ] = coef_z;
        homography[ getIndex( 0, 0 ) ] = area_3 * vector_1.x + coef_x * offset.x;
        homography[ getIndex( 1, 0 ) ] = area_1 * vector_3.x + coef_y * offset.x;
        homography[ getIndex( 2, 0 ) ] = 0.0                 + coef_z * offset.x;
        homography[ getIndex( 0, 1 ) ] = area_3 * vector_1.y + coef_x * offset.y;
        homography[ getIndex( 1, 1 ) ] = area_1 * vector_3.y + coef_y * offset.y;
        homography[ getIndex( 2, 1 ) ] = 0.0                 + coef_z * offset.y;
            
        return homography;
    }


    /*!
    @brief		3ｘ3行列の約分する.
    @author 	WapitiStew
    @return 約分結果.
        */
    template< typename _Tp >
    static std::vector< _Tp > reduceMatrix
    (
        const std::vector< _Tp >& matrix_in       //!< 3x3行列.
    )
    {
        _Tp max = ( _Tp ) 0;


        for( size_t i = 0; i < matrix_in.size(); ++i )
        {
            _Tp value = std::abs( matrix_in[ i ] );
            if( value > max )
            {
                max = value;
            }
        }

        std::vector<_Tp> output( matrix_in.size() );
        for( size_t i = 0; i < matrix_in.size(); ++i )
        {
            output[ i ] = matrix_in[ i ] / max;
        }

        return output;
    }

    /*!
    @brief		3x3行列どうしの掛け算.
    @author 	WapitiStew

    */
    static std::vector< double > productMatrix(              //!< @return 掛け算結果.
        const std::vector< double >& left_in,          //!< [in ] 左項.
        const std::vector< double >& right_in          //!< [in ] 右項.
    )
    {
        std::vector<double> output( MATRIX_SIZE );
        for( size_t j = 0; j < HOMOGRAPHY_MATRIX_ROWS; ++j )
        {
            for( size_t i = 0; i < HOMOGRAPHY_MATRIX_COLS; ++i )
            {
                double value = 0.0;
                for( size_t k = 0; k < HOMOGRAPHY_MATRIX_COLS; ++k )
                {
                    value += left_in[ getIndex( k, j ) ] * right_in[ getIndex( i, k ) ];
                }
                output[ getIndex( i, j ) ] = value;
            }
        }
        return output;
    }

         
    /*!
    @brief		アファイン平面上の4点から単位正方形への射影変換を求める.
    @author 	WapitiStew
    */
    static std::vector< double > CalculateHomographyQuadrangleToUnitSquare(       //!< @return 単位正方形への射影変換行列. 
        const std::vector< float64_xy>& quadrangle_in						        //!< [in ] SRC座標の4点.
    )
    {
        // 第0頂点を原点とした場合の第1～第3頂点の位置.
        float64_xy offset = quadrangle_in[ 0 ];
        float64_xy vector_1 = quadrangle_in[ 1 ] - offset;
        float64_xy vector_2 = quadrangle_in[ 2 ] - offset;
        float64_xy vector_3 = quadrangle_in[3] - offset;
            
        // 三角形分割した面積を求める.
        const double area_0 = vector_1.x * vector_3.y - vector_1.y * vector_3.x;		// 頂点0を挟む三角形の面積.
        const double area_1 = vector_1.x * vector_2.y - vector_1.y * vector_2.x;		// 頂点1を挟む三角形の面積.
        const double area_3 = vector_2.x * vector_3.y - vector_2.y * vector_3.x;		// 頂点3を挟む三角形の面積.
        const double area_2 = area_3+ area_1 - area_0;									// 頂点2を挟む三角形の面積.
            
        // 0除算チェック (面積0チェック).
        if( ( 0 == area_0 ) || ( 0 == area_3) || ( 0 == area_1 ) || ( 0 == area_2 ) )
        {
            //!@todo エラー処理.
            throw std::runtime_error( "ZERO_DIVISION" );
        }


        // 射影変換行列の係数を求める.
        std::vector< double > homography( MATRIX_SIZE );
        {
            const double coef_x = (  vector_3.y * area_1 * ( area_1 - area_0 ) ) - ( vector_1.y * area_3 * ( area_3- area_0 ) );
            const double coef_y = ( -vector_3.x * area_1 * ( area_1 - area_0 ) ) + ( vector_1.x * area_3 * ( area_3- area_0 ) );
            homography[getIndex( 0, 2 )] = coef_x;
            homography[getIndex( 1, 2 )] = coef_y;
            homography[getIndex( 2, 2 )] = area_0 * area_3* area_1 - coef_x * offset.x - coef_y * offset.y;
        }
        {
            const double coef_x =  vector_3.y * area_1 * area_2;
            const double coef_y = -vector_3.x * area_1 * area_2;
            homography[getIndex( 0, 0 )] = coef_x;
            homography[getIndex( 1, 0 )] = coef_y;
            homography[getIndex( 2, 0 )] = 0.0 - coef_x * offset.x - coef_y * offset.y;
        }
        {
            const double coef_x = -vector_1.y * area_3* area_2;
            const double coef_y =  vector_1.x * area_3* area_2;
            homography[getIndex( 0, 1 )] = coef_x;
            homography[getIndex( 1, 1 )] = coef_y;
            homography[getIndex( 2, 1 )] = 0.0 - coef_x * offset.x - coef_y * offset.y;
        }
        return homography;
    }

        

    /*!
    @brief		4点から射影変換行列を生成する.
    @author 	WapitiStew
    @details
    対応付けテーブルは、実態は一次元だが使う人は2次元的に使用するため
    2次元的に要素を指定されたときに実態の要素番号に変換する.
    */
    static Matrix generateHomography_4Points(                        //!< @return なし.
        const std::vector< float64_xy > &src_point_list_in            //!< [in ] SRCの座標リスト.
    ,   const std::vector< float64_xy > &dst_point_list_in            //!< [in ] DSTの座標リスト.
    )
    {
        // 単位矩形から変換後の4点への射影変換.
        const std::vector< double > unit_to_dst = CalculateHomographyUnitSquareToQuadrangle( dst_point_list_in );
             
        // 変換前の4点から単位矩形への射影変換.
        const std::vector< double > src_to_unit = CalculateHomographyQuadrangleToUnitSquare( src_point_list_in );

        // 変換前の4点から変換後の4点への射影変換.
        const std::vector< double > src_to_dst = productMatrix( unit_to_dst, src_to_unit );

        // 適度に約分.
        const std::vector< double > elements = reduceMatrix( src_to_dst );

        return Matrix( 3, 3, elements );
    }

    /*!
    @brief		多点から射影変換行列を生成する.
    @author 	WapitiStew
    @details
    対応付けテーブルは、実態は一次元だが使う人は2次元的に使用するため
    2次元的に要素を指定されたときに実態の要素番号に変換する.
    */
    static Matrix generateHomography_MultiPoints(                        //!< @return なし.
        const std::vector< float64_xy > &src_point_list_in            //!< [in ] SRCの座標リスト.
    ,   const std::vector< float64_xy > &dst_point_list_in            //!< [in ] DSTの座標リスト.
    )
    {
        (void)(src_point_list_in);
        (void)(dst_point_list_in);
        throw std::logic_error( "multi-point homography requires the OpenCV integration" );
    }












    //! @brief デフォルトコンストラクタ.
    void Homography::setup( const std::vector< float64_xy >& src_in, const std::vector< float64_xy >& dst_in )
    {
        if( src_in.empty() || dst_in.empty() ||
            src_in.size() != dst_in.size()   )
        {
            throw std::invalid_argument( "Homography::setup requires matching non-empty point lists" );
        }

        const size_t NUM = src_in.size();

        Matrix homography;
        if( NUM == 2 )
        {
            std::vector< float64_xy > src_tmp;
            std::vector< float64_xy > dst_tmp;
            src_tmp.emplace_back( float64_xy( src_in[ 0 ].x, src_in[ 0 ].y ) );
            src_tmp.emplace_back( float64_xy( src_in[ 1 ].x, src_in[ 0 ].y ) );
            src_tmp.emplace_back( float64_xy( src_in[ 1 ].x, src_in[ 1 ].y ) );
            src_tmp.emplace_back( float64_xy( src_in[ 0 ].x, src_in[ 1 ].y ) );

            dst_tmp.emplace_back( float64_xy( dst_in[ 0 ].x, dst_in[ 0 ].y ) );
            dst_tmp.emplace_back( float64_xy( dst_in[ 1 ].x, dst_in[ 0 ].y ) );
            dst_tmp.emplace_back( float64_xy( dst_in[ 1 ].x, dst_in[ 1 ].y ) );
            dst_tmp.emplace_back( float64_xy( dst_in[ 0 ].x, dst_in[ 1 ].y ) );
            homography = generateHomography_4Points( src_tmp, dst_tmp );
        }
        else if( NUM == 3 )
        {
            throw std::invalid_argument( "a three-point homography is not supported" );
        }
        else if( NUM == 4 )
        {
            homography = generateHomography_4Points( src_in, dst_in );
        }
        else if( NUM > 4 )
        {
            homography = generateHomography_MultiPoints( src_in, dst_in );
        }
        else
        {
            throw std::invalid_argument( "Homography::setup requires at least two points" );
        }

        if( homography.size() != this->m_data.size() )
        {
            throw std::logic_error( "the generated homography does not match the stored shape" );
        }

        const std::vector< float64_t > elements = homography.data();
        for( size_t i = 0; i < this->m_data.size(); ++i )
        {
            this->m_data[ i ] = elements[ i ];
        }

    }


    //---------------------------------------
    // 関数.
    //---------------------------------------
    float64_xy  Homography::transform( const float64_xy& pt_in )const
    {
        
        const double x_in = static_cast< double >( pt_in.x );
        const double y_in = static_cast< double >( pt_in.y );

        const double x = ( ( this->m_data[ getIndex( 0, 0 )] * x_in ) + ( this->m_data[getIndex( 1, 0 )] * y_in ) + this->m_data[getIndex( 2, 0 )] )
                        / ( ( this->m_data[ getIndex( 0, 2 )] * x_in ) + ( this->m_data[getIndex( 1, 2 )] * y_in ) + this->m_data[getIndex( 2, 2 )] );
        const double y = ( ( this->m_data[ getIndex( 0, 1 )] * x_in ) + ( this->m_data[getIndex( 1, 1 )] * y_in ) + this->m_data[getIndex( 2, 1 )] )
                        / ( ( this->m_data[ getIndex( 0, 2 )] * x_in ) + ( this->m_data[getIndex( 1, 2 )] * y_in ) + this->m_data[getIndex( 2, 2 )] );
        return float64_xy( x, y );
    }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
