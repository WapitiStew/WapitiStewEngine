//*****************************************************************************************************************
//!
//! @file    OpenCvAdapter.h
//! @brief   \~japanese WSEデータ型とOpenCV型の相互変換Adapter.
//! @brief   \~english  Adapter converting between the WSE data types and the OpenCV types.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-14, 2026   Create the OpenCV adapter component (legacy removal program, LEGACY-009).
//!
//! @details
//!     \~japanese
//!         本HeaderはWSE公開APIの中で唯一OpenCVへ依存してよい場所である（公開API Policyの
//!         Adapter例外）。Includeした時点でOpenCV（`opencv2/core.hpp`）がConsumer側の
//!         Include pathに存在することを要求する。WSEのCoreはOpenCVなしで完結しており、
//!         本Adapterを使わない限りOpenCVは不要である。変換はすべて値Copyであり、
//!         `cv::Mat`との相互変換は公開のInterleaved契約
//!         （`makeImageFromInterleaved`／`writeImageToInterleaved`）の上に実装する。
//!     \~english
//!         This header is the single place in the WSE public API that may depend on OpenCV
//!         (the adapter exception of the public API policy). Including it requires OpenCV
//!         (`opencv2/core.hpp`) on the consumer's include path; the WSE core is complete
//!         without OpenCV and nothing needs it unless this adapter is used. Every conversion
//!         copies by value, and the `cv::Mat` conversions are built on the public interleaved
//!         contract (`makeImageFromInterleaved` / `writeImageToInterleaved`).
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

#ifndef WONDERSTEWENGINE_CV_OPENCVADAPTER_H
#define WONDERSTEWENGINE_CV_OPENCVADAPTER_H

#include <cstdint>
#include <limits>
#include <stdexcept>

#pragma warning(push)
#pragma warning(disable: 4711)
#pragma warning(disable: 6201)
#pragma warning(disable: 6294)
#pragma warning(disable: 26439)
#pragma warning(disable: 26495)
#include <opencv2/core.hpp>
#pragma warning(pop)

#include <wse/stew.h>

namespace wse
{
namespace ocv
{
    //------------------------------------------------------------------------------------------
    // Size.
    //------------------------------------------------------------------------------------------

    //! \~japanese `cv::Size_`からWSEの`Size_`を生成する. \~english Builds a WSE `Size_` from a `cv::Size_`.
    template< typename _Tp >
    Size_< _Tp > fromCvSize( const cv::Size_< _Tp >& size_in )
    {
        return Size_< _Tp >( size_in.width, size_in.height );
    }

    //! \~japanese WSEの`Size_`から`cv::Size_`を生成する. \~english Builds a `cv::Size_` from a WSE `Size_`.
    template< typename _Tp >
    cv::Size_< _Tp > toCvSize( const Size_< _Tp >& size_in )
    {
        cv::Size_< _Tp > size;
        size.width  = size_in.width();
        size.height = size_in.height();
        return size;
    }

    //------------------------------------------------------------------------------------------
    // Point.
    //------------------------------------------------------------------------------------------

    //! \~japanese `cv::Point_`からWSEの`Point2_`を生成する. \~english Builds a WSE `Point2_` from a `cv::Point_`.
    template< typename _Tp >
    Point2_< _Tp > fromCvPoint( const cv::Point_< _Tp >& point_in )
    {
        return Point2_< _Tp >( point_in.x, point_in.y );
    }

    //! \~japanese WSEの`Point2_`から`cv::Point_`を生成する. \~english Builds a `cv::Point_` from a WSE `Point2_`.
    template< typename _Tp >
    cv::Point_< _Tp > toCvPoint( const Point2_< _Tp >& point_in )
    {
        cv::Point_< _Tp > point;
        point.x = point_in.x;
        point.y = point_in.y;
        return point;
    }

    //! \~japanese `cv::Point3_`からWSEの`Point3_`を生成する. \~english Builds a WSE `Point3_` from a `cv::Point3_`.
    template< typename _Tp >
    Point3_< _Tp > fromCvPoint( const cv::Point3_< _Tp >& point_in )
    {
        return Point3_< _Tp >( point_in.x, point_in.y, point_in.z );
    }

    //! \~japanese WSEの`Point3_`から`cv::Point3_`を生成する. \~english Builds a `cv::Point3_` from a WSE `Point3_`.
    template< typename _Tp >
    cv::Point3_< _Tp > toCvPoint( const Point3_< _Tp >& point_in )
    {
        cv::Point3_< _Tp > point;
        point.x = point_in.x;
        point.y = point_in.y;
        point.z = point_in.z;
        return point;
    }

    //------------------------------------------------------------------------------------------
    // Range.
    //------------------------------------------------------------------------------------------

    //!
    //! @brief
    //!     \~japanese `cv::Range`からWSEの`Range1_`を生成する. start／endの順序は正規化する.
    //!     \~english  Builds a WSE `Range1_` from a `cv::Range`, normalizing the start/end order.
    //!
    template< typename _Tp >
    Range1_< _Tp > fromCvRange( const cv::Range& range_in )
    {
        const int lower = ( range_in.start <= range_in.end ) ? range_in.start : range_in.end;
        const int upper = ( range_in.start <= range_in.end ) ? range_in.end : range_in.start;
        return Range1_< _Tp >( static_cast< _Tp >( lower ), static_cast< _Tp >( upper ) );
    }

    //! \~japanese WSEの`Range1_`から`cv::Range`を生成する. \~english Builds a `cv::Range` from a WSE `Range1_`.
    template< typename _Tp >
    cv::Range toCvRange( const Range1_< _Tp >& range_in )
    {
        cv::Range range;
        range.start = static_cast< int >( range_in.minimum() );
        range.end   = static_cast< int >( range_in.maximum() );
        return range;
    }

    //! \~japanese `cv::Rect_`からWSEの`Range2_`を生成する. \~english Builds a WSE `Range2_` from a `cv::Rect_`.
    template< typename _Tp >
    Range2_< _Tp > fromCvRect( const cv::Rect_< _Tp >& rect_in )
    {
        return Range2_< _Tp >(
              rect_in.x, rect_in.x + rect_in.width
            , rect_in.y, rect_in.y + rect_in.height );
    }

    //! \~japanese WSEの`Range2_`から`cv::Rect_`を生成する. \~english Builds a `cv::Rect_` from a WSE `Range2_`.
    template< typename _Tp >
    cv::Rect_< _Tp > toCvRect( const Range2_< _Tp >& range_in )
    {
        cv::Rect_< _Tp > rect;
        rect.x      = range_in.min_x();
        rect.y      = range_in.min_y();
        rect.width  = range_in.width();
        rect.height = range_in.height();
        return rect;
    }

    //------------------------------------------------------------------------------------------
    // Pixel.
    //------------------------------------------------------------------------------------------

    //!
    //! @brief
    //!     \~japanese `cv::Scalar_`からWSEの`Pixel_`を生成する.
    //!     \~english  Builds a WSE `Pixel_` from a `cv::Scalar_`.
    //!
    //! @exception std::out_of_range
    //!     \~japanese 値がPixelのBit深度に収まらない場合.
    //!     \~english  When a value does not fit the pixel bit depth.
    //!
    template< typename _Tp, uint8_t NUM, uint8_t DEPTH >
    Pixel_< _Tp, NUM, DEPTH > fromCvScalar( const cv::Scalar_< _Tp >& scalar_in )
    {
        Pixel_< _Tp, NUM, DEPTH > pixel;
        for( uint8_t i = 0; i < NUM; ++i )
        {
            pixel.set( i, scalar_in.val[ i ] );
        }
        return pixel;
    }

    //! \~japanese WSEの`Pixel_`から`cv::Scalar_<double>`を生成する. \~english Builds a `cv::Scalar_<double>` from a WSE `Pixel_`.
    template< typename _Tp, uint8_t NUM, uint8_t DEPTH >
    cv::Scalar_< double > toCvScalar( const Pixel_< _Tp, NUM, DEPTH >& pixel_in )
    {
        cv::Scalar_< double > scalar;
        for( uint8_t i = 0; i < NUM; ++i )
        {
            scalar.val[ i ] = static_cast< double >( pixel_in.data[ i ] );
        }
        return scalar;
    }

    //------------------------------------------------------------------------------------------
    // Image.
    //------------------------------------------------------------------------------------------

    //!
    //! @brief
    //!     \~japanese `cv::Mat`の型に対応する`ePixFormat`を返す.
    //!     \~english  Returns the `ePixFormat` matching the `cv::Mat` type.
    //!
    //! @exception std::invalid_argument
    //!     \~japanese 対応するFormatがない場合.
    //!     \~english  When no matching format exists.
    //!
    inline ePixFormat pixelFormatOf( const cv::Mat& image_in )
    {
        switch( image_in.type() )
        {
            case CV_8UC1  : return ePixFormat::CH1D8;
            case CV_8UC3  : return ePixFormat::CH3D8;
            case CV_8UC4  : return ePixFormat::CH4D8;
            case CV_16UC1 : return ePixFormat::CH1D16;
            case CV_16UC3 : return ePixFormat::CH3D16;
            case CV_16UC4 : return ePixFormat::CH4D16;
            default : throw std::invalid_argument( "the OpenCV format has no matching pixel format" );
        }
    }

    //!
    //! @brief
    //!     \~japanese `ePixFormat`に対応するOpenCVの型値を返す. 10〜16bitのFormatは16bit格納へ写す.
    //!     \~english  Returns the OpenCV type value for an `ePixFormat`; 10-16 bit formats map to 16-bit storage.
    //!
    //! @exception std::invalid_argument
    //!     \~japanese 対応するOpenCV型がない場合.
    //!     \~english  When no matching OpenCV type exists.
    //!
    inline int toCvType( const ePixFormat format_in )
    {
        switch( format_in )
        {
            case ePixFormat::CH1D8:
                return CV_8UC1;
            case ePixFormat::CH1D10:
            case ePixFormat::CH1D12:
            case ePixFormat::CH1D14:
            case ePixFormat::CH1D16:
                return CV_16UC1;
            case ePixFormat::CH3D8:
                return CV_8UC3;
            case ePixFormat::CH3D10:
            case ePixFormat::CH3D12:
            case ePixFormat::CH3D14:
            case ePixFormat::CH3D16:
                return CV_16UC3;
            case ePixFormat::CH4D8:
                return CV_8UC4;
            case ePixFormat::CH4D10:
            case ePixFormat::CH4D12:
            case ePixFormat::CH4D14:
            case ePixFormat::CH4D16:
                return CV_16UC4;
            default:
                throw std::invalid_argument( "the pixel format has no matching OpenCV type" );
        }
    }

    //!
    //! @brief
    //!     \~japanese `cv::Mat`からWSEの`Image_`を生成する. Dataは行単位でCopyする.
    //!     \~english  Builds a WSE `Image_` from a `cv::Mat`, copying the data row by row.
    //!
    //! @exception std::invalid_argument
    //!     \~japanese MatのFormatが`_Pf`と一致しない場合、またはViewがInterleaved契約を満たさない場合.
    //!     \~english  When the Mat format does not match `_Pf` or the view violates the interleaved contract.
    //!
    template< ePixFormat _Pf >
    Image_< _Pf > fromCvMat( const cv::Mat& image_in )
    {
        if( image_in.empty() )
        {
            return Image_< _Pf >();
        }
        if( pixelFormatOf( image_in ) != _Pf )
        {
            throw std::invalid_argument( "the pixel format does not match the image" );
        }
        const std::size_t width  = static_cast< std::size_t >( image_in.cols );
        const std::size_t height = static_cast< std::size_t >( image_in.rows );
        const std::size_t stride = image_in.step[ 0 ];

        sInterleavedView view;
        view.data             = image_in.ptr< std::uint8_t >( 0 );
        view.width            = width;
        view.height           = height;
        view.row_stride       = stride;
        view.accessible_bytes = stride * ( height - 1U ) + width * image_in.elemSize();
        return makeImageFromInterleaved< _Pf >( view );
    }

    //!
    //! @brief
    //!     \~japanese WSEの`Image_`から`cv::Mat`を生成する. Dataは行単位でCopyする.
    //!     \~english  Builds a `cv::Mat` from a WSE `Image_`, copying the data row by row.
    //!
    //! @exception std::invalid_argument
    //!     \~japanese 寸法がOpenCVの`int`に収まらない場合、または書き出しがInterleaved契約を満たさない場合.
    //!     \~english  When a dimension exceeds OpenCV's `int` or the write violates the interleaved contract.
    //!
    template< ePixFormat _Pf >
    cv::Mat toCvMat( const Image_< _Pf >& image_in )
    {
        const std::size_t width  = image_in.width();
        const std::size_t height = image_in.height();
        if( width == 0U || height == 0U )
        {
            return cv::Mat();
        }
        const std::size_t maximum_int =
            static_cast< std::size_t >( ( std::numeric_limits< int >::max )() );
        if( width > maximum_int || height > maximum_int )
        {
            throw std::invalid_argument( "the image dimensions exceed the OpenCV limits" );
        }

        cv::Mat mat( cv::Size( static_cast< int >( width ), static_cast< int >( height ) )
                   , toCvType( _Pf ) );

        sInterleavedTarget target;
        target.data             = mat.ptr< std::uint8_t >( 0 );
        target.width            = width;
        target.height           = height;
        target.row_stride       = mat.step[ 0 ];
        target.accessible_bytes = mat.step[ 0 ] * ( height - 1U ) + width * mat.elemSize();
        if( !writeImageToInterleaved< _Pf >( &target, image_in ) )
        {
            throw std::invalid_argument( "the image cannot be written through the interleaved contract" );
        }
        return mat;
    }

} // namespace ocv
} // namespace wse

#endif // WONDERSTEWENGINE_CV_OPENCVADAPTER_H
