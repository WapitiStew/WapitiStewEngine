// Internal BT.601 limited-range conversion shared by frame operations and adapters.
#pragma once
#include <tmr/camera/CameraError.h>
#include <tmr/camera/CameraTypes.h>
#include <algorithm>
#include <exception>
#include <utility>

namespace wse::tmr::detail
{
inline void yuvToRgb( std::uint8_t* const red_out, std::uint8_t* const green_out,
    std::uint8_t* const blue_out, const int y_in, const int u_in, const int v_in ) noexcept
{
    auto& red = *red_out;
    auto& green = *green_out;
    auto& blue = *blue_out;
    const auto clamp = []( int value_in ) { return static_cast< std::uint8_t >(
        ( std::max )( 0, ( std::min )( 255, value_in ) ) ); };
    const int c = ( std::max )( 0, y_in - 16 ), d = u_in - 128, e = v_in - 128;
    red = clamp( ( 298 * c + 409 * e + 128 ) >> 8 );
    green = clamp( ( 298 * c - 100 * d - 208 * e + 128 ) >> 8 );
    blue = clamp( ( 298 * c + 516 * d + 128 ) >> 8 );
}

inline bool isPackedYuv422( const eCameraPixelFormat format_in ) noexcept
{
    return format_in == eCameraPixelFormat::Uyvy422 || format_in == eCameraPixelFormat::Yuyv422;
}

inline CameraResult< sCameraFrame > packedYuvToBgra( const sCameraFrame& frame_in )
{
    if( !isPackedYuv422( frame_in.description.pixel_format ) )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "BGRA conversion requires a packed YUV422 frame." ) );
    if( !frame_in.valid() )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
            "Packed YUV frame layout or payload is invalid." ) );
    sCameraFrame result;
    result.description = { frame_in.description.width, frame_in.description.height,
        eCameraPixelFormat::Bgra8, 0U };
    result.description.row_stride = result.description.minimumRowStride();
    if( !result.description.valid() )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
            "BGRA output layout overflows." ) );
    try { result.data.resize( result.description.memorySize() ); }
    catch( const std::exception& )
    {
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
            "Allocating converted BGRA frame failed." ) );
    }
    const std::size_t stride = frame_in.description.row_stride != 0U
        ? frame_in.description.row_stride : frame_in.description.minimumRowStride();
    const bool uyvy = frame_in.description.pixel_format == eCameraPixelFormat::Uyvy422;
    for( std::size_t row = 0; row < result.description.height; ++row )
    for( std::size_t column = 0; column < result.description.width; ++column )
    {
        const auto* pair = frame_in.data.data() + row * stride + ( column / 2U ) * 4U;
        auto* pixel = result.data.data() + row * result.description.row_stride + column * 4U;
        yuvToRgb( pixel + 2U, pixel + 1U, pixel,
            pair[ ( column % 2U ) * 2U + ( uyvy ? 1U : 0U ) ],
            pair[ uyvy ? 0U : 1U ], pair[ uyvy ? 2U : 3U ] );
        pixel[ 3U ] = 255U;
    }
    result.sequence = frame_in.sequence;
    result.monotonic_timestamp_ns = frame_in.monotonic_timestamp_ns;
    return CameraResult< sCameraFrame >::success( std::move( result ) );
}
} // namespace wse::tmr::detail
