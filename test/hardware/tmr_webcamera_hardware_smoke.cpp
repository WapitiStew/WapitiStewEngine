// @file engine/wse/test/hardware/tmr_webcamera_hardware_smoke.cpp
// @brief Exercises a physical Linux USB web camera through the public WebCamera facade.
// @brief 実USB Cameraを要するOpt-in Smoke（LinuxかつWSE_ENABLE_CAMERA_HARDWARE_TESTSで登録）。
//        列挙→能力→Open→Control読み→Start→Frame→Stop→Closeを公開Facadeだけで一巡し、
//        変換無しのNative formatでFrameが届くことを確かめる。Profileは720p以下を小さい順に
//        先へ試す: 巨大なNative解像度で先に失敗して、動くはずのDeviceをFailに数えないため。
//        Cameraが居なければ77（SKIPPED）で退出し、失敗と区別される.

#include <tmr/device/WebCamera.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
constexpr int SKIPPED = 77;

void printFailure(
      const char* const stage_in
    , const wse::tmr::CameraError& error_in )
{
    std::cerr << stage_in
              << " failed; category=" << static_cast< int >( error_in.category() )
              << ", code=" << static_cast< int >( error_in.code() ) << '\n';
}

std::vector< wse::tmr::sCameraStreamProfile > orderedProfiles(
    const wse::tmr::sCameraCapability& capability_in )
{
    std::vector< wse::tmr::sCameraStreamProfile > profiles =
        capability_in.stream_profiles;
    std::stable_sort( profiles.begin(), profiles.end(),
        []( const wse::tmr::sCameraStreamProfile& left_in,
            const wse::tmr::sCameraStreamProfile& right_in )
        {
            const bool left_bounded = left_in.native_format.width <= 1280U
                && left_in.native_format.height <= 720U;
            const bool right_bounded = right_in.native_format.width <= 1280U
                && right_in.native_format.height <= 720U;
            if( left_bounded != right_bounded )
                return left_bounded;
            const std::uint64_t left_pixels =
                static_cast< std::uint64_t >( left_in.native_format.width )
                * left_in.native_format.height;
            const std::uint64_t right_pixels =
                static_cast< std::uint64_t >( right_in.native_format.width )
                * right_in.native_format.height;
            return left_pixels < right_pixels;
        } );
    return profiles;
}

bool tryProfile(
      const wse::tmr::sCameraDeviceInfo& device_in
    , const wse::tmr::sCameraCapability& capability_in
    , const wse::tmr::sCameraStreamProfile& profile_in )
{
    using namespace wse::tmr;
    if( !profile_in.valid()
        || !profile_in.supportsOutput( profile_in.native_format.pixel_format ) )
        return false;

    sCameraStreamConfiguration configuration;
    configuration.native_format = profile_in.native_format;
    configuration.output_format = profile_in.native_format.pixel_format;
    configuration.allow_conversion = false;

    WebCamera camera;
    CameraStatus result = camera.open( device_in, configuration );
    if( !result.succeeded() )
        return false;
    if( !camera.isOpen() || camera.isStreaming() )
    {
        std::cerr << "Open-state contract failed.\n";
        camera.close();
        return false;
    }

    std::size_t readable_controls = 0U;
    for( const sCameraControlCapability& control : capability_in.controls )
    {
        if( !control.readable )
            continue;
        const CameraResult< sCameraControlValue > value = camera.getControl( control.control );
        if( value.succeeded() )
            ++readable_controls;
    }

    result = camera.start();
    if( !result.succeeded() )
    {
        camera.close();
        return false;
    }
    const CameraResult< sCameraFrame > frame = camera.readFrame( 5000U );
    const CameraStatus stop_result = camera.stop();
    const bool stopped = stop_result.succeeded() && !camera.isStreaming() && camera.isOpen();
    camera.close();

    if( !frame.succeeded() )
    {
        printFailure( "Frame acquisition", frame.error() );
        return false;
    }
    if( !frame.value().valid() || !stopped || camera.isOpen() || camera.isStreaming() )
    {
        std::cerr << "Frame or terminal-state contract failed.\n";
        return false;
    }

    std::cout << "Physical WebCamera smoke passed; width="
              << frame.value().description.width
              << ", height=" << frame.value().description.height
              << ", pixel-format="
              << static_cast< int >( frame.value().description.pixel_format )
              << ", readable-controls=" << readable_controls << '\n';
    return true;
}
} // namespace

int main()
{
    using namespace wse::tmr;
    const CameraResult< std::vector< sCameraDeviceInfo > > devices =
        WebCamera::enumerate( eCameraBackend::Video4Linux2 );
    if( !devices.succeeded() )
    {
        printFailure( "WebCamera enumeration", devices.error() );
        return 1;
    }
    if( devices.value().empty() )
    {
        std::cout << "No USB web camera is available; hardware smoke skipped.\n";
        return SKIPPED;
    }

    std::size_t capability_count = 0U;
    for( const sCameraDeviceInfo& device : devices.value() )
    {
        const CameraResult< sCameraCapability > capability =
            WebCamera::capabilities( device );
        if( !capability.succeeded() )
            continue;
        ++capability_count;
        for( const sCameraStreamProfile& profile : orderedProfiles( capability.value() ) )
        {
            if( tryProfile( device, capability.value(), profile ) )
            {
                std::cout << "Privacy-filtered candidates=" << devices.value().size()
                          << ", capability-ready=" << capability_count << '\n';
                return 0;
            }
        }
    }

    std::cerr << "A connected USB web camera did not satisfy the public frame contract; "
              << "privacy-filtered candidates=" << devices.value().size()
              << ", capability-ready=" << capability_count << '\n';
    return 1;
}
