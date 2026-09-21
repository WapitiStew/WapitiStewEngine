//*****************************************************************************************************************
//! 
//! @file    wse_Matrix.cpp
//! @brief   \~japanese Matrix_の実装と明示的Template実体化.
//! @brief   \~english  Matrix_ implementation and explicit template instantiations.
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
#include <wse/data/wse_Matrix.h>

#include <algorithm>
#include <limits>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{
    // 
    // @brief 2次元座標の回転行列生成.
    // @param [in ] radian_in    回転量 rad.
    // @return 回転行列.
    // 
    template<typename _Tp, typename Enable>
    Matrix_<_Tp, Enable> Matrix_< _Tp, Enable >::Rotation2D( const _Tp radian_in )
    {
        Matrix_<_Tp> rotation = Matrix_<_Tp>::Square(2);
        const double SIN = std::sin( radian_in );
        const double COS = std::cos( radian_in );
        rotation[0][0] = static_cast< _Tp >( COS ); rotation[0][1] = static_cast< _Tp >( -SIN );
        rotation[1][0] = static_cast< _Tp >( SIN ); rotation[1][1] = static_cast< _Tp >(  COS );
        return rotation;
    }

    // 
    // @brief 3次元座標の回転行列生成.
    // @param [in ] radian_in    回転量 rad.
    // @param [in ] direction_in 回転方向.
    // @return 回転行列.
    // 
    template<typename _Tp, typename Enable>
    Matrix_<_Tp, Enable> Matrix_< _Tp, Enable >::Rotation3D( const _Tp radian_in, const eDOR direction_in )
    {
        const double SIN = std::sin( radian_in );
        const double COS = std::cos( radian_in );

        Matrix_<_Tp> matrix = Matrix_<_Tp>::Square( 3 );
        switch( direction_in )
        {
            // X軸
            case eDOR::Pitch    :
            {
                matrix[0][0] = static_cast< _Tp >( 1.0 ); matrix[0][1] = static_cast< _Tp >(  0.0 ); matrix[0][2] = static_cast< _Tp >(  0.0 );
                matrix[1][0] = static_cast< _Tp >( 0.0 ); matrix[1][1] = static_cast< _Tp >(  COS ); matrix[1][2] = static_cast< _Tp >( -SIN );
                matrix[2][0] = static_cast< _Tp >( 0.0 ); matrix[2][1] = static_cast< _Tp >(  SIN ); matrix[2][2] = static_cast< _Tp >(  COS );

            }break;

            // Y軸
            case eDOR::Yow      :
            {
                matrix[0][0] = static_cast< _Tp >(  COS ); matrix[0][1] = static_cast< _Tp >( 0.0 ); matrix[0][2] = static_cast< _Tp >( SIN );
                matrix[1][0] = static_cast< _Tp >(  0.0 ); matrix[1][1] = static_cast< _Tp >( 1.0 ); matrix[1][2] = static_cast< _Tp >( 0.0 );
                matrix[2][0] = static_cast< _Tp >( -SIN ); matrix[2][1] = static_cast< _Tp >( 0.0 ); matrix[2][2] = static_cast< _Tp >( COS );

            }break;

            // Z軸
            case eDOR::Roll     :
            {
                matrix[0][0] = static_cast< _Tp >( COS ); matrix[0][1] = static_cast< _Tp >( -SIN ); matrix[0][2] = static_cast< _Tp >( 0.0 );
                matrix[1][0] = static_cast< _Tp >( SIN ); matrix[1][1] = static_cast< _Tp >(  COS ); matrix[1][2] = static_cast< _Tp >( 0.0 );
                matrix[2][0] = static_cast< _Tp >( 0.0 ); matrix[2][1] = static_cast< _Tp >(  0.0 ); matrix[2][2] = static_cast< _Tp >( 1.0 );
            
            }break;

            default :   throw std::invalid_argument( "unknown rotation axis" );
        }

        return matrix;
    }

    
    // 
    // @brief 共分散行列生成.
    // @param [out ] p_mean_out 平均値.
    // @param [in ] data_in 入力データ.
    // @return 共分散行列.
    // 
    template<typename _Tp, typename Enable>
    Matrix_<_Tp, Enable> Matrix_< _Tp, Enable >::Covariance( Matrix_< _Tp, Enable >* p_mean_out, const std::vector< Matrix_< _Tp, Enable > >& data_in )
    {
        // 標本共分散は N-1 で割るため、2サンプル未満は計算不能として明示的に拒否する
        if( p_mean_out == nullptr )
        {
            throw std::invalid_argument( "Covariance requires a non-null mean output" );
        }
        if( data_in.size() < 2 )
        {
            throw std::invalid_argument( "Covariance requires at least two samples" );
        }

        // 全サンプルが同一次元の列ベクトルでなければ、平均・共分散の添字が範囲外になる
        for( const Matrix_< _Tp >& point : data_in )
        {
            if( ( point.cols() != 1 ) || ( point.rows() != data_in[0].rows() ) )
            {
                throw std::invalid_argument( "Covariance requires column vectors of one dimension" );
            }
        }

        // 平均ベクトルを計算
        Matrix_< _Tp, Enable >& mean = *p_mean_out;
        mean = Matrix_<_Tp, Enable>( data_in[0].rows() * data_in[0].cols(), 1 );

        for( const Matrix_< _Tp >& point : data_in )
        {
            for( size_t r = 0; r < mean.rows(); ++r )
            {
                mean[r][0] += point[r][0];
            }
        }
        mean /= static_cast< _Tp >( data_in.size() );

        // 共分散行列を計算
        Matrix_< _Tp, Enable > cov_matrix = Matrix_<_Tp, Enable>::Square( mean.rows() );
        for( const Matrix_< _Tp >& point : data_in )
        {
            for( size_t row_cov = 0; row_cov < cov_matrix.rows(); ++ row_cov )
            {
                for( size_t col_cov = 0; col_cov < cov_matrix.cols(); ++col_cov )
                {
                    cov_matrix[ row_cov ][ col_cov ] += ( ( point[ row_cov ][ 0 ] - mean[ row_cov ][ 0 ] )
                                                        * ( point[ col_cov ][ 0 ] - mean[ col_cov ][ 0 ] ) );
                }
            }
        }
        cov_matrix /= static_cast< _Tp >( data_in.size() - 1 );

        return cov_matrix;
    }

    // 
    // @brief 共分散行列生成.
    // @param [out ] p_mean_out 平均値.
    // @param [in ] data_in 入力データ.
    // @return 共分散行列.
    // 
    template<typename _Tp, typename Enable>
    Matrix_<_Tp, Enable> Matrix_< _Tp, Enable >::Covariance( _Tp* p_mean_out, const std::vector< _Tp >& data_in )
    {
        if( p_mean_out == nullptr )
        {
            throw std::invalid_argument( "Covariance requires a non-null mean output" );
        }

        // サイズ指定構築では既定構築済みの空行列が先頭に並んでしまうため、reserve で確保だけ行う
        std::vector < Matrix_<_Tp> > data_array;
        data_array.reserve( data_in.size() );
        for( size_t i = 0; i < data_in.size(); ++i )
        {
            data_array.emplace_back( Matrix_<_Tp>( 1, 1, data_in[i] ) );
        }
        Matrix_< _Tp > mean_mat( 1, 1 );
        const Matrix_<_Tp> result = Matrix_< _Tp >::Covariance( &mean_mat, data_array );
        
        // 平均値をコピー.
        _Tp &mean = *p_mean_out;
        mean = mean_mat[0][0];

        return result;
    }

    // 
    // @brief 行列式計算.
    // @return 行列式.
    // 
    template<typename _Tp, typename Enable>
    _Tp Matrix_< _Tp, Enable >::determinant( void ) const
    {
        const size_t dimention = this->getDimention();

        const Matrix_< _Tp >& current = *this;
        if( dimention == 1 )
        {
            return current[0][0];
        }
        else if( dimention == 2 )
        {
            return current[0][0] * current[1][1] - current[0][1] * current[1][0];
        }

        // 部分ピボット選択付きのガウス消去で上三角化し、対角成分の積として計算する。
        // 再帰的な余因子展開は O(n!) となり次元の増加に耐えないため使用しない。
        Matrix_< _Tp > work( *this );
        _Tp det = static_cast< _Tp >( 1.0 );
        for( size_t row = 0; row < dimention; ++row )
        {
            // ピボット選択: 絶対値最大の行を選ぶ
            size_t max_row = row;
            for( size_t next_row = row + 1; next_row < dimention; ++next_row )
            {
                if( std::abs( work[next_row][row] ) > std::abs( work[max_row][row] ) )
                {
                    max_row = next_row;
                }
            }

            // ピボット列が全て0なら行列式は厳密に0
            if( work[max_row][row] == static_cast< _Tp >( 0.0 ) )
            {
                return static_cast< _Tp >( 0.0 );
            }

            // 行の交換1回ごとに行列式の符号が反転する
            if( max_row != row )
            {
                for( size_t col = 0; col < dimention; ++col )
                {
                    std::swap( work[row][col], work[max_row][col] );
                }
                det = -det;
            }

            det *= work[row][row];

            // ピボット行より下をゼロ化
            for( size_t next_row = row + 1; next_row < dimention; ++next_row )
            {
                const _Tp factor = work[next_row][row] / work[row][row];
                for( size_t col = row; col < dimention; ++col )
                {
                    work[next_row][col] -= factor * work[row][col];
                }
            }
        }
        return det;
    }

    // 
    // @brief 転置行列計算.
    // @return 転置行列.
    // 
    template<typename _Tp, typename Enable>
    Matrix_<_Tp, Enable> Matrix_<_Tp, Enable>::transpose( void ) const
    {
        Matrix_< _Tp > transpose_matrix( this->cols(), this->rows() );
        const Matrix_< _Tp > &this_matrix = *this;
        for( size_t row = 0; row < this->rows(); row++ )
        {
            for( size_t col = 0; col < this->cols(); col++ )
            {
                transpose_matrix[col][row] = this_matrix[row][col];
            }
        }
        return transpose_matrix;
    }

    namespace
    {
        // 特異行列の失敗Result. 正常な入力でも起こり得るためExceptionにしない.
        template<typename MatrixType>
        CoreResult< MatrixType > singularMatrixFailure( void )
        {
            return CoreResult< MatrixType >::failure( CoreError(
                  eCoreErrorCategory::Computation
                , eCoreErrorCode::SingularMatrix
                , "the matrix is singular within floating-point tolerance" ) );
        }
    }

    // 
    // @brief 逆行列計算の試行.
    // @return 逆行列を運ぶCoreResult.
    // 
    // ガウス・ジョルダン法による逆行列の計算
    // 
    template<typename _Tp, typename Enable>
    CoreResult< Matrix_<_Tp, Enable> > Matrix_<_Tp, Enable>::tryInverse( void ) const
    {
        using ResultType = CoreResult< Matrix_< _Tp, Enable > >;
        if( !this->isSquare() ){ throw std::invalid_argument( "tryInverse requires a square matrix" ); }

        Matrix_< _Tp > this_matrix( *this );
        const size_t dimention = this->cols();
        if( dimention == 1 )
        {
            const _Tp det = this_matrix[0][0];
            if( det == static_cast< _Tp >( 0.0 ) ) { return singularMatrixFailure< Matrix_< _Tp, Enable > >(); }

            Matrix_< _Tp > inv_mat( dimention, dimention, 0 );
            inv_mat[0][0] = static_cast<_Tp>( 1.0 / det );
            return ResultType::success( inv_mat );
        }

        else if( dimention == 2 )
        {
            const _Tp det = this_matrix[0][0] * this_matrix[1][1] - this_matrix[0][1] * this_matrix[1][0];

            // 厳密な0だけでなく、要素積の丸め誤差に埋もれる規模の行列式も特異として拒否する
            const _Tp det_tolerance = std::numeric_limits< _Tp >::epsilon()
                * ( std::abs( this_matrix[0][0] * this_matrix[1][1] )
                  + std::abs( this_matrix[0][1] * this_matrix[1][0] ) )
                * static_cast< _Tp >( 4.0 );
            if( std::abs( det ) <= det_tolerance ) { return singularMatrixFailure< Matrix_< _Tp, Enable > >(); }

            Matrix_< _Tp > inv_mat( dimention, dimention, 0 );
            inv_mat[0][0] =  this_matrix[1][1];  inv_mat[0][1] = -this_matrix[0][1];
            inv_mat[1][0] = -this_matrix[1][0];  inv_mat[1][1] =  this_matrix[0][0];
            const _Tp coef = static_cast< _Tp >( 1 ) / det;
            return ResultType::success( coef * inv_mat );
        }


        // 特異判定の許容誤差: 最大要素の規模に対して丸め誤差程度まで小さいピボットは特異とみなす。
        // 厳密な0比較では、桁落ちした準特異行列が巨大な誤差を持つ「逆行列」を黙って返してしまう。
        _Tp max_element = static_cast< _Tp >( 0.0 );
        for( size_t row = 0; row < dimention; ++row )
        {
            for( size_t col = 0; col < dimention; ++col )
            {
                max_element = std::max( max_element, std::abs( this_matrix[row][col] ) );
            }
        }
        if( max_element == static_cast< _Tp >( 0.0 ) ) { return singularMatrixFailure< Matrix_< _Tp, Enable > >(); }
        const _Tp pivot_tolerance = max_element * static_cast< _Tp >( dimention )
                                    * std::numeric_limits< _Tp >::epsilon();

        // 拡張行列の作成 (元の行列に単位行列を追加)
        Matrix_< _Tp, Enable > inv_mat = Matrix_<_Tp, Enable>::Identify( dimention );

        // ガウス・ジョルダン法による逆行列の計算
        for( size_t row = 0; row < dimention; row++)
        {
            // ピボット選択: 最大値の行を選んで交換
            size_t max_row = row;
            for( size_t next_row = row + 1; next_row < dimention; ++next_row)
            {
                if( std::abs( this_matrix[next_row][row] ) > std::abs( this_matrix[max_row][row] ) )
                {
                    max_row = next_row;
                }
            }

            // 行の交換 (選んだ max_row と現在行を交換する)
            if( max_row != row )
            {
                for ( size_t col = 0; col < dimention; ++col)
                {
                    std::swap( this_matrix[row][col], this_matrix[max_row][col] );
                    std::swap(     inv_mat[row][col],     inv_mat[max_row][col] );
                }
            }

            // 対角成分を1にするためにスケーリング
            double pivot = static_cast< double >( this_matrix[row][row] );
            if( std::abs( this_matrix[row][row] ) <= pivot_tolerance ) { return singularMatrixFailure< Matrix_< _Tp, Enable > >(); }

            for ( size_t col = 0; col < dimention; col++)
            {
                this_matrix[row][col] /= static_cast< _Tp >( pivot );
                inv_mat[row][col]     /= static_cast< _Tp >( pivot );
            }

            // 他の行をゼロ化
            for ( size_t next_row = 0; next_row < dimention; ++next_row )
            {
                if( next_row == row ){ continue; }

                _Tp factor = this_matrix[next_row][row];
                for( size_t col = 0; col < dimention; ++col )
                {
                    this_matrix[next_row] [col] -= factor * this_matrix[row][col];
                    inv_mat[next_row] [col]     -= factor * inv_mat[row][col];
                }
            }
        }
        return ResultType::success( inv_mat );
    }

    template struct WSE_API Matrix_<F32>;
    template struct WSE_API Matrix_<F64>;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
