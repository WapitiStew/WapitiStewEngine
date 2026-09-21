// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
// No evdev node is opened: the production descriptor owner uses the fake operations below.
#include "../../platform/iui/linux/device/LinuxKeyboardDevices.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>

namespace { long fail_after = -1; }
void* operator new(std::size_t size)
{
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) --fail_after;
    if (auto* value = std::malloc(size == 0 ? 1 : size)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
#define CHECK(condition) do { if (!(condition)) { fail_after = -1; std::cerr << "line " << __LINE__ << ": " #condition "\n"; std::abort(); } } while (false)
using wse::iui::KeyboardAccessState;
namespace detail = wse::iui::detail;
namespace
{
struct Operations
{
    struct Directory {} directory;
    struct Device
    {
        bool present = false, live = false, cec = false, keyboard = true;
        int open_error = 0, id_error = 0, capability_error = 0, key_error = 0, led_error = 0;
        int interruptions = 0, opens = 0, closes = 0;
        unsigned key = KEY_Q, led = LED_CAPSL;
    };
    std::array<Device, 4> devices{};
    int directory_error = 0, enumeration_error = 0, directory_opens = 0, directory_closes = 0;
    std::size_t next = 0;
    dirent entry{};
    Directory* openDirectory()
    {
        if (directory_error) { errno = directory_error; return nullptr; }
        ++directory_opens; next = 0; return &directory;
    }
    const dirent* nextEntry(Directory*)
    {
        while (next < devices.size() && !devices[next].present) ++next;
        if (next == devices.size()) { errno = enumeration_error; return nullptr; }
        std::snprintf(entry.d_name, sizeof(entry.d_name), "event%zu", next++);
        return &entry;
    }
    void closeDirectory(Directory*) noexcept { ++directory_closes; }
    int openDevice(const char* path)
    {
        const int index = std::atoi(std::strrchr(path, '/') + 6);
        auto& device = devices[index];
        if (device.open_error) { errno = device.open_error; return -1; }
        CHECK(!device.live); device.live = true; ++device.opens;
        return 10 + index;
    }
    void closeDevice(int fd) noexcept
    {
        auto& device = devices[fd - 10];
        CHECK(device.live); device.live = false; ++device.closes;
    }
    static void bit(void* output, unsigned index)
    {
        auto* words = static_cast<unsigned long*>(output);
        words[index / detail::KEYBOARD_BITS_PER_WORD] |= 1UL << (index % detail::KEYBOARD_BITS_PER_WORD);
    }
    int query(void* output, int fd, unsigned long request)
    {
        auto& device = devices[fd - 10];
        CHECK(device.live);
        if (device.interruptions > 0) { --device.interruptions; errno = EINTR; return -1; }
        int error = 0;
        if (request == EVIOCGID)
        {
            error = device.id_error;
            static_cast<input_id*>(output)->bustype = device.cec ? BUS_CEC : BUS_USB;
        }
        else if (request == EVIOCGBIT(EV_KEY, sizeof(detail::KeyboardBitWords<KEY_MAX + 1U>)))
        {
            error = device.capability_error;
            if (device.keyboard) for (const unsigned key : {KEY_A, KEY_Z, KEY_ENTER, KEY_SPACE}) bit(output, key);
        }
        else if (request == EVIOCGKEY(sizeof(detail::KeyboardBitWords<KEY_MAX + 1U>)))
        { error = device.key_error; bit(output, device.key); }
        else if (request == EVIOCGLED(sizeof(detail::KeyboardBitWords<LED_MAX + 1U>)))
        { error = device.led_error; bit(output, device.led); }
        else CHECK(false);
        if (error) { errno = error; return -1; }
        return 0;
    }
    void balanced() const
    {
        CHECK(directory_opens == directory_closes);
        for (const auto& device : devices) CHECK(!device.live && device.opens == device.closes);
    }
};
}
int main()
{
    using State = KeyboardAccessState;
    std::array<bool, KEY_MAX + 1U> keys{};
    std::array<bool, LED_MAX + 1U> leds{};
    Operations operations;
    {
        detail::LinuxKeyboardDevices<Operations> devices(operations);
        CHECK(devices.scan() == State::Unavailable);
        operations.directory_error = EACCES; CHECK(devices.scan() == State::PermissionDenied);
        operations.directory_error = ENOENT; CHECK(devices.scan() == State::Unavailable);
        operations.directory_error = 0;
        auto& first = operations.devices[0]; first.present = true;
        first.open_error = EPERM; CHECK(devices.scan() == State::PermissionDenied);
        first.open_error = 0; first.id_error = EACCES; CHECK(devices.scan() == State::PermissionDenied);
        first.id_error = 0; first.capability_error = EPERM; CHECK(devices.scan() == State::PermissionDenied);
        first.capability_error = ENODEV; CHECK(devices.scan() == State::Unavailable);
        first.capability_error = 0; first.cec = true; CHECK(devices.scan() == State::Unavailable);
        first.cec = false; first.keyboard = false; CHECK(devices.scan() == State::Unavailable);
        first.keyboard = true; first.interruptions = 1; CHECK(devices.scan() == State::Ready);
        first.interruptions = 1; CHECK(devices.capture(&keys, &leds) == State::Ready);
        CHECK(keys[KEY_Q] && leds[LED_CAPSL] && first.live);
        auto& second = operations.devices[1]; second.present = true; second.key = KEY_W; second.led = LED_NUML;
        second.open_error = EACCES; CHECK(devices.scan() == State::Ready);
        second.open_error = 0; CHECK(devices.scan() == State::Ready);
        CHECK(devices.capture(&keys, &leds) == State::Ready);
        CHECK(keys[KEY_Q] && keys[KEY_W] && leds[LED_CAPSL] && leds[LED_NUML]);
        second.key_error = EPERM; CHECK(devices.capture(&keys, &leds) == State::Ready);
        CHECK(keys[KEY_Q] && !keys[KEY_W] && !second.live);
        second.key_error = 0; CHECK(devices.scan() == State::Ready);
        first.key_error = ENODEV; CHECK(devices.capture(&keys, &leds) == State::Ready);
        CHECK(!first.live && !keys[KEY_Q] && keys[KEY_W]);
        second.key_error = EACCES; CHECK(devices.capture(&keys, &leds) == State::PermissionDenied);
        for (const bool key : keys) CHECK(!key);
        for (const bool led : leds) CHECK(!led);
        first.present = false; second.present = false; CHECK(devices.scan() == State::Unavailable);
        first.present = true; first.key_error = 0; CHECK(devices.scan() == State::Ready);
        first.led_error = EACCES; CHECK(devices.capture(&keys, &leds) == State::Ready);
        CHECK(keys[KEY_Q]); for (const bool led : leds) CHECK(!led);
        first.key_error = ENODEV; CHECK(devices.capture(&keys, &leds) == State::Disconnected);
        first.key_error = 0; first.led_error = 0; CHECK(devices.scan() == State::Ready);
        CHECK(devices.capture(&keys, &leds) == State::Ready && keys[KEY_Q]);
        second.present = true; second.key_error = EACCES; CHECK(devices.scan() == State::Ready);
        first.key_error = ENODEV; CHECK(devices.capture(&keys, &leds) == State::PermissionDenied);
        CHECK(!first.live && !second.live);
        operations.enumeration_error = EPERM; first.present = false;
        second.present = false;
        CHECK(devices.scan() == State::PermissionDenied);
    }
    operations.balanced();
    // Sweep every allocation made by real scan storage (path/vector), including
    // vector growth after open. Local directory/candidate owners and prior fds all close.
    int failures = 0;
    bool reached_success = false;
    for (long point = 0; point < 32; ++point)
    {
        Operations injected;
        for (auto& device : injected.devices) device.present = true;
        {
            detail::LinuxKeyboardDevices<Operations> devices(injected);
            fail_after = point;
            const auto state = devices.scan();
            fail_after = -1;
            if (state == State::Unavailable) { ++failures; injected.balanced(); }
            else { CHECK(state == State::Ready); reached_success = true; }
            CHECK(devices.scan() == State::Ready); // Recovery in the same owner.
        }
        injected.balanced();
        if (reached_success) break;
    }
    CHECK(failures >= 3 && reached_success);
    std::cout << "Access/hotplug/aggregation/RAII matrix passed; allocation points: " << failures << '\n';
}
