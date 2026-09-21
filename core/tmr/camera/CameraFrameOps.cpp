//*****************************************************************************************************************
//!
//! @file    CameraFrameOps.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-02, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Camera Frameの向き補正と平均合成を実装するファイル。
//!     \~english  Implements orientation correction and average composition for a camera frame.
//!
//!
//! @details
//!     \~japanese
//!         Camera Pixel FormatをCoreのChannel数とBit深度へ写し、演算そのものはCoreへ委譲する。
//!      @n `Rgb8`と`Bgr8`はどちらも`CH3D8`へ写る。向き補正も平均合成もChannelの意味を解釈しないため、
//!      @n 順序の違いは結果に影響しない。元のPixel Formatは呼び出し側へ復元して返す。
//!     \~english
//!         Maps a camera pixel format onto a Core channel count and bit depth and delegates the
//!         operation itself to Core.
//!      @n `Rgb8` and `Bgr8` both map onto `CH3D8`. Neither orientation nor averaging interprets a
//!      @n channel, so the ordering difference does not affect the result, and the original pixel
//!      @n format is restored on the way out.
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

#include "../../../api/tmr/camera/CameraFrameOps.h"
#include "CameraYuvConversion.h"

#include "../../../api/tmr/device/WebCamera.h"
#include "../../../api/wse/data/wse_Image.h"
#include "../../../api/wse/data/wse_ImageInterleaved.h"
#include "../../../api/wse/data/wse_ImageTransform.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace wse
{
namespace tmr
{
namespace
{

CameraError frameOpsError(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraError( category_in, code_in, message_in );
}

template< typename T >
CameraResult< T > frameOpsFailure(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraResult< T >::failure( frameOpsError( category_in, code_in, message_in ) );
}

//! \~japanese 対応Formatのchannel数. 非対応は0. \~english Channel count of a supported format; zero otherwise.
std::size_t channelCount( const eCameraPixelFormat format_in ) noexcept
{
    switch( format_in )
    {
        case eCameraPixelFormat::Gray8  : return 1U;
        case eCameraPixelFormat::Rgb8   : return 3U;
        case eCameraPixelFormat::Bgr8   : return 3U;
        case eCameraPixelFormat::Bgra8  : return 4U;
        case eCameraPixelFormat::Gray16 : return 1U;
        case eCameraPixelFormat::Rgb16  : return 3U;
        case eCameraPixelFormat::Bgr16  : return 3U;
        default                         : return 0U;
    }
}

//! \~japanese 1 Sampleあたりのbyte数. 非対応は0. \~english Bytes per sample; zero when unsupported.
std::size_t bytesPerSample( const eCameraPixelFormat format_in ) noexcept
{
    switch( format_in )
    {
        case eCameraPixelFormat::Gray8  :
        case eCameraPixelFormat::Rgb8   :
        case eCameraPixelFormat::Bgr8   :
        case eCameraPixelFormat::Bgra8  : return 1U;
        case eCameraPixelFormat::Gray16 :
        case eCameraPixelFormat::Rgb16  :
        case eCameraPixelFormat::Bgr16  : return 2U;
        default                         : return 0U;
    }
}

//! \~japanese 加算が扱えるchannel数. Bayerは1. \~english Channels accumulation handles; Bayer counts as one.
std::size_t accumulationChannels( const eCameraPixelFormat format_in ) noexcept
{
    const std::size_t channels = channelCount( format_in );
    return channels != 0U ? channels : ( isBayerFormat( format_in ) ? 1U : 0U );
}

//! \~japanese 加算が扱う1 Sampleのbyte数. \~english Bytes per sample that accumulation handles.
std::size_t accumulationSampleBytes( const eCameraPixelFormat format_in ) noexcept
{
    const std::size_t bytes = bytesPerSample( format_in );
    return bytes != 0U ? bytes : ( isBayerFormat( format_in ) ? 2U : 0U );
}

//! \~japanese 記述とDataが一致しているかを検証する. \~english Validates that a frame describes its own data.
CameraError validateFrameData( const sCameraFrame& frame_in )
{
    if( !frame_in.valid() )
    {
        return frameOpsError(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "The camera frame description and data do not agree." );
    }
    return CameraError();
}

//! \~japanese 向き補正が可能かを検証する. \~english Validates that a frame can be reoriented.
CameraError validateFrameForOrientation(
      std::size_t* const  p_channels_out
    , const sCameraFrame& frame_in )
{
    std::size_t& channels_out = *p_channels_out;

    channels_out = channelCount( frame_in.description.pixel_format );
    if( channels_out == 0U )
    {
        // Moving a Bayer pixel puts it on a site of another colour, so orientation refuses a Bayer
        // frame and the caller converts it with demosaicFrame() first. Averaging is a different
        // matter: it combines each site with itself across frames, so it accepts Bayer.
        return frameOpsError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            isBayerFormat( frame_in.description.pixel_format )
                ? "A Bayer frame carries a colour layout that moving its pixels destroys; "
                  "convert it with demosaicFrame() first."
                : "This camera pixel format is subsampled or compressed and has no per-pixel operation." );
    }
    return validateFrameData( frame_in );
}

//! \~japanese 加算が可能かを検証する. \~english Validates that a frame can be accumulated.
CameraError validateFrameForAccumulation(
      std::size_t* const  p_channels_out
    , std::size_t* const  p_sample_bytes_out
    , const sCameraFrame& frame_in )
{
    std::size_t& channels_out     = *p_channels_out;
    std::size_t& sample_bytes_out = *p_sample_bytes_out;

    channels_out     = accumulationChannels( frame_in.description.pixel_format );
    sample_bytes_out = accumulationSampleBytes( frame_in.description.pixel_format );
    if( channels_out == 0U || sample_bytes_out == 0U )
    {
        return frameOpsError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "This camera pixel format is subsampled or compressed and has no per-pixel operation." );
    }
    return validateFrameData( frame_in );
}

//! \~japanese Little endianで1 Sampleを読む. \~english Reads one little-endian sample.
std::uint32_t readSample( const std::uint8_t* const source_in, const std::size_t sample_bytes_in ) noexcept
{
    return sample_bytes_in == 1U
        ? static_cast< std::uint32_t >( source_in[ 0U ] )
        : static_cast< std::uint32_t >( source_in[ 0U ] )
            | ( static_cast< std::uint32_t >( source_in[ 1U ] ) << 8U );
}

//! \~japanese Little endianで1 Sampleを書く. \~english Writes one little-endian sample.
void writeSample(
      std::uint8_t* const target_out
    , const std::uint32_t value_in
    , const std::size_t   sample_bytes_in ) noexcept
{
    target_out[ 0U ] = static_cast< std::uint8_t >( value_in & 0xFFU );
    if( sample_bytes_in != 1U )
    {
        target_out[ 1U ] = static_cast< std::uint8_t >( ( value_in >> 8U ) & 0xFFU );
    }
}

//! \~japanese 行Strideを畳んだ連続Bufferを作る. \~english Produces a packed buffer with the row stride folded away.
std::vector< std::uint8_t > packRows(
      const sCameraFrame& frame_in
    , const std::size_t   row_bytes_in )
{
    std::vector< std::uint8_t > packed( row_bytes_in * frame_in.description.height );
    for( std::size_t row = 0U; row < frame_in.description.height; ++row )
    {
        std::memcpy(
              packed.data() + row * row_bytes_in
            , frame_in.data.data() + row * frame_in.description.row_stride
            , row_bytes_in );
    }
    return packed;
}

//! \~japanese 演算結果から出力Frameを組み立てる. \~english Builds the resulting frame from an operation output.
sCameraFrame buildFrame(
      const sCameraFrame&               source_in
    , const std::uint32_t               width_in
    , const std::uint32_t               height_in
    , const std::size_t                 channels_in
    , const std::vector< std::uint8_t >& packed_in )
{
    sCameraFrame result;
    result.description.width        = width_in;
    result.description.height       = height_in;
    result.description.pixel_format = source_in.description.pixel_format;
    result.description.row_stride   = static_cast< std::size_t >( width_in ) * channels_in
                                    * bytesPerSample( source_in.description.pixel_format );
    result.data                     = packed_in;
    result.sequence                 = source_in.sequence;
    result.monotonic_timestamp_ns   = source_in.monotonic_timestamp_ns;
    return result;
}

} // namespace


bool isFrameOperationSupported( const eCameraPixelFormat format_in ) noexcept
{
    return channelCount( format_in ) != 0U;
}


bool isFrameAveragingSupported( const eCameraPixelFormat format_in ) noexcept
{
    return accumulationChannels( format_in ) != 0U;
}


CameraResult< sCameraFrame > applyOrientation(
      const sCameraFrame&          frame_in
    , const wse::eImageOrientation orientation_in )
{
    std::size_t channels = 0U;
    const CameraError validation = validateFrameForOrientation( &channels, frame_in );
    if( !validation.ok() )
    {
        return CameraResult< sCameraFrame >::failure( validation );
    }

    const std::size_t   row_bytes = static_cast< std::size_t >( frame_in.description.width )
                                  * channels * bytesPerSample( frame_in.description.pixel_format );
    const std::vector< std::uint8_t > packed = packRows( frame_in, row_bytes );

    std::size_t oriented_width  = 0U;
    std::size_t oriented_height = 0U;
    wse::orientedExtent(
          &oriented_width, &oriented_height
        , frame_in.description.width, frame_in.description.height, orientation_in );

    // The channel count and the sample width together select the Core format. Orientation moves
    // whole pixels, so a helper keeps the four combinations from repeating the same body.
    std::vector< std::uint8_t > result;
    const auto orient = [ & ]( auto image_tag )
    {
        using ImageType = decltype( image_tag );
        ImageType image( frame_in.description.width, frame_in.description.height );
        std::memcpy( image[ 0U ], packed.data(), packed.size() );
        const ImageType oriented = wse::applyOrientation( image, orientation_in );
        result.resize( oriented_width * oriented_height * image.pixel_byte() );
        std::memcpy( result.data(), oriented[ 0U ], result.size() );
    };

    const std::size_t sample_bytes = bytesPerSample( frame_in.description.pixel_format );
    if( sample_bytes == 1U )
    {
        if( channels == 1U )      { orient( wse::img1c08_t() ); }
        else if( channels == 3U ) { orient( wse::img3c08_t() ); }
        else                      { orient( wse::img4c08_t() ); }
    }
    else
    {
        if( channels == 1U )      { orient( wse::img1c16_t() ); }
        else                      { orient( wse::img3c16_t() ); }
    }

    return CameraResult< sCameraFrame >::success( buildFrame(
          frame_in
        , static_cast< std::uint32_t >( oriented_width )
        , static_cast< std::uint32_t >( oriented_height )
        , channels
        , result ) );
}


bool isBayerFormat( const eCameraPixelFormat format_in ) noexcept
{
    wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
    return bayerPatternOf( &pattern, format_in );
}


bool bayerPatternOf(
      wse::eBayerPattern* const p_pattern_out
    , const eCameraPixelFormat  format_in ) noexcept
{
    wse::eBayerPattern& pattern_out = *p_pattern_out;

    switch( format_in )
    {
        case eCameraPixelFormat::Bayer16Rggb : pattern_out = wse::eBayerPattern::Rggb; return true;
        case eCameraPixelFormat::Bayer16Bggr : pattern_out = wse::eBayerPattern::Bggr; return true;
        case eCameraPixelFormat::Bayer16Grbg : pattern_out = wse::eBayerPattern::Grbg; return true;
        case eCameraPixelFormat::Bayer16Gbrg : pattern_out = wse::eBayerPattern::Gbrg; return true;
        default                              : return false;
    }
}


CameraResult< sCameraFrame > demosaicFrame(
      const sCameraFrame&        frame_in
    , const eCameraPixelFormat   output_format_in
    , const wse::eDemosaicMethod method_in )
{
    wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
    if( !bayerPatternOf( &pattern, frame_in.description.pixel_format ) )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "demosaicFrame requires a Bayer source frame." );
    }
    if( !frame_in.valid() )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "The camera frame description and data do not agree." );
    }

    wse::eColorChannelOrder order = wse::eColorChannelOrder::Rgb;
    bool wide_result = false;
    switch( output_format_in )
    {
        case eCameraPixelFormat::Rgb8  : order = wse::eColorChannelOrder::Rgb; wide_result = false; break;
        case eCameraPixelFormat::Bgr8  : order = wse::eColorChannelOrder::Bgr; wide_result = false; break;
        case eCameraPixelFormat::Rgb16 : order = wse::eColorChannelOrder::Rgb; wide_result = true;  break;
        case eCameraPixelFormat::Bgr16 : order = wse::eColorChannelOrder::Bgr; wide_result = true;  break;
        default :
            return frameOpsFailure< sCameraFrame >(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "demosaicFrame produces Rgb8, Bgr8, Rgb16 or Bgr16." );
    }

    const std::uint32_t width  = frame_in.description.width;
    const std::uint32_t height = frame_in.description.height;
    if( width < 2U || height < 2U )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "A Bayer frame smaller than two by two has no complete block." );
    }

    // Fold the row stride away and read the little-endian sixteen-bit samples into a Core image.
    wse::img1c16_t source( width, height );
    for( std::uint32_t row = 0U; row < height; ++row )
    {
        const std::uint8_t* line = frame_in.data.data() + row * frame_in.description.row_stride;
        for( std::uint32_t column = 0U; column < width; ++column )
        {
            source[ row ][ column ][ 0U ] = static_cast< std::uint16_t >(
                  static_cast< std::uint16_t >( line[ column * 2U ] )
                | ( static_cast< std::uint16_t >( line[ column * 2U + 1U ] ) << 8U ) );
        }
    }

    const wse::img3c16_t colour =
        wse::demosaic< wse::ePixFormat::CH1D16, wse::ePixFormat::CH3D16 >(
            source, pattern, order, method_in );

    sCameraFrame result;
    result.description.width        = width;
    result.description.height       = height;
    result.description.pixel_format = output_format_in;
    result.description.row_stride   = static_cast< std::size_t >( width ) * 3U
                                    * ( wide_result ? 2U : 1U );
    result.data.assign( result.description.row_stride * height, 0U );
    result.sequence                 = frame_in.sequence;
    result.monotonic_timestamp_ns   = frame_in.monotonic_timestamp_ns;

    for( std::uint32_t row = 0U; row < height; ++row )
    {
        std::uint8_t* line = result.data.data() + row * result.description.row_stride;
        for( std::uint32_t column = 0U; column < width; ++column )
        {
            for( std::size_t channel = 0U; channel < 3U; ++channel )
            {
                const std::uint16_t value = colour[ row ][ column ][ channel ];
                if( wide_result )
                {
                    line[ ( column * 3U + channel ) * 2U      ] =
                        static_cast< std::uint8_t >( value & 0xFFU );
                    line[ ( column * 3U + channel ) * 2U + 1U ] =
                        static_cast< std::uint8_t >( value >> 8U );
                }
                else
                {
                    // An eight-bit result keeps the high eight bits of each sample.
                    line[ column * 3U + channel ] = static_cast< std::uint8_t >( value >> 8U );
                }
            }
        }
    }
    return CameraResult< sCameraFrame >::success( std::move( result ) );
}


class CameraFrameAccumulator::Impl final
{
    //! @brief Construct all members with explicit defaults.
public:
    Impl()
        : description  ()
        , channels     ( 0U )
        , sample_bytes ( 0U )
        , count        ( 0U )
        , sum          ()
        , sequence     ( 0U )
        , timestamp    ( 0 )
    {
    }
private:

  public:
    sCameraFrameDescription       description;
    std::size_t                   channels;
    //! \~japanese 1 Sampleのbyte数. 加算は必ずSample単位で行う.
    //! \~english  Bytes per sample; accumulation always works one whole sample at a time.
    std::size_t                   sample_bytes;
    std::size_t                   count;
    std::vector< std::uint32_t >  sum;
    std::uint64_t                 sequence;
    std::int64_t                  timestamp;
};


CameraFrameAccumulator::CameraFrameAccumulator()
    : m_impl ( new Impl() )
{
}

CameraFrameAccumulator::~CameraFrameAccumulator() = default;

CameraFrameAccumulator::CameraFrameAccumulator( CameraFrameAccumulator&& other_inout ) noexcept
    : m_impl ( std::move( other_inout.m_impl ) )
{
}

CameraFrameAccumulator& CameraFrameAccumulator::operator=(
    CameraFrameAccumulator&& other_inout ) noexcept
{
    if( this != &other_inout )
    {
        this->m_impl = std::move( other_inout.m_impl );
    }
    return *this;
}


CameraStatus CameraFrameAccumulator::add( const sCameraFrame& frame_in )
{
    std::size_t channels     = 0U;
    std::size_t sample_bytes = 0U;
    const CameraError validation = validateFrameForAccumulation( &channels, &sample_bytes, frame_in );
    if( !validation.ok() )
    {
        return CameraStatus::failure( validation );
    }

    Impl& impl = *this->m_impl;
    if( impl.count == 0U )
    {
        impl.description  = frame_in.description;
        impl.channels     = channels;
        impl.sample_bytes = sample_bytes;
        impl.sum.assign(
            static_cast< std::size_t >( frame_in.description.width )
                * frame_in.description.height * channels, 0U );
    }
    else if( impl.description.width        != frame_in.description.width
          || impl.description.height       != frame_in.description.height
          || impl.description.pixel_format != frame_in.description.pixel_format )
    {
        return CameraStatus::failure( frameOpsError(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "A frame whose description differs from the accumulation cannot be added." ) );
    }

    // A sixteen-bit sample has to be recombined before it is added, because summing its two bytes
    // apart from each other loses every carry between them.
    const std::size_t row_samples = static_cast< std::size_t >( frame_in.description.width ) * channels;
    for( std::size_t row = 0U; row < frame_in.description.height; ++row )
    {
        const std::uint8_t* source = frame_in.data.data() + row * frame_in.description.row_stride;
        std::uint32_t*      target = impl.sum.data() + row * row_samples;
        for( std::size_t index = 0U; index < row_samples; ++index )
        {
            target[ index ] += readSample( source + index * sample_bytes, sample_bytes );
        }
    }

    // The newest frame identifies the average, which is what a caller correlates against.
    impl.sequence  = frame_in.sequence;
    impl.timestamp = frame_in.monotonic_timestamp_ns;
    ++impl.count;
    return CameraStatus::success();
}


std::size_t CameraFrameAccumulator::count() const noexcept
{
    return this->m_impl == nullptr ? 0U : this->m_impl->count;
}


sCameraFrameDescription CameraFrameAccumulator::description() const noexcept
{
    return this->m_impl == nullptr ? sCameraFrameDescription() : this->m_impl->description;
}


CameraResult< sCameraFrame > CameraFrameAccumulator::average() const
{
    if( this->m_impl == nullptr || this->m_impl->count == 0U )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "No frame has been accumulated." );
    }

    const Impl&         impl    = *this->m_impl;
    const std::uint32_t divisor = static_cast< std::uint32_t >( impl.count );
    const std::uint32_t bias    = divisor / 2U;

    std::vector< std::uint8_t > packed( impl.sum.size() * impl.sample_bytes );
    for( std::size_t index = 0U; index < impl.sum.size(); ++index )
    {
        writeSample(
              packed.data() + index * impl.sample_bytes
            , ( impl.sum[ index ] + bias ) / divisor
            , impl.sample_bytes );
    }

    sCameraFrame result;
    result.description                = impl.description;
    result.description.row_stride     = static_cast< std::size_t >( impl.description.width )
                                      * impl.channels * impl.sample_bytes;
    result.data                       = std::move( packed );
    result.sequence                   = impl.sequence;
    result.monotonic_timestamp_ns     = impl.timestamp;
    return CameraResult< sCameraFrame >::success( std::move( result ) );
}


void CameraFrameAccumulator::reset() noexcept
{
    if( this->m_impl == nullptr )
        return;
    this->m_impl->sum.clear();
    this->m_impl->description  = sCameraFrameDescription();
    this->m_impl->channels     = 0U;
    this->m_impl->sample_bytes = 0U;
    this->m_impl->count        = 0U;
    this->m_impl->sequence     = 0U;
    this->m_impl->timestamp    = 0;
}


CameraResult< sCameraFrame > readAveragedFrame(
      WebCamera* const    p_camera_inout
    , const std::size_t   count_in
    , const std::uint32_t timeout_ms_in )
{
    if( count_in == 0U )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "readAveragedFrame requires at least one frame." );
    }

    WebCamera& camera_inout = *p_camera_inout;

    CameraFrameAccumulator accumulator;
    for( std::size_t index = 0U; index < count_in; ++index )
    {
        const CameraResult< sCameraFrame > frame = camera_inout.readFrame( timeout_ms_in );
        if( !frame.succeeded() )
        {
            return frame;
        }
        const CameraStatus added = accumulator.add( frame.value() );
        if( !added.succeeded() )
        {
            return CameraResult< sCameraFrame >::failure( added.error() );
        }
    }
    return accumulator.average();
}


namespace
{

//! \~japanese Frameを検証し、Coreへ渡すViewを組み立てる. \~english Validates a frame and builds the Core view.
template< wse::ePixFormat _Pf >
CameraStatus buildInterleavedView(
      wse::sInterleavedView* const p_view_out
    , const sCameraFrame&         frame_in )
{
    wse::sInterleavedView& view_out = *p_view_out;

    wse::ePixFormat mapped = wse::ePixFormat::CH1D8;
    if( !coreFormatOf( &mapped, frame_in.description.pixel_format ) || mapped != _Pf )
    {
        return frameOpsFailure< void >(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "This camera pixel format does not map onto the requested image format." );
    }
    const CameraError validation = validateFrameData( frame_in );
    if( !validation.ok() )
    {
        return CameraStatus::failure( validation );
    }

    view_out.data             = frame_in.data.data();
    view_out.width            = frame_in.description.width;
    view_out.height           = frame_in.description.height;
    view_out.row_stride       = frame_in.description.row_stride;
    view_out.accessible_bytes = frame_in.data.size();
    if( !wse::isInterleavedViewValid< _Pf >( view_out ) )
    {
        return frameOpsFailure< void >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "The camera frame layout does not fit inside its own data." );
    }
    return CameraStatus::success();
}

//! \~japanese 取り込みの共通形. Coreの例外は構造化Errorへ畳む.
//! \~english  Shared conversion body; a Core exception is folded into a structured error.
template< wse::ePixFormat _Pf >
CameraStatus convertFrameToImage( wse::Image_< _Pf >* const p_image_out, const sCameraFrame& frame_in )
{
    wse::Image_< _Pf >& image_out = *p_image_out;

    wse::sInterleavedView view;
    const CameraStatus prepared = buildInterleavedView< _Pf >( &view, frame_in );
    if( !prepared.succeeded() )
    {
        return prepared;
    }
    try
    {
        image_out = wse::makeImageFromInterleaved< _Pf >( view );
    }
    catch( const std::exception& )
    {
        return frameOpsFailure< void >(
            eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
            "Allocating the image for this camera frame failed." );
    }

    return CameraStatus::success();
}

//! \~japanese YUV FrameをRGB Imageへ変換する. \~english Converts a YUV frame into an RGB image.
CameraStatus convertYuvFrameToImage( wse::img3c08_t* const p_image_out, const sCameraFrame& frame_in )
{
    wse::img3c08_t& image_out = *p_image_out;

    const CameraError validation = validateFrameData( frame_in );
    if( !validation.ok() )
    {
        return CameraStatus::failure( validation );
    }

    const std::size_t width  = frame_in.description.width;
    const std::size_t height = frame_in.description.height;
    const std::size_t stride = frame_in.description.row_stride;
    const bool        is_nv12 = frame_in.description.pixel_format == eCameraPixelFormat::Nv12;
    const bool        is_uyvy = frame_in.description.pixel_format == eCameraPixelFormat::Uyvy422;

    try
    {
        wse::img3c08_t image( width, height );
        for( std::size_t row = 0U; row < height; ++row )
        {
            for( std::size_t column = 0U; column < width; ++column )
            {
                int y = 0;
                int u = 0;
                int v = 0;
                if( is_nv12 )
                {
                    // NV12は輝度Planeの後ろにUVを交互に並べた半解像度のPlaneが続く。
                    const std::size_t plane_offset = stride * height;
                    const std::size_t chroma_row   = row / 2U;
                    const std::size_t chroma_pair  = ( column / 2U ) * 2U;
                    y = frame_in.data[ row * stride + column ];
                    u = frame_in.data[ plane_offset + chroma_row * stride + chroma_pair ];
                    v = frame_in.data[ plane_offset + chroma_row * stride + chroma_pair + 1U ];
                }
                else
                {
                    // Each pair is Y0 U Y1 V (YUYV) or U Y0 V Y1 (UYVY).
                    const std::size_t pair_offset = row * stride + ( column / 2U ) * 4U;
                    y = frame_in.data[ pair_offset + ( column % 2U ) * 2U + ( is_uyvy ? 1U : 0U ) ];
                    u = frame_in.data[ pair_offset + ( is_uyvy ? 0U : 1U ) ];
                    v = frame_in.data[ pair_offset + ( is_uyvy ? 2U : 3U ) ];
                }
                detail::yuvToRgb( &image[ row ][ column ][ 0U ]
                        , &image[ row ][ column ][ 1U ]
                        , &image[ row ][ column ][ 2U ]
                        , y, u, v );
            }
        }
        image_out = std::move( image );
    }
    catch( const std::exception& )
    {
        return frameOpsFailure< void >(
            eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
            "Allocating the image for this camera frame failed." );
    }

    return CameraStatus::success();
}

//! \~japanese 書き出しの共通形. \~english Shared body that writes an image out as a frame.
template< wse::ePixFormat _Pf >
CameraResult< sCameraFrame > convertImageToFrame(
      const wse::Image_< _Pf >& image_in
    , const eCameraPixelFormat  format_in )
{
    if( image_in.width() == 0U || image_in.height() == 0U )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "An empty image has no camera frame." );
    }

    const std::size_t row_bytes = wse::packedRowBytes< _Pf >( image_in.width() );
    if( row_bytes == 0U )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "The image extent overflows a packed camera frame." );
    }

    sCameraFrame frame;
    frame.description.width        = static_cast< std::uint32_t >( image_in.width() );
    frame.description.height       = static_cast< std::uint32_t >( image_in.height() );
    frame.description.pixel_format = format_in;
    frame.description.row_stride   = row_bytes;
    try
    {
        frame.data.resize( row_bytes * image_in.height() );
    }
    catch( const std::bad_alloc& )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
            "Allocating the camera frame for this image failed." );
    }

    wse::sInterleavedTarget target;
    target.data             = frame.data.data();
    target.width            = image_in.width();
    target.height           = image_in.height();
    target.row_stride       = row_bytes;
    target.accessible_bytes = frame.data.size();
    if( !wse::writeImageToInterleaved( &target, image_in ) )
    {
        return frameOpsFailure< sCameraFrame >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "Writing the image into a camera frame failed." );
    }
    return CameraResult< sCameraFrame >::success( frame );
}

} // namespace


bool coreFormatOf( wse::ePixFormat* const p_format_out, const eCameraPixelFormat format_in ) noexcept
{
    wse::ePixFormat& format_out = *p_format_out;

    switch( format_in )
    {
        case eCameraPixelFormat::Gray8       : format_out = wse::ePixFormat::CH1D8;    return true;
        case eCameraPixelFormat::Rgb8        : format_out = wse::ePixFormat::CH3D8;    return true;
        case eCameraPixelFormat::Bgr8        : format_out = wse::ePixFormat::BGR3D8;   return true;
        case eCameraPixelFormat::Bgra8       : format_out = wse::ePixFormat::BGRA4D8;  return true;
        case eCameraPixelFormat::Gray16      : format_out = wse::ePixFormat::CH1D16;   return true;
        case eCameraPixelFormat::Rgb16       : format_out = wse::ePixFormat::CH3D16;   return true;
        case eCameraPixelFormat::Bgr16       : format_out = wse::ePixFormat::BGR3D16;  return true;
        // Bayerは配列がFormatに乗らないため、Sampleの入れ物としてだけ写す。
        case eCameraPixelFormat::Bayer16Rggb :
        case eCameraPixelFormat::Bayer16Bggr :
        case eCameraPixelFormat::Bayer16Grbg :
        case eCameraPixelFormat::Bayer16Gbrg : format_out = wse::ePixFormat::CH1D16;   return true;
        default                              : return false;
    }
}


bool isFrameImageConversionSupported( const eCameraPixelFormat format_in ) noexcept
{
    wse::ePixFormat mapped = wse::ePixFormat::CH1D8;
    if( coreFormatOf( &mapped, format_in ) )
    {
        return true;
    }
    return format_in == eCameraPixelFormat::Yuyv422
        || format_in == eCameraPixelFormat::Uyvy422
        || format_in == eCameraPixelFormat::Nv12;
}


CameraStatus toImage( wse::img1c08_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH1D8 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img1c16_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH1D16 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img3c08_t* const p_image_out, const sCameraFrame& frame_in )
{
    // RGB8への変換だけは、Subsamplingを解いてでも応じる。
    if( frame_in.description.pixel_format == eCameraPixelFormat::Yuyv422 ||
        frame_in.description.pixel_format == eCameraPixelFormat::Uyvy422 ||
        frame_in.description.pixel_format == eCameraPixelFormat::Nv12 )
    {
        return convertYuvFrameToImage( p_image_out, frame_in );
    }
    return convertFrameToImage< wse::ePixFormat::CH3D8 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img3c08_bgr_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::BGR3D8 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img3c16_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH3D16 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img3c16_bgr_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::BGR3D16 >( p_image_out, frame_in );
}

CameraStatus toImage( wse::img4c08_bgra_t* const p_image_out, const sCameraFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::BGRA4D8 >( p_image_out, frame_in );
}


CameraResult< sCameraFrame > toCameraFrame( const wse::img1c08_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Gray8 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img1c16_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Gray16 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img3c08_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Rgb8 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img3c08_bgr_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Bgr8 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img3c16_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Rgb16 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img3c16_bgr_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Bgr16 );
}

CameraResult< sCameraFrame > toCameraFrame( const wse::img4c08_bgra_t& image_in )
{
    return convertImageToFrame( image_in, eCameraPixelFormat::Bgra8 );
}


namespace
{

//! \~japanese 1枚読み出してImageへ変換する共通形. \~english Shared body that reads one frame as an image.
template< typename ImageType >
CameraStatus readOneImage(
      WebCamera* const    p_camera_inout
    , ImageType* const   p_image_out
    , const std::uint32_t timeout_ms_in )
{
    const CameraResult< sCameraFrame > frame = p_camera_inout->readFrame( timeout_ms_in );
    if( !frame.succeeded() )
    {
        return CameraStatus::failure( frame.error() );
    }
    return toImage( p_image_out, frame.value() );
}

} // namespace


CameraStatus readImage(
      WebCamera* const p_camera_inout, wse::img1c08_t* const p_image_out, const std::uint32_t timeout_ms_in )
{
    return readOneImage( p_camera_inout, p_image_out, timeout_ms_in );
}

CameraStatus readImage(
      WebCamera* const p_camera_inout, wse::img3c08_t* const p_image_out, const std::uint32_t timeout_ms_in )
{
    return readOneImage( p_camera_inout, p_image_out, timeout_ms_in );
}

CameraStatus readImage(
      WebCamera* const p_camera_inout, wse::img3c08_bgr_t* const p_image_out, const std::uint32_t timeout_ms_in )
{
    return readOneImage( p_camera_inout, p_image_out, timeout_ms_in );
}

CameraStatus readImage(
      WebCamera* const p_camera_inout, wse::img4c08_bgra_t* const p_image_out, const std::uint32_t timeout_ms_in )
{
    return readOneImage( p_camera_inout, p_image_out, timeout_ms_in );
}


CameraStatus readAveragedImage(
      WebCamera* const     p_camera_inout
    , wse::img3c08_t* const p_image_out
    , const std::size_t    count_in
    , const std::uint32_t  timeout_ms_in )
{
    const CameraResult< sCameraFrame > frame =
        readAveragedFrame( p_camera_inout, count_in, timeout_ms_in );
    if( !frame.succeeded() )
    {
        return CameraStatus::failure( frame.error() );
    }
    return toImage( p_image_out, frame.value() );
}

} // namespace tmr
} // namespace wse
