#pragma once
#include <tmr/camera/CameraTypes.h>
#include <dshow.h>
#include <cstdint>
#include <limits>

namespace wse { namespace tmr { namespace detail {

inline REFERENCE_TIME directShowInterval( const sCameraFormat& format_in ) noexcept
{
    if( format_in.frame_rate_numerator == 0U || format_in.frame_rate_denominator == 0U ) return 0;
    return ( 10000000LL * format_in.frame_rate_denominator ) / format_in.frame_rate_numerator;
}

inline bool matchesDirectShowInterval( REFERENCE_TIME actual_in, REFERENCE_TIME requested_in ) noexcept
{
    return requested_in > 0 && actual_in > 0
        && actual_in >= requested_in - 1 && actual_in <= requested_in + 1;
}

inline bool selectDirectShowInterval( VIDEOINFOHEADER* const header_inout,
    const VIDEO_STREAM_CONFIG_CAPS& caps_in, const sCameraFormat& format_in ) noexcept
{
    auto& header = *header_inout;
    const auto interval = directShowInterval( format_in );
    if( interval <= 0 ) return false;
    if( !matchesDirectShowInterval( header.AvgTimePerFrame, interval )
        && !( caps_in.MinFrameInterval > 0 && caps_in.MinFrameInterval <= interval
            && interval <= caps_in.MaxFrameInterval ) ) return false;
    header.AvgTimePerFrame = interval;
    return true;
}

inline bool matchesDirectShowFormat( const AM_MEDIA_TYPE& type_in, const GUID& subtype_in,
    const sCameraFormat& format_in, bool check_interval_in = true ) noexcept
{
    if( type_in.subtype != subtype_in || type_in.formattype != FORMAT_VideoInfo
        || type_in.pbFormat == nullptr || type_in.cbFormat < sizeof( VIDEOINFOHEADER )
        || format_in.width > static_cast< std::uint32_t >( ( std::numeric_limits< LONG >::max )() ) ) return false;
    const auto& header = *reinterpret_cast< const VIDEOINFOHEADER* >( type_in.pbFormat );
    const auto signed_height = static_cast< std::int64_t >( header.bmiHeader.biHeight );
    const auto height = signed_height < 0 ? -signed_height : signed_height;
    return header.bmiHeader.biWidth == static_cast< LONG >( format_in.width )
        && height == format_in.height && header.bmiHeader.biBitCount == 16U
        && ( !check_interval_in || matchesDirectShowInterval( header.AvgTimePerFrame, directShowInterval( format_in ) ) );
}

} } }
