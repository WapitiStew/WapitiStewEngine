// @file engine/wse/test/characterization/tmr_camera_contract.cpp
// @brief 実Cameraを必須にせずPortable Tmr CameraのData、Error、Lifecycle契約を固定する。
// @brief この契約が破れると、Tmr Cameraの公開Data型を使うApplicationが次を失う.
//        1) 既存Control IDの安定（並び替えは全BindingのABIを壊す）.
//        2) Frame記述のByte数・Stride・Memory量の算術と、短いStrideや奇数幅NV12の拒否.
//        3) Native FormatとOutput Formatの区別、変換無効時の異Format拒否.
//        4) open前の全操作がNotOpenを返し、居ないBackend／Deviceが構造化Errorになること.
//        5) Control値の正規化・物理単位換算・Clamp・Step整列.
//        Deviceが繋がっていれば列挙は実物を返すが、本Testはその存在を前提にしない.

#include <tmr/camera/Camera.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
int failures = 0;

void expect( const bool condition_in, const char* const message_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}
}

int main()
{
    using namespace wse::tmr;

    // 列挙値はBinding越しに整数として運ばれるため、既存IDの数値はCompile時に凍結する.
    static_assert( static_cast< std::uint8_t >( eCameraControl::Exposure ) == 0U,
        "Existing camera-control IDs must remain stable" );
    static_assert( static_cast< std::uint8_t >( eCameraControl::WhiteBalance ) == 6U,
        "Existing camera-control IDs must remain stable" );
    static_assert( static_cast< std::uint8_t >( eCameraControl::Zoom ) == 7U,
        "New camera-control IDs append after the existing catalog" );

    sCameraFrameDescription bgra_description;
    bgra_description.width = 640U;
    bgra_description.height = 480U;
    bgra_description.pixel_format = eCameraPixelFormat::Bgra8;
    expect( bgra_description.bytesPerPixel() == 4U, "BGRA8 byte count" );
    expect( bgra_description.minimumRowStride() == 2560U, "BGRA8 minimum row stride" );
    expect( bgra_description.memorySize() == 1228800U, "BGRA8 memory size" );
    expect( bgra_description.valid(), "BGRA8 frame description is valid" );

    sCameraFrameDescription padded_description = bgra_description;
    padded_description.row_stride = 4096U;
    expect( padded_description.memorySize() == 1966080U, "Padded frame memory size" );
    padded_description.row_stride = 128U;
    expect( !padded_description.valid(), "Short row stride is rejected" );

    sCameraFrameDescription nv12_description;
    nv12_description.width = 1280U;
    nv12_description.height = 720U;
    nv12_description.pixel_format = eCameraPixelFormat::Nv12;
    expect( nv12_description.memorySize() == 1382400U, "NV12 memory size" );
    nv12_description.row_stride = 1344U;
    expect( nv12_description.memorySize() == 1451520U, "Padded NV12 memory size" );
    nv12_description.width = 1279U;
    expect( !nv12_description.valid(), "Odd NV12 width is rejected" );
    nv12_description.width = 1280U;
    nv12_description.height = 719U;
    expect( !nv12_description.valid(), "Odd NV12 height is rejected rather than rounded up" );

    // TMR-FRAME-04: padded rows include the final row's padding; camera values allow
    // extra owned bytes, whereas a renderer upload requires an exact frame byte count.
    sCameraFrame padded_frame;
    padded_frame.description = { 3U, 2U, eCameraPixelFormat::Bgra8, 16U };
    padded_frame.data.resize( 31U );
    expect( !padded_frame.valid(), "Camera frame requires all padded rows" );
    padded_frame.data.resize( 33U );
    expect( padded_frame.valid(), "Camera frame permits owned bytes beyond minimum memory size" );

    sCameraFrameDescription compressed_description;
    compressed_description.width = 1920U;
    compressed_description.height = 1080U;
    compressed_description.pixel_format = eCameraPixelFormat::Mjpeg;
    expect( compressed_description.valid(), "MJPEG variable-size description is valid" );
    sCameraFrame compressed_frame;
    compressed_frame.description = compressed_description;
    expect( !compressed_frame.valid(), "Empty MJPEG frame is rejected" );
    compressed_frame.data = { 0xFFU, 0xD8U, 0xFFU, 0xD9U };
    expect( compressed_frame.valid(), "Owned MJPEG payload is valid" );

    sCameraFormat format;
    format.width = 640U;
    format.height = 480U;
    format.frame_rate_numerator = 30000U;
    format.frame_rate_denominator = 1001U;
    format.pixel_format = eCameraPixelFormat::Bgra8;
    expect( format.valid(), "Camera format is valid" );
    expect( format.framesPerSecond() > 29.9 && format.framesPerSecond() < 30.0,
        "Fractional camera frame rate is preserved" );

    sCameraStreamProfile native_profile;
    native_profile.native_format = {
        1920U, 1080U, 30U, 1U, eCameraPixelFormat::Mjpeg };
    native_profile.output_formats = {
        eCameraPixelFormat::Mjpeg, eCameraPixelFormat::Bgra8 };
    expect( native_profile.valid(), "Native stream profile is valid" );
    expect( native_profile.supportsOutput( eCameraPixelFormat::Bgra8 )
        && !native_profile.supportsOutput( eCameraPixelFormat::Nv12 ),
        "Native and output pixel formats remain distinguishable" );
    sCameraStreamConfiguration converted_stream;
    converted_stream.native_format = native_profile.native_format;
    converted_stream.output_format = eCameraPixelFormat::Bgra8;
    expect( converted_stream.valid(), "Explicit native-to-output conversion is valid" );
    converted_stream.allow_conversion = false;
    expect( !converted_stream.valid(), "Disabled conversion rejects different formats" );

    sCameraDeviceInfo usb_device;
    usb_device.backend = eCameraBackend::Video4Linux2;
    usb_device.id = "/dev/video0";
    usb_device.transport_type = eCameraTransport::UsbUvc;
    usb_device.usb.vendor_id = 0x046DU;
    usb_device.usb.product_id = 0x085CU;
    expect( usb_device.valid() && usb_device.usb.available(),
        "Typed UVC transport preserves optional USB identity" );
    sCameraDeviceInfo unknown_transport = usb_device;
    unknown_transport.transport_type = eCameraTransport::Unknown;
    unknown_transport.usb = {};
    expect( unknown_transport.valid() && !unknown_transport.usb.available(),
        "Unknown transport and unavailable USB identity are explicit" );

    sCameraExtensionUnitSelector extension_selector;
    extension_selector.unit_id = 3U;
    extension_selector.selector = 2U;
    extension_selector.minimum_size = 4U;
    extension_selector.maximum_size = 4U;
    extension_selector.readable = true;
    extension_selector.writable = true;
    expect( extension_selector.valid(), "Portable UVC extension selector is valid" );
    sCameraExtensionUnitValue extension_value;
    extension_value.selector = extension_selector;
    extension_value.payload = { 1U, 2U, 3U, 4U };
    expect( extension_value.valid(), "Owned UVC extension payload is valid" );
    extension_value.payload.pop_back();
    expect( !extension_value.valid(), "UVC extension payload size is validated" );

    CameraSession camera;
    expect( !camera.isOpen() && !camera.isStreaming(), "Camera starts closed" );
    const CameraStatus start_before_open = camera.start();
    expect( !start_before_open.succeeded()
        && start_before_open.error().code() == eCameraErrorCode::NotOpen,
        "Start before open returns structured lifecycle error" );
    const CameraResult< sCameraFrame > read_before_open = camera.readFrame( 10U );
    expect( !read_before_open.succeeded()
        && read_before_open.error().code() == eCameraErrorCode::NotOpen,
        "Read before open returns structured lifecycle error" );
    const CameraResult< sCameraControlValue > control_before_open =
        camera.getControl( eCameraControl::Brightness );
    expect( !control_before_open.succeeded()
        && control_before_open.error().code() == eCameraErrorCode::NotOpen,
        "Control read before open returns structured lifecycle error" );
    const CameraStatus control_write_before_open = camera.setControl( {
        eCameraControl::Brightness, eCameraControlMode::Manual, 0 } );
    expect( !control_write_before_open.succeeded()
        && control_write_before_open.error().code() == eCameraErrorCode::NotOpen,
        "Control write before open returns structured lifecycle error" );
    expect( camera.getExtensionUnit( extension_selector ).error().code()
        == eCameraErrorCode::NotOpen,
        "Extension-unit read before open returns structured lifecycle error" );
    extension_value.payload = { 1U, 2U, 3U, 4U };
    expect( camera.setExtensionUnit( extension_value ).error().code()
        == eCameraErrorCode::NotOpen,
        "Extension-unit write before open returns structured lifecycle error" );
    expect( camera.stop().succeeded(), "Stop is idempotent while closed" );
    camera.close();

    const CameraResult< std::vector< sCameraDeviceInfo > > devices = CameraSession::enumerate();
    expect( devices.succeeded(), "Automatic camera enumeration succeeds even when no device exists" );
    if( devices.succeeded() )
    {
        for( const sCameraDeviceInfo& device : devices.value() )
            expect( device.valid(), "Enumerated camera identity is valid" );
    }

    // 「他OSのBackend」を意図的に頼む。存在しないBackendがDeviceを捏造せず、構造化された
    // UnsupportedBackendを返すことがこの検査の狙いなので、選ぶBackendはOSごとに入れ替える.
#if defined( _WIN32 )
    const CameraResult< std::vector< sCameraDeviceInfo > > unsupported =
        CameraSession::enumerate( eCameraBackend::Video4Linux2 );
#else
    const CameraResult< std::vector< sCameraDeviceInfo > > unsupported =
        CameraSession::enumerate( eCameraBackend::MediaFoundation );
#endif
    expect( !unsupported.succeeded()
        && unsupported.error().code() == eCameraErrorCode::UnsupportedBackend,
        "Foreign platform backend returns structured unsupported error" );

    // libcameraはBuild構成次第で居たり居なかったりする。どちらの構成でも通るよう、
    // 以後の期待値は「Adapterが居るか」で二択に分ける.
    const CameraResult< std::vector< sCameraDeviceInfo > > libcamera =
        CameraSession::enumerate( eCameraBackend::Libcamera );
    const bool libcamera_adapter_available = libcamera.succeeded();
#if defined(WSE_EXPECT_LIBCAMERA)
    expect(libcamera_adapter_available, "Enabled libcamera build must execute the real adapter, not the stub");
#endif
    if( libcamera_adapter_available )
        expect( std::all_of( libcamera.value().begin(), libcamera.value().end(),
            []( const sCameraDeviceInfo& device_in )
            {
                return device_in.valid() && device_in.backend == eCameraBackend::Libcamera;
            } ), "Available libcamera adapter returns only valid native devices" );
    else
        expect( libcamera.error().category() == eCameraErrorCategory::Unsupported
            && libcamera.error().code() == eCameraErrorCode::UnsupportedBackend,
            "Unavailable libcamera adapter is reported without fake devices" );
    sCameraOpenDescription libcamera_description;
    libcamera_description.device = {
        eCameraBackend::Libcamera, "contract-only", "Contract Camera", "mock" };
    libcamera_description.format = {
        640U, 480U, 30U, 1U, eCameraPixelFormat::Bgra8 };
    const CameraResult< sCameraCapability > libcamera_capability =
        CameraSession::capabilities( libcamera_description.device );
    expect( !libcamera_capability.succeeded()
        && libcamera_capability.error().code() == ( libcamera_adapter_available
            ? eCameraErrorCode::DeviceNotFound : eCameraErrorCode::UnsupportedBackend ),
        "libcamera capability query returns an availability-specific structured error" );
    const CameraStatus libcamera_open = camera.open( libcamera_description );
    expect( !libcamera_open.succeeded()
        && libcamera_open.error().code() == ( libcamera_adapter_available
            ? eCameraErrorCode::DeviceNotFound : eCameraErrorCode::UnsupportedBackend ),
        "libcamera open returns an availability-specific structured error" );

    sCameraControlCapability control;
    control.minimum = 0;
    control.maximum = 100;
    control.step = 1;
    control.default_value = 50;
    control.supports_manual = true;
    expect( control.valid(), "Camera control capability range is valid" );
    expect( control.valueFromNormalized( 0.0 ) == 0,
        "Normalized camera control maps the minimum" );
    expect( control.valueFromNormalized( 0.5 ) == 50,
        "Normalized camera control maps the midpoint" );
    expect( control.valueFromNormalized( 2.0 ) == 100,
        "Normalized camera control clamps to the maximum" );
    expect( control.normalizedFromValue( 25 ) == 0.25,
        "Camera control value maps to normalized position" );
    control.unit = eCameraControlUnit::Microseconds;
    control.physical_scale = 100.0;
    expect( control.physicalFromValue( 25 ) == 2500.0,
        "Camera control value maps to its declared physical unit" );
    expect( control.valueFromPhysical( 2549.0 ) == 25,
        "Physical camera control value aligns to the capability step" );
    expect( control.valueFromPhysical( 100000.0 ) == 100,
        "Physical camera control value clamps to the capability range" );
    control.default_value = 101;
    expect( !control.valid(), "Out-of-range camera control default is rejected" );

    return failures == 0 ? 0 : 1;
}
