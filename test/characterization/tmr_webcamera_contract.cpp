// @file tmr_webcamera_contract.cpp
// @brief WebCameraの新Facadeと旧Method互換を同一Mock runtimeで固定する。
// ここが落ちると、実機なしでWebCamera Facadeが守るべき境界が崩れる. 固定するのは、Device選択前
// のLifecycle Error、Capabilityの範囲やStepを外れた値と未公開のExtension SelectorをSessionへ
// 渡さない入口検証、Controlの読み書き往復とMode遷移、Frame 1枚の所有権、start／stop／closeの
// 状態遷移とcloseの冪等性、そして他言語BindingとFixtureを共有して同じ値に合意していること.

#include "../../core/tmr/device/WebCameraInternal.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#if defined( _MSC_VER )
#pragma warning(disable: 4996) // This contract intentionally exercises deprecated compatibility methods.
#elif defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace
{
using namespace wse::tmr;

int failures = 0;

// Reads the key=value fixture that every language binding's version of this contract also reads.
// Blank keys and '#' lines are skipped rather than rejected, so an unreadable path yields an empty
// map instead of an error: the first fixtureNumber() below is what turns that into a visible
// failure, and it does so by throwing out of main rather than by counting a failure.
std::unordered_map< std::string, std::string > loadFixture( const char* path_in )
{
    std::unordered_map< std::string, std::string > result;
    std::ifstream input( path_in );
    std::string line;
    while( std::getline( input, line ) )
    {
        // The fixture is shared with every language binding and is read on both platforms, so a
        // line that was written with a carriage return has to lose it here rather than becoming
        // part of a value. Without this the same tree passes on Windows and fails on Linux.
        if( !line.empty() && line.back() == '\r' ) line.pop_back();
        const std::size_t split = line.find( '=' );
        if( split != std::string::npos && !line.empty() && line[ 0 ] != '#' )
            result.emplace( line.substr( 0U, split ), line.substr( split + 1U ) );
    }
    return result;
}

// at() and stol both throw. A renamed or missing fixture key therefore aborts the run with an
// uncaught exception rather than producing a counted FAILED line, so cross-language drift in the
// fixture surfaces as a crash and not as a message.
long fixtureNumber( const std::unordered_map< std::string, std::string >& fixture_in,
    const char* name_in )
{
    return std::stol( fixture_in.at( name_in ) );
}

void expect( const bool condition_in, const char* const message_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}

CameraError error(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in )
{
    return CameraError( category_in, code_in, message_in );
}

// Three devices chosen to cover the facade's transport filter, which admits UsbUvc and Unknown
// and drops everything else: a UVC camera it must accept, a CSI sensor it must refuse because a
// libcamera sensor is not a web camera, and a device whose transport it cannot infer but must
// still accept. The USB identifiers are the fixture's, so the same numbers are asserted from
// every language binding.
sCameraDeviceInfo usbDevice()
{
    sCameraDeviceInfo device;
    device.backend = eCameraBackend::MediaFoundation;
    device.id = "mock-usb";
    device.display_name = "Mock USB Camera";
    device.transport = "usb-uvc";
    device.transport_type = eCameraTransport::UsbUvc;
    device.usb.vendor_id = 0x046DU;
    device.usb.product_id = 0x085CU;
    device.usb.serial_number = "fixture-001";
    device.usb.uvc_version_bcd = 0x0110U;
    return device;
}

sCameraDeviceInfo csiDevice()
{
    sCameraDeviceInfo device;
    device.backend = eCameraBackend::Libcamera;
    device.id = "mock-csi";
    device.display_name = "Mock CSI Camera";
    device.transport = "libcamera";
    device.transport_type = eCameraTransport::Csi;
    return device;
}

sCameraDeviceInfo unknownDevice()
{
    sCameraDeviceInfo device;
    device.backend = eCameraBackend::MediaFoundation;
    device.id = "mock-unknown";
    device.display_name = "Mock Integrated Camera";
    device.transport = "windows-camera";
    device.transport_type = eCameraTransport::Unknown;
    return device;
}

sCameraExtensionUnitSelector extensionSelector()
{
    sCameraExtensionUnitSelector selector;
    selector.unit_guid = { 0x10U, 0x20U, 0x30U, 0x40U,
        0x50U, 0x60U, 0x70U, 0x80U, 0x90U, 0xA0U, 0xB0U, 0xC0U,
        0xD0U, 0xE0U, 0xF0U, 0x01U };
    selector.unit_id = 3U;
    selector.selector = 2U;
    selector.minimum_size = 4U;
    selector.maximum_size = 4U;
    selector.readable = true;
    selector.writable = true;
    selector.display_name = "Mock XU";
    return selector;
}

// The advertised capability every check below is measured against. The range 0..100 with a step
// of 5 is what makes 55 a legal control value and 53 an illegal one, and the single 2x2 profile
// keeps a frame small enough to compare byte for byte. Only these four controls and this one
// extension unit are advertised, so anything outside them exercises the facade's refusal paths.
sCameraCapability capabilityFor( const sCameraDeviceInfo& device_in )
{
    sCameraCapability capability;
    capability.device = device_in;
    sCameraStreamProfile profile;
    profile.native_format = { 2U, 2U, 30U, 1U, eCameraPixelFormat::Yuyv422 };
    profile.output_formats = { eCameraPixelFormat::Bgra8 };
    capability.stream_profiles.push_back( profile );
    capability.formats.push_back( { 2U, 2U, 30U, 1U, eCameraPixelFormat::Bgra8 } );

    for( const eCameraControl control : {
            eCameraControl::Exposure, eCameraControl::Gain,
            eCameraControl::Focus, eCameraControl::Brightness } )
    {
        sCameraControlCapability control_capability;
        control_capability.control = control;
        control_capability.minimum = 0;
        control_capability.maximum = 100;
        control_capability.step = 5;
        control_capability.default_value = 50;
        control_capability.supports_manual = true;
        control_capability.supports_automatic = true;
        control_capability.readable = true;
        control_capability.writable = true;
        capability.controls.push_back( control_capability );
    }
    capability.extension_units.push_back( extensionSelector() );
    return capability;
}

// Stands in for a driver session. It models only the state machine the facade depends on --
// open/close, streaming or not, and one stored value per control -- and returns a fixed 2x2 BGRA
// frame with a monotonically increasing sequence.
//
// What it deliberately does not simulate: exposure time, so a frame arrives with no wait and the
// timeout argument only distinguishes zero from non-zero; device loss or a driver that fails
// mid-stream; and any validation of its own beyond lifecycle, since getExtensionUnit() below
// accepts every selector it is handed. That last omission is what gives the unadvertised-selector
// check its value -- if the facade did not filter, the mock would happily answer.
class MockSession final : public detail::IWebCameraSession
{
    //! @brief Construct all members with explicit defaults.
public:
    MockSession()
        : open_state        ( false )
        , streaming         ( false )
        , sequence          ( 0U )
        , exposure          {
        eCameraControl::Exposure, eCameraControlMode::Manual, 50 }
        , gain              {
        eCameraControl::Gain, eCameraControlMode::Manual, 50 }
        , focus             {
        eCameraControl::Focus, eCameraControlMode::Manual, 50 }
        , brightness        {
        eCameraControl::Brightness, eCameraControlMode::Manual, 50 }
        , extension_payload { 1U, 2U, 3U, 4U }
    {
    }
private:

  public:
    bool open_state;
    bool streaming;
    std::uint64_t sequence;
    sCameraControlValue exposure;
    sCameraControlValue gain;
    sCameraControlValue focus;
    sCameraControlValue brightness;
    std::vector< std::uint8_t > extension_payload;

    CameraStatus open(
          const sCameraDeviceInfo&          device_in
        , const sCameraStreamConfiguration& configuration_in ) override
    {
        if( this->open_state )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::AlreadyOpen, "Mock session is already open." ) );
        if( !device_in.valid() || !configuration_in.valid() )
            return CameraStatus::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument, "Mock open arguments are invalid." ) );
        this->open_state = true;
        return CameraStatus::success();
    }

    void close() noexcept override
    {
        this->streaming = false;
        this->open_state = false;
    }

    CameraStatus start() override
    {
        if( !this->open_state )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock session is not open." ) );
        if( this->streaming )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::AlreadyStreaming, "Mock session is already streaming." ) );
        this->streaming = true;
        return CameraStatus::success();
    }

    // Delivers exactly one frame inline on the calling thread, so the callback count is
    // deterministic. A real backend would deliver from its own capture thread; nothing here
    // exercises that hand-off or the locking it needs. The 10 ms timeout is only required to be
    // non-zero, since readFrame() treats zero as an invalid argument.
    CameraStatus start( const CameraFrameCallback& callback_in ) override
    {
        CameraStatus result = this->start();
        if( result.succeeded() && callback_in )
            callback_in( this->readFrame( 10U ) );
        return result;
    }

    CameraStatus stop() override
    {
        this->streaming = false;
        return CameraStatus::success();
    }

    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        if( !this->open_state )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock session is not open." ) );
        if( !this->streaming )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotStreaming, "Mock session is not streaming." ) );
        if( timeout_ms_in == 0U )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument, "Mock timeout is invalid." ) );
        sCameraFrame frame;
        frame.description = { 2U, 2U, eCameraPixelFormat::Bgra8, 8U };
        frame.data = {
            10U, 20U, 30U, 255U, 40U, 50U, 60U, 255U,
            70U, 80U, 90U, 255U, 100U, 110U, 120U, 255U
        };
        frame.sequence = ++this->sequence;
        frame.monotonic_timestamp_ns = static_cast< std::int64_t >( this->sequence * 1000U );
        return CameraResult< sCameraFrame >::success( std::move( frame ) );
    }

    sCameraControlValue* value( const eCameraControl control_in )
    {
        switch( control_in )
        {
            case eCameraControl::Exposure: return &this->exposure;
            case eCameraControl::Gain: return &this->gain;
            case eCameraControl::Focus: return &this->focus;
            case eCameraControl::Brightness: return &this->brightness;
            default: return nullptr;
        }
    }

    CameraResult< sCameraControlValue > getControl(
        const eCameraControl control_in ) override
    {
        if( !this->open_state )
            return CameraResult< sCameraControlValue >::failure( error(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
                "Mock session is not open." ) );
        sCameraControlValue* const current = this->value( control_in );
        if( current == nullptr )
            return CameraResult< sCameraControlValue >::failure( error(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Mock control is unsupported." ) );
        return CameraResult< sCameraControlValue >::success( *current );
    }

    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        if( !this->open_state )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock session is not open." ) );
        sCameraControlValue* const current = this->value( value_in.control );
        if( current == nullptr )
            return CameraStatus::failure( error( eCameraErrorCategory::Unsupported,
                eCameraErrorCode::UnsupportedControl, "Mock control is unsupported." ) );
        *current = value_in;
        return CameraStatus::success();
    }

    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) override
    {
        sCameraExtensionUnitValue value;
        value.selector = selector_in;
        value.payload = this->extension_payload;
        return CameraResult< sCameraExtensionUnitValue >::success( std::move( value ) );
    }

    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) override
    {
        this->extension_payload = value_in.payload;
        return CameraStatus::success();
    }

    bool isOpen() const noexcept override { return this->open_state; }
    bool isStreaming() const noexcept override { return this->streaming; }
};

// Replaces the platform backend so the contract runs on a machine with no camera attached.
// enumerate() ignores the requested backend and always returns the same three devices, so nothing
// here covers backend selection or an enumeration that fails. last_session is kept only to assert
// that the facade creates one session and owns it; it is a raw observer and is not a lifetime
// claim of its own.
class MockRuntime final : public detail::IWebCameraRuntime
{
    //! @brief Construct all members with explicit defaults.
public:
    MockRuntime()
        : last_session ( nullptr )
    {
    }
private:

  public:
    MockSession* last_session;

    CameraResult< std::vector< sCameraDeviceInfo > > enumerate(
        const eCameraBackend ) override
    {
        return CameraResult< std::vector< sCameraDeviceInfo > >::success(
            { usbDevice(), csiDevice(), unknownDevice() } );
    }

    CameraResult< sCameraCapability > capabilities(
        const sCameraDeviceInfo& device_in ) override
    {
        if( !device_in.valid() )
            return CameraResult< sCameraCapability >::failure( error(
                eCameraErrorCategory::Validation, eCameraErrorCode::InvalidArgument,
                "Mock device is invalid." ) );
        return CameraResult< sCameraCapability >::success( capabilityFor( device_in ) );
    }

    std::unique_ptr< detail::IWebCameraSession > createSession() override
    {
        std::unique_ptr< MockSession > session( new MockSession() );
        this->last_session = session.get();
        return std::unique_ptr< detail::IWebCameraSession >( std::move( session ) );
    }
};

// The legacy free-function frame callback shape: an img3c08_t plus an opaque owner pointer, which
// is why it counts frames through a pointer instead of a capture. Nothing in main() below passes
// it to the facade, so the legacy delivery path this file's brief claims to cover is not in fact
// exercised by any check here.
void legacyFrameCallback( const wse::img3c08_t& image_in, void* object_in )
{
    int* const count = static_cast< int* >( object_in );
    if( image_in.width() == 2U && image_in.height() == 2U )
        ++( *count );
}

} // namespace

int main( const int argc_in, const char* const* argv_in )
{
    // The fixture path is the only argument, and the run stops rather than continuing without it:
    // every check below reads keys out of the map and would throw instead of reporting.
    expect( argc_in == 2, "WebCamera binding fixture path is supplied" );
    if( argc_in != 2 ) return 1;
    const auto fixture = loadFixture( argv_in[ 1 ] );
    // The cross-language agreement. Each binding's copy of this contract asserts the same fixture
    // keys against its own constants, so an enumerator that is renumbered or a control step that
    // is changed on one side is caught here instead of silently disagreeing across the boundary.
    expect( fixtureNumber( fixture, "device_backend" )
            == static_cast<long>( usbDevice().backend )
        && fixture.at( "device_id" ) == usbDevice().id
        && fixtureNumber( fixture, "device_transport_type" )
            == static_cast<long>( usbDevice().transport_type )
        && fixtureNumber( fixture, "usb_vendor_id" ) == usbDevice().usb.vendor_id
        && fixtureNumber( fixture, "usb_product_id" ) == usbDevice().usb.product_id,
        "C++ device matches the shared language fixture" );
    const auto fixture_capability = capabilityFor( usbDevice() );
    expect( fixtureNumber( fixture, "profile_width" )
            == fixture_capability.stream_profiles.front().native_format.width
        && fixtureNumber( fixture, "profile_native_pixel_format" )
            == static_cast<long>( fixture_capability.stream_profiles.front().native_format.pixel_format )
        && fixtureNumber( fixture, "profile_output_pixel_format" )
            == static_cast<long>( fixture_capability.stream_profiles.front().output_formats.front() )
        && fixtureNumber( fixture, "control_step" )
            == fixture_capability.controls.front().step,
        "C++ profile and control match the shared language fixture" );
    // Before a device is selected the facade must answer with a lifecycle error and not with an
    // empty success, and it must refuse a device whose transport it does not serve. The error code
    // is compared against the fixture so the bindings surface the same code for the same misuse.
    std::shared_ptr< MockRuntime > runtime = std::make_shared< MockRuntime >();
    std::unique_ptr< WebCamera > camera = detail::WebCameraTestAccess::create( runtime );
    expect( runtime->last_session != nullptr, "Facade creates one owned session" );
    expect( !camera->currentCapabilities().succeeded(),
        "Capabilities require a selected device" );
    expect( static_cast<long>( camera->currentCapabilities().error().code() )
            == fixtureNumber( fixture, "not_open_error_code" ),
        "C++ lifecycle error matches the shared language fixture" );
    expect( !camera->open( csiDevice() ).succeeded(),
        "Facade rejects an explicit CSI device" );

    // The mock runtime is injected into this instance, so the device set comes from the same
    // mock rather than from the static enumerate(), which would reach the real backend.
    expect( camera->open( usbDevice() ).succeeded(),
        "open selects and opens the USB device" );
    expect( camera->currentCapabilities().succeeded()
        && camera->currentCapabilities().value().device.id == "mock-usb",
        "The opened device is the enumerated one" );
    expect( camera->isOpen(), "The camera reports itself open" );

    expect( camera->writeExposure( 55 ).succeeded(),
        "Typed exposure convenience uses the common control path" );
    expect( camera->writeExposure( 53 ).error().code()
        == eCameraErrorCode::InvalidArgument,
        "Facade rejects a manual value that violates capability step" );
    expect( camera->writeGain( 65 ).succeeded(), "writeGain sets the control" );
    expect( camera->readGain().succeeded() && camera->readGain().value().value == 65,
        "readGain reports what writeGain set" );
    expect( camera->writeGain( 0, eCameraControlMode::Automatic ).succeeded()
        && camera->readGain().value().mode == eCameraControlMode::Automatic,
        "An automatic write is reported as automatic" );
    expect( camera->writeGain( 65, eCameraControlMode::Manual ).succeeded()
        && camera->readGain().value().mode == eCameraControlMode::Manual,
        "A manual write returns the control to manual" );
    const auto gain_capability = camera->controlCapability( eCameraControl::Gain );
    expect( gain_capability.succeeded()
        && gain_capability.value().minimum == 0 && gain_capability.value().maximum == 100
        && gain_capability.value().step == 5,
        "The control capability reports the advertised range and step" );

    sCameraExtensionUnitValue extension;
    extension.selector = extensionSelector();
    extension.payload = { 9U, 8U, 7U, 6U };
    expect( camera->setExtensionUnit( extension ).succeeded(),
        "Advertised extension-unit write succeeds" );
    expect( camera->getExtensionUnit( extension.selector ).succeeded()
        && camera->getExtensionUnit( extension.selector ).value().payload == extension.payload,
        "Advertised extension-unit read returns owned bytes" );
    sCameraExtensionUnitSelector unknown_selector = extension.selector;
    unknown_selector.selector = 9U;
    expect( camera->getExtensionUnit( unknown_selector ).error().code()
        == eCameraErrorCode::UnsupportedExtensionUnit,
        "Unadvertised extension selector never reaches the session" );

    expect( camera->start().succeeded(), "start begins streaming" );
    expect( camera->isStreaming(), "The camera reports itself streaming" );
    const auto first_frame = camera->readFrame( 1000U );
    expect( first_frame.succeeded()
        && first_frame.value().description.width == 2U
        && first_frame.value().description.height == 2U,
        "readFrame returns one owned frame of the advertised extent" );
    expect( camera->stop().succeeded() && !camera->isStreaming() && camera->isOpen(),
        "stop returns to the open state" );

    int callback_count = 0;
    expect( camera->start( [ &callback_count ]( const CameraResult< sCameraFrame >& frame_in )
        {
            if( frame_in.succeeded() )
                ++callback_count;
        } ).succeeded(), "start accepts an owned frame callback" );
    expect( callback_count == 1 && camera->isStreaming(),
        "The owned callback receives one frame" );
    expect( camera->stop().succeeded(), "stop ends the callback stream" );
    camera->close();
    expect( !camera->isOpen(), "Close releases the common session" );
    expect( !camera->currentCapabilities().succeeded()
        && camera->getExtensionUnit( extension.selector ).error().code()
            == eCameraErrorCode::NotOpen,
        "Close clears selection and extension operations report NotOpen" );

    expect( camera->open( unknownDevice() ).succeeded()
        && camera->currentCapabilities().value().device.id == "mock-unknown",
        "A non-inferred integrated camera can be opened after close" );
    camera->close();
    camera->close();
    expect( !camera->isOpen(), "close is idempotent" );
    return failures == 0 ? 0 : 1;
}
