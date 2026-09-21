//*****************************************************************************************************************
//! @file    V4L2CameraBackend.cpp
//! @brief   \~japanese Portable Tmr Camera境界のLinux V4L2実装.
//! @brief   \~english  Linux V4L2 implementation of the portable Tmr camera boundary.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include "LinuxCameraBackends.h"

#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <limits>
#include <poll.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
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

// ------------------------------------------------------------------------------------------------
// ioctl包みとError分類。DQBUFの再試行だけはRead deadlineで制限する.
// ------------------------------------------------------------------------------------------------

// Signalで中断されたioctlはEINTRを返すだけで失敗ではないので、透過的にやり直す。
// これを一箇所に集めることで、呼び出し側はEINTRを一切考えずに済む.
int cameraIoctl( const int descriptor_in, const unsigned long request_in, void* const argument_in )
{
    int result = 0;
    do
    {
        result = ::ioctl( descriptor_in, request_in, argument_in );
    }
    while( result < 0 && errno == EINTR );
    return result;
}

// ENODEV／ENXIO／EIOはUSB Cameraが抜かれたときにDriverが返す値なので、呼び出し元の指定より
// 優先してDeviceDisconnectedへ分類する。利用者が「切断」を一つのCategoryで検出できるようにするため.
CameraError errnoError(
      const eCameraErrorCode code_in
    , const char* const      message_in
    , const int              error_in = errno )
{
    const bool disconnected = error_in == ENODEV || error_in == ENXIO || error_in == EIO;
    return makeCameraError(
          disconnected ? eCameraErrorCategory::Device : eCameraErrorCategory::InputOutput
        , disconnected ? eCameraErrorCode::DeviceDisconnected : code_in
        , message_in
        , error_in );
}

// UVC Extension unitのioctlは、権限不足と「そのUnitが存在しない」をerrnoでしか区別できない。
// EACCES／EPERMは検証Errorとして、EINVAL／ENOTTYは未対応として返し分ける.
CameraError extensionUnitError(
      const eCameraErrorCode code_in
    , const char* const      message_in
    , const int              error_in = errno )
{
    if( error_in == EACCES || error_in == EPERM )
        return makeCameraError( eCameraErrorCategory::Validation,
            eCameraErrorCode::AccessDenied, message_in, error_in );
    if( error_in == EINVAL || error_in == ENOTTY )
        return makeCameraError( eCameraErrorCategory::Unsupported,
            eCameraErrorCode::UnsupportedExtensionUnit, message_in, error_in );
    return errnoError( code_in, message_in, error_in );
}

// ------------------------------------------------------------------------------------------------
// Portable型とV4L2型の変換、Device列挙、能力問い合わせ.
// ------------------------------------------------------------------------------------------------

// FourCCとPortable Pixel formatの相互変換。ここに無いFormatは「未対応」であり、
// 近いFormatへ黙って読み替えることはしない.
eCameraPixelFormat fromV4L2Format( const std::uint32_t format_in )
{
    switch( format_in )
    {
        case V4L2_PIX_FMT_GREY: return eCameraPixelFormat::Gray8;
        case V4L2_PIX_FMT_RGB24: return eCameraPixelFormat::Rgb8;
        case V4L2_PIX_FMT_BGR24: return eCameraPixelFormat::Bgr8;
        case V4L2_PIX_FMT_YUYV: return eCameraPixelFormat::Yuyv422;
        case V4L2_PIX_FMT_UYVY: return eCameraPixelFormat::Uyvy422;
        case V4L2_PIX_FMT_NV12: return eCameraPixelFormat::Nv12;
        case V4L2_PIX_FMT_MJPEG: return eCameraPixelFormat::Mjpeg;
        default: return eCameraPixelFormat::Unknown;
    }
}

std::uint32_t toV4L2Format( const eCameraPixelFormat format_in )
{
    switch( format_in )
    {
        case eCameraPixelFormat::Gray8: return V4L2_PIX_FMT_GREY;
        case eCameraPixelFormat::Rgb8: return V4L2_PIX_FMT_RGB24;
        case eCameraPixelFormat::Bgr8: return V4L2_PIX_FMT_BGR24;
        case eCameraPixelFormat::Yuyv422: return V4L2_PIX_FMT_YUYV;
        case eCameraPixelFormat::Uyvy422: return V4L2_PIX_FMT_UYVY;
        case eCameraPixelFormat::Nv12: return V4L2_PIX_FMT_NV12;
        case eCameraPixelFormat::Mjpeg: return V4L2_PIX_FMT_MJPEG;
        default: return 0U;
    }
}

bool queryCaptureCapability( v4l2_capability* const capability_out, const int descriptor_in,
    int (*ioctl_in)( int, unsigned long, void* ) = cameraIoctl )
{
    std::memset( capability_out, 0, sizeof( *capability_out ) );
    if( ioctl_in( descriptor_in, VIDIOC_QUERYCAP, capability_out ) < 0 )
        return false;
    // 新しいDriverはdevice_capsに「このNodeの」能力を、capabilitiesにはCard全体の能力を入れる。
    // Metadata専用Nodeを撮像Deviceと誤認しないよう、device_capsがあればそちらで判定する.
    const std::uint32_t capabilities =
        ( capability_out->capabilities & V4L2_CAP_DEVICE_CAPS ) != 0U
            ? capability_out->device_caps : capability_out->capabilities;
    return ( capabilities & V4L2_CAP_VIDEO_CAPTURE ) != 0U;
}

std::uint16_t readHexIdentity( const std::filesystem::path& path_in )
{
    std::ifstream stream( path_in );
    std::string text;
    if( !( stream >> text ) )
        return 0U;
    try
    {
        const unsigned long value = std::stoul( text, nullptr, 16 );
        return value <= 0xFFFFUL ? static_cast< std::uint16_t >( value ) : 0U;
    }
    catch( ... )
    {
        return 0U;
    }
}

// /dev/videoNからsysfsを親方向へ辿り、最初に見つかったUSB Interface階層のVendor ID／Product ID／
// Serialを拾う。V4L2自体はUSB識別子を持たないので、これがUVC Cameraを個体識別する唯一の経路である。
// 深さ10で打ち切るのは、Symlinkの循環や想定外の階層でも必ず戻るための上限.
void populateLinuxUsbIdentity(
    sCameraDeviceInfo* const device_inout, const std::string& device_path_in )
{
    const std::size_t separator = device_path_in.find_last_of( '/' );
    const std::string device_name = separator == std::string::npos
        ? device_path_in : device_path_in.substr( separator + 1U );
    std::error_code error;
    std::filesystem::path current = std::filesystem::weakly_canonical(
        std::filesystem::path( "/sys/class/video4linux" ) / device_name / "device", error );
    if( error )
        return;
    for( std::uint32_t depth = 0U; depth < 10U && !current.empty(); ++depth )
    {
        const std::uint16_t vendor = readHexIdentity( current / "idVendor" );
        const std::uint16_t product = readHexIdentity( current / "idProduct" );
        if( vendor != 0U || product != 0U )
        {
            device_inout->usb.vendor_id = vendor;
            device_inout->usb.product_id = product;
            std::ifstream serial_stream( current / "serial" );
            std::getline( serial_stream, device_inout->usb.serial_number );
            device_inout->transport_type = eCameraTransport::UsbUvc;
            return;
        }
        const std::filesystem::path parent = current.parent_path();
        if( parent == current )
            break;
        current = parent;
    }
}

sCameraDeviceInfo makeDeviceInfo(
    const std::string& path_in, const v4l2_capability& capability_in )
{
    sCameraDeviceInfo device;
    device.backend = eCameraBackend::Video4Linux2;
    device.id = path_in;
    device.display_name = reinterpret_cast< const char* >( capability_in.card );
    device.transport = reinterpret_cast< const char* >( capability_in.bus_info );
    if( device.transport.empty() )
        device.transport = "v4l2";
    // Transport種別はDriver名から推定する。bcm2835／unicam／rp1-cfe／pispはRaspberry Pi系の
    // CSI Cameraを受け持つDriver名で、これらだけがCSIと名乗れる.
    const std::string driver = reinterpret_cast< const char* >( capability_in.driver );
    if( driver == "uvcvideo" || device.transport.find( "usb" ) != std::string::npos )
        device.transport_type = eCameraTransport::UsbUvc;
    else if( driver.find( "bcm2835" ) != std::string::npos
        || driver.find( "unicam" ) != std::string::npos
        || driver.find( "rp1-cfe" ) != std::string::npos
        || driver.find( "pisp" ) != std::string::npos )
        device.transport_type = eCameraTransport::Csi;
    populateLinuxUsbIdentity( &device, path_in );
    return device;
}

CameraResult< std::vector< sCameraDeviceInfo > > enumerateV4L2Cameras()
{
    // /dev/video0..63を順に開いて撮像可能なNodeだけを拾う。O_NONBLOCKは開いた瞬間にDeviceが
    // 応答しなくてもここで固まらないため、O_CLOEXECは子ProcessへDescriptorを漏らさないため.
    // 開けないNodeは「存在しない」だけなのでErrorにせず読み飛ばす.
    std::vector< sCameraDeviceInfo > devices;
    for( std::uint32_t index = 0U; index < 64U; ++index )
    {
        const std::string path = "/dev/video" + std::to_string( index );
        const int descriptor = ::open( path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC );
        if( descriptor < 0 )
            continue;
        v4l2_capability capability = {};
        if( queryCaptureCapability( &capability, descriptor ) )
            devices.push_back( makeDeviceInfo( path, capability ) );
        (void)::close( descriptor );
    }
    return CameraResult< std::vector< sCameraDeviceInfo > >::success( std::move( devices ) );
}

// 一つの解像度についてDriverが列挙する離散Frame間隔を全てFormat一覧へ足す。間隔を一つも
// 返さないDriverもいるので、そのときは30fpsを仮の一件として置き、解像度自体は落とさない.
void appendDiscreteIntervals(
      std::vector< sCameraFormat >* const formats_out
    , const int descriptor_in
    , const std::uint32_t pixel_format_in
    , const std::uint32_t width_in
    , const std::uint32_t height_in )
{
    bool added = false;
    for( std::uint32_t interval_index = 0U; ; ++interval_index )
    {
        v4l2_frmivalenum interval = {};
        interval.index = interval_index;
        interval.pixel_format = pixel_format_in;
        interval.width = width_in;
        interval.height = height_in;
        if( cameraIoctl( descriptor_in, VIDIOC_ENUM_FRAMEINTERVALS, &interval ) < 0 )
            break;
        if( interval.type != V4L2_FRMIVAL_TYPE_DISCRETE
            || interval.discrete.numerator == 0U || interval.discrete.denominator == 0U )
            continue;
        formats_out->push_back( {
              width_in
            , height_in
            , interval.discrete.denominator
            , interval.discrete.numerator
            , fromV4L2Format( pixel_format_in )
        } );
        added = true;
    }
    if( !added )
        formats_out->push_back( {
              width_in, height_in, 30U, 1U, fromV4L2Format( pixel_format_in )
        } );
}

struct ControlMapping
{
    eCameraControl portable;
    std::uint32_t value_id;
    std::uint32_t auto_id;
};

constexpr std::uint32_t NO_CONTROL = 0U;    //!< 対応する自動Control IDが存在しないことを表す.

// Portable ControlとV4L2 Control IDの対応表。auto_idを持つControlだけがAutomatic modeを
// 名乗れる。表に無いControlは未対応Errorになるので、対応を増やすときはここへ一行足す.
constexpr ControlMapping CONTROL_MAPPINGS[] = {
    { eCameraControl::Exposure, V4L2_CID_EXPOSURE_ABSOLUTE, V4L2_CID_EXPOSURE_AUTO },
    { eCameraControl::Gain, V4L2_CID_GAIN, NO_CONTROL },
    { eCameraControl::Focus, V4L2_CID_FOCUS_ABSOLUTE, V4L2_CID_FOCUS_AUTO },
    { eCameraControl::Brightness, V4L2_CID_BRIGHTNESS, NO_CONTROL },
    { eCameraControl::Contrast, V4L2_CID_CONTRAST, NO_CONTROL },
    { eCameraControl::Saturation, V4L2_CID_SATURATION, NO_CONTROL },
    { eCameraControl::WhiteBalance, V4L2_CID_WHITE_BALANCE_TEMPERATURE, V4L2_CID_AUTO_WHITE_BALANCE },
    { eCameraControl::Zoom, V4L2_CID_ZOOM_ABSOLUTE, NO_CONTROL },
    { eCameraControl::Iris, V4L2_CID_IRIS_ABSOLUTE, NO_CONTROL },
    { eCameraControl::Hue, V4L2_CID_HUE, V4L2_CID_HUE_AUTO },
    { eCameraControl::Sharpness, V4L2_CID_SHARPNESS, NO_CONTROL },
    { eCameraControl::Gamma, V4L2_CID_GAMMA, NO_CONTROL },
    { eCameraControl::BacklightCompensation, V4L2_CID_BACKLIGHT_COMPENSATION, NO_CONTROL },
    { eCameraControl::Pan, V4L2_CID_PAN_ABSOLUTE, NO_CONTROL },
    { eCameraControl::Tilt, V4L2_CID_TILT_ABSOLUTE, NO_CONTROL },
#ifdef V4L2_CID_ROTATE
    { eCameraControl::Roll, V4L2_CID_ROTATE, NO_CONTROL },
#endif
    { eCameraControl::PowerLineFrequency, V4L2_CID_POWER_LINE_FREQUENCY, NO_CONTROL },
};

const ControlMapping* findControlMapping( const eCameraControl control_in )
{
    for( const ControlMapping& mapping : CONTROL_MAPPINGS )
    {
        if( mapping.portable == control_in )
            return &mapping;
    }
    return nullptr;
}

std::vector< sCameraControlCapability > queryControlCapabilities( const int descriptor_in )
{
    std::vector< sCameraControlCapability > controls;
    for( const ControlMapping& mapping : CONTROL_MAPPINGS )
    {
        v4l2_queryctrl query = {};
        query.id = mapping.value_id;
        if( cameraIoctl( descriptor_in, VIDIOC_QUERYCTRL, &query ) < 0
            || ( query.flags & V4L2_CTRL_FLAG_DISABLED ) != 0U )
            continue;
        sCameraControlCapability control;
        control.control = mapping.portable;
        control.minimum = query.minimum;
        control.maximum = query.maximum;
        control.step = query.step > 0 ? query.step : 1;
        control.default_value = query.default_value;
        control.supports_manual = true;
        control.readable = ( query.flags & V4L2_CTRL_FLAG_WRITE_ONLY ) == 0U;
        control.writable = ( query.flags & V4L2_CTRL_FLAG_READ_ONLY ) == 0U;
        control.display_name = reinterpret_cast< const char* >( query.name );
        // V4L2のEXPOSURE_ABSOLUTEは100マイクロ秒単位なので、Portable単位（マイクロ秒）への
        // 換算係数として100を持たせる.
        if( mapping.portable == eCameraControl::Exposure )
        {
            control.unit = eCameraControlUnit::Microseconds;
            control.physical_scale = 100.0;
        }
        else if( mapping.portable == eCameraControl::WhiteBalance )
            control.unit = eCameraControlUnit::Kelvin;
        else if( mapping.portable == eCameraControl::Pan
            || mapping.portable == eCameraControl::Tilt
            || mapping.portable == eCameraControl::Roll )
            control.unit = eCameraControlUnit::Degrees;
        if( mapping.auto_id != NO_CONTROL )
        {
            v4l2_queryctrl auto_query = {};
            auto_query.id = mapping.auto_id;
            control.supports_automatic = cameraIoctl(
                descriptor_in, VIDIOC_QUERYCTRL, &auto_query ) == 0
                && ( auto_query.flags & V4L2_CTRL_FLAG_DISABLED ) == 0U;
        }
        controls.push_back( control );
    }
    return controls;
}

CameraResult< sCameraCapability > v4L2Capabilities( const sCameraDeviceInfo& device_in )
{
    const int descriptor = ::open( device_in.id.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC );
    if( descriptor < 0 )
        return CameraResult< sCameraCapability >::failure( errnoError(
            eCameraErrorCode::OpenFailed, "Failed to open V4L2 camera for capability query." ) );
    v4l2_capability native_capability = {};
    if( !queryCaptureCapability( &native_capability, descriptor ) )
    {
        const CameraError error = errnoError(
            eCameraErrorCode::BackendFailure, "V4L2 device is not a capture camera." );
        (void)::close( descriptor );
        return CameraResult< sCameraCapability >::failure( error );
    }

    sCameraCapability capability;
    capability.device = makeDeviceInfo( device_in.id, native_capability );
    for( std::uint32_t format_index = 0U; ; ++format_index )
    {
        v4l2_fmtdesc native_format = {};
        native_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        native_format.index = format_index;
        if( cameraIoctl( descriptor, VIDIOC_ENUM_FMT, &native_format ) < 0 )
            break;
        if( fromV4L2Format( native_format.pixelformat ) == eCameraPixelFormat::Unknown )
            continue;
        for( std::uint32_t size_index = 0U; ; ++size_index )
        {
            v4l2_frmsizeenum size = {};
            size.index = size_index;
            size.pixel_format = native_format.pixelformat;
            if( cameraIoctl( descriptor, VIDIOC_ENUM_FRAMESIZES, &size ) < 0 )
                break;
            if( size.type == V4L2_FRMSIZE_TYPE_DISCRETE )
                appendDiscreteIntervals( &capability.formats, descriptor, native_format.pixelformat,
                    size.discrete.width, size.discrete.height );
            else if( size.type == V4L2_FRMSIZE_TYPE_STEPWISE
                || size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS )
            {
                appendDiscreteIntervals( &capability.formats, descriptor, native_format.pixelformat,
                    size.stepwise.min_width, size.stepwise.min_height );
                if( size.stepwise.max_width != size.stepwise.min_width
                    || size.stepwise.max_height != size.stepwise.min_height )
                    appendDiscreteIntervals( &capability.formats, descriptor, native_format.pixelformat,
                        size.stepwise.max_width, size.stepwise.max_height );
            }
        }
    }
    capability.controls = queryControlCapabilities( descriptor );
    for( const sCameraFormat& format : capability.formats )
    {
        sCameraStreamProfile profile;
        profile.native_format = format;
        profile.output_formats.push_back( format.pixel_format );
        capability.stream_profiles.push_back( std::move( profile ) );
    }
    (void)::close( descriptor );
    if( capability.formats.empty() )
        return CameraResult< sCameraCapability >::failure( makeCameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedFormat
            , "V4L2 camera exposes no supported portable format." ) );
    return CameraResult< sCameraCapability >::success( std::move( capability ) );
}

// ------------------------------------------------------------------------------------------------
// Backend本体。DescriptorとMemory-mapped bufferを一人で所有するRAII owner.
// ------------------------------------------------------------------------------------------------

struct V4L2CameraBackendTestAccess;

class V4L2CameraBackend final : public ICameraBackend
{
    friend struct V4L2CameraBackendTestAccess;
    //! @brief Construct all members with explicit defaults.
public:
    V4L2CameraBackend()
        : m_descriptor        ( -1 )
        , m_streaming         ( false )
        , m_uvc_transport     ( false )
        , m_buffers           ()
        , m_frame_description ()
    {
    }
private:

  private:
    // Kernelと共有するMemory-mapped buffer一枠。munmapに必要な長さを対で持つ.
    struct Buffer
    {
        void* address;
        std::size_t length;

        //! @brief Construct all members with explicit defaults.
        Buffer(
              void * address_in = MAP_FAILED
            , std::size_t length_in = 0U
        )
            : address ( address_in )
            , length  ( length_in )
        {
        }
    };

    int m_descriptor;
    bool m_streaming;
    bool m_queue_dirty = false;
    bool m_uvc_transport;
    std::vector< Buffer > m_buffers;
    sCameraFrameDescription m_frame_description;
    // Internal, per-instance native operations; fault tests never open a device.
    struct NativeCalls
    {
        int (*ioctl_call)( int, unsigned long, void* ) = cameraIoctl;
        int (*close_call)( int ) = ::close;
        int (*unmap_call)( void*, std::size_t ) = ::munmap;
        int (*open_call)( const char*, int ) = []( const char* path_in, int flags_in )
            { return ::open( path_in, flags_in ); };
        void* (*map_call)( void*, std::size_t, int, int, int, off_t ) = ::mmap;
        int (*poll_call)( pollfd*, nfds_t, int ) = ::poll;
        // DQBUF retries belong to the read deadline, not cameraIoctl's EINTR loop.
        int (*dequeue_call)( int, unsigned long, void* ) = []( int fd_in, unsigned long op_in, void* arg_in )
            { return ::ioctl( fd_in, op_in, arg_in ); };
        std::chrono::steady_clock::time_point (*now_call)() = std::chrono::steady_clock::now;
    } m_calls;

    struct OpenRollback
    {
        V4L2CameraBackend* owner;
        explicit OpenRollback( V4L2CameraBackend& owner_inout ) noexcept : owner( &owner_inout ) {}
        OpenRollback( const OpenRollback& ) = delete;
        OpenRollback& operator=( const OpenRollback& ) = delete;
        ~OpenRollback() noexcept { if( owner ) owner->close(); }
    };

    // A valid DQBUF index is a lease until QBUF succeeds. Failed return closes
    // the uncertain queue; never issue a duplicate QBUF from the destructor.
    struct DequeuedBuffer
    {
        V4L2CameraBackend& owner;
        const std::uint32_t index;
        bool active = true;
        DequeuedBuffer( V4L2CameraBackend& owner_inout, const std::uint32_t index_in )
            : owner( owner_inout ), index( index_in ) {}
        DequeuedBuffer( const DequeuedBuffer& ) = delete;
        DequeuedBuffer& operator=( const DequeuedBuffer& ) = delete;
        ~DequeuedBuffer() noexcept { (void)this->release(); }
        int release() noexcept
        {
            if( !this->active ) return 0;
            this->active = false;
            v4l2_buffer buffer = {};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = this->index;
            if( this->owner.m_calls.ioctl_call( this->owner.m_descriptor, VIDIOC_QBUF, &buffer ) >= 0 )
                return 0;
            const int error = errno;
            this->owner.close();
            return error;
        }
    };

    bool resetQueue() noexcept
    {
        if( !this->m_queue_dirty ) return true;
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_STREAMOFF, &type ) < 0 )
            return false;
        this->m_queue_dirty = false;
        this->m_streaming = false;
        return true;
    }

    CameraStatus failStart( const char* const message_in, const int native_error_in )
    {
        // STREAMON failure leaves queued buffers queued. STREAMOFF resets even a
        // queue that has not started. If reset fails, only a full close is reusable.
        if( !this->resetQueue() ) this->close();
        return CameraStatus::failure( errnoError(
            eCameraErrorCode::BackendFailure, message_in, native_error_in ) );
    }

    void unmapBuffers() noexcept
    {
        for( Buffer& buffer : this->m_buffers )
        {
            if( buffer.address != MAP_FAILED )
                (void)this->m_calls.unmap_call( buffer.address, buffer.length );
        }
        this->m_buffers.clear();
    }

  public:
    ~V4L2CameraBackend() override
    {
        this->close();
    }

    CameraStatus open( const sCameraOpenDescription& description_in ) override
    {
        // 二重openの拒否と、Format要求の検証。V4L2 AdapterはPixel format変換を行わない方針
        // なので、Native formatと出力Formatが食い違う要求はDeviceへ触れる前に断る.
        if( this->m_descriptor >= 0 )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyOpen,
                "V4L2 camera is already open." ) );
        const eCameraPixelFormat native_format = description_in.native_pixel_format
            == eCameraPixelFormat::Unknown
                ? description_in.format.pixel_format : description_in.native_pixel_format;
        if( native_format != description_in.format.pixel_format )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "V4L2 adapter does not perform pixel-format conversion." ) );
        const std::uint32_t pixel_format = toV4L2Format( native_format );
        if( pixel_format == 0U )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "Requested pixel format is not supported by the V4L2 adapter." ) );
        this->m_descriptor = this->m_calls.open_call(
            description_in.device.id.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC );
        if( this->m_descriptor < 0 )
            return CameraStatus::failure( errnoError(
                eCameraErrorCode::OpenFailed, "Failed to open V4L2 camera." ) );

        OpenRollback rollback( *this );

        // 開いたNodeが本当に撮像DeviceでMemory-mapped streamingができるかを確認する。
        // 途中で失敗したらcloseで自分を初期状態へ戻してから返す（半開きの状態を残さない）.
        v4l2_capability capability = {};
        if( !queryCaptureCapability( &capability, this->m_descriptor, this->m_calls.ioctl_call ) )
        {
            const CameraError error = errnoError(
                eCameraErrorCode::OpenFailed, "V4L2 device is not a capture camera." );
            this->close();
            return CameraStatus::failure( error );
        }
        const std::string driver = reinterpret_cast< const char* >( capability.driver );
        const std::string bus = reinterpret_cast< const char* >( capability.bus_info );
        this->m_uvc_transport = driver == "uvcvideo" || bus.find( "usb" ) != std::string::npos;
        const std::uint32_t capabilities =
            ( capability.capabilities & V4L2_CAP_DEVICE_CAPS ) != 0U
                ? capability.device_caps : capability.capabilities;
        if( ( capabilities & V4L2_CAP_STREAMING ) == 0U )
        {
            this->close();
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedBackend,
                "V4L2 camera does not support memory-mapped streaming." ) );
        }

        // Formatを設定し、Driverが黙って別の値へ丸めていないかを読み返して確かめる。
        // 利用者が頼んだものと違う映像を「成功」として返さないための検査である.
        v4l2_format format = {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = description_in.format.width;
        format.fmt.pix.height = description_in.format.height;
        format.fmt.pix.pixelformat = pixel_format;
        format.fmt.pix.field = V4L2_FIELD_ANY;
        if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_S_FMT, &format ) < 0 )
        {
            const CameraError error = errnoError(
                eCameraErrorCode::ConfigurationFailed, "Failed to configure V4L2 camera format." );
            this->close();
            return CameraStatus::failure( error );
        }
        if( format.fmt.pix.width != description_in.format.width
            || format.fmt.pix.height != description_in.format.height
            || format.fmt.pix.pixelformat != pixel_format )
        {
            this->close();
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedFormat,
                "V4L2 camera substituted a different format." ) );
        }

        // V4L2は「1Frameあたりの時間」で受け取るので、Frame rateの分子分母を逆に渡す.
        v4l2_streamparm parameters = {};
        parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parameters.parm.capture.timeperframe.numerator =
            description_in.format.frame_rate_denominator;
        parameters.parm.capture.timeperframe.denominator =
            description_in.format.frame_rate_numerator;
        if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_S_PARM, &parameters ) < 0 )
        {
            const CameraError error = errnoError(
                eCameraErrorCode::ConfigurationFailed, "Failed to configure V4L2 camera frame rate." );
            this->close();
            return CameraStatus::failure( error );
        }

        // Memory-mapped bufferを確保して各枠をmmapする。4枠を頼むのは取りこぼしを防ぐ余裕、
        // 2枠未満しか取れなければDequeue中の再Queueが成立しないので失敗にする.
        v4l2_requestbuffers request = {};
        request.count = 4U;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_REQBUFS, &request ) < 0 || request.count < 2U )
        {
            const CameraError error = errnoError(
                eCameraErrorCode::ResourceExhausted, "Failed to allocate V4L2 camera buffers." );
            this->close();
            return CameraStatus::failure( error );
        }
        this->m_buffers.resize( request.count );
        for( std::uint32_t index = 0U; index < request.count; ++index )
        {
            v4l2_buffer buffer = {};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = index;
            if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_QUERYBUF, &buffer ) < 0 )
            {
                const CameraError error = errnoError(
                    eCameraErrorCode::ResourceExhausted, "Failed to query V4L2 camera buffer." );
                this->close();
                return CameraStatus::failure( error );
            }
            this->m_buffers[ index ].length = buffer.length;
            this->m_buffers[ index ].address = this->m_calls.map_call(
                nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                this->m_descriptor, static_cast< off_t >( buffer.m.offset ) );
            if( this->m_buffers[ index ].address == MAP_FAILED )
            {
                const CameraError error = errnoError(
                    eCameraErrorCode::ResourceExhausted, "Failed to map V4L2 camera buffer." );
                this->close();
                return CameraStatus::failure( error );
            }
        }
        // 以後の全Frameが共有する記述を固定する。MJPEGは圧縮Streamで行の概念が無いため、
        // Strideは0にしておく.
        this->m_frame_description.width = format.fmt.pix.width;
        this->m_frame_description.height = format.fmt.pix.height;
        this->m_frame_description.pixel_format = description_in.format.pixel_format;
        this->m_frame_description.row_stride =
            description_in.format.pixel_format == eCameraPixelFormat::Mjpeg
                ? 0U : format.fmt.pix.bytesperline;
        rollback.owner = nullptr;
        return CameraStatus::success();
    }

    void close() noexcept override
    {
        // Do not construct an allocating error in noexcept cleanup. Close the native
        // owner before unmapping, including when STREAMOFF cannot prove idle.
        (void)this->resetQueue();
        if( this->m_descriptor >= 0 )
        {
            (void)this->m_calls.close_call( this->m_descriptor );
            this->m_descriptor = -1;
        }
        this->m_streaming = false;
        this->m_queue_dirty = false;
        this->unmapBuffers();
        this->m_frame_description = {};
        this->m_uvc_transport = false;
    }

    CameraStatus start() override
    {
        if( this->m_descriptor < 0 )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
                "V4L2 camera is not open." ) );
        if( this->m_streaming )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
                "V4L2 camera stream is already active." ) );
        // 全BufferをKernelへ渡してからStreamを開始する。Queueが空のままSTREAMONすると
        // 最初のFrameを受ける場所が無い.
        this->m_queue_dirty = true;
        for( std::uint32_t index = 0U; index < this->m_buffers.size(); ++index )
        {
            v4l2_buffer buffer = {};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = index;
            if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_QBUF, &buffer ) < 0 )
                return this->failStart( "Failed to queue V4L2 camera buffer.", errno );
        }
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( this->m_calls.ioctl_call( this->m_descriptor, VIDIOC_STREAMON, &type ) < 0 )
            return this->failStart( "Failed to start V4L2 camera stream.", errno );
        this->m_streaming = true;
        return CameraStatus::success();
    }

    CameraStatus stop() override
    {
        if( !this->m_streaming )
            return CameraStatus::success();
        if( !this->resetQueue() )
        {
            const int native_error = errno;
            this->close();
            return CameraStatus::failure( errnoError(
                eCameraErrorCode::BackendFailure, "Failed to stop V4L2 camera stream; camera closed.",
                native_error ) );
        }
        this->m_streaming = false;
        return CameraStatus::success();
    }

    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        if( !this->m_streaming )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotStreaming,
                "V4L2 camera stream is not active." ) );
        const auto deadline = this->m_calls.now_call() + std::chrono::milliseconds( timeout_ms_in );
        const auto timedOut = []()
        {
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::Timeout, eCameraErrorCode::TimedOut,
                "Timed out waiting for a V4L2 camera frame." ) );
        };
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        while( true )
        {
            const auto now = this->m_calls.now_call();
            if( now >= deadline ) return timedOut();
            const auto remaining = std::chrono::ceil< std::chrono::milliseconds >( deadline - now ).count();
            const int wait_ms = static_cast< int >( (std::min)( remaining,
                static_cast< decltype(remaining) >( (std::numeric_limits< int >::max)() ) ) );
            pollfd descriptor = {};
            descriptor.fd = this->m_descriptor;
            descriptor.events = POLLIN | POLLPRI;
            const int result = this->m_calls.poll_call( &descriptor, 1U, wait_ms );
            if( result < 0 && errno == EINTR ) continue;
            if( result < 0 )
                return CameraResult< sCameraFrame >::failure( errnoError(
                    eCameraErrorCode::ReadFailed, "V4L2 camera poll failed." ) );
            if( result == 0 ) continue; // INT_MAX chunks share the same deadline.
            if( ( descriptor.revents & ( POLLERR | POLLHUP | POLLNVAL ) ) != 0 )
            {
                this->close();
                return CameraResult< sCameraFrame >::failure( makeCameraError(
                    eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                    "V4L2 camera poll reported an invalid stream." ) );
            }
            if( ( descriptor.revents & POLLIN ) == 0 ) continue;
            if( this->m_calls.now_call() >= deadline ) return timedOut();
            buffer = {};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            if( this->m_calls.dequeue_call( this->m_descriptor, VIDIOC_DQBUF, &buffer ) >= 0 ) break;
            const int error = errno;
            if( error == EINTR || error == EAGAIN ) continue;
            // A failed dequeue may have consumed an unidentified buffer. Do not
            // guess an index or leave a possibly depleted queue streaming.
            this->close();
            return CameraResult< sCameraFrame >::failure( errnoError(
                eCameraErrorCode::ReadFailed, "Failed to dequeue V4L2 camera frame.", error ) );
        }
        if( buffer.index >= this->m_buffers.size() )
        {
            this->close();
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "V4L2 returned an invalid camera buffer index." ) );
        }
        DequeuedBuffer lease( *this, buffer.index );
        if( buffer.bytesused > this->m_buffers[ buffer.index ].length
            || ( buffer.flags & V4L2_BUF_FLAG_ERROR ) != 0U )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "V4L2 returned an invalid camera buffer payload." ) );

        // 共有BufferからFrameへCopyしてから、Bufferを直ちにKernelへ返す。返すのを遅らせると
        // Queueが痩せて取りこぼすし、共有Memoryを指したままのFrameを外へ出すと所有権が壊れる.
        const std::uint8_t* const bytes = static_cast< const std::uint8_t* >(
            this->m_buffers[ buffer.index ].address );
        sCameraFrame frame;
        frame.description = this->m_frame_description;
        frame.data.assign( bytes, bytes + buffer.bytesused );
        frame.sequence = buffer.sequence;
        // DriverがMonotonic時刻を刻んでいればそれが撮像時刻に最も近い。刻んでいなければ
        // 受信時刻で代用する。どちらも同じMonotonic軸なので比較はできる.
        if( ( buffer.flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC ) != 0U )
            frame.monotonic_timestamp_ns =
                static_cast< std::int64_t >( buffer.timestamp.tv_sec ) * 1000000000LL
                + static_cast< std::int64_t >( buffer.timestamp.tv_usec ) * 1000LL;
        else
            frame.monotonic_timestamp_ns = std::chrono::duration_cast< std::chrono::nanoseconds >(
                std::chrono::steady_clock::now().time_since_epoch() ).count();

        if( !frame.valid() )
            return CameraResult< sCameraFrame >::failure( makeCameraError(
                eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
                "V4L2 returned a truncated camera frame." ) );
        const int return_error = lease.release();
        if( return_error != 0 )
            return CameraResult< sCameraFrame >::failure( errnoError(
                eCameraErrorCode::ReadFailed, "Failed to return V4L2 camera buffer.", return_error ) );
        return CameraResult< sCameraFrame >::success( std::move( frame ) );
    }

    CameraResult< sCameraControlValue > getControl( const eCameraControl control_in ) override
    {
        // 値の読み出しに先立って毎回QUERYCTRLで存在と向きを確かめる。無効なIDへのG_CTRLは
        // Driverにより挙動が揺れるので、揺れをこちらの分類で吸収する.
        const ControlMapping* const mapping = findControlMapping( control_in );
        if( mapping == nullptr )
            return CameraResult< sCameraControlValue >::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Camera control is not mapped by the V4L2 adapter." ) );
        v4l2_control value = {};
        value.id = mapping->value_id;
        v4l2_queryctrl capability = {};
        capability.id = mapping->value_id;
        if( cameraIoctl( this->m_descriptor, VIDIOC_QUERYCTRL, &capability ) < 0
            || ( capability.flags & V4L2_CTRL_FLAG_DISABLED ) != 0U )
            return CameraResult< sCameraControlValue >::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Camera does not support the requested V4L2 control." ) );
        if( ( capability.flags & V4L2_CTRL_FLAG_WRITE_ONLY ) != 0U )
            return CameraResult< sCameraControlValue >::failure( makeCameraError(
                eCameraErrorCategory::Validation, eCameraErrorCode::AccessDenied,
                "V4L2 camera control is write-only." ) );
        if( cameraIoctl( this->m_descriptor, VIDIOC_G_CTRL, &value ) < 0 )
            return CameraResult< sCameraControlValue >::failure( errnoError(
                eCameraErrorCode::ControlReadFailed, "Failed to read V4L2 camera control." ) );
        sCameraControlValue result;
        result.control = control_in;
        result.value = value.value;
        result.mode = eCameraControlMode::Manual;
        if( mapping->auto_id != NO_CONTROL )
        {
            v4l2_queryctrl auto_capability = {};
            auto_capability.id = mapping->auto_id;
            const bool supports_automatic = cameraIoctl(
                this->m_descriptor, VIDIOC_QUERYCTRL, &auto_capability ) == 0
                && ( auto_capability.flags & V4L2_CTRL_FLAG_DISABLED ) == 0U;
            v4l2_control automatic = {};
            automatic.id = mapping->auto_id;
            if( supports_automatic
                && cameraIoctl( this->m_descriptor, VIDIOC_G_CTRL, &automatic ) < 0 )
                return CameraResult< sCameraControlValue >::failure( errnoError(
                    eCameraErrorCode::ControlReadFailed,
                    "Failed to read V4L2 automatic-control state." ) );
            // EXPOSURE_AUTOだけは真偽値でなく列挙型（Manual／Auto／絞り優先など）なので、
            // 「Manual以外は全て自動」と読む。他の自動Controlは0が手動を意味する.
            if( supports_automatic )
            {
                if( mapping->auto_id == V4L2_CID_EXPOSURE_AUTO )
                    result.mode = automatic.value == V4L2_EXPOSURE_MANUAL
                        ? eCameraControlMode::Manual : eCameraControlMode::Automatic;
                else
                    result.mode = automatic.value == 0
                        ? eCameraControlMode::Manual : eCameraControlMode::Automatic;
            }
        }
        return CameraResult< sCameraControlValue >::success( result );
    }

    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        const ControlMapping* const mapping = findControlMapping( value_in.control );
        if( mapping == nullptr )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Camera control is not mapped by the V4L2 adapter." ) );
        v4l2_queryctrl capability = {};
        capability.id = mapping->value_id;
        if( cameraIoctl( this->m_descriptor, VIDIOC_QUERYCTRL, &capability ) < 0
            || ( capability.flags & V4L2_CTRL_FLAG_DISABLED ) != 0U )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Camera does not support the requested V4L2 control." ) );
        if( ( capability.flags & V4L2_CTRL_FLAG_READ_ONLY ) != 0U )
            return CameraStatus::failure( makeCameraError(
                eCameraErrorCategory::Validation, eCameraErrorCode::AccessDenied,
                "V4L2 camera control is read-only." ) );
        if( value_in.mode == eCameraControlMode::Automatic )
        {
            if( mapping->auto_id == NO_CONTROL )
                return CameraStatus::failure( makeCameraError(
                    eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                    "V4L2 camera control has no automatic mode." ) );
            v4l2_queryctrl auto_capability = {};
            auto_capability.id = mapping->auto_id;
            if( cameraIoctl( this->m_descriptor, VIDIOC_QUERYCTRL, &auto_capability ) < 0
                || ( auto_capability.flags & V4L2_CTRL_FLAG_DISABLED ) != 0U )
                return CameraStatus::failure( makeCameraError(
                    eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                    "V4L2 camera control does not support automatic mode." ) );
            // UVC CameraはV4L2_EXPOSURE_AUTOそのものを受けない個体が多く、実際の自動露出は
            // 絞り優先Modeとして実装されている。他のControlは1が自動を意味する.
            v4l2_control automatic = {};
            automatic.id = mapping->auto_id;
            automatic.value = mapping->auto_id == V4L2_CID_EXPOSURE_AUTO
                ? V4L2_EXPOSURE_APERTURE_PRIORITY : 1;
            if( cameraIoctl( this->m_descriptor, VIDIOC_S_CTRL, &automatic ) < 0 )
                return CameraStatus::failure( errnoError(
                    eCameraErrorCode::ControlWriteFailed, "Failed to enable V4L2 automatic control." ) );
            return CameraStatus::success();
        }
        // 範囲とStepの検証はこちらで行い、Deviceへ丸めさせない。Driverによっては範囲外を
        // 黙って切り詰めるため、利用者の指定と実際の値が食い違ったまま成功してしまう.
        const std::int64_t native_step = capability.step > 0 ? capability.step : 1;
        if( value_in.value < capability.minimum || value_in.value > capability.maximum
            || ( value_in.value - capability.minimum ) % native_step != 0 )
            return CameraStatus::failure( makeCameraError(
                  eCameraErrorCategory::Validation
                , eCameraErrorCode::InvalidArgument
                , "Manual camera-control value is outside its capability range or step." ) );
        // 手動値を書く前に対の自動Modeを切る。自動が生きたままだとDriverが手動値を
        // 直後に上書きし、書き込みが成功したのに効かないという見え方になる.
        if( mapping->auto_id != NO_CONTROL )
        {
            v4l2_queryctrl auto_capability = {};
            auto_capability.id = mapping->auto_id;
            if( cameraIoctl( this->m_descriptor, VIDIOC_QUERYCTRL, &auto_capability ) == 0
                && ( auto_capability.flags & V4L2_CTRL_FLAG_DISABLED ) == 0U )
            {
                v4l2_control automatic = {};
                automatic.id = mapping->auto_id;
                automatic.value = mapping->auto_id == V4L2_CID_EXPOSURE_AUTO
                    ? V4L2_EXPOSURE_MANUAL : 0;
                if( cameraIoctl( this->m_descriptor, VIDIOC_S_CTRL, &automatic ) < 0 )
                    return CameraStatus::failure( errnoError(
                        eCameraErrorCode::ControlWriteFailed,
                        "Failed to enable V4L2 manual control." ) );
            }
        }
        v4l2_control value = {};
        value.id = mapping->value_id;
        value.value = static_cast< std::int32_t >( value_in.value );
        if( cameraIoctl( this->m_descriptor, VIDIOC_S_CTRL, &value ) < 0 )
            return CameraStatus::failure( errnoError(
                eCameraErrorCode::ControlWriteFailed, "Failed to write V4L2 camera control." ) );
        return CameraStatus::success();
    }

    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) override
    {
        // まずGET_LENでDeviceが宣言するPayload長を訊き、Selectorの許容範囲と照合してから
        // その長さちょうどでGET_CURを発行する。長さ違いのまま読むとDriverがEINVALを返すか、
        // 悪くすると切り詰められた値を黙って返すため.
        if( !this->m_uvc_transport )
            return CameraResult< sCameraExtensionUnitValue >::failure( makeCameraError(
                  eCameraErrorCategory::Unsupported
                , eCameraErrorCode::UnsupportedExtensionUnit
                , "V4L2 device is not identified as a UVC transport." ) );
        std::uint8_t length_bytes[ 2U ] = {};
        uvc_xu_control_query length_query = {};
        length_query.unit = selector_in.unit_id;
        length_query.selector = selector_in.selector;
        length_query.query = UVC_GET_LEN;
        length_query.size = 2U;
        length_query.data = length_bytes;
        if( cameraIoctl( this->m_descriptor, UVCIOC_CTRL_QUERY, &length_query ) < 0 )
            return CameraResult< sCameraExtensionUnitValue >::failure( extensionUnitError(
                eCameraErrorCode::ExtensionUnitReadFailed,
                "Failed to query the V4L2 UVC extension-unit payload length." ) );
        const std::size_t length = static_cast< std::size_t >( length_bytes[ 0U ] )
            | ( static_cast< std::size_t >( length_bytes[ 1U ] ) << 8U );
        if( length < selector_in.minimum_size || length > selector_in.maximum_size )
            return CameraResult< sCameraExtensionUnitValue >::failure( makeCameraError(
                  eCameraErrorCategory::InputOutput
                , eCameraErrorCode::PayloadSizeMismatch
                , "V4L2 UVC extension-unit payload length is outside the advertised range." ) );
        sCameraExtensionUnitValue value;
        value.selector = selector_in;
        value.payload.resize( length );
        uvc_xu_control_query query = {};
        query.unit = selector_in.unit_id;
        query.selector = selector_in.selector;
        query.query = UVC_GET_CUR;
        query.size = static_cast< std::uint16_t >( length );
        query.data = value.payload.data();
        if( cameraIoctl( this->m_descriptor, UVCIOC_CTRL_QUERY, &query ) < 0 )
            return CameraResult< sCameraExtensionUnitValue >::failure( extensionUnitError(
                eCameraErrorCode::ExtensionUnitReadFailed,
                "Failed to read the V4L2 UVC extension unit." ) );
        return CameraResult< sCameraExtensionUnitValue >::success( std::move( value ) );
    }

    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) override
    {
        // 書き込みはDevice宣言の長さと完全一致のPayloadだけを通す。読み出しと違って
        // 部分書き込みはDevice状態を壊し得るので、不一致は検証Errorで止める.
        if( !this->m_uvc_transport )
            return CameraStatus::failure( makeCameraError(
                  eCameraErrorCategory::Unsupported
                , eCameraErrorCode::UnsupportedExtensionUnit
                , "V4L2 device is not identified as a UVC transport." ) );
        std::uint8_t length_bytes[ 2U ] = {};
        uvc_xu_control_query length_query = {};
        length_query.unit = value_in.selector.unit_id;
        length_query.selector = value_in.selector.selector;
        length_query.query = UVC_GET_LEN;
        length_query.size = 2U;
        length_query.data = length_bytes;
        if( cameraIoctl( this->m_descriptor, UVCIOC_CTRL_QUERY, &length_query ) < 0 )
            return CameraStatus::failure( extensionUnitError(
                eCameraErrorCode::ExtensionUnitWriteFailed,
                "Failed to query the V4L2 UVC extension-unit payload length." ) );
        const std::size_t length = static_cast< std::size_t >( length_bytes[ 0U ] )
            | ( static_cast< std::size_t >( length_bytes[ 1U ] ) << 8U );
        if( length != value_in.payload.size() )
            return CameraStatus::failure( makeCameraError(
                  eCameraErrorCategory::Validation
                , eCameraErrorCode::PayloadSizeMismatch
                , "V4L2 UVC extension-unit payload must match the device length." ) );
        uvc_xu_control_query query = {};
        query.unit = value_in.selector.unit_id;
        query.selector = value_in.selector.selector;
        query.query = UVC_SET_CUR;
        query.size = static_cast< std::uint16_t >( length );
        query.data = const_cast< std::uint8_t* >( value_in.payload.data() );
        if( cameraIoctl( this->m_descriptor, UVCIOC_CTRL_QUERY, &query ) < 0 )
            return CameraStatus::failure( extensionUnitError(
                eCameraErrorCode::ExtensionUnitWriteFailed,
                "Failed to write the V4L2 UVC extension unit." ) );
        return CameraStatus::success();
    }

    bool isOpen() const noexcept override
    {
        return this->m_descriptor >= 0;
    }

    bool isStreaming() const noexcept override
    {
        return this->m_streaming;
    }
};

} // namespace

std::unique_ptr< ICameraBackend > createV4L2CameraBackend()
{
    return std::make_unique< V4L2CameraBackend >();
}

CameraResult< std::vector< sCameraDeviceInfo > > enumerateV4L2CameraDevices()
{
    return enumerateV4L2Cameras();
}

CameraResult< sCameraCapability > queryV4L2CameraCapabilities(
    const sCameraDeviceInfo& device_in )
{
    return v4L2Capabilities( device_in );
}

} // namespace detail
} // namespace tmr
} // namespace wse
