#include "../../platform/tmr/win/camera/DirectShowCaptureFormat.h"
#include <iostream>

int main()
{
    using namespace wse::tmr;
    using namespace wse::tmr::detail;
    int failures = 0;
    const auto check = [&]( bool value_in, const char* text_in ) {
        if( !value_in ) { ++failures; std::cerr << text_in << '\n'; }
    };
    sCameraFormat format( 640U, 481U, 10U, 1U, eCameraPixelFormat::Uyvy422 );
    VIDEOINFOHEADER header{};
    header.bmiHeader.biWidth = 640;
    header.bmiHeader.biHeight = -481;
    header.bmiHeader.biBitCount = 16;
    header.AvgTimePerFrame = 666666;
    VIDEO_STREAM_CONFIG_CAPS caps{};
    caps.MinFrameInterval = 666666;
    caps.MaxFrameInterval = 5000000;
    AM_MEDIA_TYPE type{};
    type.subtype = MEDIASUBTYPE_UYVY;
    type.formattype = FORMAT_VideoInfo;
    type.cbFormat = sizeof( header );
    type.pbFormat = reinterpret_cast< BYTE* >( &header );
    check( matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format, false ), "Shape accepts odd/top-down height" );
    check( !matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Default 15 fps is not requested 10 fps" );
    check( selectDirectShowInterval( &header, caps, format ) && header.AvgTimePerFrame == 1000000,
        "10 fps can be selected inside the advertised interval range" );
    check( matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Exact format readback passes" );
    header.AvgTimePerFrame = 1000001;
    check( matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "One 100 ns tick of rounding is allowed" );
    header.AvgTimePerFrame = 1000002;
    check( !matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Nearby rate substitution is rejected" );
    format.frame_rate_numerator = 2;
    check( selectDirectShowInterval( &header, caps, format ) && header.AvgTimePerFrame == 5000000,
        "Inclusive maximum interval supports 2 fps" );
    format.frame_rate_numerator = 1;
    check( !selectDirectShowInterval( &header, caps, format ), "Outside advertised range is rejected" );
    format.frame_rate_numerator = 0;
    check( !selectDirectShowInterval( &header, caps, format ), "Invalid rational rate is rejected" );
    format.frame_rate_numerator = 2;
    caps = {};
    check( selectDirectShowInterval( &header, caps, format ), "Exact default works without interval range" );
    header.bmiHeader.biWidth = 642;
    check( !matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Resolution substitution is rejected" );
    header.bmiHeader.biWidth = 640;
    type.pbFormat = nullptr;
    check( !matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Absent format block is rejected" );
    type.pbFormat = reinterpret_cast< BYTE* >( &header );
    type.cbFormat = sizeof( header ) - 1;
    check( !matchesDirectShowFormat( type, MEDIASUBTYPE_UYVY, format ), "Truncated format block is rejected" );
    return failures == 0 ? 0 : 1;
}
