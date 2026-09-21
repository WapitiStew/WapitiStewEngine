// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
// Private evdev adapter. The operation parameter supports deterministic, device-free tests.
#pragma once
#include "iui/device/Keyboard.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace wse::iui::detail
{
constexpr std::size_t KEYBOARD_BITS_PER_WORD = sizeof(unsigned long) * 8U;
template <std::size_t Count>
using KeyboardBitWords = std::array<unsigned long,
    (Count + KEYBOARD_BITS_PER_WORD - 1U) / KEYBOARD_BITS_PER_WORD>;
inline bool keyboardBitSet( const unsigned long* words_in, std::size_t bit_in )
{
    return (words_in[bit_in / KEYBOARD_BITS_PER_WORD] &
            (1UL << (bit_in % KEYBOARD_BITS_PER_WORD))) != 0UL;
}
inline bool keyboardPermissionDenied( int error_in ) noexcept
{
    return error_in == EACCES || error_in == EPERM;
}

struct NativeKeyboardOperations
{
    using Directory = DIR;
    Directory* openDirectory() { return ::opendir("/dev/input"); }
    const dirent* nextEntry( Directory* directory_inout ) { return ::readdir(directory_inout); }
    void closeDirectory( Directory* directory_inout ) noexcept { ::closedir(directory_inout); }
    int openDevice( const char* path_in ) { return ::open(path_in, O_RDONLY | O_NONBLOCK | O_CLOEXEC); }
    void closeDevice( int fd_in ) noexcept { ::close(fd_in); }
    int query( void* data_out, int fd_in, unsigned long request_in )
    {
        return ::ioctl(fd_in, request_in, data_out);
    }
};

template <typename Operations = NativeKeyboardOperations>
class LinuxKeyboardDevices final
{
    class Descriptor final
    {
    public:
        Descriptor( Operations& operations_inout, int fd_in ) noexcept
            : operations(&operations_inout), fd(fd_in) {}
        Descriptor( const Descriptor& ) = delete;
        Descriptor& operator=( const Descriptor& ) = delete;
        Descriptor( Descriptor&& other_inout ) noexcept
            : operations(other_inout.operations), fd(std::exchange(other_inout.fd, -1)) {}
        Descriptor& operator=( Descriptor&& other_inout ) noexcept
        {
            if ( this != &other_inout )
            {
                reset(); operations = other_inout.operations;
                fd = std::exchange(other_inout.fd, -1);
            }
            return *this;
        }
        ~Descriptor() { reset(); }
        int get() const noexcept { return fd; }
    private:
        void reset() noexcept { if (fd >= 0) operations->closeDevice(std::exchange(fd, -1)); }
        Operations* operations;
        int fd;
    };
    struct DirectoryCloser
    {
        Operations* operations;
        void operator()( typename Operations::Directory* directory_inout ) const noexcept
        {
            operations->closeDirectory(directory_inout);
        }
    };
public:
    explicit LinuxKeyboardDevices( Operations& operations_inout ) noexcept : operations(operations_inout) {}
    LinuxKeyboardDevices( const LinuxKeyboardDevices& ) = delete;
    LinuxKeyboardDevices& operator=( const LinuxKeyboardDevices& ) = delete;

    KeyboardAccessState scan()
    try
    {
        descriptors.clear();
        std::unique_ptr<typename Operations::Directory, DirectoryCloser> directory(
            operations.openDirectory(), DirectoryCloser{&operations});
        if ( !directory ) return unavailable(errno);
        bool denied = false;
        for (;;)
        {
            errno = 0;
            const auto* entry = operations.nextEntry(directory.get());
            if ( !entry )
            {
                denied = denied || keyboardPermissionDenied(errno);
                break;
            }
            if ( std::strncmp(entry->d_name, "event", 5U) != 0 ) continue;
            const std::string path = std::string("/dev/input/") + entry->d_name;
            const int fd = operations.openDevice(path.c_str());
            if (fd < 0) { denied = denied || keyboardPermissionDenied(errno); continue; }
            Descriptor candidate(operations, fd);
            input_id id{};
            KeyboardBitWords<KEY_MAX + 1U> keys{};
            if (query(&id, fd, EVIOCGID) < 0)
            { denied = denied || keyboardPermissionDenied(errno); continue; }
            if (id.bustype == BUS_CEC) continue;
            if (query(keys.data(), fd, EVIOCGBIT(EV_KEY, sizeof(keys))) < 0)
            { denied = denied || keyboardPermissionDenied(errno); continue; }
            if (!keyboardBitSet(keys.data(), KEY_A) || !keyboardBitSet(keys.data(), KEY_Z) ||
                !keyboardBitSet(keys.data(), KEY_ENTER) || !keyboardBitSet(keys.data(), KEY_SPACE)) continue;
            descriptors.emplace_back(std::move(candidate));
        }
        if (!descriptors.empty()) return KeyboardAccessState::Ready;
        return denied ? KeyboardAccessState::PermissionDenied : KeyboardAccessState::Unavailable;
    }
    catch (const std::bad_alloc&)
    {
        // Directory and candidate guards have unwound. Keep the worker alive for a later rescan.
        descriptors.clear();
        return KeyboardAccessState::Unavailable;
    }

    KeyboardAccessState capture(
        std::array<bool, KEY_MAX + 1U>* p_keys_out,
        std::array<bool, LED_MAX + 1U>* p_leds_out )
    {
        auto& keys_out = *p_keys_out;
        auto& leds_out = *p_leds_out;
        keys_out.fill(false); leds_out.fill(false);
        bool captured = false;
        bool denied = false;
        auto iterator = descriptors.begin();
        while (iterator != descriptors.end())
        {
            KeyboardBitWords<KEY_MAX + 1U> keys{};
            if (query(keys.data(), iterator->get(), EVIOCGKEY(sizeof(keys))) < 0)
            {
                denied = denied || keyboardPermissionDenied(errno);
                iterator = descriptors.erase(iterator);
                continue;
            }
            captured = true;
            for (std::size_t key = 0; key < keys_out.size(); ++key)
                keys_out[key] = keys_out[key] || keyboardBitSet(keys.data(), key);
            KeyboardBitWords<LED_MAX + 1U> leds{};
            if (query(leds.data(), iterator->get(), EVIOCGLED(sizeof(leds))) >= 0)
                for (std::size_t led = 0; led < leds_out.size(); ++led)
                    leds_out[led] = leds_out[led] || keyboardBitSet(leds.data(), led);
            ++iterator;
        }
        if (captured) return KeyboardAccessState::Ready;
        return denied ? KeyboardAccessState::PermissionDenied : KeyboardAccessState::Disconnected;
    }
private:
    int query( void* data_out, int fd_in, unsigned long request_in )
    {
        int result;
        do { result = operations.query(data_out, fd_in, request_in); }
        while (result < 0 && errno == EINTR);
        return result;
    }
    static KeyboardAccessState unavailable( int error_in ) noexcept
    {
        return keyboardPermissionDenied(error_in)
            ? KeyboardAccessState::PermissionDenied : KeyboardAccessState::Unavailable;
    }
    Operations& operations;
    std::vector<Descriptor> descriptors;
};
} // namespace wse::iui::detail
