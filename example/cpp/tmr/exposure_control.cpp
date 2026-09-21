// @file exposure_control.cpp
// @brief Changes the exposure of the first enumerated web camera and saves one image
//        before and one image after the change, then restores the original setting.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time; a machine without
// one reports that through the log and exits without failing the build.

#include "example_camera_utility.h"

namespace
{

//! Deadline for one frame. Two seconds covers the slow first frame a UVC device delivers after
//! start(); a much smaller value would report a Timeout on a healthy camera, and a longer
//! exposure written below makes the wait longer still.
constexpr std::uint32_t READ_TIMEOUT_MS = 2000U;
constexpr const char BEFORE_PATH[] = "tmr_exposure_before.bmp";
constexpr const char AFTER_PATH[] = "tmr_exposure_after.bmp";
//! Frames discarded after the write so the sensor settles on the new exposure.
// A UVC device applies an exposure change over the next few frames rather than at once, and
// frames already in flight still carry the old setting. Five is a pragmatic figure: too few and
// the "after" image still shows the old exposure, more only costs time.
constexpr int SETTLE_FRAMES = 5;

bool captureAndSave( wse::tmr::WebCamera* const p_camera_inout, const char* const path_in )
{
    wse::tmr::WebCamera& camera_inout = *p_camera_inout;

    wse::img3c08_t image;
    if ( !tmr_example::readRgbImage( &camera_inout, &image, READ_TIMEOUT_MS ) )
    {
        return false;
    }
    return tmr_example::saveImageAsBmp( image, path_in );
}

} // namespace

int main()
{
    using namespace wse::tmr;

    wse::registDefaultLog();

    // A machine with no camera exits 0 deliberately, so the samples can be run anywhere; a
    // failure after a device was found keeps its own non-zero code.
    WebCamera camera;
    sCameraDeviceInfo device;
    sCameraStreamConfiguration configuration;
    if ( !tmr_example::openFirstCamera( &camera, &device, &configuration ) )
    {
        return device.valid() ? 2 : 0;
    }
    // The stream runs for the whole sample. A control is read and written on the open session
    // without stopping it, which is what makes a before and an after image comparable.
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
        // One image with the device's current exposure, saved before any change.
        if ( !captureAndSave( &camera, BEFORE_PATH ) ) { exit_code = 4; break; }

        // The advertised capability carries the valid range, step, unit, and access.
        // This is the only honest source for those numbers: they differ per device, and a value
        // outside the advertised range is refused as a Validation error before the driver ever
        // sees it. A camera that does not expose the control at all reports UnsupportedControl.
        const auto capability = camera.controlCapability( eCameraControl::Exposure );
        if ( !capability.succeeded() )
        {
            tmr_example::logCameraError( "exposure capability", capability.error() );
            exit_code = 5;
            break;
        }
        const auto& exposure = capability.value();
        wse::WLog() << "exposure capability: range=[" << exposure.minimum << ","
                    << exposure.maximum << "] step=" << exposure.step
                    << "unit=" << static_cast< int >( exposure.unit )
                    << "manual=" << exposure.supports_manual
                    << "writable=" << exposure.writable;
        // A fixed-exposure or automatic-only device is not an error either; the sample leaves
        // exit_code at zero and skips to the cleanup.
        if ( !exposure.writable || !exposure.supports_manual )
        {
            wse::WLog() << "this camera does not accept a manual exposure; nothing to change";
            break;
        }

        // readExposure() is a named shortcut for getControl( eCameraControl::Exposure ) and
        // carries no behaviour of its own. It queries the driver, which is why it is not const.
        // The value read here is what the restore at the end of the sample writes back.
        const auto original = camera.readExposure();
        if ( !original.succeeded() )
        {
            tmr_example::logCameraError( "readExposure", original.error() );
            exit_code = 6;
            break;
        }
        wse::WLog() << "exposure before: value=" << original.value().value
                    << "physical=" << exposure.physicalFromValue( original.value().value )
                    << "mode=" << static_cast< int >( original.value().mode );

        // Move to a clearly different position on the normalized 0.0-1.0 scale.
        // Working in normalized terms is what keeps this portable: the raw range is per device,
        // and valueFromNormalized() also rounds onto the advertised step, so the result is
        // always a value the device will accept. The 0.75 fallback only exists so the sample
        // still changes something on a device already sitting at the quarter position.
        std::int64_t target = exposure.valueFromNormalized( 0.25 );
        if ( target == original.value().value )
        {
            target = exposure.valueFromNormalized( 0.75 );
        }
        const auto written = camera.writeExposure( target, eCameraControlMode::Manual );
        if ( !written.succeeded() )
        {
            tmr_example::logCameraError( "writeExposure", written.error() );
            exit_code = 7;
            break;
        }
        wse::WLog() << "exposure after: value=" << target
                    << "physical=" << exposure.physicalFromValue( target )
                    << "mode=Manual";

        // The results are discarded on purpose; these frames exist only to be thrown away. A
        // failure here is not acted on either, because the capture that follows reports its own.
        for ( int skipped = 0; skipped < SETTLE_FRAMES; ++skipped )
        {
            camera.readFrame( READ_TIMEOUT_MS );
        }
        // One image with the new exposure, saved after the change.
        if ( !captureAndSave( &camera, AFTER_PATH ) ) { exit_code = 8; break; }

        // Restore the setting the device had before this sample ran.
        // A UVC control survives the process, so a sample that left the exposure where it put
        // it would change every later capture on that machine. Writing back the whole
        // sCameraControlValue restores the mode as well as the number, which matters for a
        // device that was in Automatic: in that mode the integer is ignored and only the mode
        // is put back. close() does not undo a control write, so this is not optional.
        const auto restored = camera.setControl( original.value() );
        if ( !restored.succeeded() )
        {
            tmr_example::logCameraError( "restore exposure", restored.error() );
            exit_code = 9;
            break;
        }
        wse::WLog() << "restored the original exposure";
    } while ( false );

    // stop() ends the streaming and leaves the device open; close() ends the session. Both are
    // safe to call more than once, and the destructor would do the same.
    camera.stop();
    camera.close();
    return exit_code;
}
