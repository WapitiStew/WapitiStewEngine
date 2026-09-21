// @file tmr_camera_backend_contract.cpp
// @brief 実機なしでCamera backendのControl、Lifecycle、切断および複数Session契約を固定する。
// @brief この契約が破れると、ICameraBackendを実装するAdapterと、それを使うSessionが次を失う.
//        1) NotOpen／AlreadyOpen／NotStreamingのLifecycle順序Errorが正確に出ること.
//        2) Controlの範囲・Step検証、Automatic modeでの値無視、読み返しの安定.
//        3) Extension unitのSelector検証とPayload長の強制.
//        4) 切断後の全操作がDeviceDisconnectedへ収束すること.
//        5) 複数Sessionが独立した連番を持ち、CallbackがStop要求しても自己Joinしないこと.
//        実Deviceは使わずMock backendで固定する。実OS Adapterの同じ性質はtmr_windows_camera_smoke
//        と実機Smoke testが受け持つ.

#include "../../core/tmr/camera/CameraBackend.h"
#include <tmr/camera/Camera.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
using namespace wse::tmr;

int failures = 0;

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

// 実OS Adapterと同じ検証順序（Lifecycle→接続→引数）を守る最小Backend。5刻み0..100の
// Brightness一つとUnit 3／Selector 2のExtensionだけを持ち、拒否経路を確実に踏ませる.
class MockCameraBackend final : public wse::tmr::detail::ICameraBackend
{
    //! @brief Construct all members with explicit defaults.
public:
    MockCameraBackend()
        : m_open              ( false )
        , m_streaming         ( false )
        , m_connected         ( true )
        , m_sequence          ( 0U )
        , m_brightness        {
        eCameraControl::Brightness, eCameraControlMode::Manual, 50
    }
        , m_extension_payload { 1U, 2U, 3U, 4U }
    {
    }
private:

  private:
    std::atomic_bool m_open;
    std::atomic_bool m_streaming;
    std::atomic_bool m_connected;
    std::uint64_t m_sequence;
    sCameraControlValue m_brightness;
    std::vector< std::uint8_t > m_extension_payload;
    std::atomic_bool m_block_read{ false };
    bool m_script_timeout_error = false;
    std::atomic_uint m_script_reads{ 0U };
    bool m_read_waiting = false;
    std::mutex m_read_mutex;
    std::condition_variable m_read_condition;

    CameraError disconnectedError() const
    {
        return error( eCameraErrorCategory::Device,
            eCameraErrorCode::DeviceDisconnected, "Mock camera was disconnected." );
    }

  public:
    void blockReadUntilStop() { this->m_block_read = true; }
    void timeoutThenError() { this->m_script_timeout_error = true; }
    unsigned scriptedReads() const { return this->m_script_reads.load(); }
    bool waitUntilReading()
    {
        std::unique_lock< std::mutex > lock( this->m_read_mutex );
        return this->m_read_condition.wait_for( lock, std::chrono::seconds( 2 ),
            [ this ]() { return this->m_read_waiting; } );
    }
    void disconnect() noexcept
    {
        this->m_connected = false;
        this->m_streaming = false;
    }

    CameraStatus open( const sCameraOpenDescription& description_in ) override
    {
        if( this->m_open )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::AlreadyOpen, "Mock camera is already open." ) );
        if( !description_in.device.valid() || !description_in.format.valid() )
            return CameraStatus::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument, "Mock open description is invalid." ) );
        if( description_in.native_pixel_format != eCameraPixelFormat::Unknown
            && description_in.native_pixel_format != description_in.format.pixel_format
            && !description_in.allow_format_conversion )
            return CameraStatus::failure( error( eCameraErrorCategory::Unsupported,
                eCameraErrorCode::UnsupportedFormat, "Mock conversion is disabled." ) );
        this->m_open = true;
        this->m_connected = true;
        return CameraStatus::success();
    }

    void close() noexcept override
    {
        this->m_streaming = false;
        this->m_open = false;
    }

    CameraStatus start() override
    {
        if( !this->m_open )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock camera is not open." ) );
        if( !this->m_connected )
            return CameraStatus::failure( this->disconnectedError() );
        if( this->m_streaming )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::AlreadyStreaming, "Mock stream is already active." ) );
        this->m_streaming = true;
        return CameraStatus::success();
    }

    CameraStatus stop() override
    {
        std::lock_guard< std::mutex > lock( this->m_read_mutex );
        this->m_streaming = false;
        this->m_read_condition.notify_all();
        return CameraStatus::success();
    }

    CameraResult< sCameraFrame > readFrame( const std::uint32_t timeout_ms_in ) override
    {
        if( !this->m_open )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock camera is not open." ) );
        if( !this->m_connected )
            return CameraResult< sCameraFrame >::failure( this->disconnectedError() );
        if( !this->m_streaming )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotStreaming, "Mock stream is not active." ) );
        if( timeout_ms_in == 0U )
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument, "Mock timeout is invalid." ) );

        if( this->m_script_timeout_error )
        {
            const bool timed_out = ++this->m_script_reads <= 2U;
            return CameraResult< sCameraFrame >::failure( error(
                timed_out ? eCameraErrorCategory::Timeout : eCameraErrorCategory::InputOutput,
                timed_out ? eCameraErrorCode::TimedOut : eCameraErrorCode::ReadFailed,
                "Scripted timeout followed by terminal read failure." ) );
        }
        if( this->m_block_read )
        {
            std::unique_lock< std::mutex > lock( this->m_read_mutex );
            this->m_read_waiting = true;
            this->m_read_condition.notify_all();
            this->m_read_condition.wait_for( lock, std::chrono::milliseconds( timeout_ms_in ),
                [ this ]() { return !this->m_streaming.load(); } );
            return CameraResult< sCameraFrame >::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotStreaming, "Stopped read was unblocked." ) );
        }
        sCameraFrame frame;
        frame.description = { 2U, 2U, eCameraPixelFormat::Bgra8, 8U };
        frame.data.resize( 16U, static_cast< std::uint8_t >( ++this->m_sequence ) );
        frame.sequence = this->m_sequence;
        frame.monotonic_timestamp_ns = static_cast< std::int64_t >( this->m_sequence * 1000U );
        return CameraResult< sCameraFrame >::success( std::move( frame ) );
    }

    CameraResult< sCameraControlValue > getControl(
        const eCameraControl control_in ) override
    {
        if( !this->m_open )
            return CameraResult< sCameraControlValue >::failure( error(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
                "Mock camera is not open." ) );
        if( !this->m_connected )
            return CameraResult< sCameraControlValue >::failure( this->disconnectedError() );
        if( control_in != eCameraControl::Brightness )
            return CameraResult< sCameraControlValue >::failure( error(
                eCameraErrorCategory::Unsupported, eCameraErrorCode::UnsupportedControl,
                "Mock control is unsupported." ) );
        return CameraResult< sCameraControlValue >::success( this->m_brightness );
    }

    CameraStatus setControl( const sCameraControlValue& value_in ) override
    {
        if( !this->m_open )
            return CameraStatus::failure( error( eCameraErrorCategory::Lifecycle,
                eCameraErrorCode::NotOpen, "Mock camera is not open." ) );
        if( !this->m_connected )
            return CameraStatus::failure( this->disconnectedError() );
        if( value_in.control != eCameraControl::Brightness )
            return CameraStatus::failure( error( eCameraErrorCategory::Unsupported,
                eCameraErrorCode::UnsupportedControl, "Mock control is unsupported." ) );
        if( value_in.mode == eCameraControlMode::Manual
            && ( value_in.value < 0 || value_in.value > 100 || value_in.value % 5 != 0 ) )
            return CameraStatus::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::InvalidArgument, "Mock control value is invalid." ) );
        this->m_brightness.mode = value_in.mode;
        if( value_in.mode == eCameraControlMode::Manual )
            this->m_brightness.value = value_in.value;
        return CameraStatus::success();
    }

    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) override
    {
        if( !this->m_connected )
            return CameraResult< sCameraExtensionUnitValue >::failure( this->disconnectedError() );
        if( selector_in.unit_id != 3U || selector_in.selector != 2U )
            return CameraResult< sCameraExtensionUnitValue >::failure( error(
                eCameraErrorCategory::Unsupported,
                eCameraErrorCode::UnsupportedExtensionUnit,
                "Mock extension selector is unsupported." ) );
        sCameraExtensionUnitValue value;
        value.selector = selector_in;
        value.payload = this->m_extension_payload;
        return CameraResult< sCameraExtensionUnitValue >::success( std::move( value ) );
    }

    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) override
    {
        if( !this->m_connected )
            return CameraStatus::failure( this->disconnectedError() );
        if( value_in.selector.unit_id != 3U || value_in.selector.selector != 2U )
            return CameraStatus::failure( error( eCameraErrorCategory::Unsupported,
                eCameraErrorCode::UnsupportedExtensionUnit,
                "Mock extension selector is unsupported." ) );
        if( value_in.payload.size() != 4U )
            return CameraStatus::failure( error( eCameraErrorCategory::Validation,
                eCameraErrorCode::PayloadSizeMismatch,
                "Mock extension payload has the wrong length." ) );
        this->m_extension_payload = value_in.payload;
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

sCameraOpenDescription mockDescription( const char* const id_in )
{
    sCameraOpenDescription description;
    description.device = { eCameraBackend::Libcamera, id_in, "Mock Camera", "mock" };
    description.format = { 2U, 2U, 30U, 1U, eCameraPixelFormat::Bgra8 };
    return description;
}

sCameraExtensionUnitSelector mockExtensionSelector()
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
    return selector;
}

} // namespace

int main()
{
    using namespace wse::tmr;

    MockCameraBackend first;
    MockCameraBackend second;
    expect( first.start().error().code() == eCameraErrorCode::NotOpen,
        "Start before open reports NotOpen" );
    expect( first.open( mockDescription( "mock-1" ) ).succeeded(),
        "First mock opens" );
    expect( first.open( mockDescription( "mock-1" ) ).error().code()
        == eCameraErrorCode::AlreadyOpen, "Double open reports AlreadyOpen" );

    const CameraResult< sCameraControlValue > initial =
        first.getControl( eCameraControl::Brightness );
    expect( initial.succeeded() && initial.value().value == 50,
        "Control read returns the initial value" );
    expect( first.setControl( { eCameraControl::Brightness,
        eCameraControlMode::Manual, 53 } ).error().code() == eCameraErrorCode::InvalidArgument,
        "Control rejects a value that violates its step" );
    expect( first.setControl( { eCameraControl::Brightness,
        eCameraControlMode::Manual, 60 } ).succeeded(), "Manual control write succeeds" );

    const sCameraExtensionUnitSelector extension_selector = mockExtensionSelector();
    CameraResult< sCameraExtensionUnitValue > extension_read =
        first.getExtensionUnit( extension_selector );
    expect( extension_read.succeeded() && extension_read.value().payload.size() == 4U,
        "Extension-unit read returns an owned payload" );
    sCameraExtensionUnitValue extension_write;
    extension_write.selector = extension_selector;
    extension_write.payload = { 9U, 8U, 7U, 6U };
    expect( first.setExtensionUnit( extension_write ).succeeded(),
        "Extension-unit write succeeds for an advertised selector" );
    extension_read = first.getExtensionUnit( extension_selector );
    expect( extension_read.succeeded() && extension_read.value().payload == extension_write.payload,
        "Extension-unit write is observable on readback" );
    sCameraExtensionUnitSelector unknown_extension = extension_selector;
    unknown_extension.selector = 9U;
    expect( first.getExtensionUnit( unknown_extension ).error().code()
        == eCameraErrorCode::UnsupportedExtensionUnit,
        "Unknown extension-unit selector never reports success" );
    extension_write.payload.pop_back();
    expect( first.setExtensionUnit( extension_write ).error().code()
        == eCameraErrorCode::PayloadSizeMismatch,
        "Extension-unit payload boundary is enforced" );

    expect( first.start().succeeded(), "First stream starts" );
    const CameraResult< sCameraFrame > first_frame = first.readFrame( 10U );
    expect( first_frame.succeeded() && first_frame.value().valid()
        && first_frame.value().sequence == 1U, "First owned frame is valid" );
    expect( first.setControl( { eCameraControl::Brightness,
        eCameraControlMode::Automatic, -999 } ).succeeded(),
        "Automatic control ignores the supplied native value while streaming" );
    expect( first.getControl( eCameraControl::Brightness ).value().mode
        == eCameraControlMode::Automatic, "Automatic mode is observable" );
    expect( first.stop().succeeded() && first.stop().succeeded(), "Stop is idempotent" );
    expect( first.start().succeeded(), "Stream restarts without reopening" );
    const CameraResult< sCameraFrame > restarted_frame = first.readFrame( 10U );
    expect( restarted_frame.succeeded() && restarted_frame.value().sequence == 2U
        && restarted_frame.value().monotonic_timestamp_ns
            > first_frame.value().monotonic_timestamp_ns,
        "Restart preserves increasing sequence and timestamp" );

    expect( second.open( mockDescription( "mock-2" ) ).succeeded()
        && second.start().succeeded(), "A second independent session starts" );
    const CameraResult< sCameraFrame > second_frame = second.readFrame( 10U );
    expect( second_frame.succeeded() && second_frame.value().sequence == 1U,
        "Second session owns an independent frame sequence" );

    // 1000回のStressで、Control書き込み・読み返し・二Sessionの独立連番・100回ごとの
    // 停止再開が揺らがないことを見る。単発では通るが繰り返すと壊れる回帰を捕まえるため.
    std::uint64_t previous_first_sequence = restarted_frame.value().sequence;
    std::uint64_t previous_second_sequence = second_frame.value().sequence;
    for( std::uint32_t iteration = 0U; iteration < 1000U; ++iteration )
    {
        const std::int64_t brightness = static_cast< std::int64_t >(
            ( iteration % 21U ) * 5U );
        expect( first.setControl( { eCameraControl::Brightness,
            eCameraControlMode::Manual, brightness } ).succeeded(),
            "Repeated control write succeeds" );
        const CameraResult< sCameraControlValue > readback =
            first.getControl( eCameraControl::Brightness );
        expect( readback.succeeded() && readback.value().value == brightness,
            "Repeated control readback is stable" );
        const CameraResult< sCameraFrame > first_stress_frame = first.readFrame( 10U );
        const CameraResult< sCameraFrame > second_stress_frame = second.readFrame( 10U );
        expect( first_stress_frame.succeeded() && first_stress_frame.value().valid()
            && first_stress_frame.value().sequence > previous_first_sequence,
            "First stress session keeps producing owned ordered frames" );
        expect( second_stress_frame.succeeded() && second_stress_frame.value().valid()
            && second_stress_frame.value().sequence > previous_second_sequence,
            "Second stress session remains independent" );
        if( first_stress_frame.succeeded() )
            previous_first_sequence = first_stress_frame.value().sequence;
        if( second_stress_frame.succeeded() )
            previous_second_sequence = second_stress_frame.value().sequence;
        if( iteration % 100U == 99U )
        {
            expect( first.stop().succeeded() && first.start().succeeded(),
                "Stress session repeatedly stops and restarts" );
        }
    }

    first.disconnect();
    expect( first.readFrame( 10U ).error().code() == eCameraErrorCode::DeviceDisconnected,
        "Frame read reports a disconnected device" );
    expect( first.getControl( eCameraControl::Brightness ).error().code()
        == eCameraErrorCode::DeviceDisconnected, "Control read reports a disconnected device" );
    expect( first.setControl( { eCameraControl::Brightness,
        eCameraControlMode::Manual, 50 } ).error().code()
        == eCameraErrorCode::DeviceDisconnected, "Control write reports a disconnected device" );
    expect( first.getExtensionUnit( extension_selector ).error().code()
        == eCameraErrorCode::DeviceDisconnected,
        "Extension-unit read reports a disconnected device" );

    first.close();
    second.close();
    expect( !first.isOpen() && !second.isOpen(), "Both sessions close independently" );

    // Callback threadの一番危ない経路: Frame callbackの中からstopを呼ぶ。Sessionが自己Joinを
    // 避けてWorkerを遅延回収し、その後のstart／stopが普通に成立することまでを固定する.
    std::shared_ptr< MockCameraBackend > callback_backend =
        std::make_shared< MockCameraBackend >();
    expect( callback_backend->open( mockDescription( "mock-callback" ) ).succeeded(),
        "Callback mock opens" );
    CameraSession callback_session;
    detail::CameraSessionTestAccess::installBackend( &callback_session, callback_backend );
    std::atomic_bool callback_ran { false };
    std::atomic_bool callback_stop_succeeded { false };
    expect( callback_session.start( [&]( const CameraResult< sCameraFrame >& frame_in )
        {
            callback_ran.store( frame_in.succeeded() );
            callback_stop_succeeded.store( callback_session.stop().succeeded() );
        } ).succeeded(), "Owned callback stream starts" );
    const auto callback_deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds( 2 );
    while( !callback_stop_succeeded.load()
        && std::chrono::steady_clock::now() < callback_deadline )
        std::this_thread::yield();
    expect( callback_ran.load() && callback_stop_succeeded.load(),
        "Callback can request stop without self-join or normal-path detach" );
    expect( callback_session.stop().succeeded(),
        "Owner reaps the callback worker after callback-requested stop" );
    expect( callback_session.start().succeeded() && callback_session.stop().succeeded(),
        "Session restarts after the deferred callback join" );
    callback_session.close();

    // Cancellation waits for the bounded read before stopping the native backend.
    // Its result must not be delivered as an unsolicited final callback.
    auto blocking_backend = std::make_shared< MockCameraBackend >();
    expect( blocking_backend->open( mockDescription( "mock-stop-wakeup" ) ).succeeded(), "Blocking mock opens" );
    blocking_backend->blockReadUntilStop();
    CameraSession blocking_session;
    detail::CameraSessionTestAccess::installBackend( &blocking_session, blocking_backend );
    std::atomic_uint delivered{ 0U };
    expect( blocking_session.start( [ & ]( const CameraResult< sCameraFrame >& ) { ++delivered; } ).succeeded(),
        "Blocking callback capture starts" );
    expect( blocking_backend->waitUntilReading(), "Stop test observes an in-flight read" );
    expect( blocking_session.stop().succeeded(), "Stop waits for the read and joins the callback worker" );
    expect( delivered.load() == 0U, "Owner cancellation never becomes a terminal error callback" );
    blocking_session.close();

    // TMR-CALLBACK-03: timeouts are suppressed, a terminal result is delivered once,
    // and a throwing callback stops delivery without implicitly stopping the backend.
    // Capture destruction marks worker-function exit without racing a session method.
    for( const bool terminal_error : { true, false } )
    {
        auto backend = std::make_shared< MockCameraBackend >();
        expect( backend->open( mockDescription( "mock-delivery-end" ) ).succeeded(),
            "Delivery-end mock opens" );
        if( terminal_error ) backend->timeoutThenError();
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        auto capture = std::make_shared< int >( 1 );
        std::weak_ptr< int > lifetime = capture;
        std::atomic_uint callback_count{ 0U };
        std::atomic_bool expected_result{ false };
        expect( session.start( [ &, capture ]( const CameraResult< sCameraFrame >& result_in )
            {
                ++callback_count;
                expected_result = terminal_error
                    ? !result_in.succeeded() && result_in.error().code() == eCameraErrorCode::ReadFailed
                    : result_in.succeeded();
                if( !terminal_error ) throw std::runtime_error( "callback test" );
            } ).succeeded(), "Scripted callback stream starts" );
        capture.reset();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 2 );
        while( !lifetime.expired() && std::chrono::steady_clock::now() < deadline )
            std::this_thread::yield();
        expect( lifetime.expired(), "Worker releases its callable after delivery ends" );
        expect( callback_count.load() == 1U && expected_result.load(),
            "Only one terminal result or throwing frame callback is delivered" );
        if( terminal_error )
            expect( backend->scriptedReads() == 3U, "Two timeout results are skipped before one failure" );
        expect( session.isStreaming(), "Delivery termination does not itself stop the backend" );
        expect( session.stop().succeeded() && !session.isStreaming(),
            "Owner stops the backend and reaps the finished callback worker" );
        expect( session.start().succeeded() && session.stop().succeeded(),
            "Explicit stop permits restart after callback termination" );
        session.close();
    }
    return failures == 0 ? 0 : 1;
}
