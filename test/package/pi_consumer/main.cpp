// @file engine/wse/test/package/pi_consumer/main.cpp
// @brief Raspberry Pi向けPackageをTargetの上でLink・実行できることを確かめる最小Consumer。
// @brief この検査が破れると、Pi 4のARM64 Packageを使うProjectが次を失う.
//        1) Cross buildされたPackageのHeaderとLibraryがTargetで揃っていること.
//        2) 各Componentの代表APIが実行時に呼べること（共有LibraryのLoadを含む）.
//        Hardwareは前提にしない: Keyboardは「読めるDeviceが無い」状態も正、Cameraは
//        開かずにNotOpenの構造化Errorだけを見る。検査するComponentはWSE_CONSUMER_*の
//        Compile定義が決め、Exit codeはComponent別に分かれる.

#include <wse/stew.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#if defined(WSE_CONSUMER_XPT)
#include <xpt/stew.h>
#endif

#if defined(WSE_CONSUMER_GEF)
#include <gef/stew.h>
#endif

#if defined(WSE_CONSUMER_IUI)
#include <iui/stew.h>
#endif

#if defined(WSE_CONSUMER_TMR)
#include <tmr/stew.h>
#endif

int main()
{
    const wse::double_xy point(10.0, 50.0);
    if(point.x != 10.0 || point.y != 50.0)
    {
        return 1;
    }

#if defined(WSE_CONSUMER_XPT)
    const wse::xpt::Endpoint endpoint("127.0.0.1", 1U);
    if(!endpoint.isValid() || endpoint.host() != "127.0.0.1" || endpoint.port() != 1U)
    {
        return 5;
    }
    wse::xpt::SerialPort serial;
    const auto invalid_open = serial.open(
        "", 9600, wse::xpt::OperationContext(wse::xpt::Timeout::milliseconds(100)));
    if(invalid_open.succeeded() ||
        invalid_open.error().code() != wse::xpt::eTransportErrorCode::InvalidArgument)
    {
        return 6;
    }
#endif

#if defined(WSE_CONSUMER_GEF)
    wse::gef::BINController binary;
    const std::vector<std::uint8_t> expected{1U, 2U, 3U};
    binary.setContents(expected);
    if(binary.index_num() != 1U ||
        binary.contents<std::uint8_t>(0U) != expected)
    {
        return 2;
    }
#endif

#if defined(WSE_CONSUMER_IUI)
    wse::iui::Keyboard keyboard;
    for(int attempt = 0;
        attempt < 100 &&
            keyboard.accessState() == wse::iui::KeyboardAccessState::Starting;
        ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(keyboard.isAvailable() !=
        (keyboard.accessState() == wse::iui::KeyboardAccessState::Ready))
    {
        return 3;
    }
    (void)keyboard.snapshot();
#endif

#if defined(WSE_CONSUMER_TMR)
    wse::tmr::WebCamera camera;
    const auto start_result = camera.start();
    if(camera.isOpen() || camera.isStreaming() || start_result.succeeded() ||
        start_result.error().code() != wse::tmr::eCameraErrorCode::NotOpen)
    {
        return 4;
    }
#endif

    return 0;
}
