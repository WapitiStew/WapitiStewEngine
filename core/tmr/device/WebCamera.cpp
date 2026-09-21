// @file WebCamera.cpp
// @brief \~japanese CameraSessionを土台にした公開WebCamera facadeの実装.
//        \~english  CameraSession-backed implementation of the public WebCamera facade.
//
// The facade validates every request against the capability the device advertised before any of
// it reaches the session: a control write checks range, step, mode, and access; an extension-unit
// access checks that the selector is advertised with the right direction. The session and the
// enumeration behind the facade sit behind detail::IWebCameraRuntime, so the software contract
// test injects a fake runtime while production code uses the one platform runtime below. The
// deprecated legacy surface is implemented on top of the modern one and is the only caller of
// the remembered last-error and legacy-event state.

#include <tmr/device/WebCamera.h>

#include "WebCameraInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#if defined( _MSC_VER )
#pragma warning(disable: 4996) // Compatibility methods intentionally call other deprecated methods.
#elif defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace wse
{
namespace tmr
{
namespace
{

CameraError webCameraError(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraError( category_in, code_in, message_in );
}

CameraStatus webCameraFailure(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraStatus::failure( webCameraError( category_in, code_in, message_in ) );
}

template <typename T>
CameraResult< T > webCameraResultFailure(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraResult< T >::failure( webCameraError( category_in, code_in, message_in ) );
}

// Unknown transports stay in: a backend that cannot identify the bus (some Media Foundation
// devices) would otherwise make real web cameras invisible. Devices positively identified as a
// non-web transport (CSI) are what this facade excludes.
bool isWebCameraCandidate( const sCameraDeviceInfo& device_in ) noexcept
{
    return device_in.transport_type == eCameraTransport::UsbUvc
        || device_in.transport_type == eCameraTransport::Unknown;
}

CameraResult< std::vector< sCameraDeviceInfo > > filterWebCameraDevices(
    CameraResult< std::vector< sCameraDeviceInfo > > devices_in )
{
    if( !devices_in.succeeded() )
        return devices_in;
    std::vector< sCameraDeviceInfo > devices;
    for( const sCameraDeviceInfo& device : devices_in.value() )
    {
        if( isWebCameraCandidate( device ) )
            devices.push_back( device );
    }
    return CameraResult< std::vector< sCameraDeviceInfo > >::success( std::move( devices ) );
}

bool sameFormat( const sCameraFormat& left_in, const sCameraFormat& right_in ) noexcept
{
    return left_in.width == right_in.width
        && left_in.height == right_in.height
        && left_in.frame_rate_numerator == right_in.frame_rate_numerator
        && left_in.frame_rate_denominator == right_in.frame_rate_denominator
        && left_in.pixel_format == right_in.pixel_format;
}

// Order of preference for the default output: the renderable BGRA8 first, then the raw color
// and luma layouts, and compressed MJPEG last, because a caller who did not choose a format is
// better served by pixels than by a stream it still has to decode.
std::optional< eCameraPixelFormat > preferredOutput(
    const sCameraStreamProfile& profile_in )
{
    constexpr eCameraPixelFormat PREFERRED_FORMATS[] = {
          eCameraPixelFormat::Bgra8
        , eCameraPixelFormat::Bgr8
        , eCameraPixelFormat::Rgb8
        , eCameraPixelFormat::Yuyv422
        , eCameraPixelFormat::Nv12
        , eCameraPixelFormat::Gray8
        , eCameraPixelFormat::Mjpeg
    };
    for( const eCameraPixelFormat format : PREFERRED_FORMATS )
    {
        if( profile_in.supportsOutput( format ) )
            return format;
    }
    return std::nullopt;
}

// The default configuration is the first valid advertised profile with a preferred output.
// Profile order comes from the backend, which lists the device's own ordering.
CameraResult< sCameraStreamConfiguration > defaultConfiguration(
    const sCameraCapability& capability_in )
{
    for( const sCameraStreamProfile& profile : capability_in.stream_profiles )
    {
        const std::optional< eCameraPixelFormat > output = preferredOutput( profile );
        if( profile.valid() && output )
        {
            sCameraStreamConfiguration configuration;
            configuration.native_format = profile.native_format;
            configuration.output_format = *output;
            configuration.allow_conversion = true;
            return CameraResult< sCameraStreamConfiguration >::success( configuration );
        }
    }
    return webCameraResultFailure< sCameraStreamConfiguration >(
        eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
        "Web camera does not advertise a usable stream profile." );
}

const sCameraStreamProfile* findProfile(
      const sCameraCapability&          capability_in
    , const sCameraStreamConfiguration& configuration_in ) noexcept
{
    const auto found = std::find_if(
        capability_in.stream_profiles.begin(), capability_in.stream_profiles.end(),
        [ &configuration_in ]( const sCameraStreamProfile& profile_in )
        {
            return sameFormat( profile_in.native_format, configuration_in.native_format )
                && profile_in.supportsOutput( configuration_in.output_format );
        } );
    return found == capability_in.stream_profiles.end() ? nullptr : &( *found );
}

const sCameraControlCapability* findControlCapability(
      const sCameraCapability& capability_in
    , const eCameraControl     control_in ) noexcept
{
    const auto found = std::find_if(
        capability_in.controls.begin(), capability_in.controls.end(),
        [ control_in ]( const sCameraControlCapability& capability )
        {
            return capability.control == control_in;
        } );
    return found == capability_in.controls.end() ? nullptr : &( *found );
}

bool sameExtensionSelector(
      const sCameraExtensionUnitSelector& left_in
    , const sCameraExtensionUnitSelector& right_in ) noexcept
{
    return left_in.unit_guid == right_in.unit_guid
        && left_in.unit_id == right_in.unit_id
        && left_in.selector == right_in.selector;
}

std::optional< eCameraControl > portableControl( const eParam param_in ) noexcept
{
    switch( param_in )
    {
        case eParam::Zoom: return eCameraControl::Zoom;
        case eParam::Exposure: return eCameraControl::Exposure;
        case eParam::Iris: return eCameraControl::Iris;
        case eParam::Focus: return eCameraControl::Focus;
        case eParam::Brightness: return eCameraControl::Brightness;
        case eParam::Contrast: return eCameraControl::Contrast;
        case eParam::Hue: return eCameraControl::Hue;
        case eParam::Saturation: return eCameraControl::Saturation;
        case eParam::Sharpness: return eCameraControl::Sharpness;
        case eParam::Gamma: return eCameraControl::Gamma;
        case eParam::ColorEnable: return eCameraControl::ColorEnable;
        case eParam::WhiteBalance: return eCameraControl::WhiteBalance;
        case eParam::BacklightCompensation: return eCameraControl::BacklightCompensation;
        case eParam::Gain: return eCameraControl::Gain;
    }
    return std::nullopt;
}

// Collapses the structured error onto the four events the legacy callback API can express.
eEvent legacyEvent( const CameraError& error_in ) noexcept
{
    if( error_in.ok() )
        return eEvent::TASK_SUCCESS;
    if( error_in.code() == eCameraErrorCode::TimedOut )
        return eEvent::TASK_TIMEOUT;
    if( error_in.category() == eCameraErrorCategory::Unsupported )
        return eEvent::TASK_DEVICE_UNSUPPORTED;
    return eEvent::TASK_FAILED;
}

// Production implementation of the session seam: a plain pass-through onto CameraSession.
// The software contract test replaces this with a scripted fake.
class CameraSessionAdapter final : public detail::IWebCameraSession
{
  private:
    CameraSession m_session;

  public:
    CameraStatus open(
          const sCameraDeviceInfo&          device_in
        , const sCameraStreamConfiguration& configuration_in ) override
    {
        return this->m_session.open( device_in, configuration_in );
    }
    void close() noexcept override { this->m_session.close(); }
    CameraStatus start() override { return this->m_session.start(); }
    CameraStatus start( const CameraFrameCallback& callback_in ) override
    {
        return this->m_session.start( callback_in );
    }
    CameraStatus stop() override { return this->m_session.stop(); }
    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        return this->m_session.readFrame( timeout_ms_in );
    }
    CameraResult< sCameraControlValue > getControl(
        const eCameraControl control_in ) override
    {
        return this->m_session.getControl( control_in );
    }
    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        return this->m_session.setControl( value_in );
    }
    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) override
    {
        return this->m_session.getExtensionUnit( selector_in );
    }
    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) override
    {
        return this->m_session.setExtensionUnit( value_in );
    }
    bool isOpen() const noexcept override { return this->m_session.isOpen(); }
    bool isStreaming() const noexcept override { return this->m_session.isStreaming(); }
};

class PlatformWebCameraRuntime final : public detail::IWebCameraRuntime
{
  public:
    CameraResult< std::vector< sCameraDeviceInfo > > enumerate(
        const eCameraBackend backend_in ) override
    {
        return CameraSession::enumerate( backend_in );
    }
    CameraResult< sCameraCapability > capabilities(
        const sCameraDeviceInfo& device_in ) override
    {
        return CameraSession::capabilities( device_in );
    }
    std::unique_ptr< detail::IWebCameraSession > createSession() override
    {
        return std::unique_ptr< detail::IWebCameraSession >( new CameraSessionAdapter() );
    }
};

// One shared runtime for every default-constructed WebCamera; enumeration and capability
// queries carry no per-facade state.
std::shared_ptr< detail::IWebCameraRuntime > platformRuntime()
{
    static const std::shared_ptr< detail::IWebCameraRuntime > runtime =
        std::make_shared< PlatformWebCameraRuntime >();
    return runtime;
}

} // namespace

class WebCamera::Impl final
{
  public:
    std::shared_ptr< detail::IWebCameraRuntime > runtime;
    std::unique_ptr< detail::IWebCameraSession > session;
    sCameraDeviceInfo selected_device;
    sCameraCapability capability;
    sCameraStreamConfiguration configuration;
    bool has_selection;
    std::int64_t legacy_index;
    std::uint32_t preferred_width;
    std::uint32_t preferred_height;
    std::uint32_t preferred_fps;
    eDOR direction;
    CameraError last_error;
    void ( *event_callback )( eEvent, void* ) = nullptr;
    void* event_object;

    explicit Impl( std::shared_ptr< detail::IWebCameraRuntime > runtime_in )
        : runtime          ( std::move( runtime_in ) )
        , session          ( this->runtime ? this->runtime->createSession() : nullptr )
        , selected_device  ()
        , capability       ()
        , configuration    ()
        , has_selection    ( false )
        , legacy_index     ( -1 )
        , preferred_width  ( 0U )
        , preferred_height ( 0U )
        , preferred_fps    ( 0U )
        , direction        ( eDOR::Raw )
        , last_error       ()
        , event_object     ( nullptr )
    {
    }

    void remember( const CameraError& error_in )
    {
        this->last_error = error_in;
    }

    void remember( const CameraStatus& status_in )
    {
        this->remember( status_in.succeeded() ? CameraError() : status_in.error() );
    }

    void notifyEvent( const CameraError& error_in ) const
    {
        if( this->event_callback != nullptr )
            this->event_callback( legacyEvent( error_in ), this->event_object );
    }

    CameraStatus select( const sCameraDeviceInfo& device_in )
    {
        if( !isWebCameraCandidate( device_in ) )
            return webCameraFailure( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument,
                "WebCamera accepts USB UVC or unknown web-camera transports only." );
        CameraResult< sCameraCapability > capability_result =
            this->runtime->capabilities( device_in );
        if( !capability_result.succeeded() )
            return CameraStatus::failure( capability_result.error() );
        CameraResult< sCameraStreamConfiguration > configuration_result =
            defaultConfiguration( capability_result.value() );
        if( !configuration_result.succeeded() )
            return CameraStatus::failure( configuration_result.error() );
        this->selected_device = device_in;
        this->capability = capability_result.value();
        this->configuration = configuration_result.value();
        this->has_selection = true;
        this->preferred_width = this->configuration.native_format.width;
        this->preferred_height = this->configuration.native_format.height;
        this->preferred_fps = this->configuration.native_format.frame_rate_denominator == 0U
            ? 0U : this->configuration.native_format.frame_rate_numerator
                / this->configuration.native_format.frame_rate_denominator;
        return CameraStatus::success();
    }

    // Resolves the legacy width/height/fps wishes into an advertised profile. Zero means
    // "any", so an application that only set a size still gets the device's own frame rate.
    CameraStatus resolveLegacyConfiguration()
    {
        if( !this->has_selection )
            return webCameraFailure( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "No web camera has been selected." );
        for( const sCameraStreamProfile& profile : this->capability.stream_profiles )
        {
            const sCameraFormat& format = profile.native_format;
            const bool size_matches = ( this->preferred_width == 0U
                    || format.width == this->preferred_width )
                && ( this->preferred_height == 0U
                    || format.height == this->preferred_height );
            const bool fps_matches = this->preferred_fps == 0U
                || static_cast< std::uint64_t >( format.frame_rate_numerator )
                    == static_cast< std::uint64_t >( this->preferred_fps )
                        * format.frame_rate_denominator;
            const std::optional< eCameraPixelFormat > output = preferredOutput( profile );
            if( size_matches && fps_matches && profile.valid() && output )
            {
                this->configuration.native_format = format;
                this->configuration.output_format = *output;
                this->configuration.allow_conversion = true;
                return CameraStatus::success();
            }
        }
        return webCameraFailure( eCameraErrorCategory::Unsupported,
            eCameraErrorCode::UnsupportedFormat,
            "Requested legacy stream size and frame rate are not advertised together." );
    }
};

namespace detail
{

std::shared_ptr< IWebCameraRuntime > platformCameraRuntime()
{
    return platformRuntime();
}

std::unique_ptr< WebCamera > WebCameraTestAccess::create(
    std::shared_ptr< IWebCameraRuntime > runtime_in )
{
    std::unique_ptr< WebCamera > camera( new WebCamera() );
    camera->m_impl = std::make_unique< WebCamera::Impl >( std::move( runtime_in ) );
    if( !camera->m_impl->runtime || !camera->m_impl->session )
        camera->m_impl->remember( webCameraError( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure,
            "WebCamera runtime did not create a camera session." ) );
    return camera;
}

} // namespace detail

WebCamera::WebCamera()
    : m_impl ( std::make_unique< Impl >( platformRuntime() ) )
{
    if( !this->m_impl->runtime || !this->m_impl->session )
        this->m_impl->remember( webCameraError( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure,
            "WebCamera runtime did not create a camera session." ) );
}

WebCamera::WebCamera( const std::int64_t index_in )
    : WebCamera ()
{
    // Selects the enumerated device at this index without opening it. A failure is remembered and
    // reported by lastError(), because a constructor cannot return a status.
    if( this->m_impl == nullptr || !this->m_impl->runtime || index_in < 0 )
        return;
    CameraResult< std::vector< sCameraDeviceInfo > > devices =
        filterWebCameraDevices( this->m_impl->runtime->enumerate( eCameraBackend::Automatic ) );
    if( !devices.succeeded() || static_cast< std::size_t >( index_in ) >= devices.value().size() )
    {
        const CameraError error = devices.succeeded()
            ? webCameraError( eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
                "The web camera index is out of range." )
            : devices.error();
        this->m_impl->remember( error );
        return;
    }
    const CameraStatus result =
        this->m_impl->select( devices.value()[ static_cast< std::size_t >( index_in ) ] );
    this->m_impl->remember( result );
}

WebCamera::~WebCamera()
{
    if( this->m_impl != nullptr && this->m_impl->session )
        this->m_impl->session->close();
}

WebCamera::WebCamera( WebCamera&& other_inout ) noexcept
    : m_impl ( std::move( other_inout.m_impl ) )
{}

WebCamera& WebCamera::operator=( WebCamera&& other_inout ) noexcept
{
    if( this != &other_inout )
    {
        if( this->m_impl != nullptr )
        {
            if( this->m_impl->session )
                this->m_impl->session->close();
        }
        this->m_impl = std::move( other_inout.m_impl );
    }
    return *this;
}

CameraResult< std::vector< sCameraDeviceInfo > > WebCamera::enumerate(
    const eCameraBackend backend_in )
{
    return filterWebCameraDevices( platformRuntime()->enumerate( backend_in ) );
}

CameraResult< sCameraCapability > WebCamera::capabilities(
    const sCameraDeviceInfo& device_in )
{
    if( !isWebCameraCandidate( device_in ) )
        return webCameraResultFailure< sCameraCapability >(
            eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
            "WebCamera accepts USB UVC or unknown web-camera transports only." );
    return platformRuntime()->capabilities( device_in );
}

CameraStatus WebCamera::open( const sCameraDeviceInfo& device_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    CameraStatus selection = this->m_impl->select( device_in );
    if( !selection.succeeded() )
    {
        this->m_impl->remember( selection );
        return selection;
    }
    return this->open( device_in, this->m_impl->configuration );
}

CameraStatus WebCamera::open(
      const sCameraDeviceInfo&          device_in
    , const sCameraStreamConfiguration& configuration_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    if( this->m_impl->session->isOpen() )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Lifecycle,
            eCameraErrorCode::AlreadyOpen, "WebCamera is already open." );
        this->m_impl->remember( result );
        return result;
    }
    if( !configuration_in.valid() )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Validation,
            eCameraErrorCode::InvalidArgument, "WebCamera stream configuration is invalid." );
        this->m_impl->remember( result );
        return result;
    }
    if( !isWebCameraCandidate( device_in ) )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Validation,
            eCameraErrorCode::InvalidArgument,
            "WebCamera accepts USB UVC or unknown web-camera transports only." );
        this->m_impl->remember( result );
        return result;
    }
    CameraResult< sCameraCapability > capability_result =
        this->m_impl->runtime->capabilities( device_in );
    if( !capability_result.succeeded() )
    {
        const CameraStatus result = CameraStatus::failure( capability_result.error() );
        this->m_impl->remember( result );
        return result;
    }
    if( findProfile( capability_result.value(), configuration_in ) == nullptr )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Unsupported,
            eCameraErrorCode::UnsupportedFormat,
            "WebCamera configuration is not advertised by this web camera." );
        this->m_impl->remember( result );
        return result;
    }
    CameraStatus result = this->m_impl->session->open( device_in, configuration_in );
    if( result.succeeded() )
    {
        this->m_impl->selected_device = device_in;
        this->m_impl->capability = capability_result.value();
        this->m_impl->configuration = configuration_in;
        this->m_impl->has_selection = true;
    }
    this->m_impl->remember( result );
    return result;
}

void WebCamera::close() noexcept
{
    if( this->m_impl != nullptr && this->m_impl->session )
    {
        this->m_impl->session->close();
        this->m_impl->selected_device = {};
        this->m_impl->capability = {};
        this->m_impl->configuration = {};
        this->m_impl->has_selection = false;
        this->m_impl->legacy_index = -1;
        this->m_impl->remember( CameraError() );
    }
}

CameraStatus WebCamera::start()
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    CameraStatus result = this->m_impl->session->start();
    this->m_impl->remember( result );
    return result;
}

CameraStatus WebCamera::start( const CameraFrameCallback& callback_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    CameraStatus result = this->m_impl->session->start( callback_in );
    this->m_impl->remember( result );
    return result;
}

CameraStatus WebCamera::stop()
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return CameraStatus::success();
    CameraStatus result = this->m_impl->session->stop();
    this->m_impl->remember( result );
    return result;
}

CameraResult< sCameraFrame > WebCamera::readFrame( const std::uint32_t timeout_ms_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraResultFailure< sCameraFrame >( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    CameraResult< sCameraFrame > result = this->m_impl->session->readFrame( timeout_ms_in );
    this->m_impl->remember( result.succeeded() ? CameraError() : result.error() );
    return result;
}

CameraResult< sCameraCapability > WebCamera::currentCapabilities() const
{
    if( this->m_impl == nullptr || !this->m_impl->has_selection )
        return webCameraResultFailure< sCameraCapability >( eCameraErrorCategory::Lifecycle,
            eCameraErrorCode::NotOpen, "No web camera has been selected." );
    return CameraResult< sCameraCapability >::success( this->m_impl->capability );
}

CameraResult< sCameraControlCapability > WebCamera::controlCapability(
    const eCameraControl control_in ) const
{
    const CameraResult< sCameraCapability > capability_result = this->currentCapabilities();
    if( !capability_result.succeeded() )
        return CameraResult< sCameraControlCapability >::failure( capability_result.error() );
    const sCameraControlCapability* const capability =
        findControlCapability( capability_result.value(), control_in );
    if( capability == nullptr )
        return webCameraResultFailure< sCameraControlCapability >(
            eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
            "Web camera does not advertise the requested control." );
    return CameraResult< sCameraControlCapability >::success( *capability );
}

CameraResult< sCameraControlValue > WebCamera::getControl( const eCameraControl control_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraResultFailure< sCameraControlValue >( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    const CameraResult< sCameraControlCapability > capability =
        this->controlCapability( control_in );
    if( !capability.succeeded() || !capability.value().readable )
    {
        CameraError error = capability.succeeded()
            ? webCameraError( eCameraErrorCategory::Validation,
                eCameraErrorCode::AccessDenied, "Web camera control is not readable." )
            : capability.error();
        this->m_impl->remember( error );
        return CameraResult< sCameraControlValue >::failure( error );
    }
    CameraResult< sCameraControlValue > result = this->m_impl->session->getControl( control_in );
    this->m_impl->remember( result.succeeded() ? CameraError() : result.error() );
    return result;
}

CameraStatus WebCamera::setControl( const sCameraControlValue& value_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    const CameraResult< sCameraControlCapability > capability_result =
        this->controlCapability( value_in.control );
    if( !capability_result.succeeded() )
    {
        const CameraStatus result = CameraStatus::failure( capability_result.error() );
        this->m_impl->remember( result );
        return result;
    }
    const sCameraControlCapability& capability = capability_result.value();
    bool valid = capability.writable;
    if( value_in.mode == eCameraControlMode::Manual )
    {
        valid = valid && capability.supports_manual
            && value_in.value >= capability.minimum && value_in.value <= capability.maximum
            && ( value_in.value - capability.minimum ) % capability.step == 0;
    }
    else
    {
        valid = valid && capability.supports_automatic;
    }
    if( !valid )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Validation,
            capability.writable ? eCameraErrorCode::InvalidArgument : eCameraErrorCode::AccessDenied,
            "Web camera control value, mode, or access does not match its capability." );
        this->m_impl->remember( result );
        return result;
    }
    CameraStatus result = this->m_impl->session->setControl( value_in );
    this->m_impl->remember( result );
    return result;
}

CameraResult< sCameraExtensionUnitValue > WebCamera::getExtensionUnit(
    const sCameraExtensionUnitSelector& selector_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraResultFailure< sCameraExtensionUnitValue >(
            eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
            "WebCamera has no runtime session." );
    const CameraResult< sCameraCapability > capability_result =
        this->currentCapabilities();
    if( !capability_result.succeeded() )
    {
        this->m_impl->remember( capability_result.error() );
        return CameraResult< sCameraExtensionUnitValue >::failure( capability_result.error() );
    }
    const auto found = std::find_if( capability_result.value().extension_units.begin(),
        capability_result.value().extension_units.end(),
        [ &selector_in ]( const sCameraExtensionUnitSelector& advertised_in )
        {
            return sameExtensionSelector( advertised_in, selector_in ) && advertised_in.readable;
        } );
    if( found == capability_result.value().extension_units.end() )
    {
        CameraError error = webCameraError( eCameraErrorCategory::Unsupported,
            eCameraErrorCode::UnsupportedExtensionUnit,
            "Web camera does not advertise the requested readable extension selector." );
        this->m_impl->remember( error );
        return CameraResult< sCameraExtensionUnitValue >::failure( error );
    }
    CameraResult< sCameraExtensionUnitValue > result =
        this->m_impl->session->getExtensionUnit( selector_in );
    this->m_impl->remember( result.succeeded() ? CameraError() : result.error() );
    return result;
}

CameraStatus WebCamera::setExtensionUnit( const sCameraExtensionUnitValue& value_in )
{
    if( this->m_impl == nullptr || !this->m_impl->session )
        return webCameraFailure( eCameraErrorCategory::Backend,
            eCameraErrorCode::BackendFailure, "WebCamera has no runtime session." );
    const CameraResult< sCameraCapability > capability_result =
        this->currentCapabilities();
    if( !capability_result.succeeded() )
    {
        const CameraStatus result = CameraStatus::failure( capability_result.error() );
        this->m_impl->remember( result );
        return result;
    }
    const auto found = std::find_if( capability_result.value().extension_units.begin(),
        capability_result.value().extension_units.end(),
        [ &value_in ]( const sCameraExtensionUnitSelector& advertised_in )
        {
            return sameExtensionSelector( advertised_in, value_in.selector)
                && advertised_in.writable
                && value_in.payload.size() >= advertised_in.minimum_size
                && value_in.payload.size() <= advertised_in.maximum_size;
        } );
    if( found == capability_result.value().extension_units.end() )
    {
        const CameraStatus result = webCameraFailure( eCameraErrorCategory::Unsupported,
            eCameraErrorCode::UnsupportedExtensionUnit,
            "Web camera does not advertise this writable extension selector and payload." );
        this->m_impl->remember( result );
        return result;
    }
    CameraStatus result = this->m_impl->session->setExtensionUnit( value_in );
    this->m_impl->remember( result );
    return result;
}

CameraResult< sCameraControlValue > WebCamera::readExposure()
{
    return this->getControl( eCameraControl::Exposure );
}

CameraStatus WebCamera::writeExposure(
    const std::int64_t value_in, const eCameraControlMode mode_in )
{
    return this->setControl( { eCameraControl::Exposure, mode_in, value_in } );
}

CameraResult< sCameraControlValue > WebCamera::readGain()
{
    return this->getControl( eCameraControl::Gain );
}

CameraStatus WebCamera::writeGain(
    const std::int64_t value_in, const eCameraControlMode mode_in )
{
    return this->setControl( { eCameraControl::Gain, mode_in, value_in } );
}

CameraResult< sCameraControlValue > WebCamera::readFocus()
{
    return this->getControl( eCameraControl::Focus );
}

CameraStatus WebCamera::writeFocus(
    const std::int64_t value_in, const eCameraControlMode mode_in )
{
    return this->setControl( { eCameraControl::Focus, mode_in, value_in } );
}

bool WebCamera::isOpen() const noexcept
{
    return this->m_impl != nullptr && this->m_impl->session
        && this->m_impl->session->isOpen();
}

bool WebCamera::isStreaming() const noexcept
{
    return this->m_impl != nullptr && this->m_impl->session
        && this->m_impl->session->isStreaming();
}

CameraError WebCamera::lastError() const
{
    return this->m_impl == nullptr
        ? webCameraError( eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
            "WebCamera was moved from." )
        : this->m_impl->last_error;
}

} // namespace tmr
} // namespace wse
