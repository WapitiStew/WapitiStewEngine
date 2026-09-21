// @file example_camera_utility.h
// @brief Shared helpers for the C++ tmr camera samples: first-device selection, reading a
//        frame as a Core wse::Image_, structured-error logging, and a BMP writer that takes
//        an image rather than raw frame bytes.
//
// This header belongs to the samples only; it is not part of the public API.
//
// Every tmr call reached from here reports through the camera contract rather than by throwing:
// a CameraStatus or a CameraResult carries the failure, and the sample decides what to do with
// it. The Core image layer underneath toRgbImage() and saveImageAsBmp() is the exception; it
// signals an empty or mismatched image by throwing std::invalid_argument, which no sample catches.
//
// Ownership: a WebCamera owns its device session and none of these helpers take it. Whoever
// calls openFirstCamera() owns the matching close(). close() is noexcept and safe to repeat, and
// the destructor stops the stream and closes anyway, so the explicit close() in each sample is
// there to show the call rather than to prevent a leak.

#ifndef WONDERSTEWENGINE_EXAMPLE_TMR_EXAMPLE_CAMERA_UTILITY_H
#define WONDERSTEWENGINE_EXAMPLE_TMR_EXAMPLE_CAMERA_UTILITY_H

#include <tmr/stew.h>
#include <wse/stew.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace tmr_example
{

//! Logs one structured camera failure without collapsing it into free text.
// A caller branches on category() and code(), which are stable across backends. nativeCode() is
// the Media Foundation or V4L2 value behind the failure and is for diagnosis only; message() is
// free text and may be reworded at any time, so neither belongs in a condition.
inline void logCameraError(
      const std::string& operation_in
    , const wse::tmr::CameraError& error_in )
{
    wse::WLog() << "ERROR:" << operation_in << "failed."
                << "category=" << static_cast< int >( error_in.category() )
                << "code=" << static_cast< int >( error_in.code() )
                << "native=" << error_in.nativeCode()
                << "message=" << error_in.message();
}

//! Picks the profile to open: the first one that can convert to a format the samples can turn
//! into an image (BGR8, BGRA8, or RGB8), or the first profile with its own first output format.
inline bool selectStreamConfiguration(
      wse::tmr::sCameraStreamConfiguration* const p_configuration_out
    , const wse::tmr::sCameraCapability&         capability_in )
{
    // These three are the formats that map onto a Core pixel format without a colour
    // conversion, so a frame in one of them reaches an image by a memory rearrangement only.
    // BGR8 leads because it is what a Windows UVC device usually offers first.
    const wse::tmr::eCameraPixelFormat preferred[] = {
          wse::tmr::eCameraPixelFormat::Bgr8
        , wse::tmr::eCameraPixelFormat::Bgra8
        , wse::tmr::eCameraPixelFormat::Rgb8
    };
    // Format wins over resolution: the first profile that can produce a preferred format is
    // taken, whatever its frame size. A sample that cared about size would sort the profiles
    // first and only then ask supportsOutput().
    for ( const auto format : preferred )
    {
        for ( const auto& profile : capability_in.stream_profiles )
        {
            if ( profile.supportsOutput( format ) )
            {
                *p_configuration_out = { profile.native_format, format, true };
                return true;
            }
        }
    }
    // Fallback for a device that offers none of the three, for instance a YUYV-only or
    // MJPEG-only camera. The configuration still asks for a conversion, and the trailing true
    // is allow_conversion: with false the device would have to produce the output format
    // natively and open() would fail here instead of converting.
    for ( const auto& profile : capability_in.stream_profiles )
    {
        if ( !profile.output_formats.empty() )
        {
            *p_configuration_out = { profile.native_format,
                                     profile.output_formats.front(), true };
            return true;
        }
    }
    // No profile advertises any output format at all; the caller reports this rather than
    // opening the device on a guess.
    return false;
}

//! Enumerates the attached web cameras, logs the list, and opens the first device.
//! When p_configuration_inout already holds a valid request it is used as-is; otherwise
//! the helper fills it from the device capability.
// p_device_out is filled as soon as one device is enumerated, which is before the open is
// attempted. Each sample reads device.valid() afterwards to tell "no camera on this machine"
// from "a camera was there and a later step failed", and returns 0 for the first. An
// enumeration or capability failure also leaves p_device_out unset, so it takes the same
// silent exit as an absent camera.
inline bool openFirstCamera(
      wse::tmr::WebCamera*                    p_camera_inout
    , wse::tmr::sCameraDeviceInfo*            p_device_out
    , wse::tmr::sCameraStreamConfiguration*   p_configuration_inout )
{
    using namespace wse::tmr;

    // The default backend argument lets the platform choose Media Foundation or V4L2. The
    // result lists USB/UVC devices only: CSI, network, and virtual cameras are filtered out,
    // so an empty list on a laptop with a virtual meeting camera is the expected answer.
    const auto devices = WebCamera::enumerate();
    if ( !devices.succeeded() )
    {
        logCameraError( "camera enumeration", devices.error() );
        return false;
    }
    if ( devices.value().empty() )
    {
        wse::WLog() << "no USB/UVC web camera is attached";
        return false;
    }
    for ( std::size_t index = 0; index < devices.value().size(); ++index )
    {
        const auto& device = devices.value()[ index ];
        wse::WLog() << "device" << index << ": id=" << device.id
                    << "name=" << device.display_name;
    }

    // The samples connect to the first enumerated ID.
    *p_device_out = devices.value().front();

    // A caller that already knows which profile it wants skips the query entirely; this is how
    // resolution_change.cpp opens the same device twice with two different frame sizes.
    if ( !p_configuration_inout->valid() )
    {
        // The static capabilities() inspects a device it does not own, so no session exists yet
        // and nothing has to be closed if this fails. The member currentCapabilities() is the
        // one that reports on an open session.
        const auto capability = WebCamera::capabilities( *p_device_out );
        if ( !capability.succeeded() )
        {
            logCameraError( "capability query", capability.error() );
            return false;
        }
        if ( !selectStreamConfiguration( p_configuration_inout, capability.value() ) )
        {
            wse::WLog() << "ERROR: the first camera has no usable stream profile";
            return false;
        }
    }

    // open() fixes the resolution for the whole session; changing it means close() and open()
    // again. It only creates the session, so start() is still needed before any frame arrives.
    // On failure no session exists and the caller has nothing to close.
    const auto opened = p_camera_inout->open( *p_device_out, *p_configuration_inout );
    if ( !opened.succeeded() )
    {
        logCameraError( "open", opened.error() );
        return false;
    }
    wse::WLog() << "opened" << p_device_out->display_name << ":"
                << p_configuration_inout->native_format.width << "x"
                << p_configuration_inout->native_format.height
                << "output_format="
                << static_cast< int >( p_configuration_inout->output_format );
    return true;
}

//! Converts one frame into an RGB image, naming every conversion the camera format needs.
//! A frame whose channel order or channel count differs is converted explicitly, because the
//! engine never reorders channels or drops an alpha channel on its own.
// The destination type, not a runtime argument, selects which toImage() overload runs, and
// toImage() folds the frame's row_stride away, so the resulting image is always packed. The
// frame keeps ownership of its bytes throughout; every image below is a fresh allocation.
// Note the difference in how failures arrive: toImage() returns a CameraStatus, while
// convertChannelOrder() and castData::image() throw std::invalid_argument instead.
inline bool toRgbImage( wse::img3c08_t* const p_image_out, const wse::tmr::sCameraFrame& frame_in )
{
    wse::img3c08_t& image_out = *p_image_out;

    using namespace wse::tmr;

    switch ( frame_in.description.pixel_format )
    {
        // These already are the destination format, or are converted by the engine.
        // Yuyv422 and Nv12 are subsampled, so they have no direct Core format; toImage() into
        // an img3c08_t is the one conversion offered for them, using the BT.601 matrix.
        case eCameraPixelFormat::Rgb8:
        case eCameraPixelFormat::Yuyv422:
        case eCameraPixelFormat::Uyvy422:
        case eCameraPixelFormat::Nv12:
        {
            const CameraStatus converted = toImage( &image_out, frame_in );
            if ( !converted.succeeded() )
            {
                logCameraError( "toImage", converted.error() );
                return false;
            }
            return true;
        }
        // A BGR frame keeps its order in the image; swapping it is an explicit call.
        case eCameraPixelFormat::Bgr8:
        {
            wse::img3c08_bgr_t bgr;
            const CameraStatus converted = toImage( &bgr, frame_in );
            if ( !converted.succeeded() )
            {
                logCameraError( "toImage", converted.error() );
                return false;
            }
            image_out =
                wse::convertChannelOrder< wse::ePixFormat::CH3D8, wse::ePixFormat::BGR3D8 >( bgr );
            return true;
        }
        // A BGRA frame keeps four channels; dropping the alpha is also an explicit call.
        case eCameraPixelFormat::Bgra8:
        {
            wse::img4c08_bgra_t bgra;
            const CameraStatus converted = toImage( &bgra, frame_in );
            if ( !converted.succeeded() )
            {
                logCameraError( "toImage", converted.error() );
                return false;
            }
            const wse::img4c08_t rgba =
                wse::convertChannelOrder< wse::ePixFormat::CH4D8, wse::ePixFormat::BGRA4D8 >( bgra );
            image_out = wse::castData::image< wse::ePixFormat::CH3D08, wse::ePixFormat::CH4D08 >( rgba );
            return true;
        }
        // Gray8, the sixteen-bit formats, and Bayer all have a Core format, but not a
        // three-channel eight-bit one, and Mjpeg has none at all because the engine carries no
        // decoder. A sample that wanted them would pick the matching image type, or call
        // demosaicFrame() first for Bayer.
        default:
        {
            wse::WLog() << "ERROR: this camera pixel format has no RGB image conversion:"
                        << static_cast< int >( frame_in.description.pixel_format );
            return false;
        }
    }
}

//! Reads one frame from the camera and hands it back as an RGB image.
// The camera has to be streaming already; readFrame() on a stopped session reports
// NotStreaming rather than waiting. timeout_ms_in bounds the wait for one frame only: too small
// a value turns an ordinary slow first frame into a Timeout-category failure, while a large one
// simply blocks the calling thread for longer, since the call has no other way to give up.
// The returned sCameraFrame owns a copy of the pixel bytes, so it outlives the driver buffer
// and stays valid after stop() or close().
inline bool readRgbImage(
      wse::tmr::WebCamera* const p_camera_inout
    , wse::img3c08_t* const     p_image_out
    , const std::uint32_t        timeout_ms_in )
{
    const wse::tmr::CameraResult< wse::tmr::sCameraFrame > frame =
        p_camera_inout->readFrame( timeout_ms_in );
    if ( !frame.succeeded() )
    {
        logCameraError( "readFrame", frame.error() );
        return false;
    }
    return toRgbImage( p_image_out, frame.value() );
}

//! Writes a Core image as an uncompressed bottom-up 24-bit BMP file. The image type carries
//! the channel count and the channel order, so the writer needs no runtime format argument.
//! The repository has no image writer, so the samples carry this minimal one.
template< wse::ePixFormat _Pf >
inline bool saveImageAsBmp( const wse::Image_< _Pf >& image_in, const std::string& path_in )
{
    constexpr std::size_t channels = static_cast< std::size_t >( wse::PixelSize< _Pf > );
    constexpr bool is_bgr = wse::getChannelOrder( _Pf ) == wse::eColorChannelOrder::Bgr;
    static_assert( wse::PixelBitDepth< _Pf > == 8U
                 , "The BMP writer takes an eight-bit image." );
    static_assert( channels == 1U || channels == 3U || channels == 4U
                 , "The BMP writer takes a one-, three-, or four-channel image." );

    if ( image_in.width() == 0U || image_in.height() == 0U )
    {
        wse::WLog() << "ERROR: an empty image has no BMP file";
        return false;
    }

    // Whatever the source carries, the file is written as 24-bit BGR: a one-channel image is
    // replicated across the three channels and a four-channel image loses its alpha here. That
    // is a limit of this writer and of the BMP layout it emits, not of the image type.
    const std::uint32_t width  = static_cast< std::uint32_t >( image_in.width() );
    const std::uint32_t height = static_cast< std::uint32_t >( image_in.height() );
    // BMP pads every row up to a four-byte boundary, so the row length is not width * 3.
    const std::uint32_t row_bytes = ( width * 3U + 3U ) & ~3U;
    const std::uint32_t pixel_bytes = row_bytes * height;
    const std::uint32_t header_bytes = 14U + 40U;
    const std::uint32_t file_bytes = header_bytes + pixel_bytes;

    // Fourteen-byte file header followed by a forty-byte BITMAPINFOHEADER, every multi-byte
    // field little-endian. A positive height in the header is what declares the bottom-up row
    // order the loop below writes.
    std::vector< std::uint8_t > header( header_bytes, 0U );
    const auto put32 = [&header]( const std::size_t offset, const std::uint32_t value )
    {
        header[ offset ] = static_cast< std::uint8_t >( value );
        header[ offset + 1U ] = static_cast< std::uint8_t >( value >> 8U );
        header[ offset + 2U ] = static_cast< std::uint8_t >( value >> 16U );
        header[ offset + 3U ] = static_cast< std::uint8_t >( value >> 24U );
    };
    header[ 0U ] = 'B';
    header[ 1U ] = 'M';
    put32( 2U, file_bytes );
    put32( 10U, header_bytes );
    put32( 14U, 40U );
    put32( 18U, width );
    put32( 22U, height );
    header[ 26U ] = 1U;  // planes
    header[ 28U ] = 24U; // bits per pixel
    put32( 34U, pixel_bytes );

    std::ofstream file( path_in, std::ios::binary );
    if ( !file )
    {
        wse::WLog() << "ERROR: cannot create BMP file" << path_in;
        return false;
    }
    file.write( reinterpret_cast< const char* >( header.data() ),
                static_cast< std::streamsize >( header.size() ) );

    std::vector< std::uint8_t > row( row_bytes, 0U );
    for ( std::uint32_t y = 0; y < height; ++y )
    {
        // BMP stores rows bottom-up; an image is top-down. BMP itself is BGR.
        const auto* const p_source = image_in[ height - 1U - y ];
        for ( std::uint32_t x = 0; x < width; ++x )
        {
            if ( channels == 1U )
            {
                const std::uint8_t gray = p_source[ x ][ 0U ];
                row[ x * 3U + 0U ] = gray;
                row[ x * 3U + 1U ] = gray;
                row[ x * 3U + 2U ] = gray;
            }
            else
            {
                row[ x * 3U + 0U ] = is_bgr ? p_source[ x ][ 0U ] : p_source[ x ][ 2U ];
                row[ x * 3U + 1U ] = p_source[ x ][ 1U ];
                row[ x * 3U + 2U ] = is_bgr ? p_source[ x ][ 2U ] : p_source[ x ][ 0U ];
            }
        }
        file.write( reinterpret_cast< const char* >( row.data() ),
                    static_cast< std::streamsize >( row.size() ) );
    }
    if ( !file )
    {
        wse::WLog() << "ERROR: BMP write failed for" << path_in;
        return false;
    }

    wse::WLog() << "saved image" << path_in << "(" << width << "x" << height
                << "channels=" << channels << ")";
    return true;
}

} // namespace tmr_example

#endif // WONDERSTEWENGINE_EXAMPLE_TMR_EXAMPLE_CAMERA_UTILITY_H
