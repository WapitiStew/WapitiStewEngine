// @file camera_frame_ops_contract.cpp
// @brief Coreの向き補正・平均合成と、Tmr側のCamera Frame適用範囲を固定する。
// @details ここが崩れたときの症状は、例外でもErrorでもなく静かに壊れた画像である.
//          上下が反転した映像、Padding分だけ行ごとに斜めへずれた画、上位byteを落として
//          暗くなった16bit、RGBとBGRが入れ替わった色、回転させて色が混ざったBayer.
//          いずれも呼び出しは成功を返すため、後段のTestでは原因まで辿れない.
//          そこでこのFileは、Formatごとの可否判定、非対応時に明示的なErrorが返ること、
//          そして幾何・Stride・sequence・timestampが変換をまたいで保たれることを併せて記録する.
//          可否判定と実際の呼び出しの両方を見るのは、事前に問い合わせた答えと実行結果が
//          食い違わないことがCallerにとっての契約だからである.

#include "../../api/tmr/camera/CameraFrameOps.h"
#include "../../core/tmr/camera/CameraYuvConversion.h"
#include "../../api/wse/data/wse_ImageTransform.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace wse::tmr;

int failures = 0;

void expect( const bool condition_in, const char* const description_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << description_in << '\n';
        ++failures;
    }
}

//! 各画素が (row, column, channel) から一意に決まる値を持つFrameを作る。
sCameraFrame makeFrame(
      const std::uint32_t      width_in
    , const std::uint32_t      height_in
    , const eCameraPixelFormat format_in
    , const std::size_t        channels_in
    , const std::size_t        stride_padding_in = 0U )
{
    sCameraFrame frame;
    frame.description.width        = width_in;
    frame.description.height       = height_in;
    frame.description.pixel_format = format_in;
    frame.description.row_stride   =
        static_cast< std::size_t >( width_in ) * channels_in + stride_padding_in;
    frame.data.assign( frame.description.row_stride * height_in, 0U );
    frame.sequence               = 42U;
    frame.monotonic_timestamp_ns = 1234567890;

    for( std::uint32_t row = 0U; row < height_in; ++row )
    {
        for( std::uint32_t column = 0U; column < width_in; ++column )
        {
            for( std::size_t channel = 0U; channel < channels_in; ++channel )
            {
                frame.data[ row * frame.description.row_stride + column * channels_in + channel ] =
                    static_cast< std::uint8_t >( 1U + row * 10U + column + channel * 100U );
            }
        }
    }
    return frame;
}

std::uint8_t sampleAt(
      const sCameraFrame& frame_in
    , const std::uint32_t row_in
    , const std::uint32_t column_in
    , const std::size_t   channel_in
    , const std::size_t   channels_in )
{
    return frame_in.data[ row_in * frame_in.description.row_stride
        + column_in * channels_in + channel_in ];
}

void verifyOrientationGeometry()
{
    // 3x2 なので、90度回転で幅と高さが入れ替わることが観測できる。
    const sCameraFrame source = makeFrame( 3U, 2U, eCameraPixelFormat::Rgb8, 3U );

    const auto rotated_cw = applyOrientation( source, wse::eImageOrientation::Rotate90CW );
    expect( rotated_cw.succeeded(), "Rotate90CW must succeed" );
    expect( rotated_cw.value().description.width == 2U
         && rotated_cw.value().description.height == 3U,
        "Rotate90CW must exchange the width and the height" );
    expect( rotated_cw.value().description.row_stride == 2U * 3U,
        "Rotate90CW must recompute the row stride from the corrected width" );
    // 元の左上 (0,0) は時計回り90度で右上へ移る。
    expect( sampleAt( rotated_cw.value(), 0U, 1U, 0U, 3U ) == sampleAt( source, 0U, 0U, 0U, 3U ),
        "Rotate90CW must move the top-left pixel to the top-right" );

    const auto rotated_ccw = applyOrientation( source, wse::eImageOrientation::Rotate90CCW );
    expect( rotated_ccw.succeeded()
         && rotated_ccw.value().description.width == 2U
         && rotated_ccw.value().description.height == 3U,
        "Rotate90CCW must exchange the width and the height" );
    expect( sampleAt( rotated_ccw.value(), 2U, 0U, 0U, 3U ) == sampleAt( source, 0U, 0U, 0U, 3U ),
        "Rotate90CCW must move the top-left pixel to the bottom-left" );

    const auto flipped_h = applyOrientation( source, wse::eImageOrientation::FlipHorizontal );
    expect( flipped_h.succeeded()
         && flipped_h.value().description.width == 3U
         && flipped_h.value().description.height == 2U,
        "FlipHorizontal must preserve the extent" );
    expect( sampleAt( flipped_h.value(), 0U, 2U, 0U, 3U ) == sampleAt( source, 0U, 0U, 0U, 3U ),
        "FlipHorizontal must mirror left to right" );

    const auto flipped_v = applyOrientation( source, wse::eImageOrientation::FlipVertical );
    expect( flipped_v.succeeded()
         && sampleAt( flipped_v.value(), 1U, 0U, 0U, 3U ) == sampleAt( source, 0U, 0U, 0U, 3U ),
        "FlipVertical must mirror top to bottom" );

    const auto identity = applyOrientation( source, wse::eImageOrientation::None );
    expect( identity.succeeded() && identity.value().data.size() == 3U * 2U * 3U,
        "None must return a packed copy of the same extent" );
}

void verifyOrientationInvolution()
{
    const sCameraFrame source = makeFrame( 4U, 3U, eCameraPixelFormat::Bgra8, 4U );

    // 同じ操作を二度適用すると元へ戻る組と、90度を二度で180度になる組を確認する。
    const wse::eImageOrientation involutions[] = {
          wse::eImageOrientation::Rotate180
        , wse::eImageOrientation::FlipHorizontal
        , wse::eImageOrientation::FlipVertical
    };
    for( const wse::eImageOrientation orientation : involutions )
    {
        const auto once  = applyOrientation( source, orientation );
        const auto twice = applyOrientation( once.value(), orientation );
        expect( twice.succeeded() && twice.value().data == applyOrientation(
            source, wse::eImageOrientation::None ).value().data,
            "Applying an involution twice must restore the source" );
    }

    const auto cw_once  = applyOrientation( source, wse::eImageOrientation::Rotate90CW );
    const auto cw_twice = applyOrientation( cw_once.value(), wse::eImageOrientation::Rotate90CW );
    const auto half     = applyOrientation( source, wse::eImageOrientation::Rotate180 );
    expect( cw_twice.succeeded() && cw_twice.value().data == half.value().data,
        "Rotate90CW twice must equal Rotate180" );
}

void verifyStrideAndMetadata()
{
    // row_stride が最小値を超えるFrameでも、Paddingを読み込まずに正しく処理する。
    const sCameraFrame padded = makeFrame( 3U, 2U, eCameraPixelFormat::Gray8, 1U, 5U );
    const sCameraFrame packed = makeFrame( 3U, 2U, eCameraPixelFormat::Gray8, 1U );

    const auto from_padded = applyOrientation( padded, wse::eImageOrientation::Rotate180 );
    const auto from_packed = applyOrientation( packed, wse::eImageOrientation::Rotate180 );
    expect( from_padded.succeeded() && from_packed.succeeded()
         && from_padded.value().data == from_packed.value().data,
        "A padded row stride must produce the same result as a packed one" );
    expect( from_padded.value().description.row_stride == 3U,
        "The result must be packed" );
    expect( from_padded.value().sequence == padded.sequence
         && from_padded.value().monotonic_timestamp_ns == padded.monotonic_timestamp_ns,
        "Orientation must carry the sequence and the timestamp" );
    expect( from_padded.value().description.pixel_format == eCameraPixelFormat::Gray8,
        "Orientation must preserve the pixel format" );
}

void verifyUnsupportedFormats()
{
    expect( isFrameOperationSupported( eCameraPixelFormat::Gray8 )
         && isFrameOperationSupported( eCameraPixelFormat::Rgb8 )
         && isFrameOperationSupported( eCameraPixelFormat::Bgr8 )
         && isFrameOperationSupported( eCameraPixelFormat::Bgra8 ),
        "Unpacked eight-bit formats must be supported" );
    expect( !isFrameOperationSupported( eCameraPixelFormat::Yuyv422 )
         && !isFrameOperationSupported( eCameraPixelFormat::Nv12 )
         && !isFrameOperationSupported( eCameraPixelFormat::Mjpeg )
         && !isFrameOperationSupported( eCameraPixelFormat::Unknown ),
        "Subsampled, compressed, and unknown formats must be unsupported" );

    sCameraFrame compressed;
    compressed.description.width        = 4U;
    compressed.description.height       = 4U;
    compressed.description.pixel_format = eCameraPixelFormat::Mjpeg;
    compressed.description.row_stride   = 4U;
    compressed.data.assign( 16U, 0U );

    const auto rejected = applyOrientation( compressed, wse::eImageOrientation::Rotate180 );
    expect( !rejected.succeeded()
         && rejected.error().code() == eCameraErrorCode::UnsupportedFormat,
        "An unsupported pixel format must be refused explicitly" );

    CameraFrameAccumulator accumulator;
    expect( !accumulator.add( compressed ).succeeded(),
        "The accumulator must refuse an unsupported pixel format" );
}

void verifyAveraging()
{
    // 値が 10 と 21 のFrameを1枚ずつ足すと、平均は四捨五入で 16 になる。
    sCameraFrame low  = makeFrame( 2U, 2U, eCameraPixelFormat::Gray8, 1U );
    sCameraFrame high = low;
    low.data.assign( low.data.size(), 10U );
    high.data.assign( high.data.size(), 21U );

    CameraFrameAccumulator accumulator;
    expect( accumulator.count() == 0U, "A new accumulator must be empty" );
    expect( !accumulator.average().succeeded(),
        "Averaging nothing must fail rather than return an empty frame" );

    expect( accumulator.add( low ).succeeded() && accumulator.add( high ).succeeded(),
        "Two frames of one shape must be accepted" );
    expect( accumulator.count() == 2U, "The accumulator must report the frame count" );

    const auto averaged = accumulator.average();
    expect( averaged.succeeded() && averaged.value().data[ 0U ] == 16U,
        "The average of 10 and 21 must round to 16" );
    expect( averaged.value().description.width == 2U
         && averaged.value().description.height == 2U
         && averaged.value().description.pixel_format == eCameraPixelFormat::Gray8,
        "The average must keep the accumulated description" );

    // 64枚の 255 を足しても桁が溢れない。
    CameraFrameAccumulator saturated;
    sCameraFrame maximum = makeFrame( 2U, 2U, eCameraPixelFormat::Gray8, 1U );
    maximum.data.assign( maximum.data.size(), 255U );
    for( int index = 0; index < 64; ++index )
    {
        saturated.add( maximum );
    }
    expect( saturated.average().value().data[ 0U ] == 255U,
        "Accumulating 64 maximum samples must not overflow" );

    // 記述子が異なるFrameは拒否し、累積内容を壊さない。
    const sCameraFrame other_shape = makeFrame( 3U, 2U, eCameraPixelFormat::Gray8, 1U );
    expect( !accumulator.add( other_shape ).succeeded(),
        "A frame of another shape must be refused" );
    expect( accumulator.count() == 2U,
        "A refused frame must leave the accumulation unchanged" );

    accumulator.reset();
    expect( accumulator.count() == 0U, "reset must discard the accumulation" );
}

void verifyCoreImageOperations()
{
    // Core側だけを直接使う経路も固定する。Camera型を知らずに使えることが要点である。
    wse::img1c08_t image( 2U, 2U );
    image[ 0U ][ 0U ][ 0U ] = 1U;
    image[ 0U ][ 1U ][ 0U ] = 2U;
    image[ 1U ][ 0U ][ 0U ] = 3U;
    image[ 1U ][ 1U ][ 0U ] = 4U;

    const wse::img1c08_t rotated = wse::applyOrientation( image, wse::eImageOrientation::Rotate180 );
    expect( rotated[ 0U ][ 0U ][ 0U ] == 4U && rotated[ 1U ][ 1U ][ 0U ] == 1U,
        "Core Rotate180 must reverse both axes" );

    wse::img1c08_t other( 2U, 2U );
    other[ 0U ][ 0U ][ 0U ] = 3U;
    other[ 0U ][ 1U ][ 0U ] = 4U;
    other[ 1U ][ 0U ][ 0U ] = 5U;
    other[ 1U ][ 1U ][ 0U ] = 6U;

    const wse::img1c08_t averaged = wse::averageImages< wse::ePixFormat::CH1D8 >( { image, other } );
    expect( averaged[ 0U ][ 0U ][ 0U ] == 2U,
        "Core averageImages must average matching pixels" );

    bool threw = false;
    try
    {
        const wse::img1c08_t empty;
        static_cast< void >( wse::applyOrientation( empty, wse::eImageOrientation::None ) );
    }
    catch( const std::invalid_argument& )
    {
        threw = true;
    }
    expect( threw, "Core operations must reject an empty image" );
}

//! @brief Bayer Frameを作る. Sample値は (row, column) から一意に決まる.
sCameraFrame makeBayerFrame(
      const std::uint32_t      width_in
    , const std::uint32_t      height_in
    , const eCameraPixelFormat format_in
    , const std::size_t        stride_padding_in = 0U )
{
    sCameraFrame frame;
    frame.description.width        = width_in;
    frame.description.height       = height_in;
    frame.description.pixel_format = format_in;
    frame.description.row_stride   =
        static_cast< std::size_t >( width_in ) * 2U + stride_padding_in;
    frame.data.assign( frame.description.row_stride * height_in, 0U );
    frame.sequence               = 7U;
    frame.monotonic_timestamp_ns = 99;

    for( std::uint32_t row = 0U; row < height_in; ++row )
    {
        for( std::uint32_t column = 0U; column < width_in; ++column )
        {
            const std::uint16_t value =
                static_cast< std::uint16_t >( 1000U + row * 100U + column * 10U );
            const std::size_t offset = row * frame.description.row_stride + column * 2U;
            frame.data[ offset      ] = static_cast< std::uint8_t >( value & 0xFFU );
            frame.data[ offset + 1U ] = static_cast< std::uint8_t >( value >> 8U );
        }
    }
    return frame;
}

std::uint16_t bayerSample( const sCameraFrame& frame_in, std::uint32_t row_in, std::uint32_t column_in )
{
    const std::size_t offset = row_in * frame_in.description.row_stride + column_in * 2U;
    return static_cast< std::uint16_t >(
          static_cast< std::uint16_t >( frame_in.data[ offset ] )
        | ( static_cast< std::uint16_t >( frame_in.data[ offset + 1U ] ) << 8U ) );
}

std::uint16_t colourSample(
      const sCameraFrame& frame_in
    , const std::uint32_t row_in
    , const std::uint32_t column_in
    , const std::size_t   channel_in )
{
    const std::size_t base = row_in * frame_in.description.row_stride;
    if( frame_in.description.pixel_format == eCameraPixelFormat::Rgb16
     || frame_in.description.pixel_format == eCameraPixelFormat::Bgr16 )
    {
        const std::size_t offset = base + ( column_in * 3U + channel_in ) * 2U;
        return static_cast< std::uint16_t >(
              static_cast< std::uint16_t >( frame_in.data[ offset ] )
            | ( static_cast< std::uint16_t >( frame_in.data[ offset + 1U ] ) << 8U ) );
    }
    return frame_in.data[ base + column_in * 3U + channel_in ];
}

void verifyBayerFormatIdentity()
{
    const eCameraPixelFormat bayer[] = {
          eCameraPixelFormat::Bayer16Rggb, eCameraPixelFormat::Bayer16Bggr
        , eCameraPixelFormat::Bayer16Grbg, eCameraPixelFormat::Bayer16Gbrg };
    const wse::eBayerPattern expected[] = {
          wse::eBayerPattern::Rggb, wse::eBayerPattern::Bggr
        , wse::eBayerPattern::Grbg, wse::eBayerPattern::Gbrg };
    for( std::size_t index = 0U; index < 4U; ++index )
    {
        wse::eBayerPattern pattern = wse::eBayerPattern::Bggr;
        expect( isBayerFormat( bayer[ index ] ), "A Bayer format must be recognised" );
        expect( bayerPatternOf( &pattern, bayer[ index ] ) && pattern == expected[ index ],
            "A Bayer format must report its layout" );
    }

    wse::eBayerPattern untouched = wse::eBayerPattern::Gbrg;
    expect( !isBayerFormat( eCameraPixelFormat::Rgb8 )
         && !bayerPatternOf( &untouched, eCameraPixelFormat::Rgb8 )
         && untouched == wse::eBayerPattern::Gbrg,
        "A non-Bayer format reports no layout and leaves the output untouched" );
}

void verifyBayerRejectsOrientation()
{
    // Moving a Bayer pixel puts it on a site of another colour, so orientation refuses the frame.
    // Averaging is a different matter and is covered by verifyBayerAveraging().
    const sCameraFrame frame = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    expect( !isFrameOperationSupported( eCameraPixelFormat::Bayer16Rggb ),
        "A Bayer format is not an orientation format" );
    expect( isFrameAveragingSupported( eCameraPixelFormat::Bayer16Rggb ),
        "A Bayer format is an averaging format" );

    const auto rotated = applyOrientation( frame, wse::eImageOrientation::Rotate180 );
    expect( !rotated.succeeded()
         && rotated.error().code() == eCameraErrorCode::UnsupportedFormat,
        "Orientation must refuse a Bayer frame" );

    // A subsampled or compressed format is refused by both.
    expect( !isFrameAveragingSupported( eCameraPixelFormat::Yuyv422 )
         && !isFrameAveragingSupported( eCameraPixelFormat::Mjpeg ),
        "Averaging must still refuse a subsampled or compressed format" );
}

void verifyBayerAveraging()
{
    // Averaging combines each site with itself across frames, so a Bayer frame keeps its layout.
    // The two frames differ in their high byte by an odd amount, which is exactly the carry that a
    // per-byte sum would drop: 0x0100 and 0x0200 average to 0x0180, never to 0x0200.
    sCameraFrame low  = makeBayerFrame( 2U, 2U, eCameraPixelFormat::Bayer16Rggb );
    sCameraFrame high = low;
    for( std::uint32_t row = 0U; row < 2U; ++row )
    {
        for( std::uint32_t column = 0U; column < 2U; ++column )
        {
            const std::uint16_t site   = static_cast< std::uint16_t >( row * 2U + column );
            const std::size_t   offset = row * low.description.row_stride + column * 2U;
            low.data[ offset       ] = static_cast< std::uint8_t >( site );
            low.data[ offset  + 1U ] = 0x01U;
            high.data[ offset      ] = static_cast< std::uint8_t >( site );
            high.data[ offset + 1U ] = 0x02U;
        }
    }

    CameraFrameAccumulator accumulator;
    expect( accumulator.add( low ).succeeded() && accumulator.add( high ).succeeded(),
        "Averaging must accept a Bayer frame" );
    const auto averaged = accumulator.average();
    expect( averaged.succeeded(), "A Bayer accumulation must produce an average" );
    if( !averaged.succeeded() )
        return;
    expect( averaged.value().description.pixel_format == eCameraPixelFormat::Bayer16Rggb
         && averaged.value().description.row_stride == 2U * 2U,
        "A Bayer average keeps the Bayer format and packs its rows" );
    for( std::uint32_t row = 0U; row < 2U; ++row )
    {
        for( std::uint32_t column = 0U; column < 2U; ++column )
        {
            const std::uint16_t site = static_cast< std::uint16_t >( row * 2U + column );
            expect( bayerSample( averaged.value(), row, column ) == 0x0180U + site,
                "A sixteen-bit average keeps the two bytes of a sample together" );
        }
    }

    // The averaged Bayer frame still converts, which is the order the device uses.
    expect( demosaicFrame( averaged.value(), eCameraPixelFormat::Rgb16 ).succeeded(),
        "An averaged Bayer frame still converts to colour" );
}

void verifySixteenBitAveraging()
{
    // The same carry question for a sixteen-bit colour frame.
    const sCameraFrame bayer  = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    const auto         colour = demosaicFrame( bayer, eCameraPixelFormat::Rgb16 );
    expect( colour.succeeded(), "Demosaic must succeed" );
    if( !colour.succeeded() )
        return;

    CameraFrameAccumulator accumulator;
    expect( accumulator.add( colour.value() ).succeeded()
         && accumulator.add( colour.value() ).succeeded(),
        "A sixteen-bit colour frame accumulates" );
    const auto averaged = accumulator.average();
    expect( averaged.succeeded()
         && averaged.value().description.row_stride == 4U * 3U * 2U,
        "A sixteen-bit average sizes its rows in samples, not in bytes" );
    // Averaging one frame with itself has to return that frame unchanged.
    expect( averaged.succeeded() && averaged.value().data == colour.value().data,
        "Averaging a frame with itself returns it unchanged" );
}

void verifyDemosaicBlock2x2()
{
    // Block2x2 copies each block's samples, so the source values survive unchanged.
    const sCameraFrame frame = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    const auto colour = demosaicFrame(
        frame, eCameraPixelFormat::Rgb16, wse::eDemosaicMethod::Block2x2 );
    expect( colour.succeeded(), "Block2x2 demosaic must succeed" );
    if( !colour.succeeded() )
        return;

    // RGGB: (0,0) is red, (0,1) and (1,0) are green, (1,1) is blue.
    const std::uint16_t red   = bayerSample( frame, 0U, 0U );
    const std::uint16_t green = bayerSample( frame, 0U, 1U );
    const std::uint16_t blue  = bayerSample( frame, 1U, 1U );
    for( std::uint32_t row = 0U; row < 2U; ++row )
    {
        for( std::uint32_t column = 0U; column < 2U; ++column )
        {
            expect( colourSample( colour.value(), row, column, 0U ) == red
                 && colourSample( colour.value(), row, column, 1U ) == green
                 && colourSample( colour.value(), row, column, 2U ) == blue,
                "Block2x2 must copy the block samples to every pixel of that block" );
        }
    }

    expect( colour.value().description.width == 4U
         && colour.value().description.height == 4U
         && colour.value().description.row_stride == 4U * 3U * 2U,
        "The colour frame keeps the extent and packs three sixteen-bit channels" );
    expect( colour.value().sequence == frame.sequence
         && colour.value().monotonic_timestamp_ns == frame.monotonic_timestamp_ns,
        "Demosaic carries the sequence and the timestamp" );
}

void verifyDemosaicEdgesAndOrder()
{
    const sCameraFrame frame = makeBayerFrame( 5U, 3U, eCameraPixelFormat::Bayer16Rggb );

    // Every pixel is written, including the last row and column that an aligned block loop drops.
    for( const wse::eDemosaicMethod method :
        { wse::eDemosaicMethod::Block2x2, wse::eDemosaicMethod::Bilinear } )
    {
        const auto colour = demosaicFrame( frame, eCameraPixelFormat::Rgb16, method );
        expect( colour.succeeded(), "Demosaic must handle an odd extent" );
        if( !colour.succeeded() )
            continue;
        bool every_pixel_written = true;
        for( std::uint32_t row = 0U; row < 3U; ++row )
        {
            for( std::uint32_t column = 0U; column < 5U; ++column )
            {
                if( colourSample( colour.value(), row, column, 0U ) == 0U
                 && colourSample( colour.value(), row, column, 1U ) == 0U
                 && colourSample( colour.value(), row, column, 2U ) == 0U )
                {
                    every_pixel_written = false;
                }
            }
        }
        expect( every_pixel_written, "No row or column may be left unwritten" );
    }

    // Bgr16 is the same data with the channel order reversed.
    const auto rgb = demosaicFrame( frame, eCameraPixelFormat::Rgb16 );
    const auto bgr = demosaicFrame( frame, eCameraPixelFormat::Bgr16 );
    expect( rgb.succeeded() && bgr.succeeded()
         && colourSample( rgb.value(), 1U, 1U, 0U ) == colourSample( bgr.value(), 1U, 1U, 2U )
         && colourSample( rgb.value(), 1U, 1U, 2U ) == colourSample( bgr.value(), 1U, 1U, 0U ),
        "Bgr16 reverses the channel order of Rgb16" );

    // An eight-bit result keeps the high eight bits.
    const auto rgb8 = demosaicFrame( frame, eCameraPixelFormat::Rgb8 );
    expect( rgb8.succeeded()
         && rgb8.value().description.row_stride == 5U * 3U
         && colourSample( rgb8.value(), 1U, 1U, 0U )
                == ( colourSample( rgb.value(), 1U, 1U, 0U ) >> 8U ),
        "An eight-bit result keeps the high eight bits" );
}

void verifyDemosaicPatternsAndPadding()
{
    // Each layout puts the primary colours in different places, so the same samples differ.
    const sCameraFrame rggb = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    const sCameraFrame bggr = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Bggr );
    const auto from_rggb = demosaicFrame( rggb, eCameraPixelFormat::Rgb16, wse::eDemosaicMethod::Block2x2 );
    const auto from_bggr = demosaicFrame( bggr, eCameraPixelFormat::Rgb16, wse::eDemosaicMethod::Block2x2 );
    expect( from_rggb.succeeded() && from_bggr.succeeded()
         && colourSample( from_rggb.value(), 0U, 0U, 0U ) == colourSample( from_bggr.value(), 0U, 0U, 2U ),
        "Rggb and Bggr exchange the red and blue sites" );

    // A padded row stride must not change the result.
    const sCameraFrame padded = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb, 6U );
    const auto from_padded = demosaicFrame( padded, eCameraPixelFormat::Rgb16, wse::eDemosaicMethod::Block2x2 );
    expect( from_padded.succeeded() && from_padded.value().data == from_rggb.value().data,
        "A padded row stride produces the same colour frame" );
}

void verifyDemosaicRejections()
{
    const sCameraFrame frame = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    expect( !demosaicFrame( frame, eCameraPixelFormat::Gray8 ).succeeded(),
        "An unsupported result format is refused" );

    sCameraFrame not_bayer = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Gray16 );
    expect( !demosaicFrame( not_bayer, eCameraPixelFormat::Rgb16 ).succeeded(),
        "A non-Bayer source is refused" );

    const sCameraFrame tiny = makeBayerFrame( 1U, 1U, eCameraPixelFormat::Bayer16Rggb );
    expect( !demosaicFrame( tiny, eCameraPixelFormat::Rgb16 ).succeeded(),
        "A frame smaller than one Bayer block is refused" );
}

void verifySixteenBitFrameOperations()
{
    // The sixteen-bit colour formats participate in orientation like the eight-bit ones.
    const sCameraFrame bayer = makeBayerFrame( 4U, 4U, eCameraPixelFormat::Bayer16Rggb );
    const auto colour = demosaicFrame( bayer, eCameraPixelFormat::Rgb16 );
    expect( colour.succeeded(), "Demosaic must succeed" );
    if( !colour.succeeded() )
        return;

    const auto rotated = applyOrientation( colour.value(), wse::eImageOrientation::Rotate180 );
    expect( rotated.succeeded()
         && rotated.value().description.pixel_format == eCameraPixelFormat::Rgb16
         && rotated.value().description.row_stride == 4U * 3U * 2U,
        "A colour frame may be reoriented once it is no longer Bayer" );
    expect( rotated.succeeded()
         && colourSample( rotated.value(), 3U, 3U, 0U ) == colourSample( colour.value(), 0U, 0U, 0U ),
        "Rotate180 reverses both axes of a sixteen-bit colour frame" );
}

void verifyCoreColorMatrix()
{
    // The matrix is applied in the result's channel order and saturates.
    wse::img3c08_t image( 1U, 1U );
    image[ 0U ][ 0U ][ 0U ] = 10U;
    image[ 0U ][ 0U ][ 1U ] = 20U;
    image[ 0U ][ 0U ][ 2U ] = 30U;

    const std::array< std::array< double, 3U >, 3U > identity = {{
        {{ 1.0, 0.0, 0.0 }}, {{ 0.0, 1.0, 0.0 }}, {{ 0.0, 0.0, 1.0 }} }};
    const wse::img3c08_t unchanged = wse::applyColorMatrix( image, identity );
    expect( unchanged[ 0U ][ 0U ][ 0U ] == 10U && unchanged[ 0U ][ 0U ][ 2U ] == 30U,
        "An identity colour matrix leaves the image alone" );

    const std::array< std::array< double, 3U >, 3U > swap = {{
        {{ 0.0, 0.0, 1.0 }}, {{ 0.0, 1.0, 0.0 }}, {{ 1.0, 0.0, 0.0 }} }};
    const wse::img3c08_t swapped = wse::applyColorMatrix( image, swap );
    expect( swapped[ 0U ][ 0U ][ 0U ] == 30U && swapped[ 0U ][ 0U ][ 2U ] == 10U,
        "A colour matrix mixes the channels it names" );

    const std::array< std::array< double, 3U >, 3U > amplify = {{
        {{ 100.0, 0.0, 0.0 }}, {{ 0.0, 1.0, 0.0 }}, {{ 0.0, 0.0, -5.0 }} }};
    const wse::img3c08_t saturated = wse::applyColorMatrix( image, amplify );
    expect( saturated[ 0U ][ 0U ][ 0U ] == 255U && saturated[ 0U ][ 0U ][ 2U ] == 0U,
        "A colour matrix saturates rather than wrapping" );
}

//! Camera FormatとCore Formatの対応表を固定する。
void verifyCoreFormatMapping()
{
    const std::initializer_list< std::pair< eCameraPixelFormat, wse::ePixFormat > > mapped = {
          { eCameraPixelFormat::Gray8       , wse::ePixFormat::CH1D8   }
        , { eCameraPixelFormat::Rgb8        , wse::ePixFormat::CH3D8   }
        , { eCameraPixelFormat::Bgr8        , wse::ePixFormat::BGR3D8  }
        , { eCameraPixelFormat::Bgra8       , wse::ePixFormat::BGRA4D8 }
        , { eCameraPixelFormat::Gray16      , wse::ePixFormat::CH1D16  }
        , { eCameraPixelFormat::Rgb16       , wse::ePixFormat::CH3D16  }
        , { eCameraPixelFormat::Bgr16       , wse::ePixFormat::BGR3D16 }
        , { eCameraPixelFormat::Bayer16Rggb , wse::ePixFormat::CH1D16  }
        , { eCameraPixelFormat::Bayer16Bggr , wse::ePixFormat::CH1D16  }
        , { eCameraPixelFormat::Bayer16Grbg , wse::ePixFormat::CH1D16  }
        , { eCameraPixelFormat::Bayer16Gbrg , wse::ePixFormat::CH1D16  }
    };
    for( const auto& entry : mapped )
    {
        wse::ePixFormat core = wse::ePixFormat::CH1D8;
        expect( coreFormatOf( &core, entry.first ) && core == entry.second,
            "Every convertible camera format maps onto its Core format" );
        expect( isFrameImageConversionSupported( entry.first ),
            "A mapped format supports an image conversion" );
    }

    // Bgra8は4 Channelのままである。Alphaを落とすのは呼び出し側の明示的な選択に限る。
    wse::ePixFormat bgra = wse::ePixFormat::CH1D8;
    expect( coreFormatOf( &bgra, eCameraPixelFormat::Bgra8 ) &&
            wse::getDataNum( bgra ) == 4U,
        "Bgra8 keeps four channels so its alpha is not dropped" );
    expect( wse::getChannelOrder( wse::ePixFormat::BGR3D8 ) == wse::eColorChannelOrder::Bgr &&
            wse::getChannelOrder( wse::ePixFormat::CH3D8  ) == wse::eColorChannelOrder::Rgb,
        "The channel order travels in the mapped format" );

    // 色変換を伴う形式は対応表に載らないが、RGB8への変換だけは受け付ける。
    for( const eCameraPixelFormat format :
         { eCameraPixelFormat::Yuyv422, eCameraPixelFormat::Nv12 } )
    {
        wse::ePixFormat core = wse::ePixFormat::CH1D8;
        expect( !coreFormatOf( &core, format ),
            "A subsampled format has no direct Core format" );
        expect( isFrameImageConversionSupported( format ),
            "A subsampled format is still convertible into RGB8" );
    }
    for( const eCameraPixelFormat format :
         { eCameraPixelFormat::Mjpeg, eCameraPixelFormat::Unknown } )
    {
        wse::ePixFormat core = wse::ePixFormat::CH1D8;
        expect( !coreFormatOf( &core, format ) && !isFrameImageConversionSupported( format ),
            "A compressed or unknown format is refused" );
    }
}

//! Frame -> Image -> Frame の往復と、余白の畳み込みを固定する。
void verifyFrameImageRoundTrip()
{
    // 余白付きのRGB8。取り込みで余白は消え、書き出しは詰まったFrameになる。
    const sCameraFrame source = makeFrame( 4U, 3U, eCameraPixelFormat::Rgb8, 3U, 5U );
    wse::img3c08_t image;
    expect( toImage( &image, source ).succeeded(), "A padded RGB8 frame converts into an image" );
    expect( image.width() == 4U && image.height() == 3U, "The image takes the frame extent" );

    bool identical = true;
    for( std::uint32_t row = 0U; row < 3U; ++row )
    {
        for( std::uint32_t column = 0U; column < 4U; ++column )
        {
            for( std::size_t channel = 0U; channel < 3U; ++channel )
            {
                identical = identical &&
                    image[ row ][ column ][ channel ] == sampleAt( source, row, column, channel, 3U );
            }
        }
    }
    expect( identical, "The row stride is folded away and every sample survives" );

    const CameraResult< sCameraFrame > written = toCameraFrame( image );
    expect( written.succeeded(), "An image converts back into a camera frame" );
    expect( written.value().description.pixel_format == eCameraPixelFormat::Rgb8,
        "The image type selects the camera format" );
    expect( written.value().description.row_stride == 4U * 3U,
        "A converted frame is packed" );
    expect( written.value().description.width == 4U && written.value().description.height == 3U,
        "A converted frame keeps its extent" );

    // Bgr8はBGRのまま運ばれ、RGB8の宛先では受け付けられない。
    const sCameraFrame bgr_frame = makeFrame( 2U, 2U, eCameraPixelFormat::Bgr8, 3U );
    wse::img3c08_bgr_t bgr_image;
    expect( toImage( &bgr_image, bgr_frame ).succeeded(), "A BGR8 frame converts into a BGR image" );
    expect( bgr_image[ 0U ][ 0U ][ 0U ] == sampleAt( bgr_frame, 0U, 0U, 0U, 3U ),
        "A BGR frame keeps its channel order" );
    wse::img3c08_t wrong_order;
    const CameraStatus refused = toImage( &wrong_order, bgr_frame );
    expect( !refused.succeeded() &&
            refused.error().code() == eCameraErrorCode::UnsupportedFormat,
        "A BGR frame does not silently become an RGB image" );
    expect( wrong_order.width() == 0U, "A refused conversion leaves the destination untouched" );

    // Bgra8は4 Channelの画像になり、Alphaが残る。
    const sCameraFrame bgra_frame = makeFrame( 2U, 2U, eCameraPixelFormat::Bgra8, 4U );
    wse::img4c08_bgra_t bgra_image;
    expect( toImage( &bgra_image, bgra_frame ).succeeded(), "A BGRA8 frame converts into an image" );
    expect( bgra_image[ 0U ][ 0U ][ 3U ] == sampleAt( bgra_frame, 0U, 0U, 3U, 4U ),
        "A BGRA8 frame keeps its alpha channel" );
    expect( toCameraFrame( bgra_image ).value().description.pixel_format ==
            eCameraPixelFormat::Bgra8,
        "A BGRA image converts back into a BGRA8 frame" );

    // 16bitはLittle EndianのByte順で往復する。
    sCameraFrame gray16;
    gray16.description.width        = 2U;
    gray16.description.height       = 1U;
    gray16.description.pixel_format = eCameraPixelFormat::Gray16;
    gray16.description.row_stride   = 4U;
    gray16.data = { 0x34U, 0x12U, 0x78U, 0x56U };
    wse::img1c16_t gray_image;
    expect( toImage( &gray_image, gray16 ).succeeded(), "A Gray16 frame converts into an image" );
    expect( gray_image[ 0U ][ 0U ][ 0U ] == 0x1234U && gray_image[ 0U ][ 1U ][ 0U ] == 0x5678U,
        "A sixteen-bit sample is read little endian" );
    expect( toCameraFrame( gray_image ).value().data == gray16.data,
        "A sixteen-bit round trip restores the original bytes" );
}

//! 色変換を伴うFormatと、対応しないFormatの扱いを固定する。
void verifyYuvAndUnsupportedImageConversion()
{
    // Distinct luminance/chroma catches UYVY being silently interpreted as YUYV.
    // Odd height and padded rows are valid; an incomplete horizontal pair is not.
    sCameraFrame uyvy;
    uyvy.description = { 2U, 3U, eCameraPixelFormat::Uyvy422, 6U };
    uyvy.data = { 90U, 81U, 240U, 145U, 0xEEU, 0xEEU,
                 90U, 81U, 240U, 145U, 0xEEU, 0xEEU,
                 90U, 81U, 240U, 145U, 0xEEU, 0xEEU };
    expect( uyvy.valid() && uyvy.description.bytesPerPixel() == 2U,
        "UYVY allows an odd height and a padded row" );
    wse::img3c08_t uyvy_image;
    expect( toImage( &uyvy_image, uyvy ).succeeded(), "UYVY converts to RGB" );
    expect( uyvy_image[ 2U ][ 0U ][ 0U ] == 255U && uyvy_image[ 2U ][ 0U ][ 1U ] == 0U
        && uyvy_image[ 2U ][ 0U ][ 2U ] == 0U, "UYVY red uses U Y V Y order" );
    expect( uyvy_image[ 0U ][ 1U ][ 0U ] == 255U && uyvy_image[ 0U ][ 1U ][ 1U ] == 74U
        && uyvy_image[ 0U ][ 1U ][ 2U ] == 74U, "UYVY keeps the second pixel luminance" );
    uyvy.sequence = 73U;
    uyvy.monotonic_timestamp_ns = 1234567;
    const auto bgra = detail::packedYuvToBgra( uyvy );
    expect( bgra.succeeded() && bgra.value().description.row_stride == 8U
        && bgra.value().description.pixel_format == eCameraPixelFormat::Bgra8
        && bgra.value().sequence == 73U && bgra.value().monotonic_timestamp_ns == 1234567,
        "Packed conversion preserves extent and capture metadata with tight BGRA rows" );
    const std::vector< std::uint8_t > expected_bgra{
        0,0,255,255, 74,74,255,255, 0,0,255,255, 74,74,255,255, 0,0,255,255, 74,74,255,255 };
    expect( bgra.succeeded() && bgra.value().data == expected_bgra,
        "Odd-height padded UYVY becomes literal BGRA pixels, alpha 255, without padding bytes" );
    sCameraFrame packed_yuyv;
    packed_yuyv.description = { 2U, 3U, eCameraPixelFormat::Yuyv422, 0U };
    packed_yuyv.data = { 81,90,145,240, 81,90,145,240, 81,90,145,240 };
    const auto converted_yuyv = detail::packedYuvToBgra( packed_yuyv );
    expect( converted_yuyv.succeeded() && converted_yuyv.value().data == expected_bgra,
        "YUYV byte order and implicit packed stride produce the same literal colors" );
    packed_yuyv.data = {16,128,235,128};
    packed_yuyv.description.height = 1U;
    expect( detail::packedYuvToBgra( packed_yuyv ).value().data
        == std::vector< std::uint8_t >{0,0,0,255,255,255,255,255},
        "Limited-range black and white clamp to the full RGB range" );
    packed_yuyv.description.pixel_format = eCameraPixelFormat::Gray8;
    expect( detail::packedYuvToBgra( packed_yuyv ).error().code() == eCameraErrorCode::UnsupportedFormat,
        "Packed conversion rejects unrelated formats" );
    expect( !isFrameOperationSupported( eCameraPixelFormat::Uyvy422 )
        && !isFrameAveragingSupported( eCameraPixelFormat::Uyvy422 ), "UYVY needs colour conversion before pixel operations" );
    uyvy.description.width = 3U;
    expect( !detail::packedYuvToBgra( uyvy ).succeeded(), "BGRA converter rejects an incomplete pixel pair" );
    expect( !uyvy.valid() && !toImage( &uyvy_image, uyvy ).succeeded(), "UYVY refuses an incomplete pixel pair" );
    uyvy.description.width = 2U;
    uyvy.data.pop_back();
    expect( !detail::packedYuvToBgra( uyvy ).succeeded(), "BGRA converter rejects truncated padded storage" );
    expect( !toImage( &uyvy_image, uyvy ).succeeded(), "UYVY rejects a truncated final row" );

    // YUYV422。灰色一様なY=128, U=V=128 はRGBでもほぼ灰色になる。
    sCameraFrame yuyv;
    yuyv.description.width        = 2U;
    yuyv.description.height       = 1U;
    yuyv.description.pixel_format = eCameraPixelFormat::Yuyv422;
    yuyv.description.row_stride   = 4U;
    yuyv.data = { 128U, 128U, 128U, 128U };
    wse::img3c08_t yuyv_image;
    expect( toImage( &yuyv_image, yuyv ).succeeded(), "A YUYV422 frame converts into RGB8" );
    expect( yuyv_image.width() == 2U && yuyv_image.height() == 1U,
        "A YUYV422 conversion keeps the frame extent" );
    const int gray = ( 298 * ( 128 - 16 ) + 128 ) >> 8;
    expect( yuyv_image[ 0U ][ 0U ][ 0U ] == static_cast< std::uint8_t >( gray ) &&
            yuyv_image[ 0U ][ 0U ][ 1U ] == static_cast< std::uint8_t >( gray ) &&
            yuyv_image[ 0U ][ 0U ][ 2U ] == static_cast< std::uint8_t >( gray ),
        "A neutral YUV sample converts to the BT.601 grey value" );

    // NV12。輝度Planeの後ろに半解像度のUV Planeが続く。
    sCameraFrame nv12;
    nv12.description.width        = 2U;
    nv12.description.height       = 2U;
    nv12.description.pixel_format = eCameraPixelFormat::Nv12;
    nv12.description.row_stride   = 2U;
    nv12.data = { 128U, 128U, 128U, 128U, 128U, 128U };
    wse::img3c08_t nv12_image;
    expect( toImage( &nv12_image, nv12 ).succeeded(), "An NV12 frame converts into RGB8" );
    expect( nv12_image[ 1U ][ 1U ][ 1U ] == static_cast< std::uint8_t >( gray ),
        "An NV12 conversion reads its chroma plane" );

    // MJPEGは復号器を持たないため拒否する。
    sCameraFrame mjpeg;
    mjpeg.description.width        = 2U;
    mjpeg.description.height       = 2U;
    mjpeg.description.pixel_format = eCameraPixelFormat::Mjpeg;
    mjpeg.description.row_stride   = 0U;
    mjpeg.data.assign( 16U, 0U );
    wse::img3c08_t mjpeg_image;
    const CameraStatus refused = toImage( &mjpeg_image, mjpeg );
    expect( !refused.succeeded() &&
            refused.error().category() == eCameraErrorCategory::Unsupported &&
            refused.error().code() == eCameraErrorCode::UnsupportedFormat,
        "A compressed frame is refused with a stable category and code" );

    // 記述とDataが食い違うFrameは、例外ではなく構造化Errorで返る。
    sCameraFrame truncated = makeFrame( 4U, 3U, eCameraPixelFormat::Rgb8, 3U );
    truncated.data.pop_back();
    wse::img3c08_t truncated_image;
    const CameraStatus rejected = toImage( &truncated_image, truncated );
    expect( !rejected.succeeded() &&
            rejected.error().category() == eCameraErrorCategory::Validation,
        "A frame whose data is short is refused without throwing" );

    // 空の画像からFrameは作れない。
    const wse::img3c08_t empty;
    const CameraResult< sCameraFrame > from_empty = toCameraFrame( empty );
    expect( !from_empty.succeeded() &&
            from_empty.error().code() == eCameraErrorCode::InvalidArgument,
        "An empty image has no camera frame" );
}

} // namespace


int main()
{
    verifyOrientationGeometry();
    verifyOrientationInvolution();
    verifyStrideAndMetadata();
    verifyUnsupportedFormats();
    verifyAveraging();
    verifyCoreImageOperations();
    verifyBayerFormatIdentity();
    verifyBayerRejectsOrientation();
    verifyBayerAveraging();
    verifySixteenBitAveraging();
    verifyDemosaicBlock2x2();
    verifyDemosaicEdgesAndOrder();
    verifyDemosaicPatternsAndPadding();
    verifyDemosaicRejections();
    verifySixteenBitFrameOperations();
    verifyCoreColorMatrix();
    verifyCoreFormatMapping();
    verifyFrameImageRoundTrip();
    verifyYuvAndUnsupportedImageConversion();

    if( failures == 0 )
    {
        std::cout << "camera frame ops contract passed\n";
        return 0;
    }
    std::cerr << failures << " camera frame ops expectation(s) failed\n";
    return 1;
}
