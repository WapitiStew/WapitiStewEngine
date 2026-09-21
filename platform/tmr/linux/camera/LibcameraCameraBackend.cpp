//*****************************************************************************************************************
//! @file    LibcameraCameraBackend.cpp
//! @brief   \~japanese Portable Tmr Camera境界のlibcamera実装. WSE_HAS_LIBCAMERA無しのBuildでは
//!                     全操作がUnsupportedBackendを返すStubとして同じ関数群を提供する.
//! @brief   \~english  libcamera implementation of the portable Tmr camera boundary. A build without
//!                     WSE_HAS_LIBCAMERA provides the same functions as a stub whose every operation
//!                     returns UnsupportedBackend.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include "LinuxCameraBackends.h"

#if defined(WSE_HAS_LIBCAMERA)

#include "LibcameraResources.h"
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/property_ids.h>
#include <libcamera/request.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sys/mman.h>
#include <utility>
#include <vector>

namespace wse
{
namespace tmr
{
namespace detail
{
namespace
{

// libcameraは失敗を負のerrnoで返すので符号を戻してから分類する。ENODEV／ENXIO／EIOは
// Device切断として、呼び出し元の指定より優先してDeviceDisconnectedへ寄せる.
CameraError libcameraError(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode code_in
    , const char* const message_in
    , const int native_code_in = 0 )
{
    const int error = native_code_in < 0 ? -native_code_in : native_code_in;
    const bool disconnected = error == ENODEV || error == ENXIO || error == EIO;
    return makeCameraError(
          disconnected ? eCameraErrorCategory::Device : category_in
        , disconnected ? eCameraErrorCode::DeviceDisconnected : code_in
        , message_in, native_code_in );
}

eCameraPixelFormat fromLibcameraFormat( const libcamera::PixelFormat& format_in )
{
    using namespace libcamera;
    if( format_in == formats::R8 ) return eCameraPixelFormat::Gray8;
    if( format_in == formats::RGB888 ) return eCameraPixelFormat::Rgb8;
    if( format_in == formats::BGR888 ) return eCameraPixelFormat::Bgr8;
    if( format_in == formats::XRGB8888 || format_in == formats::ARGB8888 )
        return eCameraPixelFormat::Bgra8;
    if( format_in == formats::YUYV ) return eCameraPixelFormat::Yuyv422;
    if( format_in == formats::UYVY ) return eCameraPixelFormat::Uyvy422;
    if( format_in == formats::NV12 ) return eCameraPixelFormat::Nv12;
    if( format_in == formats::MJPEG ) return eCameraPixelFormat::Mjpeg;
    return eCameraPixelFormat::Unknown;
}

std::optional< libcamera::PixelFormat > toLibcameraFormat( const eCameraPixelFormat format_in )
{
    using namespace libcamera;
    switch( format_in )
    {
        case eCameraPixelFormat::Gray8: return formats::R8;
        case eCameraPixelFormat::Rgb8: return formats::RGB888;
        case eCameraPixelFormat::Bgr8: return formats::BGR888;
        case eCameraPixelFormat::Bgra8: return formats::XRGB8888;
        case eCameraPixelFormat::Yuyv422: return formats::YUYV;
        case eCameraPixelFormat::Uyvy422: return formats::UYVY;
        case eCameraPixelFormat::Nv12: return formats::NV12;
        case eCameraPixelFormat::Mjpeg: return formats::MJPEG;
        default: return std::nullopt;
    }
}

sCameraDeviceInfo makeDeviceInfo( const std::shared_ptr< libcamera::Camera >& camera_in )
{
    sCameraDeviceInfo device;
    device.backend = eCameraBackend::Libcamera;
    device.id = camera_in->id();
    const std::optional< std::string_view > model =
        camera_in->properties().get( libcamera::properties::Model );
    device.display_name = model ? std::string( *model ) : camera_in->id();
    device.transport = "libcamera";
    std::string lower_id = device.id;
    std::transform( lower_id.begin(), lower_id.end(), lower_id.begin(),
        []( const unsigned char value ) { return static_cast< char >( std::tolower( value ) ); } );
    if( lower_id.find( "usb" ) != std::string::npos
        || lower_id.find( "uvc" ) != std::string::npos )
        device.transport_type = eCameraTransport::UsbUvc;
    return device;
}

const char* libcameraControlName( const eCameraControl control_in ) noexcept
{
    switch( control_in )
    {
        case eCameraControl::Exposure: return "Exposure";
        case eCameraControl::Gain: return "Gain";
        case eCameraControl::Focus: return "Focus";
        case eCameraControl::Brightness: return "Brightness";
        case eCameraControl::Contrast: return "Contrast";
        case eCameraControl::Saturation: return "Saturation";
        case eCameraControl::WhiteBalance: return "White balance";
        case eCameraControl::Sharpness: return "Sharpness";
        default: return "Camera control";
    }
}

template <typename T>
const libcamera::ControlInfo* controlInfo(
      const libcamera::Camera& camera_in
    , const libcamera::Control< T >& control_in )
{
    const auto entry = camera_in.controls().find( control_in.id() );
    return entry == camera_in.controls().end() ? nullptr : &entry->second;
}

// Portable Control値は整数で運ぶ決まりなので、libcameraのfloat Controlは1000倍した
// 千分率整数として持つ。対になるphysical_scaleは0.001で、往復すると元のfloatに戻る.
std::int64_t scaledValue( const float value_in )
{
    return static_cast< std::int64_t >( std::llround(
        static_cast< double >( value_in ) * 1000.0 ) );
}

void appendIntegerControl(
      std::vector< sCameraControlCapability >* const controls_out
    , const libcamera::Camera& camera_in
    , const libcamera::Control< int32_t >& native_in
    , const eCameraControl portable_in
    , const bool automatic_in
    , const eCameraControlUnit unit_in )
{
    const libcamera::ControlInfo* const info = controlInfo( camera_in, native_in );
    if( info == nullptr || info->min().isNone() || info->max().isNone() ) return;
    sCameraControlCapability capability;
    capability.control = portable_in;
    capability.minimum = info->min().get< int32_t >();
    capability.maximum = info->max().get< int32_t >();
    capability.step = 1;
    capability.default_value = info->def().isNone()
        ? capability.minimum : info->def().get< int32_t >();
    capability.supports_manual = true;
    capability.supports_automatic = automatic_in;
    capability.unit = unit_in;
    capability.readable = true;
    capability.writable = true;
    capability.display_name = libcameraControlName( portable_in );
    if( capability.valid() ) controls_out->push_back( capability );
}

void appendFloatControl(
      std::vector< sCameraControlCapability >* const controls_out
    , const libcamera::Camera& camera_in
    , const libcamera::Control< float >& native_in
    , const eCameraControl portable_in
    , const bool automatic_in
    , const eCameraControlUnit unit_in )
{
    const libcamera::ControlInfo* const info = controlInfo( camera_in, native_in );
    if( info == nullptr || info->min().isNone() || info->max().isNone() ) return;
    sCameraControlCapability capability;
    capability.control = portable_in;
    capability.minimum = scaledValue( info->min().get< float >() );
    capability.maximum = scaledValue( info->max().get< float >() );
    capability.step = 1;
    capability.default_value = info->def().isNone()
        ? capability.minimum : scaledValue( info->def().get< float >() );
    capability.supports_manual = true;
    capability.supports_automatic = automatic_in;
    capability.unit = unit_in;
    capability.physical_scale = 0.001;
    capability.readable = true;
    capability.writable = true;
    capability.display_name = libcameraControlName( portable_in );
    if( capability.valid() ) controls_out->push_back( capability );
}

// Portable Controlとlibcamera Controlの対応をここで一望できるようにする。自動Modeの有無は
// 対応するMode Control（ExposureTimeMode等）をCameraが公開しているかで決まる.
std::vector< sCameraControlCapability > queryControlCapabilities(
    const libcamera::Camera& camera_in )
{
    using namespace libcamera;
    std::vector< sCameraControlCapability > result;
    appendIntegerControl( &result, camera_in, controls::ExposureTime, eCameraControl::Exposure,
        controlInfo( camera_in, controls::ExposureTimeMode ) != nullptr,
        eCameraControlUnit::Microseconds );
    appendFloatControl( &result, camera_in, controls::AnalogueGain, eCameraControl::Gain,
        controlInfo( camera_in, controls::AnalogueGainMode ) != nullptr,
        eCameraControlUnit::GainMultiplier );
    appendFloatControl( &result, camera_in, controls::LensPosition, eCameraControl::Focus,
        controlInfo( camera_in, controls::AfMode ) != nullptr,
        eCameraControlUnit::Diopters );
    appendFloatControl( &result, camera_in, controls::Brightness, eCameraControl::Brightness,
        false, eCameraControlUnit::Relative );
    appendFloatControl( &result, camera_in, controls::Contrast, eCameraControl::Contrast,
        false, eCameraControlUnit::Relative );
    appendFloatControl( &result, camera_in, controls::Saturation, eCameraControl::Saturation,
        false, eCameraControlUnit::Relative );
    appendIntegerControl( &result, camera_in, controls::ColourTemperature, eCameraControl::WhiteBalance,
        controlInfo( camera_in, controls::AwbEnable ) != nullptr,
        eCameraControlUnit::Kelvin );
    appendFloatControl( &result, camera_in, controls::Sharpness, eCameraControl::Sharpness,
        false, eCameraControlUnit::Relative );
    return result;
}

std::optional< std::pair< std::int64_t, std::int64_t > > frameDurationRange(
    const libcamera::Camera& camera_in )
{
    const libcamera::ControlInfo* const info =
        controlInfo( camera_in, libcamera::controls::FrameDurationLimits );
    if( info == nullptr || info->min().isNone() || info->max().isNone() )
        return std::nullopt;
    const std::int64_t minimum = info->min().get< std::int64_t >();
    const std::int64_t maximum = info->max().get< std::int64_t >();
    if( minimum <= 0 || maximum < minimum
        || maximum > static_cast< std::int64_t >(
            std::numeric_limits< std::uint32_t >::max() ) )
        return std::nullopt;
    return std::make_pair( minimum, maximum );
}

// libcameraはV4L2のような離散Frame間隔の列挙を持たず、最短と最長のFrame時間しか教えない。
// そこで両端を、そして範囲に収まるときは実用上の標準である30fps相当を候補として並べる。
// Frame rateは分子1000000のマイクロ秒表現で持ち、重複はここで除いておく.
void appendFormats(
      std::vector< sCameraFormat >* const formats_out
    , const libcamera::Camera& camera_in
    , const libcamera::StreamConfiguration& configuration_in )
{
    const auto duration_range = frameDurationRange( camera_in );
    if( !duration_range ) return;
    std::vector< std::int64_t > durations = {
        duration_range->first, duration_range->second };
    constexpr std::int64_t nominal_30_fps_us = 33333;
    if( duration_range->first <= nominal_30_fps_us
        && nominal_30_fps_us <= duration_range->second )
        durations.push_back( nominal_30_fps_us );
    for( const libcamera::PixelFormat& native : configuration_in.formats().pixelformats() )
    {
        const eCameraPixelFormat portable_format = fromLibcameraFormat( native );
        if( portable_format == eCameraPixelFormat::Unknown ) continue;
        std::vector< libcamera::Size > sizes = configuration_in.formats().sizes( native );
        if( sizes.empty() )
        {
            const libcamera::SizeRange range = configuration_in.formats().range( native );
            sizes.push_back( range.min );
            if( range.max != range.min ) sizes.push_back( range.max );
        }
        for( const libcamera::Size& size : sizes )
        {
            for( const std::int64_t duration : durations )
            {
                sCameraFormat portable = { size.width, size.height, 1000000U,
                    static_cast< std::uint32_t >( duration ), portable_format };
                const bool duplicate = std::any_of( formats_out->begin(), formats_out->end(),
                    [ &portable ]( const sCameraFormat& existing_in )
                    {
                        return existing_in.width == portable.width
                            && existing_in.height == portable.height
                            && existing_in.frame_rate_numerator
                                == portable.frame_rate_numerator
                            && existing_in.frame_rate_denominator
                                == portable.frame_rate_denominator
                            && existing_in.pixel_format == portable.pixel_format;
                    } );
                if( !duplicate ) formats_out->push_back( portable );
            }
        }
    }
}

// ManagerとCameraと占有権を一体で所有するRAII owner。破棄はCameraのreleaseが先、
// Managerのstopが後という決まった順序を持つので、個別に持たせず一つに束ねる.
class CameraHandle final
{
    //! @brief Construct all members with explicit defaults.
public:
    CameraHandle()
        : manager  ()
        , camera   ()
        , acquired ( false )
    {
    }
private:

  public:
    LibcameraManagerLease< libcamera::CameraManager > manager;
    std::shared_ptr< libcamera::Camera > camera;
    bool acquired;

    ~CameraHandle()
    {
        if( this->acquired && this->camera ) (void)this->camera->release();
        this->camera.reset();
        this->manager.reset();
    }
};

CameraResult< CameraHandle* > openCameraHandle(
      std::unique_ptr< CameraHandle >* const handle_out
    , const std::string& id_in )
{
    auto handle = std::make_unique< CameraHandle >();
    const int start_result = handle->manager.acquire();
    if( start_result < 0 )
        return CameraResult< CameraHandle* >::failure( libcameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
            "Failed to start the libcamera manager.", start_result ) );
    handle->camera = handle->manager->get( id_in );
    if( !handle->camera )
        return CameraResult< CameraHandle* >::failure( makeCameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::DeviceNotFound,
            "The requested libcamera device was not found." ) );
    const int acquire_result = handle->camera->acquire();
    if( acquire_result < 0 )
        return CameraResult< CameraHandle* >::failure( libcameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::OpenFailed,
            "Failed to acquire the libcamera device.", acquire_result ) );
    handle->acquired = true;
    *handle_out = std::move( handle );
    return CameraResult< CameraHandle* >::success( handle_out->get() );
}

CameraResult< sCameraCapability > libcameraCapabilities( const sCameraDeviceInfo& device_in )
{
    std::unique_ptr< CameraHandle > handle;
    const CameraResult< CameraHandle* > opened = openCameraHandle( &handle, device_in.id );
    if( !opened.succeeded() ) return CameraResult< sCameraCapability >::failure( opened.error() );
    std::unique_ptr< libcamera::CameraConfiguration > configuration =
        handle->camera->generateConfiguration( { libcamera::StreamRole::Viewfinder } );
    if( !configuration || configuration->empty() )
        return CameraResult< sCameraCapability >::failure( makeCameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "libcamera did not provide a viewfinder configuration." ) );
    sCameraCapability capability;
    capability.device = makeDeviceInfo( handle->camera );
    appendFormats( &capability.formats, *handle->camera, configuration->at( 0U ) );
    for( const sCameraFormat& format : capability.formats )
    {
        sCameraStreamProfile profile;
        profile.native_format = format;
        profile.output_formats.push_back( format.pixel_format );
        capability.stream_profiles.push_back( std::move( profile ) );
    }
    capability.controls = queryControlCapabilities( *handle->camera );
    if( capability.formats.empty() )
        return CameraResult< sCameraCapability >::failure( makeCameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "libcamera exposes no supported portable format." ) );
    return CameraResult< sCameraCapability >::success( std::move( capability ) );
}

const sCameraControlCapability* findCapability(
      const std::vector< sCameraControlCapability >& controls_in
    , const eCameraControl control_in )
{
    const auto found = std::find_if( controls_in.begin(), controls_in.end(),
        [ control_in ]( const sCameraControlCapability& capability_in )
        { return capability_in.control == control_in; } );
    return found == controls_in.end() ? nullptr : &*found;
}

// ------------------------------------------------------------------------------------------------
// Backend本体。完了RequestはCamera manager threadから届くため、m_mutexが両Threadの境界になる.
// ------------------------------------------------------------------------------------------------

class LibcameraCameraBackend final : public ICameraBackend
{
    //! @brief Construct all members with explicit defaults.
public:
    LibcameraCameraBackend()
        : m_handle            ()
        , m_configuration     ()
        , m_allocator         ()
        , m_requests          ()
        , m_stream            ( nullptr )
        , m_frame_description ()
        , m_frame_duration_us ( 0 )
        , m_streaming         ( false )
        , m_disconnected      ( false )
        , m_mutex             ()
        , m_condition         ()
        , m_callback_failed   ( false )
        , m_native_active     ( false )
        , m_capabilities      ()
        , m_control_values    ()
    {
    }
private:

  private:
    std::unique_ptr< CameraHandle > m_handle;
    std::unique_ptr< libcamera::CameraConfiguration > m_configuration;
    std::unique_ptr< libcamera::FrameBufferAllocator > m_allocator;
    LibcameraRequests<libcamera::Request> m_requests;
    libcamera::Stream* m_stream;
    sCameraFrameDescription m_frame_description;
    std::int64_t m_frame_duration_us;
    std::atomic_bool m_streaming;
    std::atomic_bool m_disconnected;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic_bool m_callback_failed;
    bool m_native_active;
    std::vector< sCameraControlCapability > m_capabilities;
    std::map< eCameraControl, sCameraControlValue > m_control_values;

    // libcameraには「現在値を読む」APIが無く、実際に適用された値は各Frameに付くMetadataとして
    // しか返ってこない。完了Requestごとにここで写し取り、getControlはその写しを読む.
    void updateControlMetadata( const libcamera::ControlList& metadata_in )
    {
        using namespace libcamera;
        const auto update_integer = [ this, &metadata_in ](
              const eCameraControl portable_in, const Control< int32_t >& native_in )
        {
            const std::optional< int32_t > value = metadata_in.get( native_in );
            if( value ) this->m_control_values[ portable_in ].value = *value;
        };
        const auto update_float = [ this, &metadata_in ](
              const eCameraControl portable_in, const Control< float >& native_in )
        {
            const std::optional< float > value = metadata_in.get( native_in );
            if( value ) this->m_control_values[ portable_in ].value = scaledValue( *value );
        };
        update_integer( eCameraControl::Exposure, controls::ExposureTime );
        update_float( eCameraControl::Gain, controls::AnalogueGain );
        update_float( eCameraControl::Focus, controls::LensPosition );
        update_float( eCameraControl::Brightness, controls::Brightness );
        update_float( eCameraControl::Contrast, controls::Contrast );
        update_float( eCameraControl::Saturation, controls::Saturation );
        update_integer( eCameraControl::WhiteBalance, controls::ColourTemperature );
        update_float( eCameraControl::Sharpness, controls::Sharpness );
        if( const auto mode = metadata_in.get( controls::ExposureTimeMode ) )
            this->m_control_values[ eCameraControl::Exposure ].mode =
                *mode == controls::ExposureTimeModeManual ? eCameraControlMode::Manual
                                                         : eCameraControlMode::Automatic;
        if( const auto mode = metadata_in.get( controls::AnalogueGainMode ) )
            this->m_control_values[ eCameraControl::Gain ].mode =
                *mode == controls::AnalogueGainModeManual ? eCameraControlMode::Manual
                                                          : eCameraControlMode::Automatic;
        if( const auto mode = metadata_in.get( controls::AfMode ) )
            this->m_control_values[ eCameraControl::Focus ].mode =
                *mode == controls::AfModeManual ? eCameraControlMode::Manual
                                                : eCameraControlMode::Automatic;
        if( const auto automatic = metadata_in.get( controls::AwbEnable ) )
            this->m_control_values[ eCameraControl::WhiteBalance ].mode =
                *automatic ? eCameraControlMode::Automatic : eCameraControlMode::Manual;
    }

    // Camera manager threadから呼ばれる。CancelledはstopがQueueを畳んだ痕跡なので黙って捨てる.
    void onRequestComplete( libcamera::Request* const request_in ) noexcept
    {
        try
        {
            std::lock_guard< std::mutex > lock( this->m_mutex );
            this->m_requests.complete(request_in, this->m_streaming
                && request_in->status() != libcamera::Request::RequestCancelled);
            if(this->m_requests.fault()) this->m_callback_failed = true;
            this->m_condition.notify_all();
        }
        catch(...) { this->m_callback_failed = true; this->m_condition.notify_all(); }
    }

    void onDisconnected() noexcept
    {
        try
        {
            std::lock_guard<std::mutex> lock(this->m_mutex);
            this->m_disconnected = true;
            this->m_condition.notify_all();
        }
        catch(...) { this->m_disconnected = true; this->m_condition.notify_all(); }
    }

    // 一つのPortable Control値をlibcameraのControl listへ翻訳する。Modeを持つControlは
    // Mode設定と値設定が別Controlになっており、自動のときは値を送らない（Automatic modeは
    // 値を無視するという公開契約に合わせる）.
    void appendControl(
          libcamera::ControlList* const controls_out
        , const sCameraControlValue& value_in ) const
    {
        using namespace libcamera;
        const bool automatic = value_in.mode == eCameraControlMode::Automatic;
        const sCameraControlCapability* const capability =
            findCapability( this->m_capabilities, value_in.control );
        const bool has_mode_control = capability != nullptr
            && capability->supports_automatic;
        switch( value_in.control )
        {
            case eCameraControl::Exposure:
                if( has_mode_control ) controls_out->set( controls::ExposureTimeMode, automatic
                    ? controls::ExposureTimeModeAuto : controls::ExposureTimeModeManual );
                if( !automatic ) controls_out->set( controls::ExposureTime,
                    static_cast< int32_t >( value_in.value ) );
                break;
            case eCameraControl::Gain:
                if( has_mode_control ) controls_out->set( controls::AnalogueGainMode, automatic
                    ? controls::AnalogueGainModeAuto : controls::AnalogueGainModeManual );
                if( !automatic ) controls_out->set( controls::AnalogueGain,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            case eCameraControl::Focus:
                if( has_mode_control ) controls_out->set( controls::AfMode, automatic
                    ? controls::AfModeContinuous : controls::AfModeManual );
                if( !automatic ) controls_out->set( controls::LensPosition,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            case eCameraControl::Brightness:
                controls_out->set( controls::Brightness,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            case eCameraControl::Contrast:
                controls_out->set( controls::Contrast,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            case eCameraControl::Saturation:
                controls_out->set( controls::Saturation,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            case eCameraControl::WhiteBalance:
                if( has_mode_control ) controls_out->set( controls::AwbEnable, automatic );
                if( !automatic ) controls_out->set( controls::ColourTemperature,
                    static_cast< int32_t >( value_in.value ) );
                break;
            case eCameraControl::Sharpness:
                controls_out->set( controls::Sharpness,
                    static_cast< float >( value_in.value ) / 1000.0F );
                break;
            default:
                break;
        }
    }

    // libcameraのControlはRequest単位でしか運べないので、保持している全Control値と固定Frame
    // 時間を毎Requestに載せ直す。FrameDurationLimitsを最短＝最長にするのは、要求された
    // Frame rateを「正確に」守るため.
    libcamera::ControlList requestControls() const
    {
        std::lock_guard< std::mutex > lock( this->m_mutex );
        libcamera::ControlList controls( this->m_handle->camera->controls() );
        if( this->m_frame_duration_us > 0 )
            controls.set( libcamera::controls::FrameDurationLimits,
                { this->m_frame_duration_us, this->m_frame_duration_us } );
        for( const auto& control : this->m_control_values )
            this->appendControl( &controls, control.second );
        return controls;
    }

    CameraStatus createRequests()
    {
        const auto& buffers = this->m_allocator->buffers( this->m_stream );
        std::vector<std::unique_ptr<libcamera::Request>> requests;
        requests.reserve(buffers.size());
        for( std::size_t index = 0U; index < buffers.size(); ++index )
        {
            std::unique_ptr< libcamera::Request > request =
                this->m_handle->camera->createRequest( index );
            if( !request ) return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
                "Failed to allocate a libcamera request." ) );
            const int result = request->addBuffer( this->m_stream, buffers[ index ].get() );
            if( result < 0 ) return CameraStatus::failure( libcameraError(
                eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
                "Failed to attach a libcamera frame buffer.", result ) );
            requests.push_back( std::move( request ) );
        }
        std::lock_guard<std::mutex> lock(this->m_mutex);
        this->m_requests.prepare(std::move(requests));
        return CameraStatus::success();
    }

    int queueRequest(libcamera::Request* const request_in)
    {
        {
            std::lock_guard<std::mutex> lock(this->m_mutex);
            if(!this->m_requests.queued(request_in)) return -EINVAL;
        }
        const int result = this->m_handle->camera->queueRequest(request_in);
        if(result < 0)
        {
            std::lock_guard<std::mutex> lock(this->m_mutex);
            this->m_requests.rejected(request_in);
        }
        return result;
    }

    // No diagnostics are allocated during rollback. Never free a request that a
    // failed stop, or a disconnected pipeline, might still reference.
    int stopNative() noexcept
    {
        int result = 0;
        try
        {
            {
                std::lock_guard<std::mutex> lock(this->m_mutex);
                this->m_streaming = false;
                this->m_condition.notify_all();
            }
            if(this->m_native_active)
            {
                result = this->m_handle->camera->stop();
                if(result == 0) this->m_native_active = false;
            }
            std::lock_guard<std::mutex> lock(this->m_mutex);
            if(result == 0 && !this->m_disconnected) this->m_requests.quiesced();
            if(result == 0 && !this->m_requests.clear()) result = -EBUSY;
        }
        catch(const std::bad_alloc&) { result = -ENOMEM; }
        catch(...) { result = -EFAULT; }
        return result;
    }

    // 完了RequestのBufferを所有権付きのPortable frameへ写す。各Planeはその場でmmapして
    // Copyし、直ちにmunmapする。Frameが共有Memoryを指したまま外へ出ないので、Requestを
    // 再利用してもFrameは壊れない。Planeのoffsetは同じDMA Bufferを複数Planeが共有する
    // 場合があるため、Page境界からのmapに足し込んで読む.
    CameraResult< sCameraFrame > copyFrame( libcamera::Request* const request_in ) const
    {
        libcamera::FrameBuffer* const buffer = request_in->findBuffer( this->m_stream );
        if( buffer == nullptr || buffer->metadata().status != libcamera::FrameMetadata::FrameSuccess )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "libcamera returned an unsuccessful frame." ) );
        const auto native_planes = buffer->planes();
        const auto metadata_planes = buffer->metadata().planes();
        if( native_planes.size() != metadata_planes.size() )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "libcamera returned inconsistent plane metadata." ) );
        sCameraFrame frame;
        frame.description = this->m_frame_description;
        frame.sequence = buffer->metadata().sequence;
        frame.monotonic_timestamp_ns = buffer->metadata().timestamp
            > static_cast< std::uint64_t >( std::numeric_limits< std::int64_t >::max() )
                ? std::numeric_limits< std::int64_t >::max()
                : static_cast< std::int64_t >( buffer->metadata().timestamp );
        for( std::size_t index = 0U; index < native_planes.size(); ++index )
        {
            const libcamera::FrameBuffer::Plane& plane = native_planes[ index ];
            const std::size_t used = metadata_planes[ index ].bytesused;
            if( used > plane.length || plane.offset == libcamera::FrameBuffer::Plane::kInvalidOffset )
                return CameraResult< sCameraFrame >::failure( makeCameraError(
                    eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                    "libcamera returned an invalid plane length." ) );
            const int mapping_error = appendLibcameraPlane(&frame.data, {}, plane.fd.get(),
                plane.offset, plane.length, used);
            if( mapping_error != 0 ) return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "Failed to validate, copy or release a libcamera frame plane.", mapping_error ) );
        }
        if( !frame.valid() ) return CameraResult< sCameraFrame >::failure( makeCameraError(
            eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
            "libcamera returned a truncated portable frame." ) );
        return CameraResult< sCameraFrame >::success( std::move( frame ) );
    }

  public:
    ~LibcameraCameraBackend() override { this->close(); }

    CameraStatus open( const sCameraOpenDescription& description_in ) override
    {
        // 要求の検証はDeviceを触る前に済ませる。AdapterはPixel format変換をしない方針であり、
        // Frame rateもFormatも「頼んだ通りか、失敗か」の二択で、近い値への黙った丸めはしない.
        if( this->m_handle ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyOpen,
            "libcamera device is already open." ) );
        if( !description_in.device.valid() || !description_in.format.valid() )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
                "libcamera open requires a valid device and format." ) );
        const eCameraPixelFormat native_format = description_in.native_pixel_format
            == eCameraPixelFormat::Unknown
                ? description_in.format.pixel_format : description_in.native_pixel_format;
        if( native_format != description_in.format.pixel_format )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "libcamera adapter does not perform pixel-format conversion." ) );
        const std::optional< libcamera::PixelFormat > pixel_format =
            toLibcameraFormat( native_format );
        if( !pixel_format ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "Requested pixel format is not supported by the libcamera adapter." ) );
        std::unique_ptr< CameraHandle > handle;
        const CameraResult< CameraHandle* > opened = openCameraHandle( &handle, description_in.device.id );
        if( !opened.succeeded() ) return CameraStatus::failure( opened.error() );
        const auto duration_range = frameDurationRange( *handle->camera );
        const std::int64_t requested_duration = static_cast< std::int64_t >(
            ( static_cast< std::uint64_t >( description_in.format.frame_rate_denominator )
                * 1000000ULL ) / description_in.format.frame_rate_numerator );
        if( !duration_range || requested_duration < duration_range->first
            || requested_duration > duration_range->second )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "libcamera cannot provide the requested exact frame rate." ) );
        std::unique_ptr< libcamera::CameraConfiguration > configuration =
            handle->camera->generateConfiguration( { libcamera::StreamRole::Viewfinder } );
        if( !configuration || configuration->empty() ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
            "libcamera did not provide a viewfinder configuration." ) );
        libcamera::StreamConfiguration& stream_configuration = configuration->at( 0U );
        stream_configuration.pixelFormat = *pixel_format;
        stream_configuration.size = { description_in.format.width, description_in.format.height };
        // validateは通せる形へ勝手に調整することがある（StatusはAdjusted）。調整後の値が要求と
        // 一つでも違えば、それは頼んだFormatではないので失敗として返す.
        const libcamera::CameraConfiguration::Status validation = configuration->validate();
        if( validation == libcamera::CameraConfiguration::Invalid
            || stream_configuration.pixelFormat != *pixel_format
            || stream_configuration.size.width != description_in.format.width
            || stream_configuration.size.height != description_in.format.height )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "libcamera cannot provide the requested exact format." ) );
        const int configure_result = handle->camera->configure( configuration.get() );
        if( configure_result < 0 ) return CameraStatus::failure( libcameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::ConfigurationFailed,
            "Failed to configure the libcamera stream.", configure_result ) );
        auto allocator = std::make_unique< libcamera::FrameBufferAllocator >( handle->camera );
        libcamera::Stream* const stream = stream_configuration.stream();
        const int allocate_result = allocator->allocate( stream );
        if( allocate_result < 0 || allocator->buffers( stream ).empty() )
            return CameraStatus::failure( libcameraError(
                eCameraErrorCategory::Backend, eCameraErrorCode::ResourceExhausted,
                "Failed to allocate libcamera frame buffers.", allocate_result ) );
        this->m_frame_description.width = stream_configuration.size.width;
        this->m_frame_description.height = stream_configuration.size.height;
        this->m_frame_description.pixel_format = description_in.format.pixel_format;
        this->m_frame_description.row_stride = description_in.format.pixel_format
            == eCameraPixelFormat::Mjpeg ? 0U : stream_configuration.stride;
        this->m_frame_duration_us = requested_duration;
        this->m_capabilities = queryControlCapabilities( *handle->camera );
        this->m_control_values.clear();
        for( const sCameraControlCapability& capability : this->m_capabilities )
            this->m_control_values[ capability.control ] = {
                capability.control,
                capability.supports_automatic ? eCameraControlMode::Automatic
                                              : eCameraControlMode::Manual,
                capability.default_value };
        // 全ての失敗し得る段階を越えてからMemberへ移す。Signal接続は最後に行い、Callbackが
        // 届き始めた時点でこのObjectが完成していることを保証する.
        this->m_handle = std::move( handle );
        this->m_configuration = std::move( configuration );
        this->m_allocator = std::move( allocator );
        this->m_stream = stream;
        this->m_disconnected = false;
        LibcameraRollback rollback([this]() noexcept { this->close(); });
        this->m_handle->camera->requestCompleted.connect(
            this, &LibcameraCameraBackend::onRequestComplete );
        this->m_handle->camera->disconnected.connect(
            this, &LibcameraCameraBackend::onDisconnected );
        rollback.commit();
        return CameraStatus::success();
    }

    void close() noexcept override
    {
        // Streamを止め、Signalを切ってから所有物を畳む。Signalを先に切るのは、解体中の
        // このObjectへCamera manager threadがCallbackを届けないようにするため.
        if( !this->m_handle ) return;
        (void)this->stopNative();
        {
            std::unique_lock<std::mutex> lock(this->m_mutex);
            // A disconnected pipeline in pinned libcamera can retain pending requests.
            // Wait for actual returns rather than freeing live buffers; no time bound.
            this->m_condition.wait(lock, [this]() { return this->m_requests.outstanding() == 0; });
        }
        this->m_handle->camera->requestCompleted.disconnect( this );
        this->m_handle->camera->disconnected.disconnect( this );
        (void)this->m_requests.clear();
        this->m_allocator.reset();
        this->m_configuration.reset();
        this->m_stream = nullptr;
        this->m_capabilities.clear();
        this->m_control_values.clear();
        this->m_handle.reset();
        this->m_frame_description = {};
        this->m_frame_duration_us = 0;
        this->m_disconnected = false;
        this->m_callback_failed = false;
        this->m_native_active = false;
    }

    CameraStatus start() override
    {
        if( !this->m_handle ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
            "libcamera device is not open." ) );
        if( this->m_streaming ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
            "libcamera stream is already active." ) );
        if( this->m_disconnected ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::DeviceDisconnected,
            "libcamera device has been disconnected." ) );
        {
            std::lock_guard<std::mutex> lock(this->m_mutex);
            if(this->m_native_active || this->m_requests.outstanding() != 0)
                return CameraStatus::failure(makeCameraError(eCameraErrorCategory::Lifecycle,
                    eCameraErrorCode::BackendFailure, "libcamera cleanup is still pending; stop before restarting."));
        }
        return startLibcameraTransaction(
            [this]() { return this->createRequests(); },
            [this]()
            {
                libcamera::ControlList initial_controls = this->requestControls();
                this->m_native_active = true;
                const int result = this->m_handle->camera->start(&initial_controls);
                if(result < 0) return CameraStatus::failure(libcameraError(
                    eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
                    "Failed to start the libcamera stream.", result));
                this->m_streaming = true;
                this->m_callback_failed = false;
                return CameraStatus::success();
            },
            [this]()
            {
                for(const auto& request : this->m_requests.owners())
                {
                    request->controls() = this->requestControls();
                    const int result = this->queueRequest(request.get());
                    if(result < 0) return CameraStatus::failure(libcameraError(
                        eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
                        "Failed to queue a libcamera request.", result));
                }
                return CameraStatus::success();
            },
            [this]() noexcept { (void)this->stopNative(); });
    }

    CameraStatus stop() override
    {
        // Camera停止後に完了Queueを空にし、待っているreadFrameを全て起こす。捨てられた
        // RequestのFrameはもう届かないので、待たせ続けるとTimeoutまで固まってしまう.
        const int result = this->stopNative();
        if( result < 0 ) return CameraStatus::failure( libcameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
            "Failed to stop the libcamera stream.", result ) );
        return CameraStatus::success();
    }

    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        // Frame到着・切断・停止のどれでも起きるまで待つ。どの理由で起きたかを順に見分け、
        // Timeout／切断／停止をそれぞれのErrorで返す.
        std::unique_lock< std::mutex > lock( this->m_mutex );
        if( !this->m_streaming ) return CameraResult< sCameraFrame >::failure( makeCameraError(
            eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotStreaming,
            "libcamera stream is not active." ) );
        const bool ready = this->m_condition.wait_for( lock,
            std::chrono::milliseconds( timeout_ms_in ), [ this ]()
            { return !this->m_requests.empty() || this->m_callback_failed
                || this->m_disconnected || !this->m_streaming; } );
        if( !ready ) return CameraResult< sCameraFrame >::failure( makeCameraError(
            eCameraErrorCategory::Timeout, eCameraErrorCode::TimedOut,
            "Timed out waiting for a libcamera frame." ) );
        if( this->m_disconnected ) return CameraResult< sCameraFrame >::failure( makeCameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::DeviceDisconnected,
            "libcamera device was disconnected." ) );
        if(this->m_callback_failed)
        {
            lock.unlock();
            (void)this->stopNative();
            return CameraResult<sCameraFrame>::failure(makeCameraError(
                eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
                "libcamera completion bookkeeping failed; stream stopped."));
        }
        if( this->m_requests.empty() ) return CameraResult< sCameraFrame >::failure( makeCameraError(
            eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotStreaming,
            "libcamera stream stopped while waiting for a frame." ) );
        // Copyと再Queueの間はLockを持たない。copyFrameのmmapとmemcpyは長く、Callback側を
        // 待たせるとFrameの取りこぼしに直結する.
        libcamera::Request* const request = this->m_requests.pop();
        lock.unlock();
        return readLibcameraTransaction(
            [this, request]()
            {
                {
                    std::lock_guard<std::mutex> metadata_lock(this->m_mutex);
                    this->updateControlMetadata(request->metadata());
                }
                return this->copyFrame(request);
            },
            [this, request]()
            {
                request->reuse(libcamera::Request::ReuseBuffers);
                request->controls() = this->requestControls();
                const int result = this->queueRequest(request);
                if(result < 0) return CameraStatus::failure(libcameraError(
                    eCameraErrorCategory::Backend, eCameraErrorCode::ReadFailed,
                    "Failed to return a libcamera request to the stream.", result));
                return CameraStatus::success();
            },
            [this]() noexcept { (void)this->stopNative(); });
    }

    CameraResult< sCameraControlValue > getControl( const eCameraControl control_in ) override
    {
        std::lock_guard< std::mutex > lock( this->m_mutex );
        const auto value = this->m_control_values.find( control_in );
        if( value == this->m_control_values.end() )
            return CameraResult< sCameraControlValue >::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "libcamera device does not support the requested control." ) );
        if( this->m_disconnected ) return CameraResult< sCameraControlValue >::failure( makeCameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::DeviceDisconnected,
            "libcamera device was disconnected." ) );
        return CameraResult< sCameraControlValue >::success( value->second );
    }

    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        // ここでは検証して手元の写しを書き換えるだけで、Deviceへは触れない。値は次に
        // Queueされる各RequestのControl listに載って初めて効く（libcameraの設計上、
        // Streaming外で即時に書く経路が無い）.
        std::lock_guard< std::mutex > lock( this->m_mutex );
        const sCameraControlCapability* const capability =
            findCapability( this->m_capabilities, value_in.control );
        if( capability == nullptr ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
            "libcamera device does not support the requested control." ) );
        if( this->m_disconnected ) return CameraStatus::failure( makeCameraError(
            eCameraErrorCategory::Device, eCameraErrorCode::DeviceDisconnected,
            "libcamera device was disconnected." ) );
        if( value_in.mode == eCameraControlMode::Automatic )
        {
            if( !capability->supports_automatic ) return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "libcamera control does not support automatic mode." ) );
        }
        else if( !capability->supports_manual
            || value_in.value < capability->minimum || value_in.value > capability->maximum
            || ( value_in.value - capability->minimum ) % capability->step != 0 )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
                "Manual libcamera control is outside its capability range or step." ) );
        this->m_control_values[ value_in.control ] = value_in;
        return CameraStatus::success();
    }

    bool isOpen() const noexcept override { return this->m_handle != nullptr; }
    bool isStreaming() const noexcept override { return this->m_streaming; }
};

} // namespace

bool libcameraCameraBackendAvailable() noexcept { return true; }

std::unique_ptr< ICameraBackend > createLibcameraCameraBackend()
{
    return std::make_unique< LibcameraCameraBackend >();
}

CameraResult< std::vector< sCameraDeviceInfo > > enumerateLibcameraCameraDevices()
{
    LibcameraManagerLease< libcamera::CameraManager > manager;
    const int result = manager.acquire();
    if( result < 0 )
        return CameraResult< std::vector< sCameraDeviceInfo > >::failure( libcameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
            "Failed to start libcamera device enumeration.", result ) );
    std::vector< sCameraDeviceInfo > devices;
    for( const std::shared_ptr< libcamera::Camera >& camera : manager->cameras() )
        devices.push_back( makeDeviceInfo( camera ) );
    return CameraResult< std::vector< sCameraDeviceInfo > >::success( std::move( devices ) );
}

CameraResult< sCameraCapability > queryLibcameraCameraCapabilities(
    const sCameraDeviceInfo& device_in )
{
    return libcameraCapabilities( device_in );
}

} // namespace detail
} // namespace tmr
} // namespace wse

#else

// libcamera無しのBuildで同じ関数群を提供するStub。存在しないBackendを選んだ利用者には
// 構造化されたUnsupportedBackendが返り、Linkはどちらの構成でも成立する.

namespace wse
{
namespace tmr
{
namespace detail
{
namespace
{
CameraError unavailableLibcameraError()
{
    return makeCameraError( eCameraErrorCategory::Unsupported,
        eCameraErrorCode::UnsupportedBackend,
        "libcamera support is not present in this WSE build." );
}
} // namespace

bool libcameraCameraBackendAvailable() noexcept { return false; }
std::unique_ptr< ICameraBackend > createLibcameraCameraBackend() { return nullptr; }
CameraResult< std::vector< sCameraDeviceInfo > > enumerateLibcameraCameraDevices()
{
    return CameraResult< std::vector< sCameraDeviceInfo > >::failure( unavailableLibcameraError() );
}
CameraResult< sCameraCapability > queryLibcameraCameraCapabilities( const sCameraDeviceInfo& )
{
    return CameraResult< sCameraCapability >::failure( unavailableLibcameraError() );
}

} // namespace detail
} // namespace tmr
} // namespace wse

#endif
