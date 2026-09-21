//*****************************************************************************************************************
//! @file    CameraTypes.cpp
//! @brief   \~japanese Portable Tmr CameraのData契約の実装.
//! @brief   \~english  Portable Tmr camera data-contract implementation.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <tmr/camera/CameraTypes.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace wse
{
namespace tmr
{

bool sCameraUsbIdentity::available() const noexcept
{
    return this->vendor_id != 0U || this->product_id != 0U
        || !this->serial_number.empty() || this->uvc_version_bcd != 0U;
}

// Automatic names a selection policy, not a device, so an identity claiming it is malformed.
bool sCameraDeviceInfo::valid() const noexcept
{
    return this->backend != eCameraBackend::Automatic && !this->id.empty();
}

bool sCameraFormat::valid() const noexcept
{
    return this->width > 0U && this->height > 0U
        && this->frame_rate_numerator > 0U && this->frame_rate_denominator > 0U
        && this->pixel_format != eCameraPixelFormat::Unknown;
}

double sCameraFormat::framesPerSecond() const noexcept
{
    if( this->frame_rate_denominator == 0U )
        return 0.0;
    return static_cast< double >( this->frame_rate_numerator )
        / static_cast< double >( this->frame_rate_denominator );
}

bool sCameraStreamProfile::valid() const noexcept
{
    if( !this->native_format.valid() || this->output_formats.empty() )
        return false;
    return std::none_of( this->output_formats.begin(), this->output_formats.end(),
        []( const eCameraPixelFormat format )
        {
            return format == eCameraPixelFormat::Unknown;
        } );
}

bool sCameraStreamProfile::supportsOutput( const eCameraPixelFormat format_in ) const noexcept
{
    return std::find( this->output_formats.begin(), this->output_formats.end(), format_in )
        != this->output_formats.end();
}

bool sCameraStreamConfiguration::valid() const noexcept
{
    return this->native_format.valid() && this->output_format != eCameraPixelFormat::Unknown
        && ( this->allow_conversion
            || this->native_format.pixel_format == this->output_format );
}

bool sCameraExtensionUnitSelector::valid() const noexcept
{
    constexpr std::size_t MAXIMUM_PUBLIC_XU_PAYLOAD = 65535U;
    return this->unit_id != 0U && this->selector != 0U
        && this->maximum_size > 0U && this->minimum_size <= this->maximum_size
        && this->maximum_size <= MAXIMUM_PUBLIC_XU_PAYLOAD
        && ( this->readable || this->writable );
}

bool sCameraExtensionUnitValue::valid() const noexcept
{
    return this->selector.valid()
        && this->payload.size() >= this->selector.minimum_size
        && this->payload.size() <= this->selector.maximum_size;
}

bool sCameraControlCapability::valid() const noexcept
{
    return this->minimum <= this->default_value && this->default_value <= this->maximum
        && this->minimum <= this->maximum && this->step > 0
        && std::isfinite( this->physical_scale ) && this->physical_scale > 0.0
        && ( this->supports_manual || this->supports_automatic )
        && ( this->readable || this->writable );
}

// Maps 0..1 onto the control range by whole steps: the position picks a step index, so the
// result always lands on a value the device can actually hold. Arithmetic runs in long double
// because a 64-bit range times a step count overflows double's integer precision.
std::int64_t sCameraControlCapability::valueFromNormalized(
    const double normalized_in ) const noexcept
{
    if( !this->valid() || !std::isfinite( normalized_in ) )
        return this->default_value;
    const double position = std::max( 0.0, std::min( 1.0, normalized_in ) );
    const long double minimum = static_cast< long double >( this->minimum );
    const long double maximum = static_cast< long double >( this->maximum );
    const long double step = static_cast< long double >( this->step );
    const long double step_count = std::floor( ( maximum - minimum ) / step );
    const long double step_index = std::round(
        static_cast< long double >( position ) * step_count );
    const long double aligned = std::max(
        minimum, std::min( maximum, minimum + step_index * step ) );
    return static_cast< std::int64_t >( aligned );
}

double sCameraControlCapability::normalizedFromValue( const std::int64_t value_in ) const noexcept
{
    if( !this->valid() || this->minimum == this->maximum )
        return 0.0;
    const std::int64_t bounded_value = std::max(
        this->minimum, std::min( this->maximum, value_in ) );
    return static_cast< double >(
        ( static_cast< long double >( bounded_value ) - this->minimum )
        / ( static_cast< long double >( this->maximum ) - this->minimum ) );
}

double sCameraControlCapability::physicalFromValue( const std::int64_t value_in ) const noexcept
{
    if( !this->valid() )
        return 0.0;
    const std::int64_t bounded_value = std::max(
        this->minimum, std::min( this->maximum, value_in ) );
    return static_cast< double >( bounded_value ) * this->physical_scale;
}

std::int64_t sCameraControlCapability::valueFromPhysical(
    const double physical_in ) const noexcept
{
    if( !this->valid() || !std::isfinite( physical_in ) )
        return this->default_value;
    const long double native = static_cast< long double >( physical_in )
        / static_cast< long double >( this->physical_scale );
    const long double minimum = static_cast< long double >( this->minimum );
    const long double maximum = static_cast< long double >( this->maximum );
    const long double step = static_cast< long double >( this->step );
    const long double bounded = std::max( minimum, std::min( maximum, native ) );
    const long double aligned = std::max( minimum, std::min(
        maximum, minimum + std::round( ( bounded - minimum ) / step ) * step ) );
    return static_cast< std::int64_t >( aligned );
}

std::size_t sCameraFrameDescription::bytesPerPixel() const noexcept
{
    switch( this->pixel_format )
    {
        case eCameraPixelFormat::Gray8:   return 1U;
        case eCameraPixelFormat::Rgb8:
        case eCameraPixelFormat::Bgr8:    return 3U;
        case eCameraPixelFormat::Bgra8:   return 4U;
        case eCameraPixelFormat::Yuyv422:
        case eCameraPixelFormat::Uyvy422: return 2U;
        case eCameraPixelFormat::Gray16:  return 2U;
        case eCameraPixelFormat::Rgb16:
        case eCameraPixelFormat::Bgr16:   return 6U;
        // A Bayer pixel carries one sixteen-bit sample; the colour it stands for is positional.
        case eCameraPixelFormat::Bayer16Rggb:
        case eCameraPixelFormat::Bayer16Bggr:
        case eCameraPixelFormat::Bayer16Grbg:
        case eCameraPixelFormat::Bayer16Gbrg: return 2U;
        default:                          return 0U;
    }
}

// NV12's stride is its luma width; the interleaved chroma plane reuses the same stride at half
// the height, so "width times bytes per pixel" would be wrong for it.
std::size_t sCameraFrameDescription::minimumRowStride() const noexcept
{
    if( this->pixel_format == eCameraPixelFormat::Nv12 )
        return this->width;
    const std::size_t bytes_per_pixel = this->bytesPerPixel();
    if( this->width == 0U || bytes_per_pixel == 0U
        || static_cast< std::size_t >( this->width )
            > std::numeric_limits< std::size_t >::max() / bytes_per_pixel )
        return 0U;
    return static_cast< std::size_t >( this->width ) * bytes_per_pixel;
}

// Every multiplication is overflow-checked and any inconsistency answers 0, which valid()
// then rejects — a corrupt description must never turn into a huge allocation. MJPEG answers 0
// by design: a compressed frame has no computable size.
std::size_t sCameraFrameDescription::memorySize() const noexcept
{
    if( this->height == 0U )
        return 0U;
    // Packed 4:2:2 shares chroma across complete two-pixel groups; height may be odd.
    if( ( this->pixel_format == eCameraPixelFormat::Uyvy422
            || this->pixel_format == eCameraPixelFormat::Yuyv422 ) && this->width % 2U != 0U )
        return 0U;
    if( this->pixel_format == eCameraPixelFormat::Nv12 )
    {
        if( this->width == 0U || ( this->width % 2U ) != 0U || ( this->height % 2U ) != 0U )
            return 0U;
        const std::size_t effective_stride = this->row_stride == 0U
            ? static_cast< std::size_t >( this->width ) : this->row_stride;
        if( effective_stride < this->width
            || effective_stride > std::numeric_limits< std::size_t >::max() / this->height )
            return 0U;
        const std::size_t luma_size = effective_stride * this->height;
        const std::size_t chroma_rows = this->height / 2U;
        if( effective_stride > ( std::numeric_limits< std::size_t >::max() - luma_size ) / chroma_rows )
            return 0U;
        return luma_size + effective_stride * chroma_rows;
    }
    if( this->pixel_format == eCameraPixelFormat::Mjpeg )
        return 0U;
    const std::size_t minimum_stride = this->minimumRowStride();
    const std::size_t effective_stride = this->row_stride == 0U ? minimum_stride : this->row_stride;
    if( minimum_stride == 0U || effective_stride < minimum_stride
        || effective_stride > std::numeric_limits< std::size_t >::max() / this->height )
        return 0U;
    return effective_stride * this->height;
}

bool sCameraFrameDescription::valid() const noexcept
{
    if( this->width == 0U || this->height == 0U || this->pixel_format == eCameraPixelFormat::Unknown )
        return false;
    if( this->pixel_format == eCameraPixelFormat::Mjpeg )
        return this->row_stride == 0U;
    return this->memorySize() > 0U;
}

bool sCameraFrame::valid() const noexcept
{
    if( !this->description.valid() )
        return false;
    if( this->description.pixel_format == eCameraPixelFormat::Mjpeg )
        return !this->data.empty();
    return this->data.size() >= this->description.memorySize();
}

} // namespace tmr
} // namespace wse
