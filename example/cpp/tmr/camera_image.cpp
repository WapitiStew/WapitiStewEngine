// @file camera_image.cpp
// @brief Reads a frame from the first enumerated web camera straight into a Core
//        wse::Image_ whose type carries the camera's own channel order, then shows what
//        an explicit order conversion and an explicit alpha drop look like.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time; a machine without
// one reports that through the log and exits without failing the build.

#include "example_camera_utility.h"

#include <string>

namespace
{

//! Deadline for one frame. A UVC device commonly needs a few hundred milliseconds to produce
//! its first frame after start(), so a much smaller value would report a Timeout-category
//! failure on a perfectly healthy camera; a larger one only makes a dead device take longer to
//! say so, because readFrame() has no other way to give up.
constexpr std::uint32_t READ_TIMEOUT_MS = 2000U;
//! Both files land in the process working directory, and an existing file is overwritten.
constexpr const char NATIVE_PATH[] = "tmr_camera_image_native.bmp";
constexpr const char RGB_PATH[] = "tmr_camera_image_rgb.bmp";

//! The channel order travels in the image type, so name it rather than guess.
std::string describeOrder( const wse::ePixFormat format_in )
{
    return wse::getChannelOrder( format_in ) == wse::eColorChannelOrder::Bgr ? "BGR" : "RGB";
}

//! Reports what an image carries. The type answers both questions at compile time.
template< wse::ePixFormat _Pf >
void reportImage( const std::string& label_in, const wse::Image_< _Pf >& image_in )
{
    wse::WLog() << label_in << ":" << image_in.width() << "x" << image_in.height()
                << "channels=" << image_in.pixel_size()
                << "order=" << describeOrder( _Pf )
                << "bytes=" << image_in.image_memory_size();
}

} // namespace

int main()
{
    using namespace wse::tmr;

    // Nothing is written anywhere until a log sink is installed, so this comes before the first
    // call that might report a failure.
    wse::registDefaultLog();

    // An empty configuration asks the helper to pick a profile from the device capability.
    // A machine with no camera exits 0 on purpose: the samples are built and run on machines
    // that have no camera attached, and an absent device is a reportable state rather than a
    // failure. Anything that goes wrong after a device was found keeps its own non-zero code.
    WebCamera camera;
    sCameraDeviceInfo device;
    sCameraStreamConfiguration configuration;
    if ( !tmr_example::openFirstCamera( &camera, &device, &configuration ) )
    {
        return device.valid() ? 2 : 0;
    }

    // The camera format decides which Core format a frame maps onto, and therefore which
    // image type readImage() will accept.
    // coreFormatOf() leaves its out parameter untouched when it answers false, so core_format
    // is seeded with a value the branch below will not mistake for a mapped BGRA camera.
    wse::ePixFormat core_format = wse::ePixFormat::CH1D8;
    const bool mapped = coreFormatOf( &core_format, configuration.output_format );
    if ( mapped )
    {
        wse::WLog() << "camera format maps onto Core format:"
                    << static_cast< int >( core_format )
                    << "channels=" << wse::getDataNum( core_format )
                    << "depth=" << wse::getBitDepth( core_format )
                    << "order=" << describeOrder( core_format );
    }
    else
    {
        wse::WLog() << "this camera format is converted rather than mapped directly";
    }

    // start() without a callback is the pull model: frames are collected by readFrame() or by
    // readImage() on the calling thread. Passing a callback instead would deliver them on the
    // session's worker thread, as video_stream.cpp shows.
    if ( !camera.start().succeeded() )
    {
        tmr_example::logCameraError( "start", camera.lastError() );
        return 3;
    }

    // A do-while that runs once gives every failure below one exit path, so the stop() and
    // close() after the loop are reached whatever happens inside it.
    int exit_code = 0;
    do
    {
        // A BGRA camera is the common Windows case, so show that path in full: the image
        // keeps four channels and its own order, and every step away from that is a call
        // the caller writes rather than something the engine does silently.
        if ( core_format == wse::ePixFormat::BGRA4D8 )
        {
            wse::img4c08_bgra_t bgra;
            const CameraStatus read = readImage( &camera, &bgra, READ_TIMEOUT_MS );
            if ( !read.succeeded() )
            {
                tmr_example::logCameraError( "readImage", read.error() );
                exit_code = 4;
                break;
            }
            reportImage( "native image", bgra );

            // A Core operation works on the image directly and never interprets a channel,
            // so a BGRA image rotates exactly like an RGB one.
            const wse::img4c08_bgra_t rotated =
                wse::applyOrientation( bgra, wse::eImageOrientation::Rotate90CW );
            reportImage( "rotated image", rotated );

            if ( !tmr_example::saveImageAsBmp( bgra, NATIVE_PATH ) ) { exit_code = 5; break; }

            // Explicit order swap, then an explicit alpha drop.
            const wse::img4c08_t rgba =
                wse::convertChannelOrder< wse::ePixFormat::CH4D8, wse::ePixFormat::BGRA4D8 >( bgra );
            reportImage( "after convertChannelOrder", rgba );
            const wse::img3c08_t rgb =
                wse::castData::image< wse::ePixFormat::CH3D08, wse::ePixFormat::CH4D08 >( rgba );
            reportImage( "after castData::image", rgb );
            if ( !tmr_example::saveImageAsBmp( rgb, RGB_PATH ) ) { exit_code = 6; break; }
        }
        else
        {
            // Every other camera reaches an RGB image through the shared helper, which
            // performs whichever of the same conversions the format needs.
            wse::img3c08_t rgb;
            if ( !tmr_example::readRgbImage( &camera, &rgb, READ_TIMEOUT_MS ) )
            {
                exit_code = 4;
                break;
            }
            reportImage( "image", rgb );
            const wse::img3c08_t rotated =
                wse::applyOrientation( rgb, wse::eImageOrientation::Rotate90CW );
            reportImage( "rotated image", rotated );
            if ( !tmr_example::saveImageAsBmp( rgb, RGB_PATH ) ) { exit_code = 5; break; }
        }
    } while ( false );

    // stop() ends the streaming but leaves the device open; close() ends the session and lets
    // the object be opened again. Both are safe to call more than once, and the destructor
    // would do the same, so this pair is here to name the lifecycle rather than to free memory.
    camera.stop();
    camera.close();
    return exit_code;
}
