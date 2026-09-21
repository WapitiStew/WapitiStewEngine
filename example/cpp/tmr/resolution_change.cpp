// @file resolution_change.cpp
// @brief Captures one image from the first enumerated web camera in two different
//        resolutions. The resolution is fixed at open(), so the sample closes the
//        session and reopens it with a second stream profile.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time; a machine without
// one reports that through the log and exits without failing the build.

#include "example_camera_utility.h"

#include <string>

namespace
{

//! Deadline for one frame, applied once per session below. Two seconds covers the slow first
//! frame after start(); a much smaller value would report a Timeout on a healthy camera, and it
//! has to hold for the larger of the two resolutions as well.
constexpr std::uint32_t READ_TIMEOUT_MS = 2000U;

std::string describeFormat( const wse::tmr::sCameraFormat& format_in )
{
    return std::to_string( format_in.width ) + "x" + std::to_string( format_in.height );
}

//! Opens with the given configuration, captures one image, and saves it as a BMP whose
//! name carries the resolution.
// One whole session lives inside this function, because a resolution is fixed at open() and a
// new one needs a new session. The same WebCamera object serves both calls: close() ends the
// session but leaves the object usable, so main() does not construct a second camera.
// The return value is the process exit code, zero on success.
int captureOnce(
      wse::tmr::WebCamera* const                  p_camera_inout
    , const wse::tmr::sCameraDeviceInfo&          device_in
    , const wse::tmr::sCameraStreamConfiguration& configuration_in )
{
    wse::tmr::WebCamera& camera_inout = *p_camera_inout;

    const auto opened = camera_inout.open( device_in, configuration_in );
    if ( !opened.succeeded() )
    {
        tmr_example::logCameraError(
            "open " + describeFormat( configuration_in.native_format ), opened.error() );
        return 4;
    }
    // start() failed, so there is no stream to stop, but the session from open() still has to
    // be released before the next resolution can be opened on the same object.
    if ( !camera_inout.start().succeeded() )
    {
        tmr_example::logCameraError( "start", camera_inout.lastError() );
        camera_inout.close();
        return 5;
    }
    // The session is torn down before the read result is examined, so a failed read cannot
    // leave the device open and block the second open() below. The image already owns its
    // pixels and stays valid after close().
    wse::img3c08_t image;
    const bool read = tmr_example::readRgbImage( &camera_inout, &image, READ_TIMEOUT_MS );
    camera_inout.stop();
    camera_inout.close();
    if ( !read )
    {
        return 6;
    }
    const std::string path = "tmr_resolution_"
        + describeFormat( configuration_in.native_format ) + ".bmp";
    return tmr_example::saveImageAsBmp( image, path ) ? 0 : 7;
}

} // namespace

int main()
{
    using namespace wse::tmr;

    wse::registDefaultLog();

    // This sample enumerates for itself rather than calling openFirstCamera(), because it needs
    // the capability list to choose two profiles before any session is opened.
    const auto devices = WebCamera::enumerate();
    if ( !devices.succeeded() )
    {
        tmr_example::logCameraError( "camera enumeration", devices.error() );
        return 2;
    }
    // No camera is a reportable state rather than a failure, so the sample runs anywhere.
    if ( devices.value().empty() )
    {
        wse::WLog() << "no USB/UVC web camera is attached";
        return 0;
    }
    const sCameraDeviceInfo device = devices.value().front();

    // capabilities() reads a device it does not own, so both profiles are chosen while nothing
    // is open and neither open() below has to compete with a live session.
    const auto capability = WebCamera::capabilities( device );
    if ( !capability.succeeded() )
    {
        tmr_example::logCameraError( "capability query", capability.error() );
        return 3;
    }

    // Collect two profiles with different frame sizes; both must offer a usable output.
    sCameraStreamConfiguration first_configuration;
    if ( !tmr_example::selectStreamConfiguration( &first_configuration, capability.value() ) )
    {
        wse::WLog() << "ERROR: the camera has no usable stream profile";
        return 3;
    }
    sCameraStreamConfiguration second_configuration;
    bool second_found = false;
    for ( const auto& profile : capability.value().stream_profiles )
    {
        const bool different_size =
            profile.native_format.width != first_configuration.native_format.width ||
            profile.native_format.height != first_configuration.native_format.height;
        if ( !different_size || profile.output_formats.empty() ) continue;
        // Keeping the first configuration's output format where the profile allows it leaves
        // the frame size as the only difference between the two captures.
        const eCameraPixelFormat output =
            profile.supportsOutput( first_configuration.output_format )
                ? first_configuration.output_format
                : profile.output_formats.front();
        second_configuration = { profile.native_format, output, true };
        second_found = true;
        break;
    }

    wse::WLog() << "first resolution:" << describeFormat( first_configuration.native_format );
    WebCamera camera;
    const int first_result = captureOnce( &camera, device, first_configuration );
    if ( first_result != 0 )
    {
        return first_result;
    }

    // A single-resolution device has nothing left to demonstrate; the first capture already
    // succeeded, so this is a clean exit rather than a failure.
    if ( !second_found )
    {
        wse::WLog() << "the camera advertises only one resolution; nothing to change";
        return 0;
    }
    wse::WLog() << "second resolution:" << describeFormat( second_configuration.native_format );
    const int second_result = captureOnce( &camera, device, second_configuration );
    if ( second_result == 0 )
    {
        wse::WLog() << "captured both resolutions:"
                    << describeFormat( first_configuration.native_format ) << "and"
                    << describeFormat( second_configuration.native_format );
    }
    return second_result;
}
