// @file engine/wse/test/characterization/tmr_windows_camera_smoke.cpp
// @brief Windows Cameraを列挙し、Frame取得と停止／再開を実機Characterizationする。
// @brief この契約が破れると、Windowsで実CameraをOpenするApplicationが次を失う.
//        1) 列挙されたDeviceが実際にOpenでき、BGRA8変換でFrameが届くこと.
//        2) 読めるControlが能力表と矛盾せず、変更→読み返し→復元の一巡が成立すること.
//        3) 停止→再開でFrameが再び流れること。WebCamera facadeの現行Lifecycleを含む.
//        --camera-nameは完全一致で対象を限定し、別CameraへのFallbackを禁止する.
//        Cameraが一台も繋がっていないMachineでは77（SKIPPED）で退出し、失敗とは区別される。
//        CMakeはWindowsかつWSE_ENABLE_CAMERA_HARDWARE_TESTSのときだけ本Testを登録する.

#include <tmr/camera/Camera.h>
#include <tmr/device/WebCamera.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace
{
constexpr int SKIPPED = 77;

class RestoreControl final
{
    wse::tmr::CameraSession& m_camera;
    wse::tmr::sCameraControlValue m_original;
    bool m_pending = true;
public:
    RestoreControl(wse::tmr::CameraSession& camera_inout,
        const wse::tmr::sCameraControlValue& original_in)
        : m_camera(camera_inout), m_original(original_in) {}
    ~RestoreControl() noexcept
    {
        if(m_pending) try { (void)m_camera.setControl(m_original); } catch(...) {}
    }
    bool restore()
    {
        if(!m_camera.setControl(m_original).succeeded()) return false;
        const auto readback = m_camera.getControl(m_original.control);
        if(!readback.succeeded() || readback.value().value != m_original.value
            || readback.value().mode != m_original.mode) return false;
        m_pending = false;
        return true;
    }
};

bool testControls(
      wse::tmr::CameraSession* const camera_in
    , const std::vector< wse::tmr::sCameraControlCapability >& controls_in )
{
    using namespace wse::tmr;
    if( controls_in.empty() )
    {
        std::cerr << "Camera exposes no portable controls.\n";
        return false;
    }

    std::size_t readable_controls = 0U;
    bool changed_and_restored = false;
    for( const sCameraControlCapability& capability : controls_in )
    {
        if( !capability.readable )
            continue;
        const CameraResult< sCameraControlValue > original =
            camera_in->getControl( capability.control );
        if( !original.succeeded() )
        {
            std::cerr << "Control read failed: " << original.error().message()
                      << " native=" << original.error().nativeCode() << '\n';
            return false;
        }
        if( original.value().value < capability.minimum
            || original.value().value > capability.maximum
            || ( original.value().mode == eCameraControlMode::Manual
                && !capability.supports_manual )
            || ( original.value().mode == eCameraControlMode::Automatic
                && !capability.supports_automatic ) )
        {
            std::cerr << "Control value is inconsistent with its capability.\n";
            return false;
        }
        ++readable_controls;

        if( !capability.writable )
            continue;
        CameraStatus result = camera_in->setControl( original.value() );
        if( !result.succeeded() )
        {
            std::cerr << "Idempotent control write failed: " << result.error().message()
                      << " native=" << result.error().nativeCode() << '\n';
            return false;
        }

        if( changed_and_restored || original.value().mode != eCameraControlMode::Manual
            || !capability.supports_manual || capability.minimum == capability.maximum )
            continue;
        const auto step = std::max<std::int64_t>(1, capability.step);
        sCameraControlValue changed = original.value();
        changed.value = original.value().value <= capability.maximum - step
            ? original.value().value + step : original.value().value - step;
        if( changed.value < capability.minimum || changed.value > capability.maximum )
            continue;
        RestoreControl restore(*camera_in, original.value());
        result = camera_in->setControl( changed );
        if( !result.succeeded() )
        {
            std::cerr << "Control change failed: " << result.error().message()
                      << " native=" << result.error().nativeCode() << '\n';
            return false;
        }
        const CameraResult< sCameraControlValue > changed_readback =
            camera_in->getControl( capability.control );
        const bool restored = restore.restore();
        if( !changed_readback.succeeded() || changed_readback.value().value != changed.value
            || changed_readback.value().mode != eCameraControlMode::Manual
            || !restored )
        {
            std::cerr << "Control readback or restoration failed.\n";
            return false;
        }
        std::cout << "CONTROL_RESTORED control=" << static_cast<int>(capability.control)
                  << " original=" << original.value().value << " changed=" << changed.value
                  << " restored=" << original.value().value << " readback=pass\n";
        changed_and_restored = true;
    }
    if( readable_controls == 0U || !changed_and_restored )
    {
        std::cerr << "Camera controls could not complete a change/readback/restore cycle; readable="
                  << readable_controls << ", changed_and_restored=" << changed_and_restored << '\n';
        return false;
    }
    std::cout << "Portable controls=" << readable_controls
              << ", change/readback/restore=pass\n";
    return true;
}

bool matchesBgraProfile( const wse::tmr::CameraResult< wse::tmr::sCameraFrame >& result_in,
    const wse::tmr::sCameraFormat& format_in )
{
    return result_in.succeeded() && result_in.value().valid()
        && result_in.value().description.pixel_format == wse::tmr::eCameraPixelFormat::Bgra8
        && result_in.value().description.width == format_in.width
        && result_in.value().description.height == format_in.height;
}

bool tryCamera(
      const wse::tmr::sCameraDeviceInfo& device_in
    , const wse::tmr::sCameraCapability& capability_in
    , const wse::tmr::sCameraStreamProfile& profile_in )
{
    using namespace wse::tmr;
    // Captured synchronization state outlives the session even on an early return.
    std::mutex callback_mutex;
    std::condition_variable callback_condition;
    bool callback_received = false;
    bool callback_valid = false;
    CameraSession camera;
    sCameraStreamConfiguration configuration;
    configuration.native_format = profile_in.native_format;
    configuration.output_format = eCameraPixelFormat::Bgra8;
    configuration.allow_conversion = true;
    CameraStatus result = camera.open( device_in, configuration );
    if( !result.succeeded() )
    {
        std::cerr << "Open failed for " << device_in.display_name << ' '
                  << profile_in.native_format.width << 'x' << profile_in.native_format.height << '@'
                  << profile_in.native_format.framesPerSecond() << ": " << result.error().message()
                  << " native=" << result.error().nativeCode() << '\n';
        return false;
    }
    const bool controls_verified = testControls( &camera, capability_in.controls );
    result = camera.start();
    if( !result.succeeded() )
    {
        std::cerr << "Start failed: " << result.error().message() << '\n';
        return false;
    }
    CameraResult< sCameraFrame > first_frame = camera.readFrame( 5000U );
    if( !matchesBgraProfile( first_frame, profile_in.native_format ) )
    {
        std::cerr << "First frame failed";
        if( !first_frame.succeeded() ) std::cerr << ": " << first_frame.error().message()
                  << " native=" << first_frame.error().nativeCode();
        std::cerr << '\n';
        return false;
    }
    result = camera.stop();
    if( !result.succeeded() || camera.isStreaming() )
    {
        std::cerr << "First stop failed";
        if( !result.succeeded() ) std::cerr << ": " << result.error().message();
        std::cerr << '\n';
        return false;
    }

    constexpr std::uint32_t STRESS_RESTART_COUNT = 3U;
    constexpr std::uint32_t STRESS_FRAMES_PER_RESTART = 30U;
    std::uint32_t stress_frames = 0U;
    for( std::uint32_t restart = 0U; restart < STRESS_RESTART_COUNT; ++restart )
    {
        result = camera.start();
        if( !result.succeeded() )
        {
            std::cerr << "Bounded stress restart failed: " << result.error().message() << '\n';
            return false;
        }
        for( std::uint32_t frame_index = 0U;
            frame_index < STRESS_FRAMES_PER_RESTART; ++frame_index )
        {
            const CameraResult< sCameraFrame > stress_frame = camera.readFrame( 5000U );
            if( !matchesBgraProfile( stress_frame, profile_in.native_format ) )
            {
                std::cerr << "Bounded stress frame failed";
                if( !stress_frame.succeeded() ) std::cerr << ": " << stress_frame.error().message();
                std::cerr << '\n';
                return false;
            }
            ++stress_frames;
            const auto stress_control = std::find_if( capability_in.controls.begin(),
                capability_in.controls.end(), []( const sCameraControlCapability& value_in )
                { return value_in.readable && value_in.writable; } );
            if( frame_index % 10U == 0U && stress_control != capability_in.controls.end() )
            {
                const CameraResult< sCameraControlValue > current =
                    camera.getControl( stress_control->control );
                if( !current.succeeded() || !camera.setControl( current.value() ).succeeded() )
                {
                    std::cerr << "Bounded stress control round-trip failed.\n";
                    return false;
                }
            }
        }
        result = camera.stop();
        if( !result.succeeded() )
        {
            std::cerr << "Bounded stress stop failed: " << result.error().message() << '\n';
            return false;
        }
    }
    result = camera.start();
    if( !result.succeeded() )
    {
        std::cerr << "Restart failed: " << result.error().message() << '\n';
        return false;
    }
    CameraResult< sCameraFrame > restarted_frame = camera.readFrame( 5000U );
    if( !matchesBgraProfile( restarted_frame, profile_in.native_format ) )
    {
        std::cerr << "Restarted frame failed";
        if( !restarted_frame.succeeded() ) std::cerr << ": " << restarted_frame.error().message();
        std::cerr << '\n';
        return false;
    }
    result = camera.stop();
    if( !result.succeeded() )
    {
        std::cerr << "Second stop failed: " << result.error().message() << '\n';
        return false;
    }

    result = camera.start( [&]( const CameraResult< sCameraFrame >& frame_in )
    {
        std::lock_guard< std::mutex > lock( callback_mutex );
        callback_received = true;
        callback_valid = matchesBgraProfile( frame_in, profile_in.native_format );
        callback_condition.notify_all();
    } );
    if( !result.succeeded() )
    {
        std::cerr << "Callback start failed: " << result.error().message() << '\n';
        return false;
    }
    bool callback_timed_out = false;
    {
        std::unique_lock< std::mutex > lock( callback_mutex );
        if( !callback_condition.wait_for( lock, std::chrono::seconds( 5 ),
            [&callback_received]() { return callback_received; } ) )
        {
            callback_timed_out = true;
        }
    }
    result = camera.stop();
    if( callback_timed_out )
    {
        camera.close(); // Join while callback captures still exist, outside callback_mutex.
        std::cerr << "Callback timed out\n";
        return false;
    }
    if( !result.succeeded() || !callback_valid )
    {
        std::cerr << "Callback stop/frame failed";
        if( !result.succeeded() ) std::cerr << ": " << result.error().message();
        std::cerr << '\n';
        return false;
    }

    for(unsigned cycle = 0; cycle < 5; ++cycle)
    {
        if(!camera.start().succeeded()) return false;
        const auto pending = camera.readFrame(1);
        if(!pending.succeeded() && pending.error().code() != eCameraErrorCode::TimedOut)
        {
            std::cerr << "Short read failed: " << pending.error().message() << '\n';
            return false;
        }
        const auto stopped = camera.stop();
        if(!stopped.succeeded())
        {
            std::cerr << "Pending-read stop failed: " << stopped.error().message() << '\n';
            return false;
        }
    }
    callback_received = false;
    callback_valid = false;
    result = camera.start([&](const CameraResult<sCameraFrame>& frame_in)
    {
        const bool valid = matchesBgraProfile(frame_in, profile_in.native_format);
        const auto stopped = camera.stop();
        std::lock_guard<std::mutex> lock(callback_mutex);
        callback_valid = valid && stopped.succeeded();
        callback_received = true;
        callback_condition.notify_all();
    });
    if(!result.succeeded()) return false;
    {
        std::unique_lock<std::mutex> lock(callback_mutex);
        if(!callback_condition.wait_for(lock, std::chrono::seconds(8),
            [&]() { return callback_received; }))
        {
            lock.unlock(); camera.close();
            std::cerr << "Callback self-stop timed out.\n";
            return false;
        }
    }
    if(!camera.stop().succeeded() || !callback_valid) return false;

    std::cout << "Camera: " << device_in.display_name
              << ", " << profile_in.native_format.width << 'x'
              << profile_in.native_format.height
              << " native=" << static_cast< int >( profile_in.native_format.pixel_format )
              << " -> BGRA8, frames=" << first_frame.value().sequence
              << '/' << restarted_frame.value().sequence
              << ", bounded-stress=" << stress_frames << '\n';
    std::thread finalizer([&]() { camera.close(); });
    finalizer.join();
    std::cout << "Pending-read restart=5 callback-self-stop=pass cross-thread-close=pass\n";

    WebCamera facade;
    result = facade.open( device_in, configuration );
    if( !result.succeeded() || !facade.currentCapabilities().succeeded() )
    {
        std::cerr << "WebCamera facade open/capability failed";
        if( !result.succeeded() ) std::cerr << ": " << result.error().message();
        std::cerr << '\n';
        return false;
    }
    result = facade.start();
    if( !result.succeeded() || !facade.isStreaming() )
    {
        std::cerr << "WebCamera start did not start the shared session.\n";
        return false;
    }
    const auto facade_first_frame = facade.readFrame( 5000U );
    if( !matchesBgraProfile( facade_first_frame, profile_in.native_format )
        || facade_first_frame.value().description.width != profile_in.native_format.width
        || facade_first_frame.value().description.height != profile_in.native_format.height )
    {
        std::cerr << "WebCamera owned-frame read failed.\n";
        return false;
    }
    result = facade.stop();
    if( !result.succeeded() || facade.isStreaming() || !facade.isOpen() )
    {
        std::cerr << "WebCamera stop did not return to Open.\n";
        return false;
    }
    result = facade.start();
    if( !result.succeeded() )
    {
        std::cerr << "WebCamera modern restart failed: " << result.error().message() << '\n';
        return false;
    }
    const CameraResult< sCameraFrame > facade_frame = facade.readFrame( 5000U );
    result = facade.stop();
    if( !matchesBgraProfile( facade_frame, profile_in.native_format ) || !result.succeeded() )
    {
        std::cerr << "WebCamera modern owned-frame path failed.\n";
        return false;
    }
    facade.close();
    std::cout << "WebCamera facade start/read/stop/restart/close=pass\n";
    std::cout << "CAMERA_FRAME_SMOKE_PASS controls_verified=" << controls_verified << '\n';
    return controls_verified;
}
}

int main( const int argument_count_in, const char* const* const pp_arguments_in )
{
    using namespace wse::tmr;
    std::string selected_name;
    if( argument_count_in == 3 && std::string( pp_arguments_in[ 1 ] ) == "--camera-name"
        && pp_arguments_in[ 2 ][ 0 ] != '\0' )
        selected_name = pp_arguments_in[ 2 ];
    else if( argument_count_in != 1 )
    {
        std::cerr << "Expected optional --camera-name EXACT_NAME.\n";
        return 2;
    }
    const CameraResult< std::vector< sCameraDeviceInfo > > devices =
        CameraSession::enumerate( eCameraBackend::MediaFoundation );
    if( !devices.succeeded() )
    {
        std::cerr << "Camera enumeration failed: " << devices.error().message() << '\n';
        return 1;
    }
    if( devices.value().empty() )
    {
        std::cout << "No Windows camera is connected; hardware smoke skipped.\n";
        return SKIPPED;
    }

    std::vector< sCameraDeviceInfo > ordered_devices = devices.value();
    if( !selected_name.empty() )
    {
        ordered_devices.erase( std::remove_if( ordered_devices.begin(), ordered_devices.end(),
            [&selected_name]( const sCameraDeviceInfo& device_in )
            { return device_in.display_name != selected_name; } ), ordered_devices.end() );
        if( ordered_devices.size() != 1U )
        {
            std::cerr << "Selected camera name must match exactly one connected device.\n";
            return 2;
        }
    }
    std::stable_sort( ordered_devices.begin(), ordered_devices.end(),
        []( const sCameraDeviceInfo& left_in, const sCameraDeviceInfo& right_in )
        {
            const bool left_is_c922 = left_in.display_name.find( "C922" ) != std::string::npos
                || left_in.display_name.find( "c922" ) != std::string::npos;
            const bool right_is_c922 = right_in.display_name.find( "C922" ) != std::string::npos
                || right_in.display_name.find( "c922" ) != std::string::npos;
            return left_is_c922 && !right_is_c922;
        } );
    for( const sCameraDeviceInfo& device : ordered_devices )
    {
        const CameraResult< sCameraCapability > capability = CameraSession::capabilities( device );
        if( !capability.succeeded() )
        {
            std::cerr << "Capability query failed for " << device.display_name << ": "
                      << capability.error().message() << " native="
                      << capability.error().nativeCode() << '\n';
            continue;
        }
        if( capability.value().stream_profiles.empty() )
        {
            std::cerr << "Camera advertises no native stream profiles.\n";
            continue;
        }
        for( const sCameraStreamProfile& profile : capability.value().stream_profiles )
        {
            if( !profile.valid() )
            {
                std::cerr << "Camera advertised an invalid native stream profile.\n";
                return 1;
            }
            std::cout << "Native profile " << profile.native_format.width << 'x'
                      << profile.native_format.height << '@'
                      << profile.native_format.framesPerSecond() << " bgra8="
                      << profile.supportsOutput( eCameraPixelFormat::Bgra8 ) << '\n';
            if( profile.supportsOutput( eCameraPixelFormat::Bgra8 )
                && tryCamera( device, capability.value(), profile ) )
                return 0;
        }
    }
    std::cerr << "Connected Windows cameras did not satisfy the portable frame contract.\n";
    return 1;
}
