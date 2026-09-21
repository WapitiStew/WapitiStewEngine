//*****************************************************************************************************************
//! @file    MediaFoundationCameraBackend.cpp
//! @brief   \~japanese Portable Tmr Camera境界のWindows Media Foundation実装.
//! @brief   \~english  Windows Media Foundation implementation of the portable Tmr camera boundary.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include "../../../../core/tmr/camera/CameraBackend.h"
#include "../../../../core/tmr/camera/CameraOwnerThread.h"
#include "DirectShowRawCapture.h"
#include "MediaFoundationReadError.h"
#include "MediaFoundationFrame.h"
#include "MediaFoundationResources.h"
#include "MediaFoundationReadState.h"
#include "../../../../core/tmr/camera/CameraYuvConversion.h"

#include <windows.h>
#include <dshow.h>
#include <ks.h>
#include <ksproxy.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <string>
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

using Microsoft::WRL::ComPtr;
using MediaFoundationActivations = std::vector< ComPtr< IMFActivate > >;

std::string toUtf8( const wchar_t* const text_in, const std::uint32_t length_in )
{
    if( text_in == nullptr || length_in == 0U )
        return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, text_in, static_cast< int >( length_in ), nullptr, 0, nullptr, nullptr );
    if( required <= 0 )
        return {};
    std::string result( static_cast< std::size_t >( required ), '\0' );
    (void)WideCharToMultiByte(
        CP_UTF8, 0, text_in, static_cast< int >( length_in ), result.data(), required, nullptr, nullptr );
    return result;
}

CameraError backendError(
      const eCameraErrorCode code_in
    , const char* const      message_in
    , const HRESULT          result_in )
{
    return makeCameraError(
          eCameraErrorCategory::Backend
        , code_in
        , message_in
        , static_cast< std::int64_t >( result_in ) );
}

CameraResult< MediaFoundationActivations > enumerateActivations()
{
    ComPtr< IMFAttributes > attributes;
    IMFActivate** activations = nullptr;
    UINT32 activation_count = 0U;
    HRESULT result = MFCreateAttributes( attributes.GetAddressOf(), 1U );
    if( SUCCEEDED( result ) )
        result = attributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID );
    if( SUCCEEDED( result ) )
        result = MFEnumDeviceSources( attributes.Get(), &activations, &activation_count );
    MediaFoundationActivationArray< IMFActivate > activation_owner( activations, activation_count );
    if( FAILED( result ) )
        return CameraResult< MediaFoundationActivations >::failure( backendError(
            eCameraErrorCode::BackendFailure, "Media Foundation camera enumeration failed.", result ) );

    return CameraResult< MediaFoundationActivations >::success(
        adoptMediaFoundationActivations( activation_owner ) );
}

std::string activationString( IMFActivate* const activation_in, const GUID& key_in )
{
    wchar_t* text = nullptr;
    UINT32 length = 0U;
    const HRESULT result = activation_in->GetAllocatedString( key_in, &text, &length );
    const MediaFoundationTaskMemory< wchar_t > text_owner( text );
    if( FAILED( result ) )
        return {};
    return toUtf8( text_owner.get(), length );
}

// A Windows camera symbolic link embeds the USB identity as "vid_xxxx&pid_xxxx". Parsing it out
// of the link is the only way Media Foundation exposes vendor and product identifiers at all.
std::uint16_t parseUsbHexIdentity( const std::string& text_in, const char* const marker_in )
{
    std::string lower = text_in;
    std::transform( lower.begin(), lower.end(), lower.begin(),
        []( const unsigned char value ) { return static_cast< char >( std::tolower( value ) ); } );
    const std::size_t marker = lower.find( marker_in );
    if( marker == std::string::npos || marker + 4U + 4U > lower.size() )
        return 0U;
    const std::string digits = lower.substr( marker + 4U, 4U );
    char* end = nullptr;
    const unsigned long value = std::strtoul( digits.c_str(), &end, 16 );
    return end == digits.c_str() + digits.size() && value <= 0xFFFFUL
        ? static_cast< std::uint16_t >( value ) : 0U;
}

void populateWindowsTransport( sCameraDeviceInfo* const device_inout )
{
    device_inout->usb.vendor_id = parseUsbHexIdentity( device_inout->id, "vid_" );
    device_inout->usb.product_id = parseUsbHexIdentity( device_inout->id, "pid_" );
    if( device_inout->usb.vendor_id != 0U || device_inout->usb.product_id != 0U )
    {
        device_inout->transport_type = eCameraTransport::UsbUvc;
        device_inout->transport = "usb-uvc";
    }
    else
    {
        device_inout->transport_type = eCameraTransport::Unknown;
        device_inout->transport = "windows-camera";
    }
}

CameraResult< std::vector< sCameraDeviceInfo > > enumerateMediaFoundationCameras()
{
    MediaFoundationRuntime<> runtime;
    const HRESULT runtime_status = runtime.initialize();
    if( FAILED( runtime_status ) )
        return CameraResult< std::vector< sCameraDeviceInfo > >::failure( backendError(
            eCameraErrorCode::BackendFailure, "Media Foundation initialization failed.", runtime_status ) );
    CameraResult< MediaFoundationActivations > activation_result = enumerateActivations();
    if( !activation_result.succeeded() )
        return CameraResult< std::vector< sCameraDeviceInfo > >::failure( activation_result.error() );

    std::vector< sCameraDeviceInfo > devices;
    for( const ComPtr< IMFActivate >& activation : activation_result.value() )
    {
        sCameraDeviceInfo device;
        device.backend = eCameraBackend::MediaFoundation;
        device.id = activationString(
            activation.Get(), MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK );
        device.display_name = activationString(
            activation.Get(), MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME );
        populateWindowsTransport( &device );
        if( device.valid() )
            devices.push_back( std::move( device ) );
    }
    return CameraResult< std::vector< sCameraDeviceInfo > >::success( std::move( devices ) );
}

CameraResult< ComPtr< IMFActivate > > findActivation( const std::string& id_in )
{
    CameraResult< MediaFoundationActivations > activation_result = enumerateActivations();
    if( !activation_result.succeeded() )
        return CameraResult< ComPtr< IMFActivate > >::failure( activation_result.error() );
    ComPtr< IMFActivate > found;
    for( ComPtr< IMFActivate >& activation : activation_result.value() )
    {
        if( activationString(
                activation.Get(), MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK ) == id_in )
        {
            found = std::move( activation );
            break;
        }
    }
    if( found == nullptr )
        return CameraResult< ComPtr< IMFActivate > >::failure( makeCameraError(
              eCameraErrorCategory::Device
            , eCameraErrorCode::DeviceNotFound
            , "Media Foundation camera was not found." ) );
    return CameraResult< ComPtr< IMFActivate > >::success( std::move( found ) );
}

eCameraPixelFormat mediaFoundationPixelFormat( const GUID& subtype_in ) noexcept
{
    if( subtype_in == MFVideoFormat_RGB32 ) return eCameraPixelFormat::Bgra8;
    if( subtype_in == MFVideoFormat_RGB24 ) return eCameraPixelFormat::Bgr8;
    if( subtype_in == MFVideoFormat_YUY2 )  return eCameraPixelFormat::Yuyv422;
    if( subtype_in == MFVideoFormat_UYVY )  return eCameraPixelFormat::Uyvy422;
    if( subtype_in == MFVideoFormat_NV12 )  return eCameraPixelFormat::Nv12;
    if( subtype_in == MFVideoFormat_MJPG )  return eCameraPixelFormat::Mjpeg;
    return eCameraPixelFormat::Unknown;
}

bool readMediaTypeFormat( sCameraFormat* const format_out, IMFMediaType* const type_in )
{
    UINT32 width = 0U;
    UINT32 height = 0U;
    UINT32 numerator = 0U;
    UINT32 denominator = 0U;
    GUID subtype = GUID_NULL;
    if( FAILED( MFGetAttributeSize( type_in, MF_MT_FRAME_SIZE, &width, &height ) )
        || FAILED( MFGetAttributeRatio(
            type_in, MF_MT_FRAME_RATE, &numerator, &denominator ) )
        || FAILED( type_in->GetGUID( MF_MT_SUBTYPE, &subtype ) )
        || width == 0U || height == 0U || numerator == 0U || denominator == 0U )
        return false;
    format_out->width = width;
    format_out->height = height;
    format_out->frame_rate_numerator = numerator;
    format_out->frame_rate_denominator = denominator;
    format_out->pixel_format = mediaFoundationPixelFormat( subtype );
    return format_out->pixel_format != eCameraPixelFormat::Unknown;
}

bool sameFormat( const sCameraFormat& left_in, const sCameraFormat& right_in )
{
    return left_in.width == right_in.width && left_in.height == right_in.height
        && left_in.frame_rate_numerator == right_in.frame_rate_numerator
        && left_in.frame_rate_denominator == right_in.frame_rate_denominator
        && left_in.pixel_format == right_in.pixel_format;
}

enum class eWindowsControlInterface : std::uint8_t
{
      Camera
    , VideoProcessing
};

struct WindowsControlMapping
{
    eCameraControl           portable;
    eWindowsControlInterface interface_type;
    long                     property;
};

// Windows splits camera controls across two DirectShow-era interfaces: lens-and-body properties
// live on IAMCameraControl, image-processing properties on IAMVideoProcAmp. The table records
// which interface each portable control must be asked through; a control absent here is
// unsupported by this adapter.
constexpr WindowsControlMapping WINDOWS_CONTROL_MAPPINGS[] = {
    { eCameraControl::Exposure, eWindowsControlInterface::Camera, CameraControl_Exposure },
    { eCameraControl::Gain, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Gain },
    { eCameraControl::Focus, eWindowsControlInterface::Camera, CameraControl_Focus },
    { eCameraControl::Brightness, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Brightness },
    { eCameraControl::Contrast, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Contrast },
    { eCameraControl::Saturation, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Saturation },
    { eCameraControl::WhiteBalance, eWindowsControlInterface::VideoProcessing, VideoProcAmp_WhiteBalance },
    { eCameraControl::Zoom, eWindowsControlInterface::Camera, CameraControl_Zoom },
    { eCameraControl::Iris, eWindowsControlInterface::Camera, CameraControl_Iris },
    { eCameraControl::Hue, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Hue },
    { eCameraControl::Sharpness, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Sharpness },
    { eCameraControl::Gamma, eWindowsControlInterface::VideoProcessing, VideoProcAmp_Gamma },
    { eCameraControl::ColorEnable, eWindowsControlInterface::VideoProcessing, VideoProcAmp_ColorEnable },
    { eCameraControl::BacklightCompensation, eWindowsControlInterface::VideoProcessing, VideoProcAmp_BacklightCompensation },
    { eCameraControl::Pan, eWindowsControlInterface::Camera, CameraControl_Pan },
    { eCameraControl::Tilt, eWindowsControlInterface::Camera, CameraControl_Tilt },
    { eCameraControl::Roll, eWindowsControlInterface::Camera, CameraControl_Roll },
};

const char* windowsControlName( const eCameraControl control_in ) noexcept
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
        case eCameraControl::Zoom: return "Zoom";
        case eCameraControl::Iris: return "Iris";
        case eCameraControl::Hue: return "Hue";
        case eCameraControl::Sharpness: return "Sharpness";
        case eCameraControl::Gamma: return "Gamma";
        case eCameraControl::ColorEnable: return "Color enable";
        case eCameraControl::BacklightCompensation: return "Backlight compensation";
        case eCameraControl::Pan: return "Pan";
        case eCameraControl::Tilt: return "Tilt";
        case eCameraControl::Roll: return "Roll";
        case eCameraControl::PowerLineFrequency: return "Power line frequency";
    }
    return "Camera control";
}

const WindowsControlMapping* findWindowsControlMapping( const eCameraControl control_in )
{
    for( const WindowsControlMapping& mapping : WINDOWS_CONTROL_MAPPINGS )
    {
        if( mapping.portable == control_in )
            return &mapping;
    }
    return nullptr;
}

HRESULT queryWindowsControlRange(
      long* const                  minimum_out
    , long* const                  maximum_out
    , long* const                  step_out
    , long* const                  default_out
    , long* const                  flags_out
    , const WindowsControlMapping& mapping_in
    , IAMCameraControl* const      camera_control_in
    , IAMVideoProcAmp* const       video_processing_in )
{
    if( mapping_in.interface_type == eWindowsControlInterface::Camera )
    {
        if( camera_control_in == nullptr )
            return E_NOINTERFACE;
        return camera_control_in->GetRange(
            mapping_in.property, minimum_out, maximum_out, step_out, default_out, flags_out );
    }
    if( video_processing_in == nullptr )
        return E_NOINTERFACE;
    return video_processing_in->GetRange(
        mapping_in.property, minimum_out, maximum_out, step_out, default_out, flags_out );
}

bool supportsWindowsManual(
    const WindowsControlMapping& mapping_in, const long flags_in ) noexcept
{
    return mapping_in.interface_type == eWindowsControlInterface::Camera
        ? ( flags_in & CameraControl_Flags_Manual ) != 0
        : ( flags_in & VideoProcAmp_Flags_Manual ) != 0;
}

bool supportsWindowsAutomatic(
    const WindowsControlMapping& mapping_in, const long flags_in ) noexcept
{
    return mapping_in.interface_type == eWindowsControlInterface::Camera
        ? ( flags_in & CameraControl_Flags_Auto ) != 0
        : ( flags_in & VideoProcAmp_Flags_Auto ) != 0;
}

std::vector< sCameraControlCapability > queryWindowsControlCapabilities(
      IAMCameraControl* const camera_control_in
    , IAMVideoProcAmp* const  video_processing_in )
{
    std::vector< sCameraControlCapability > controls;
    for( const WindowsControlMapping& mapping : WINDOWS_CONTROL_MAPPINGS )
    {
        long minimum = 0;
        long maximum = 0;
        long step = 0;
        long default_value = 0;
        long flags = 0;
        if( FAILED( queryWindowsControlRange(
                &minimum, &maximum, &step, &default_value, &flags,
                mapping, camera_control_in, video_processing_in ) ) )
            continue;
        sCameraControlCapability capability;
        capability.control = mapping.portable;
        capability.minimum = minimum;
        capability.maximum = maximum;
        capability.step = step > 0 ? step : 1;
        capability.default_value = default_value;
        capability.supports_manual = supportsWindowsManual( mapping, flags );
        capability.supports_automatic = supportsWindowsAutomatic( mapping, flags );
        capability.readable = true;
        capability.writable = true;
        capability.display_name = windowsControlName( mapping.portable );
        if( mapping.portable == eCameraControl::WhiteBalance )
            capability.unit = eCameraControlUnit::Kelvin;
        else if( mapping.portable == eCameraControl::Pan
            || mapping.portable == eCameraControl::Tilt
            || mapping.portable == eCameraControl::Roll )
            capability.unit = eCameraControlUnit::Degrees;
        else if( mapping.portable == eCameraControl::ColorEnable
            || mapping.portable == eCameraControl::BacklightCompensation )
            capability.unit = eCameraControlUnit::Boolean;
        if( capability.valid() )
            controls.push_back( capability );
    }
    return controls;
}

CameraError unsupportedWindowsControlError(
    const char* const message_in, const HRESULT result_in = E_NOINTERFACE )
{
    return makeCameraError(
          eCameraErrorCategory::Unsupported
        , eCameraErrorCode::UnsupportedControl
        , message_in
        , static_cast< std::int64_t >( result_in ) );
}

CameraError windowsControlIoError(
      const eCameraErrorCode code_in
    , const char* const      message_in
    , const HRESULT          result_in )
{
    return makeCameraError(
          eCameraErrorCategory::InputOutput
        , code_in
        , message_in
        , static_cast< std::int64_t >( result_in ) );
}

CameraResult< sCameraCapability > mediaFoundationCapabilities( const sCameraDeviceInfo& device_in )
{
    MediaFoundationRuntime<> runtime;
    const HRESULT runtime_status = runtime.initialize();
    if( FAILED( runtime_status ) )
        return CameraResult< sCameraCapability >::failure( backendError(
            eCameraErrorCode::BackendFailure, "Media Foundation initialization failed.", runtime_status ) );
    CameraResult< ComPtr< IMFActivate > > activation_result = findActivation( device_in.id );
    if( !activation_result.succeeded() )
        return CameraResult< sCameraCapability >::failure( activation_result.error() );

    MediaFoundationSource< IMFMediaSource > source;
    ComPtr< IMFSourceReader > reader;
    ComPtr< IMFAttributes > reader_attributes;
    HRESULT result = activation_result.value()->ActivateObject(
        IID_PPV_ARGS( source.GetAddressOf() ) );
    if( FAILED( result ) )
        return CameraResult< sCameraCapability >::failure( backendError(
            eCameraErrorCode::OpenFailed, "Media Foundation camera activation failed.", result ) );
    result = createMediaFoundationReaderAttributes( reader_attributes.GetAddressOf() );
    if( SUCCEEDED( result ) )
        result = MFCreateSourceReaderFromMediaSource(
            source.Get(), reader_attributes.Get(), reader.GetAddressOf() );
    if( FAILED( result ) )
    {
        return CameraResult< sCameraCapability >::failure( backendError(
            eCameraErrorCode::OpenFailed, "Media Foundation source-reader creation failed.", result ) );
    }

    sCameraCapability capability;
    capability.device = device_in;
    ComPtr< IAMCameraControl > camera_control;
    ComPtr< IAMVideoProcAmp > video_processing;
    (void)source->QueryInterface( IID_PPV_ARGS( camera_control.GetAddressOf() ) );
    (void)source->QueryInterface( IID_PPV_ARGS( video_processing.GetAddressOf() ) );
    if( SUCCEEDED( result ) )
    {
        for( DWORD index = 0U; ; ++index )
        {
            ComPtr< IMFMediaType > native_type;
            result = reader->GetNativeMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, native_type.GetAddressOf() );
            if( result == MF_E_NO_MORE_TYPES )
            {
                result = S_OK;
                break;
            }
            if( FAILED( result ) )
                break;
            sCameraFormat format;
            if( readMediaTypeFormat( &format, native_type.Get() )
                && std::none_of( capability.stream_profiles.begin(), capability.stream_profiles.end(),
                    [&format]( const sCameraStreamProfile& existing_in )
                    {
                        return sameFormat( existing_in.native_format, format );
                    } ) )
            {
                sCameraStreamProfile profile;
                profile.native_format = format;
                profile.output_formats.push_back( eCameraPixelFormat::Bgra8 );
                // The native format is also an output, delivered without conversion. A sensor that
                // packs something other than picture data into its frames needs those bytes intact.
                if( format.pixel_format != eCameraPixelFormat::Bgra8 )
                    profile.output_formats.push_back( format.pixel_format );
                capability.stream_profiles.push_back( profile );

                const eCameraPixelFormat openable_formats[] = {
                    eCameraPixelFormat::Bgra8, format.pixel_format };
                for( const eCameraPixelFormat openable : openable_formats )
                {
                    sCameraFormat compatibility_format = format;
                    compatibility_format.pixel_format = openable;
                    if( std::none_of( capability.formats.begin(), capability.formats.end(),
                            [&compatibility_format]( const sCameraFormat& existing_in )
                            {
                                return sameFormat( existing_in, compatibility_format );
                            } ) )
                        capability.formats.push_back( compatibility_format );
                }
            }
        }
    }
    capability.controls = queryWindowsControlCapabilities(
        camera_control.Get(), video_processing.Get() );
    if( FAILED( result ) )
        return CameraResult< sCameraCapability >::failure( backendError(
            eCameraErrorCode::BackendFailure,
            "Media Foundation native-format enumeration failed.", result ) );
    if( capability.formats.empty() )
        return CameraResult< sCameraCapability >::failure( makeCameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedFormat
            , "Camera exposes no convertible video format." ) );
    return CameraResult< sCameraCapability >::success( std::move( capability ) );
}

// Receives completed samples on a Media Foundation worker thread and hands them to the reader
// thread through the mutex/condition pair. OnFlush also reports through the same path so a
// stop() can unblock a readFrame() that is waiting for a sample which will never come.
class SourceReaderCallback final : public IMFSourceReaderCallback, public MediaFoundationReadState
{
    //! @brief Construct all members with explicit defaults.
public:
    SourceReaderCallback()
        : m_references      ( 1U )
    {
    }
private:

  private:
    std::atomic< ULONG > m_references;

  public:

    // COM decides this parameter list, so the out parameter stays last and unprefixed. The public
    // API policy exempts a function bound to a foreign callback or interface for exactly this.
    STDMETHODIMP QueryInterface( REFIID identifier_in, void** object_out ) override
    {
        if( object_out == nullptr )
            return E_POINTER;
        if( identifier_in == IID_IUnknown || identifier_in == __uuidof( IMFSourceReaderCallback ) )
        {
            *object_out = static_cast< IMFSourceReaderCallback* >( this );
            this->AddRef();
            return S_OK;
        }
        *object_out = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_( ULONG ) AddRef() override
    {
        return ++this->m_references;
    }

    STDMETHODIMP_( ULONG ) Release() override
    {
        const ULONG remaining = --this->m_references;
        if( remaining == 0U )
            delete this;
        return remaining;
    }

    STDMETHODIMP OnReadSample(
          const HRESULT status_in
        , const DWORD
        , const DWORD flags_in
        , const LONGLONG timestamp_in
        , IMFSample* const sample_in ) override
    {
        this->receive(status_in, flags_in, timestamp_in, sample_in);
        return S_OK;
    }

    STDMETHODIMP OnEvent( DWORD, IMFMediaEvent* ) override
    {
        return S_OK;
    }

    STDMETHODIMP OnFlush( DWORD ) override
    {
        this->flushed();
        return S_OK;
    }
};

class MediaFoundationCameraBackend final : public ICameraBackend
{
    //! @brief Construct all members with explicit defaults.
public:
    MediaFoundationCameraBackend()
        : m_runtime          ()
        , m_open             ( false )
        , m_streaming        ( false )
        , m_source           ()
        , m_reader           ()
        , m_camera_control   ()
        , m_video_processing ()
        , m_ks_control       ()
        , m_callback         ()
        , m_format           ()
        , m_output_format    ( eCameraPixelFormat::Bgra8 )
        , m_row_stride       ( 0U )
        , m_sequence         ( 0U )
    {
    }
private:

  private:
    MediaFoundationRuntime<> m_runtime;
    bool m_open;
    bool m_streaming;
    bool m_reader_failed = false;
    MediaFoundationSource< IMFMediaSource > m_source;
    ComPtr< IMFSourceReader > m_reader;
    ComPtr< IAMCameraControl > m_camera_control;
    ComPtr< IAMVideoProcAmp > m_video_processing;
    ComPtr< IKsControl > m_ks_control;
    ComPtr< SourceReaderCallback > m_callback;
    std::unique_ptr< DirectShowRawCapture > m_native_capture;
    sCameraFormat m_format;
    //! \~japanese 実際に配信するFormatとRow stride. \~english Format and row stride actually delivered.
    eCameraPixelFormat m_output_format;
    std::size_t m_row_stride;
    std::uint64_t m_sequence;

    //! \~japanese 現在のMedia Typeから配信Row strideを決める.
    //! \~english  Decides the delivered row stride from the current media type.
    std::size_t deliveredRowStride( const eCameraPixelFormat format_in ) const
    {
        // A compressed frame is a blob, so it carries no stride at all.
        if( format_in == eCameraPixelFormat::Mjpeg )
            return 0U;

        sCameraFrameDescription description;
        description.width        = this->m_format.width;
        description.height       = this->m_format.height;
        description.pixel_format = format_in;
        const std::size_t minimum = description.minimumRowStride();

        ComPtr< IMFMediaType > current_type;
        if( this->m_reader == nullptr
            || FAILED( this->m_reader->GetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, current_type.GetAddressOf() ) ) )
            return minimum;
        UINT32 declared = 0U;
        if( FAILED( current_type->GetUINT32( MF_MT_DEFAULT_STRIDE, &declared ) ) )
            return minimum;
        // A bottom-up layout declares a negative stride; only its magnitude describes the pitch.
        const std::size_t magnitude = static_cast< std::size_t >(
            std::abs( static_cast< std::int32_t >( declared ) ) );
        return magnitude > minimum ? magnitude : minimum;
    }

    CameraStatus initializeRuntime()
    {
        const HRESULT result = this->m_runtime.initialize();
        if( FAILED( result ) )
            return CameraStatus::failure( backendError(
                eCameraErrorCode::OpenFailed, "Media Foundation initialization failed.", result ) );
        return CameraStatus::success();
    }

  public:
    ~MediaFoundationCameraBackend() override
    {
        this->close();
    }

    CameraStatus open( const sCameraOpenDescription& description_in ) override
    {
        if( this->m_open )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyOpen,
                "Media Foundation camera is already open." ) );
        // BGRA8 is produced by converting whatever the device sends. Any other requested output has
        // to be a format the device already produces, and is then delivered untouched.
        const eCameraPixelFormat output_format = description_in.format.pixel_format;
        const bool passthrough = output_format != eCameraPixelFormat::Bgra8;
        if( output_format == eCameraPixelFormat::Unknown )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "Media Foundation camera cannot open an unknown output format." ) );
        if( passthrough
            && description_in.native_pixel_format != eCameraPixelFormat::Unknown
            && description_in.native_pixel_format != output_format )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "Media Foundation delivers a non-BGRA8 output only as the native format itself." ) );
        if( !description_in.allow_format_conversion
            && !passthrough
            && description_in.native_pixel_format != eCameraPixelFormat::Unknown
            && description_in.native_pixel_format != output_format )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "Media Foundation format conversion was disabled for different native and output formats." ) );
        MediaFoundationOpenGuard< MediaFoundationCameraBackend > rollback( *this );
        CameraStatus runtime_result = this->initializeRuntime();
        if( !runtime_result.succeeded() )
            return runtime_result;

        // Both native and BGRA requests use the same acquisition path for odd-height
        // packed profiles. Resolve old output-only descriptions against this device only.
        eCameraPixelFormat direct_native = passthrough ? output_format : description_in.native_pixel_format;
        if( direct_native == eCameraPixelFormat::Unknown && output_format == eCameraPixelFormat::Bgra8
            && description_in.format.height % 2U != 0U )
        {
            const auto capability = mediaFoundationCapabilities( description_in.device );
            if( !capability.succeeded() ) { return CameraStatus::failure( capability.error() ); }
            for( const auto& profile : capability.value().stream_profiles )
            {
                const auto& native = profile.native_format;
                if( isPackedYuv422( native.pixel_format ) && native.width == description_in.format.width
                    && native.height == description_in.format.height
                    && native.frame_rate_numerator == description_in.format.frame_rate_numerator
                    && native.frame_rate_denominator == description_in.format.frame_rate_denominator )
                { direct_native = native.pixel_format; break; }
            }
        }
        if( description_in.format.height % 2U != 0U && isPackedYuv422( direct_native ) )
        {
            if( output_format != direct_native && !description_in.allow_format_conversion )
            { return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "Packed camera conversion is disabled." ) ); }
            auto native_description = description_in;
            native_description.format.pixel_format = direct_native;
            native_description.native_pixel_format = direct_native;
            native_description.allow_format_conversion = false;
            this->m_native_capture = std::make_unique< DirectShowRawCapture >();
            const auto opened = this->m_native_capture->open( native_description );
            if( !opened.succeeded() ) { return opened; }
            auto* source = this->m_native_capture->source();
            (void)source->QueryInterface( IID_PPV_ARGS( this->m_camera_control.GetAddressOf() ) );
            (void)source->QueryInterface( IID_PPV_ARGS( this->m_video_processing.GetAddressOf() ) );
            (void)source->QueryInterface( IID_PPV_ARGS( this->m_ks_control.GetAddressOf() ) );
            this->m_format = description_in.format;
            this->m_output_format = output_format;
            this->m_open = true;
            rollback.commit();
            return CameraStatus::success();
        }

        CameraResult< ComPtr< IMFActivate > > activation_result =
            findActivation( description_in.device.id );
        if( !activation_result.succeeded() )
        {
            return CameraStatus::failure( activation_result.error() );
        }
        HRESULT result = activation_result.value()->ActivateObject(
            IID_PPV_ARGS( this->m_source.GetAddressOf() ) );
        if( SUCCEEDED( result ) )
        {
            (void)this->m_source->QueryInterface(
                IID_PPV_ARGS( this->m_camera_control.ReleaseAndGetAddressOf() ) );
            (void)this->m_source->QueryInterface(
                IID_PPV_ARGS( this->m_video_processing.ReleaseAndGetAddressOf() ) );
            (void)this->m_source->QueryInterface(
                IID_PPV_ARGS( this->m_ks_control.ReleaseAndGetAddressOf() ) );
        }

        ComPtr< IMFAttributes > reader_attributes;
        if( SUCCEEDED( result ) )
            result = createMediaFoundationReaderAttributes( reader_attributes.GetAddressOf() );
        if( SUCCEEDED( result ) )
            this->m_callback.Attach( new SourceReaderCallback() );
        if( SUCCEEDED( result ) )
            result = reader_attributes->SetUnknown(
                MF_SOURCE_READER_ASYNC_CALLBACK, this->m_callback.Get() );
        // Video processing is what converts a native frame into BGRA8. A passthrough open must not
        // enable it, or Media Foundation is free to insert a converter that rewrites the samples.
        if( SUCCEEDED( result ) && !passthrough )
            result = reader_attributes->SetUINT32(
                MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE );
        if( SUCCEEDED( result ) )
            result = MFCreateSourceReaderFromMediaSource(
                this->m_source.Get(), reader_attributes.Get(),
                this->m_reader.ReleaseAndGetAddressOf() );

        // Pin the native media type by exact width, height, frame rate, and subtype. Letting the
        // reader pick a "close" native type would silently change the frame rate the caller asked
        // for, so no match means failure rather than substitution.
        const eCameraPixelFormat requested_native =
            passthrough ? output_format : description_in.native_pixel_format;
        if( SUCCEEDED( result ) && requested_native != eCameraPixelFormat::Unknown )
        {
            ComPtr< IMFMediaType > selected_native;
            for( DWORD index = 0U; ; ++index )
            {
                ComPtr< IMFMediaType > candidate;
                const HRESULT candidate_result = this->m_reader->GetNativeMediaType(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, candidate.GetAddressOf() );
                if( candidate_result == MF_E_NO_MORE_TYPES )
                    break;
                if( FAILED( candidate_result ) )
                {
                    result = candidate_result;
                    break;
                }
                sCameraFormat candidate_format;
                if( readMediaTypeFormat( &candidate_format, candidate.Get() )
                    && candidate_format.width == description_in.format.width
                    && candidate_format.height == description_in.format.height
                    && candidate_format.frame_rate_numerator
                        == description_in.format.frame_rate_numerator
                    && candidate_format.frame_rate_denominator
                        == description_in.format.frame_rate_denominator
                    && candidate_format.pixel_format == requested_native )
                {
                    selected_native = std::move( candidate );
                    break;
                }
            }
            if( SUCCEEDED( result ) && selected_native == nullptr )
            {
                return CameraStatus::failure( makeCameraError(
                      eCameraErrorCategory::Unsupported
                    , eCameraErrorCode::UnsupportedFormat
                    , "Media Foundation camera does not expose the requested native profile."
                    , static_cast< std::int64_t >( MF_E_INVALIDMEDIATYPE ) ) );
            }
            if( SUCCEEDED( result ) )
                result = this->m_reader->SetCurrentMediaType(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, selected_native.Get() );
        }

        // A converting open asks the reader for RGB32 output; the enabled video processing
        // supplies whatever conversion chain that needs.
        if( !passthrough )
        {
            ComPtr< IMFMediaType > output_type;
            if( SUCCEEDED( result ) )
                result = MFCreateMediaType( output_type.GetAddressOf() );
            if( SUCCEEDED( result ) )
                result = output_type->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
            if( SUCCEEDED( result ) )
                result = output_type->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
            if( SUCCEEDED( result ) )
                result = MFSetAttributeSize( output_type.Get(), MF_MT_FRAME_SIZE,
                    description_in.format.width, description_in.format.height );
            if( SUCCEEDED( result ) )
                result = MFSetAttributeRatio( output_type.Get(), MF_MT_FRAME_RATE,
                    description_in.format.frame_rate_numerator,
                    description_in.format.frame_rate_denominator );
            if( SUCCEEDED( result ) )
                result = this->m_reader->SetCurrentMediaType(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output_type.Get() );
        }
        if( FAILED( result ) )
        {
            const CameraError error = backendError(
                eCameraErrorCode::ConfigurationFailed,
                "Media Foundation camera format configuration failed.", result );
            return CameraStatus::failure( error );
        }
        this->m_format = description_in.format;
        this->m_output_format = output_format;
        this->m_row_stride = this->deliveredRowStride( output_format );
        this->m_sequence = 0U;
        this->m_open = true;
        rollback.commit();
        return CameraStatus::success();
    }

    void close() noexcept override
    {
        // A successful stop waits for OnFlush. Failure closes the publication gate and
        // forbids reuse; late callbacks only retain their own closed state.
        try { if(this->m_streaming) (void)this->stop(); } catch(...) {}
        if(this->m_callback) this->m_callback->close();
        this->m_streaming = false;
        this->m_open = false;
        this->m_reader.Reset();
        this->m_ks_control.Reset();
        this->m_video_processing.Reset();
        this->m_camera_control.Reset();
        this->m_source.Reset();
        this->m_callback.Reset();
        this->m_native_capture.reset();
        this->m_reader_failed = false;
        this->m_runtime.close();
        this->m_format = {};
    }

    CameraStatus start() override
    {
        if( !this->m_open )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
                "Media Foundation camera is not open." ) );
        if( this->m_reader_failed )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure,
                "Media Foundation reader failed; close and reopen the camera." ) );
        if( this->m_streaming )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
                "Media Foundation camera stream is already active." ) );
        // The source reader has no explicit start; frames begin flowing with the first
        // ReadSample. Starting therefore only clears stale callback state and flips the flag.
        if( this->m_native_capture )
        {
            const auto started = this->m_native_capture->start();
            if( !started.succeeded() ) return started;
        }
        else this->m_callback->reset();
        this->m_streaming = true;
        return CameraStatus::success();
    }

    CameraStatus stop() override
    {
        if( this->m_native_capture )
        {
            this->m_streaming = false;
            return this->m_native_capture->stop();
        }
        if( !this->m_streaming )
            return CameraStatus::success();
        // Flush is asynchronous: reuse is permitted only after OnFlush completes.
        this->m_streaming = false;
        const auto flushed = flushMediaFoundationReader(*this->m_callback.Get(),
            [this]() { return this->m_reader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM); },
            std::chrono::milliseconds(2000));
        if( FAILED( flushed.status ) )
        {
            this->m_reader_failed = true;
            return CameraStatus::failure( backendError(
                eCameraErrorCode::BackendFailure, "Media Foundation camera flush failed.", flushed.status ) );
        }
        if(flushed.timed_out)
        {
            this->m_reader_failed = true;
            return CameraStatus::failure(makeCameraError(eCameraErrorCategory::Timeout,
                eCameraErrorCode::TimedOut, "Media Foundation flush completion timed out; close and reopen."));
        }
        return CameraStatus::success();
    }

    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        if( this->m_native_capture )
        {
            auto native = this->m_native_capture->readFrame( timeout_ms_in );
            if( !native.succeeded() || this->m_output_format != eCameraPixelFormat::Bgra8 ) return native;
            return packedYuvToBgra( native.value() );
        }
        if( !this->m_streaming )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotStreaming,
                "Media Foundation camera stream is not active." ) );
        // One request at a time: issue a ReadSample only when no earlier one is outstanding, then
        // wait for the callback to deliver. The loop exists because a callback can legitimately
        // complete with no sample (a stream tick); those are retried until the deadline.
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds( timeout_ms_in );
        while( true )
        {
            std::unique_lock< std::mutex > lock( this->m_callback->mutex );
            if( !this->m_callback->ready && !this->m_callback->request_in_flight )
            {
                this->m_callback->request_in_flight = true;
                lock.unlock();
                const HRESULT request_result = this->m_reader->ReadSample(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0U, nullptr, nullptr, nullptr, nullptr );
                lock.lock();
                if( FAILED( request_result ) )
                {
                    this->m_callback->request_in_flight = false;
                    this->m_reader_failed = true;
                    this->m_streaming = false;
                    return CameraResult< sCameraFrame >::failure( mediaFoundationReadError( request_result ) );
                }
            }
            if( !this->m_callback->condition.wait_until(
                    lock, deadline, [this]() { return this->m_callback->ready; } ) )
                return CameraResult< sCameraFrame >::failure( makeCameraError(
                    eCameraErrorCategory::Timeout, eCameraErrorCode::TimedOut,
                    "Timed out waiting for a Media Foundation camera frame." ) );

            ComPtr< IMFSample > sample = std::move( this->m_callback->sample );
            const HRESULT status = this->m_callback->status;
            const DWORD flags = this->m_callback->flags;
            const LONGLONG timestamp = this->m_callback->timestamp;
            this->m_callback->ready = false;
            lock.unlock();

            if( FAILED( status ) || ( flags & MF_SOURCE_READERF_ERROR ) != 0U
                || ( flags & MF_SOURCE_READERF_ENDOFSTREAM ) != 0U )
            {
                this->m_reader_failed = true;
                this->m_streaming = false;
                return CameraResult< sCameraFrame >::failure( mediaFoundationReadError( status ) );
            }
            if( sample == nullptr )
            {
                if( std::chrono::steady_clock::now() >= deadline )
                    return CameraResult< sCameraFrame >::failure( makeCameraError(
                        eCameraErrorCategory::Timeout, eCameraErrorCode::TimedOut,
                        "Camera produced no frame before the deadline." ) );
                continue;
            }

            ComPtr< IMFMediaBuffer > buffer;
            HRESULT result = sample->ConvertToContiguousBuffer( buffer.GetAddressOf() );
            if( FAILED( result ) )
                return CameraResult< sCameraFrame >::failure( backendError(
                    eCameraErrorCode::ReadFailed,
                    "Media Foundation frame mapping failed.", result ) );
            return copyMediaFoundationFrame( *buffer.Get(),
                { this->m_format.width, this->m_format.height,
                    this->m_output_format, this->m_row_stride },
                ++this->m_sequence, timestamp );
        }
    }

    CameraResult< sCameraControlValue > getControl( const eCameraControl control_in ) override
    {
        const WindowsControlMapping* const mapping = findWindowsControlMapping( control_in );
        if( mapping == nullptr )
            return CameraResult< sCameraControlValue >::failure( unsupportedWindowsControlError(
                "Camera control is not mapped by the Windows adapter." ) );
        long value = 0;
        long flags = 0;
        HRESULT result = E_NOINTERFACE;
        if( mapping->interface_type == eWindowsControlInterface::Camera
            && this->m_camera_control != nullptr )
            result = this->m_camera_control->Get( mapping->property, &value, &flags );
        else if( mapping->interface_type == eWindowsControlInterface::VideoProcessing
            && this->m_video_processing != nullptr )
            result = this->m_video_processing->Get( mapping->property, &value, &flags );
        if( result == E_NOINTERFACE )
            return CameraResult< sCameraControlValue >::failure( unsupportedWindowsControlError(
                "Camera does not expose the requested Windows control interface.", result ) );
        if( FAILED( result ) )
            return CameraResult< sCameraControlValue >::failure( windowsControlIoError(
                eCameraErrorCode::ControlReadFailed,
                "Failed to read the Windows camera control.", result ) );

        sCameraControlValue portable_value;
        portable_value.control = control_in;
        portable_value.value = value;
        portable_value.mode = supportsWindowsAutomatic( *mapping, flags )
            ? eCameraControlMode::Automatic : eCameraControlMode::Manual;
        return CameraResult< sCameraControlValue >::success( portable_value );
    }

    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        const WindowsControlMapping* const mapping = findWindowsControlMapping( value_in.control );
        if( mapping == nullptr )
            return CameraStatus::failure( unsupportedWindowsControlError(
                "Camera control is not mapped by the Windows adapter." ) );

        long minimum = 0;
        long maximum = 0;
        long step = 0;
        long default_value = 0;
        long supported_flags = 0;
        const HRESULT range_result = queryWindowsControlRange(
            &minimum, &maximum, &step, &default_value, &supported_flags,
            *mapping, this->m_camera_control.Get(), this->m_video_processing.Get() );
        if( FAILED( range_result ) )
            return CameraStatus::failure( unsupportedWindowsControlError(
                "Camera does not support the requested Windows control.", range_result ) );

        const bool automatic = value_in.mode == eCameraControlMode::Automatic;
        if( automatic && !supportsWindowsAutomatic( *mapping, supported_flags ) )
            return CameraStatus::failure( unsupportedWindowsControlError(
                "Camera control does not support automatic mode." ) );
        if( !automatic && !supportsWindowsManual( *mapping, supported_flags ) )
            return CameraStatus::failure( unsupportedWindowsControlError(
                "Camera control does not support manual mode." ) );
        if( !automatic && ( value_in.value < minimum || value_in.value > maximum
            || ( step > 0 && ( value_in.value - minimum ) % step != 0 ) ) )
            return CameraStatus::failure( makeCameraError(
                  eCameraErrorCategory::Validation
                , eCameraErrorCode::InvalidArgument
                , "Manual camera-control value is outside its capability range or step." ) );

        // The Windows interfaces require some value even when switching to automatic; the
        // documented convention is to pass the default alongside the auto flag.
        const long native_value = automatic ? default_value : static_cast< long >( value_in.value );
        const long native_flags = mapping->interface_type == eWindowsControlInterface::Camera
            ? ( automatic ? CameraControl_Flags_Auto : CameraControl_Flags_Manual )
            : ( automatic ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual );
        HRESULT result = E_NOINTERFACE;
        if( mapping->interface_type == eWindowsControlInterface::Camera
            && this->m_camera_control != nullptr )
            result = this->m_camera_control->Set( mapping->property, native_value, native_flags );
        else if( mapping->interface_type == eWindowsControlInterface::VideoProcessing
            && this->m_video_processing != nullptr )
            result = this->m_video_processing->Set( mapping->property, native_value, native_flags );
        if( FAILED( result ) )
            return CameraStatus::failure( windowsControlIoError(
                eCameraErrorCode::ControlWriteFailed,
                "Failed to write the Windows camera control.", result ) );
        return CameraStatus::success();
    }

    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) override
    {
        if( this->m_ks_control == nullptr )
            return CameraResult< sCameraExtensionUnitValue >::failure( makeCameraError(
                  eCameraErrorCategory::Unsupported
                , eCameraErrorCode::UnsupportedExtensionUnit
                , "Media Foundation source does not expose the Windows KS control boundary." ) );
        // An extension unit is addressed as a topology node: the unit GUID names the property
        // set, the selector is the property id, and the node id picks the unit instance.
        KSP_NODE property = {};
        std::copy( selector_in.unit_guid.begin(), selector_in.unit_guid.end(),
            reinterpret_cast< std::uint8_t* >( &property.Property.Set ) );
        property.Property.Id = selector_in.selector;
        property.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
        property.NodeId = selector_in.unit_id;
        sCameraExtensionUnitValue value;
        value.selector = selector_in;
        value.payload.resize( selector_in.maximum_size );
        ULONG returned = 0U;
        const HRESULT result = this->m_ks_control->KsProperty(
            &property.Property, sizeof( property ), value.payload.data(),
            static_cast< ULONG >( value.payload.size() ), &returned );
        if( FAILED( result ) )
            return CameraResult< sCameraExtensionUnitValue >::failure( windowsControlIoError(
                eCameraErrorCode::ExtensionUnitReadFailed,
                "Failed to read the Windows UVC extension unit.", result ) );
        if( returned < selector_in.minimum_size || returned > selector_in.maximum_size )
            return CameraResult< sCameraExtensionUnitValue >::failure( makeCameraError(
                  eCameraErrorCategory::InputOutput
                , eCameraErrorCode::PayloadSizeMismatch
                , "Windows UVC extension unit returned an unexpected payload length." ) );
        value.payload.resize( returned );
        return CameraResult< sCameraExtensionUnitValue >::success( std::move( value ) );
    }

    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) override
    {
        if( this->m_ks_control == nullptr )
            return CameraStatus::failure( makeCameraError(
                  eCameraErrorCategory::Unsupported
                , eCameraErrorCode::UnsupportedExtensionUnit
                , "Media Foundation source does not expose the Windows KS control boundary." ) );
        KSP_NODE property = {};
        std::copy( value_in.selector.unit_guid.begin(), value_in.selector.unit_guid.end(),
            reinterpret_cast< std::uint8_t* >( &property.Property.Set ) );
        property.Property.Id = value_in.selector.selector;
        property.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
        property.NodeId = value_in.selector.unit_id;
        ULONG returned = 0U;
        const HRESULT result = this->m_ks_control->KsProperty(
            &property.Property, sizeof( property ),
            const_cast< std::uint8_t* >( value_in.payload.data() ),
            static_cast< ULONG >( value_in.payload.size() ), &returned );
        if( FAILED( result ) )
            return CameraStatus::failure( windowsControlIoError(
                eCameraErrorCode::ExtensionUnitWriteFailed,
                "Failed to write the Windows UVC extension unit.", result ) );
        return CameraStatus::success();
    }

    bool isOpen() const noexcept override
    {
        return this->m_open;
    }

    bool isStreaming() const noexcept override
    {
        return this->m_streaming;
    }
};

} // namespace

std::unique_ptr< ICameraBackend > createPlatformCameraBackend( const eCameraBackend backend_in )
{
    if( backend_in == eCameraBackend::MediaFoundation )
        return std::make_unique< ThreadOwnedCameraBackend >(
            +[]() -> std::unique_ptr<ICameraBackend> { return std::make_unique<MediaFoundationCameraBackend>(); });
    return nullptr;
}

CameraResult< std::vector< sCameraDeviceInfo > > enumeratePlatformCameras(
    const eCameraBackend backend_in )
{
    if( backend_in == eCameraBackend::Automatic
        || backend_in == eCameraBackend::MediaFoundation )
        return enumerateMediaFoundationCameras();
    return CameraResult< std::vector< sCameraDeviceInfo > >::failure( makeCameraError(
          eCameraErrorCategory::Unsupported
        , eCameraErrorCode::UnsupportedBackend
        , "Requested camera backend is unavailable on Windows." ) );
}

CameraResult< sCameraCapability > queryPlatformCameraCapabilities(
    const sCameraDeviceInfo& device_in )
{
    if( device_in.backend != eCameraBackend::MediaFoundation )
        return CameraResult< sCameraCapability >::failure( makeCameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedBackend
            , "Requested camera backend is unavailable on Windows." ) );
    return mediaFoundationCapabilities( device_in );
}

} // namespace detail
} // namespace tmr
} // namespace wse
