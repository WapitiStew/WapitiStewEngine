//*****************************************************************************************************************
//!
//! @file    wse_DevicePickup.cpp
//! @brief   \~japanese Linuxの汎用Network／Serial列挙Adapter.
//! @brief   \~english  Generic Linux network and serial enumeration adapter.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Add explicit unsupported behavior for the Linux Core gate.
//!   Aug-27, 2026   Add generic network and serial enumeration for Linux and Raspberry Pi.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include <utility>
#include <stdexcept>
#include "../../../../api/wse/utility/wse_DevicePickup.h"

#include <algorithm>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <map>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <set>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;

    std::string readFirstLine(const fs::path& path_in)
    {
        std::ifstream stream(path_in);
        std::string value;
        if (stream && std::getline(stream, value))
        {
            return value;
        }
        return std::string();
    }

    std::string canonicalPath(const fs::path& path_in)
    {
        std::error_code error;
        const fs::path resolved = fs::weakly_canonical(path_in, error);
        return error ? path_in.string() : resolved.string();
    }

    std::string readAncestorFile(fs::path path_in, const char* fileName_in)
    {
        for (int depth = 0; depth < 6 && !path_in.empty(); ++depth)
        {
            const std::string value = readFirstLine(path_in / fileName_in);
            if (!value.empty())
            {
                return value;
            }
            path_in = path_in.parent_path();
        }
        return std::string();
    }

    std::string linkFileName(const fs::path& path_in)
    {
        std::error_code error;
        const fs::path target = fs::read_symlink(path_in, error);
        return error ? std::string() : target.filename().string();
    }

    std::string stableSerialId(const fs::path& portPath_in)
    {
        std::error_code error;
        const fs::path byIdRoot("/dev/serial/by-id");
        if (!fs::is_directory(byIdRoot, error))
        {
            return std::string();
        }

        const std::string canonicalPort = canonicalPath(portPath_in);
        std::vector<fs::path> entries;
        for (fs::directory_iterator iterator(
                 byIdRoot,
                 fs::directory_options::skip_permission_denied,
                 error);
             !error && iterator != fs::directory_iterator();
             iterator.increment(error))
        {
            entries.push_back(iterator->path());
        }
        std::sort(entries.begin(), entries.end());
        for (const fs::path& entry : entries)
        {
            if (canonicalPath(entry) == canonicalPort)
            {
                return std::string("linux-by-id:") + entry.filename().string();
            }
        }
        return std::string();
    }

    bool startsWith(const std::string& value_in, const char* prefix_in)
    {
        return value_in.rfind(prefix_in, 0) == 0;
    }

    bool isSupportedSerialName(const std::string& name_in)
    {
        return startsWith(name_in, "ttyUSB")
            || startsWith(name_in, "ttyACM")
            || startsWith(name_in, "ttyAMA")
            || startsWith(name_in, "ttyS")
            || startsWith(name_in, "ttySC")
            || startsWith(name_in, "ttyXRUSB")
            || startsWith(name_in, "rfcomm");
    }

    wse::eSerialDevice serialDeviceType(const std::string& name_in)
    {
        if (startsWith(name_in, "ttyUSB") || startsWith(name_in, "ttyACM")
                || startsWith(name_in, "ttyXRUSB"))
        {
            return wse::eSerialDevice::USBtoSerial;
        }
        if (startsWith(name_in, "ttyAMA") || startsWith(name_in, "ttyS")
                || startsWith(name_in, "ttySC"))
        {
            return wse::eSerialDevice::UART;
        }
        return wse::eSerialDevice::Unknown;
    }

    std::string formatMacAddress(const sockaddr_ll& address_in)
    {
        if (address_in.sll_halen != 6)
        {
            return std::string();
        }

        bool allZero = true;
        std::ostringstream text;
        text << std::hex << std::setfill('0');
        for (int index = 0; index < 6; ++index)
        {
            const unsigned int octet = address_in.sll_addr[index];
            allZero = allZero && octet == 0;
            if (index != 0)
            {
                text << ':';
            }
            text << std::setw(2) << octet;
        }
        return allZero ? std::string() : text.str();
    }

    wse::eNetworkConnection networkConnection(
        const std::string& name_in,
        const int hardwareType_in)
    {
        const fs::path sysPath = fs::path("/sys/class/net") / name_in;
        std::error_code error;
        if (startsWith(name_in, "tun") || startsWith(name_in, "tap")
                || startsWith(name_in, "wg") || startsWith(name_in, "ppp"))
        {
            return wse::eNetworkConnection::VPN;
        }
        if (fs::exists(sysPath / "wireless", error))
        {
            // The legacy enum has no generation-neutral Wi-Fi value. Do not guess
            // Wi-Fi 4/5/6 from the Linux interface alone.
            return wse::eNetworkConnection::Unknown;
        }

        const std::string resolved = canonicalPath(sysPath);
        if (name_in == "lo" || resolved.find("/virtual/") != std::string::npos)
        {
            return wse::eNetworkConnection::VirtualEthernet;
        }
        if (hardwareType_in == ARPHRD_ETHER)
        {
            return wse::eNetworkConnection::Ethernet;
        }
        return wse::eNetworkConnection::Unknown;
    }

    struct NetworkEntry
    {
        wse::sNetworkInfo info;
        int hardwareType;
        std::set<std::string> ipv4;
        std::set<std::string> ipv6;

        //! @brief Construct all members with explicit defaults.
        NetworkEntry(
              const wse::sNetworkInfo& info_in = {}
            , int hardwareType_in = 0
            , const std::set<std::string>& ipv4_in = {}
            , const std::set<std::string>& ipv6_in = {}
        )
            : info         ( info_in )
            , hardwareType ( hardwareType_in )
            , ipv4         ( ipv4_in )
            , ipv6         ( ipv6_in )
        {
        }
    };
}

namespace wse
{
    std::vector< sSerialInfo > pickupDeviceInfo::Serials()
    {
        std::vector<sSerialInfo> result;
        std::error_code error;
        const fs::path ttyRoot("/sys/class/tty");
        if (!fs::is_directory(ttyRoot, error))
        {
            return result;
        }

        std::vector<fs::path> entries;
        for (fs::directory_iterator iterator(
                 ttyRoot,
                 fs::directory_options::skip_permission_denied,
                 error);
             !error && iterator != fs::directory_iterator();
             iterator.increment(error))
        {
            entries.push_back(iterator->path());
        }
        std::sort(entries.begin(), entries.end());

        for (const fs::path& entry : entries)
        {
            const std::string name = entry.filename().string();
            if (!isSupportedSerialName(name))
            {
                continue;
            }

            const fs::path portPath = fs::path("/dev") / name;
            if (!fs::exists(portPath, error))
            {
                continue;
            }

            const fs::path devicePath = entry / "device";
            sSerialInfo info;
            info.friendly_name = readAncestorFile(devicePath, "product");
            if (info.friendly_name.empty())
            {
                info.friendly_name = name;
            }
            info.hardware_id = canonicalPath(devicePath);
            info.stable_id = stableSerialId(portPath);
            info.manufacturer = readAncestorFile(devicePath, "manufacturer");
            info.port_name = portPath.string();
            info.driver = linkFileName(devicePath / "driver");
            info.device_type = serialDeviceType(name);
            info.state = eSerialState::Connected;
            result.emplace_back(std::move(info));
        }
        return result;
    }

    std::vector< sNetworkInfo > pickupDeviceInfo::Networks()
    {
        ifaddrs* addresses = nullptr;
        if (getifaddrs(&addresses) != 0)
        {
            throw std::runtime_error( "network interface enumeration failed" );
        }

        std::map<std::string, NetworkEntry> entries;
        for (const ifaddrs* current = addresses; current != nullptr; current = current->ifa_next)
        {
            if (current->ifa_name == nullptr || current->ifa_addr == nullptr)
            {
                continue;
            }

            const std::string name(current->ifa_name);
            NetworkEntry& entry = entries[name];
            entry.info.friendly_name = name;
            entry.info.adapter_name = name;
            entry.info.device_id = name;
            entry.info.hardware_id = canonicalPath(fs::path("/sys/class/net") / name);

            char addressText[INET6_ADDRSTRLEN] = {};
            const int family = current->ifa_addr->sa_family;
            if (family == AF_INET)
            {
                const sockaddr_in* address = reinterpret_cast<const sockaddr_in*>(current->ifa_addr);
                if (inet_ntop(AF_INET, &address->sin_addr, addressText, sizeof(addressText)) != nullptr)
                {
                    entry.ipv4.emplace(addressText);
                }
            }
            else if (family == AF_INET6)
            {
                const sockaddr_in6* address = reinterpret_cast<const sockaddr_in6*>(current->ifa_addr);
                if (inet_ntop(AF_INET6, &address->sin6_addr, addressText, sizeof(addressText)) != nullptr)
                {
                    entry.ipv6.emplace(addressText);
                }
            }
            else if (family == AF_PACKET)
            {
                const sockaddr_ll* address = reinterpret_cast<const sockaddr_ll*>(current->ifa_addr);
                entry.hardwareType = address->sll_hatype;
                entry.info.mac_address = formatMacAddress(*address);
            }
        }
        freeifaddrs(addresses);

        std::vector<sNetworkInfo> result;
        result.reserve(entries.size());
        for (auto& pair : entries)
        {
            NetworkEntry& entry = pair.second;
            entry.info.ip4_addresses.assign(entry.ipv4.begin(), entry.ipv4.end());
            entry.info.ip6_addresses.assign(entry.ipv6.begin(), entry.ipv6.end());
            entry.info.connection = networkConnection(pair.first, entry.hardwareType);
            entry.info.manufacturer = readAncestorFile(
                fs::path("/sys/class/net") / pair.first / "device",
                "manufacturer");
            result.emplace_back(std::move(entry.info));
        }
        return result;
    }

    std::vector< sMonitorInfo > pickupDeviceInfo::Monitors()
    {
        throw std::logic_error( "this device enumeration is not implemented on Linux" );
    }

    std::vector< sKeyboardInfo > pickupDeviceInfo::Keyboards()
    {
        throw std::logic_error( "this device enumeration is not implemented on Linux" );
    }

    std::vector< sMouseInfo > pickupDeviceInfo::Mouses()
    {
        throw std::logic_error( "this device enumeration is not implemented on Linux" );
    }

    std::vector< sTouchscreenInfo > pickupDeviceInfo::Touchscreens()
    {
        throw std::logic_error( "this device enumeration is not implemented on Linux" );
    }

    std::vector< sGamepadInfo > pickupDeviceInfo::Gamepads()
    {
        throw std::logic_error( "this device enumeration is not implemented on Linux" );
    }
}
