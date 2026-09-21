// @file single_frame.cpp
// @brief Captures exactly one frame from the first enumerated web camera as a Core
//        wse::Image_ and saves it as a BMP.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time; a machine without
// one reports that through the log and exits without failing the build.

#include "example_camera_utility.h"

namespace
{

//! Deadline for the one frame this sample takes. A UVC device commonly needs a few hundred
//! milliseconds to deliver its first frame after start(), so a much smaller value would report
//! a Timeout-category failure on a healthy camera; a larger one only makes a dead device take
//! longer to say so, because readFrame() has no other way to give up.
constexpr std::uint32_t READ_TIMEOUT_MS = 2000U;
//! Written to the process working directory, overwriting any existing file.
constexpr const char OUTPUT_PATH[] = "tmr_single_frame.bmp";

} // namespace

int main()
{
    using namespace wse::tmr;

    wse::registDefaultLog();

    // The empty configuration lets the helper choose a stream profile from the device
    // capability. A machine with no camera exits 0 on purpose, so this sample runs anywhere;
    // a failure after a device was found keeps its own non-zero code.
    WebCamera camera;
    sCameraDeviceInfo device;
    sCameraStreamConfiguration configuration;
    if ( !tmr_example::openFirstCamera( &camera, &device, &configuration ) )
    {
        return device.valid() ? 2 : 0;
    }

    // open() only creates the session; nothing is captured until start() runs. start() without
    // a callback means the frame is pulled on this thread by the read below.
    if ( !camera.start().succeeded() )
    {
        tmr_example::logCameraError( "start", camera.lastError() );
        return 3;
    }

    // One frame, handed back as a packed RGB image. The row stride is already folded away.
    // The image owns its pixels, so the device is released before the result is examined; the
    // image and the BMP written from it stay valid after the session is gone. stop() and
    // close() are both safe to repeat, and the destructor would do the same.
    wse::img3c08_t image;
    const bool read = tmr_example::readRgbImage( &camera, &image, READ_TIMEOUT_MS );
    camera.stop();
    camera.close();
    if ( !read )
    {
        return 4;
    }

    wse::WLog() << "captured image:" << image.width() << "x" << image.height()
                << "channels=" << image.pixel_size()
                << "bytes=" << image.image_memory_size();

    return tmr_example::saveImageAsBmp( image, OUTPUT_PATH ) ? 0 : 5;
}
