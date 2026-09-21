//*****************************************************************************************************************
//! 
//! @file    wse_DevicePickup.cpp
//! @brief   \~japanese SetupAPIとHID APIで種別ごとのDevice一覧（Serial／Network／Monitor／入力機器）を集めるWindows実装.
//! @brief   \~english  Windows implementation that gathers per-kind device lists (serial, network, monitor,
//!                     input devices) through the Setup and HID APIs.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @details  
//!
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma warning(push)
#pragma warning(disable: 4244)  // ../
#pragma warning(disable: 4464)  // ../
#pragma warning(disable: 4514)  // ../
#pragma warning(disable: 4702)  // ../
#pragma warning(disable: 4820)  // ../
        #include <utility>
#include <stdexcept>
#include <windows.h>
        #include <initguid.h>
        #include <devguid.h>
        #include <regstr.h>
        #include <regex>
        #include <tchar.h>
        #include <setupapi.h>
        #include <devpkey.h>
        #pragma comment(lib, "setupapi.lib")
        #pragma comment(lib, "hid.lib")
        #pragma comment(lib, "User32.lib")
        #pragma comment(lib, "Gdi32.lib")
        #include <hidsdi.h>
        #include <iostream>

#include <string>
#include "Utf8.h"

#include "../../../../api/wse/utility/wse_DevicePickup.h"
#include "../../../../api/wse/utility/wse_Log.h"

namespace wse
{
    static std::string convert( const std::wstring& w_str_in )
    {
        return detail::toUtf8( w_str_in );
    }



    static HDEVINFO getDeviceInfo( GUID* p_guid_in,  const uint8_t flag_in )
    {
        
        // GUID for COM ports
        HDEVINFO hDevInfo = SetupDiGetClassDevs(
              p_guid_in  // クラス GUID: ポート（COM/LPT）
            , NULL
            , NULL
            , flag_in
        );

        if( hDevInfo == INVALID_HANDLE_VALUE )
        {
            throw std::runtime_error( "the device class enumeration failed" );
        }
        return hDevInfo;
    }

    
    static std::string getDeviceProperty( HDEVINFO hDevInfo_in, SP_DEVINFO_DATA* pDevInfoData_in, DWORD property_in )
    {
        char buffer[512] = {};
        if( SetupDiGetDeviceRegistryPropertyA(
              hDevInfo_in
            , pDevInfoData_in
            , property_in
            , NULL
            , (PBYTE)buffer
            , sizeof(buffer)
            , NULL
            )) 
        {
            return buffer;
        }
        return "";
    }

    static std::string getStableDeviceId(
          HDEVINFO hDevInfo_in
        , SP_DEVINFO_DATA* pDevInfoData_in )
    {
        DWORD required_size = 0;
        SetupDiGetDeviceInstanceIdA(
            hDevInfo_in, pDevInfoData_in, nullptr, 0U, &required_size );
        if( required_size == 0U )
        {
            return "";
        }
        std::vector<char> buffer( required_size, '\0' );
        if( !SetupDiGetDeviceInstanceIdA(
                hDevInfo_in, pDevInfoData_in, buffer.data(), required_size, nullptr ) )
        {
            return "";
        }
        return std::string( "windows-instance:" ) + buffer.data();
    }
    
    static std::string getDeviceProperty( HDEVINFO hDevInfo_in, SP_DEVINFO_DATA* pDevInfoData_in, std::string property_in )
    {
        // PortName（COM3など）→ レジストリから取得
        HKEY h_key = SetupDiOpenDevRegKey(
               hDevInfo_in
            ,  pDevInfoData_in
            ,  DICS_FLAG_GLOBAL
            ,  0
            , DIREG_DEV, KEY_READ
        );

        if (h_key == INVALID_HANDLE_VALUE) 
        {
            DDLog() << "Invalid";
            return "";
        }
        
        char port_name[256] = {};
        DWORD type = 0, size = sizeof(port_name);
        if ( RegQueryValueExA(h_key, property_in.c_str(), NULL, &type, (LPBYTE)port_name, &size) == ERROR_SUCCESS )
        {
            if (type == REG_SZ) 
            {
                RegCloseKey(h_key);
                return port_name;
            }
        }
        RegCloseKey(h_key);
        return "";
    }

    
    static std::wstring getHIDDeviceProperty( HDEVINFO hDevInfo_in, SP_DEVICE_INTERFACE_DATA* pDevInfoData_in, uint32_t property_in )
    {
        wchar_t buffer[256] = {};
        if( SetupDiGetDeviceRegistryProperty(
              hDevInfo_in
            , reinterpret_cast< SP_DEVINFO_DATA* >( pDevInfoData_in )
            , property_in
            , NULL
            , (PBYTE)buffer
            , sizeof(buffer)
            , NULL
            )) 
        {
            return buffer;
        }
        return L"";
    }


    static void fillAdapterDetails( std::vector<sNetworkInfo>* p_devices_out )
    {
        std::vector<sNetworkInfo> &devices = *p_devices_out;
        ( void )( devices );
/*
        ULONG outBufLen = 0;
        GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &outBufLen );
        std::vector<BYTE> buffer(outBufLen);
        IP_ADAPTER_ADDRESSES* pAddresses = reinterpret_cast< IP_ADAPTER_ADDRESSES* >( buffer.data() );

        if( GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR )
        {
            for (IP_ADAPTER_ADDRESSES* adapter = pAddresses; adapter != NULL; adapter = adapter->Next) 
            {
                std::string adapter_name = adapter->AdapterName;
                std::vector<std::string> ip4_list;
                std::vector<std::string> ip6_list;

                // IPアドレスの取得（複数）
                for( IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next )
                {
                    SOCKADDR* sa = unicast->Address.lpSockaddr;
                    char ip[INET6_ADDRSTRLEN] = {};
                    if (sa->sa_family == AF_INET)
                    {
                        sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(sa);
                        inet_ntop(AF_INET, &ipv4->sin_addr, ip, sizeof(ip));
                        ip4_list.emplace_back(ip);
                    } else if (sa->sa_family == AF_INET6) 
                    {
                        sockaddr_in6* ipv6 = reinterpret_cast<sockaddr_in6*>(sa);
                        inet_ntop(AF_INET6, &ipv6->sin6_addr, ip, sizeof(ip));
                        ip6_list.emplace_back(ip);
                    }
                }

                // MACアドレス
                char mac[18] = {};
                if (adapter->PhysicalAddressLength == 6) 
                {
                    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                        adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
                        adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
                        adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]);
                }

                // 接続タイプ
                std::string conn_type = "Unknown";
                if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD) {
                    conn_type = "Ethernet";
                } else if (adapter->IfType == IF_TYPE_IEEE80211) {
                    conn_type = "Wi-Fi";
                }

                for(auto& dev : devices) 
                {
                    if (dev.adapter_name.empty() || dev.adapter_name == adapter_name)
                    {
                        dev.adapter_name = adapter_name;
                        dev.ip4_addresses = ip4_list;
                        dev.ip6_addresses = ip6_list;
                        dev.mac_address = mac;
                        dev.connection_type = conn_type;
                        break;
                    }
                }
            }
        }
*/

    }

    /*
    static void GetDisplayDetailsAndConnectionType() 
    {
        DISPLAY_DEVICE dd;
        DEVMODE devmode;
        ZeroMemory(&dd, sizeof(dd));
        ZeroMemory(&devmode, sizeof(devmode));

        dd.cb = sizeof(dd);
        devmode.dmSize = sizeof(devmode);

        // 画面情報を取得
        for( int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i ) 
        {
            std::wcout << L"Display sDevice " << i + 1 << L": " << dd.DeviceName << std::endl;
        
            // ディスプレイの解像度、リフレッシュレート、色深度を取得
            if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &devmode)) 
            {
                std::wcout << L"  Resolution: " << devmode.dmPelsWidth << L" x " << devmode.dmPelsHeight << std::endl;
                std::wcout << L"  Refresh Rate: " << devmode.dmDisplayFrequency << L" Hz" << std::endl;
                std::wcout << L"  Color Depth: " << devmode.dmBitsPerPel << L" bits per pixel" << std::endl;
            }

            // SetupAPIを使用してデバイスの詳細情報（製造元、フレンドリーネーム、HardwareID）を取得
            HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVCLASS_MONITOR, NULL, NULL, DIGCF_PRESENT);
            if (deviceInfoSet == INVALID_HANDLE_VALUE) {
                DDLog() << "Failed to get device information set";
                return;
            }

            SP_DEVINFO_DATA deviceInfoData = {};
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            TCHAR deviceName[1024]  = {};
            TCHAR manufacturer[1024]= {};
            DWORD requiredSize = 0;

            // デバイス情報を列挙
            for (DWORD j = 0; SetupDiEnumDeviceInfo(deviceInfoSet, j, &deviceInfoData); ++j) {
                // フレンドリーネーム（デバイス名）の取得
                if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceName, sizeof(deviceName), NULL)) {
                    std::wcout << L"  sDevice Name: " << deviceName << std::endl;
                }

                // 製造元の取得
                if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_MFG, NULL, (PBYTE)manufacturer, sizeof(manufacturer), NULL)) {
                    std::wcout << L"  Manufacturer: " << manufacturer << std::endl;
                }

                // HardwareIDの取得
                SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL, NULL, 0, &requiredSize);
                if (requiredSize > 0) {
                    PBYTE hardwareID = new BYTE[requiredSize];
                    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL, hardwareID, requiredSize, NULL)) {
                        std::wcout << L"  HardwareID: ";
                        for (DWORD k = 0; k < requiredSize / sizeof(WCHAR); ++k) {
                            std::wcout << ((WCHAR*)hardwareID)[k] << L" ";
                        }
                        std::wcout << std::endl;
                    }
                    delete[] hardwareID;
                }
            }
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }
    }
    */

    struct sMonitorDesc
    {
        uint64_xy   position;
        uint64_size size;
        uint64_t  refresh_rate;
        uint64_t  color_depth;

        public : bool isEmpty() const
        {
            return ( size == uint64_size( 0, 0 ) ) &&
                   ( refresh_rate == 0 ) &&
                   ( color_depth  == 0 );
        }


        //! @brief Construct all members with explicit defaults.
        sMonitorDesc(
              const uint64_xy& position_in = uint64_xy( 0, 0 )
            , const uint64_size& size_in = uint64_size( 0, 0 )
            , uint64_t refresh_rate_in = 0
            , uint64_t color_depth_in = 0
        )
            : position     ( position_in )
            , size         ( size_in )
            , refresh_rate ( refresh_rate_in )
            , color_depth  ( color_depth_in )
        {
        }
    };
    static sMonitorDesc getMonitorDesc( const DISPLAY_DEVICE dd_in )
    {
        DEVMODE devmode;
        ZeroMemory(&devmode, sizeof(devmode));
        devmode.dmSize = sizeof(devmode);
        if (EnumDisplaySettings( dd_in.DeviceName, ENUM_CURRENT_SETTINGS, &devmode ) ) 
        {
            sMonitorDesc desc;
            desc.position     = uint64_xy( devmode.dmPosition.x, devmode.dmPosition.y );
            desc.size         = uint64_size( devmode.dmPelsWidth, devmode.dmPelsHeight );
            desc.refresh_rate = devmode.dmDisplayFrequency;
            desc.color_depth  = devmode.dmBitsPerPel;
            return desc;
        }
        else 
        {
            DDLog() << L"Not Found Monitor Detail for " << dd_in.DeviceName;
            return sMonitorDesc();
        }
    }



    struct sHIDInfo
    {
        HIDP_CAPS     capabilities;
        std::wstring  device_path;
        std::wstring  friendry_name;
        std::wstring  hardware_id;
        std::wstring  manufacturer;

        //! @brief Construct all members with explicit defaults.
        sHIDInfo(
              const HIDP_CAPS& capabilities_in = { }
            , const std::wstring& device_path_in = L""
            , const std::wstring& friendry_name_in = L""
            , const std::wstring& hardware_id_in = L""
            , const std::wstring& manufacturer_in = L""
        )
            : capabilities  ( capabilities_in )
            , device_path   ( device_path_in )
            , friendry_name ( friendry_name_in )
            , hardware_id   ( hardware_id_in )
            , manufacturer  ( manufacturer_in )
        {
        }
    };
    
    static std::vector< sHIDInfo > getHIDInfo( HDEVINFO hDevInfo_in, GUID* p_guid_in, std::wstring type_in )
    {
        
        SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
        deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        // デバイスインターフェースの列挙
        std::vector< sHIDInfo > result;
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo_in, NULL, p_guid_in, i, &deviceInterfaceData); i++)
        {
            DDLog() << "HID" << i;

            DWORD requiredSize = 0;
            // インターフェースの詳細情報を取得
            if( !SetupDiGetDeviceInterfaceDetail(hDevInfo_in, &deviceInterfaceData, NULL, 0, &requiredSize, NULL) )
            {
                continue;
            }

            std::vector< BYTE > buffer( requiredSize );
            SP_DEVICE_INTERFACE_DETAIL_DATA* deviceDetail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(buffer.data());

            deviceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            if( !SetupDiGetDeviceInterfaceDetail( hDevInfo_in, &deviceInterfaceData, deviceDetail, requiredSize, NULL, NULL) )
            {
                continue;
            }

            // デバイスパスに基づいてデバイスタイプを識別
            std::wstring devicePath = deviceDetail->DevicePath;
            if( devicePath.find( type_in ) == std::wstring::npos)
            {
                continue;
            }

            // HIDデバイスの情報を取得する
            HANDLE deviceHandle = CreateFile(deviceDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (deviceHandle == INVALID_HANDLE_VALUE)
            {
                CloseHandle(deviceHandle);
                continue;
            }

            HIDP_CAPS capabilities;
            PHIDP_PREPARSED_DATA preparsedData;
            if( !HidD_GetPreparsedData( deviceHandle, &preparsedData ) )
            {
                HidD_FreePreparsedData(preparsedData);
                CloseHandle(deviceHandle);
                continue;
            }
            // デバイスの能力を取得
            if( !( HidP_GetCaps(preparsedData, &capabilities) == HIDP_STATUS_SUCCESS ) )
            {
                HidD_FreePreparsedData(preparsedData);
                CloseHandle(deviceHandle);
                continue;
            }

            sHIDInfo info;
            info.capabilities  = capabilities;
            info.device_path   = devicePath;
            info.friendry_name = getHIDDeviceProperty( hDevInfo_in, &deviceInterfaceData, ( uint32_t )( SPDRP_FRIENDLYNAME ) );
            info.hardware_id   = getHIDDeviceProperty( hDevInfo_in, &deviceInterfaceData, ( uint32_t )( SPDRP_HARDWAREID   ) );
            info.manufacturer   = getHIDDeviceProperty( hDevInfo_in, &deviceInterfaceData, ( uint32_t )( SPDRP_MFG          ) );
            result.emplace_back( info );
            
            // ハンドルを閉じる
            HidD_FreePreparsedData(preparsedData);
            CloseHandle(deviceHandle);
        }

        return result;
    }
//
//    static bool isSameDeviceName( const DISPLAY_DEVICE& dd, const HDEVINFO& deviceInfoSet, SP_DEVINFO_DATA* p_deviceInfoData )
//    {
//        // デバイス名が一致するか確認
//        TCHAR deviceName[1024];
//        if( !SetupDiGetDeviceRegistryProperty(deviceInfoSet, p_deviceInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceName, sizeof(deviceName), NULL ) )
//        {
//            return false;
//        }
//        // DeviceNameが一致するか確認
//        if( _wcsicmp(dd.DeviceName, deviceName) != 0 )
//        {
//            return false;
//        }
//        return true;
//    }

    //!
    //! @brief シリアルデバイス一覧を取得する.
    //! 
    std::vector< sSerialInfo >pickupDeviceInfo::Serials()
    {
        GUID guid = GUID_DEVCLASS_PORTS;        // GUID for COM ports
        HDEVINFO hDevInfo = getDeviceInfo( &guid, DIGCF_PRESENT );

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        std::vector< sSerialInfo > result;
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) 
        {
            sSerialInfo info = {};
            info.friendly_name = getDeviceProperty( hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
            info.hardware_id   = getDeviceProperty( hDevInfo, &devInfoData, SPDRP_HARDWAREID);
            info.stable_id     = getStableDeviceId( hDevInfo, &devInfoData );
            info.manufacturer  = getDeviceProperty( hDevInfo, &devInfoData, SPDRP_MFG);
            info.port_name     = getDeviceProperty( hDevInfo, &devInfoData, "PortName" );
            result.emplace_back( info );
        }
        SetupDiDestroyDeviceInfoList( hDevInfo );
        return result;
    }

    //!
    //! @brief ネットワークデバイス一覧を取得する.
    //! 
    std::vector< sNetworkInfo >pickupDeviceInfo::Networks()
    {
        GUID guid = GUID_DEVCLASS_NET;        // GUID for COM ports
        HDEVINFO hDevInfo = getDeviceInfo( &guid, ( DIGCF_PRESENT | DIGCF_PROFILE ) );

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        std::vector<sNetworkInfo> result;
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) 
        {
            sNetworkInfo info;
            info.device_id       = getDeviceProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC);
            info.hardware_id     = getDeviceProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID);
            info.manufacturer    = getDeviceProperty(hDevInfo, &devInfoData, SPDRP_MFG);
            info.friendly_name   = getDeviceProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
            result.emplace_back(info);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
        fillAdapterDetails( &result );
        return result;
    }

    //!
    //! @brief モニターデバイス一覧を取得する.
    //! 
    std::vector< sMonitorInfo > pickupDeviceInfo::Monitors()
    {

        // HIDのGUIDを取得
        DISPLAY_DEVICE dd;
        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
        std::vector<sMonitorInfo> result;
        for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i) 
        {
            sMonitorInfo info;
            // ディスプレイの解像度、リフレッシュレート、色深度を取得
            const sMonitorDesc desc = getMonitorDesc(dd);
            if( desc.isEmpty() )
            {
                DDLog() << "Device is EMPTY";
                continue;
            }

            info.friendly_name  = convert( dd.DeviceName   );
            info.friendly_name2 = convert( dd.DeviceString );
            info.hardware_id    = convert( dd.DeviceID     );
            info.key            = convert( dd.DeviceKey    );
            info.position       = sint64_xy( desc.position );
            info.size           = sint64_size( desc.size   );
            info.color_depth    = desc.color_depth;
            info.refresh_rate   = desc.refresh_rate;

            result.emplace_back( info );

            //info.connection     = ;
        }
        return result;
    }

    //!
    //! @brief キーボードデバイス一覧を取得する.
    //! 
    std::vector< sKeyboardInfo > pickupDeviceInfo::Keyboards()
    {
        DDLog() << "Keyboards";
        GUID guid;
        HidD_GetHidGuid(&guid);
        HDEVINFO hDevInfo = getDeviceInfo( &guid, ( DIGCF_PRESENT ) );

        static const std::wstring TAG = L"HID\\KEYBOARD";
        const std::vector< sHIDInfo > hid_infos =  getHIDInfo( hDevInfo, &guid, TAG );

        std::vector< sKeyboardInfo > result;
        for( const sHIDInfo hid_info : hid_infos ) 
        {
            sKeyboardInfo info = {};

            info.friendly_name  = convert( hid_info.friendry_name );
            info.hardware_id    = convert( hid_info.hardware_id   );
            info.manufacturer   = convert( hid_info.manufacturer   );
            info.device_path    = convert( hid_info.device_path   );
            info.key_state      = hid_info.capabilities.InputReportByteLength;
            info.hot_key_stting = hid_info.capabilities.FeatureReportByteLength;
            info.key_num        = hid_info.capabilities.NumberLinkCollectionNodes;
            info.key_index      = hid_info.capabilities.NumberInputDataIndices;
            result.emplace_back( info );

        }
        SetupDiDestroyDeviceInfoList( hDevInfo );
        return result;
    }

    //!
    //! @brief マウスデバイス一覧を取得する.
    //! 
    std::vector< sMouseInfo > pickupDeviceInfo::Mouses()
    {
        GUID guid;
        HidD_GetHidGuid(&guid);
        HDEVINFO hDevInfo = getDeviceInfo( &guid, ( DIGCF_PRESENT ) );

        static const std::wstring TAG = L"HID\\MOUSE";
        const std::vector< sHIDInfo > hid_infos =  getHIDInfo( hDevInfo, &guid, TAG );

        std::vector< sMouseInfo > result;
        for( const sHIDInfo hid_info : hid_infos ) 
        {
            sMouseInfo info = {};

            info.friendly_name  = convert( hid_info.friendry_name );
            info.hardware_id    = convert( hid_info.hardware_id   );
            info.manufacturer   = convert( hid_info.manufacturer   );
            info.device_path    = convert( hid_info.device_path   );
            info.input_state      = hid_info.capabilities.InputReportByteLength;
            info.output_state     = hid_info.capabilities.OutputReportByteLength;
            info.internal_setting = hid_info.capabilities.FeatureReportByteLength;
            info.button_num       = hid_info.capabilities.NumberInputButtonCaps;
            info.scroll_position  = hid_info.capabilities.NumberInputValueCaps;
            info.wheel_position   = hid_info.capabilities.NumberInputDataIndices;

            result.emplace_back( info );

        }
        SetupDiDestroyDeviceInfoList( hDevInfo );
        return result;
    }

    //!
    //! @brief タッチパネルデバイス一覧を取得する.
    //! 
    std::vector< sTouchscreenInfo > pickupDeviceInfo::Touchscreens()
    {
        GUID guid;
        HidD_GetHidGuid(&guid);
        HDEVINFO hDevInfo = getDeviceInfo( &guid, ( DIGCF_PRESENT ) );

        static const std::wstring TAG = L"HID\\TOUCHSCREEN";
        const std::vector< sHIDInfo > hid_infos =  getHIDInfo( hDevInfo, &guid, TAG );

        std::vector< sTouchscreenInfo > result;
        for( const sHIDInfo hid_info : hid_infos ) 
        {
            sTouchscreenInfo info = {};

            info.friendly_name   = convert( hid_info.friendry_name );
            info.hardware_id     = convert( hid_info.hardware_id   );
            info.manufacturer    = convert( hid_info.manufacturer   );
            info.device_path     = convert( hid_info.device_path   );
            info.input_state     = hid_info.capabilities.InputReportByteLength;
            info.internal_setting= hid_info.capabilities.FeatureReportByteLength;
            info.input_num       = hid_info.capabilities.NumberInputButtonCaps;
            info.input_position  = hid_info.capabilities.NumberInputValueCaps;
            info.input_index     = hid_info.capabilities.NumberInputDataIndices;
            result.emplace_back( info );

        }
        SetupDiDestroyDeviceInfoList( hDevInfo );
        return result;
    }

    //!
    //! @brief ゲームパッドデバイス一覧を取得する.
    //! 
    std::vector< sGamepadInfo > pickupDeviceInfo::Gamepads()
    {
        GUID guid;
        HidD_GetHidGuid(&guid);
        HDEVINFO hDevInfo = getDeviceInfo( &guid, ( DIGCF_PRESENT ) );

        static const std::wstring TAG = L"HID\\GAMEPAD";
        const std::vector< sHIDInfo > hid_infos =  getHIDInfo( hDevInfo, &guid, TAG );

        std::vector< sGamepadInfo > result;
        for( const sHIDInfo hid_info : hid_infos ) 
        {
            sGamepadInfo info = {};
            info.friendly_name  = convert( hid_info.friendry_name );
            info.hardware_id    = convert( hid_info.hardware_id   );
            info.manufacturer   = convert( hid_info.manufacturer   );
            info.device_path    = convert( hid_info.device_path   );
            info.input_state      = hid_info.capabilities.InputReportByteLength;
            info.output_state     = hid_info.capabilities.OutputReportByteLength;
            info.internal_setting = hid_info.capabilities.FeatureReportByteLength;
            info.button_num       = hid_info.capabilities.NumberInputButtonCaps;
            info.stick_num        = hid_info.capabilities.NumberInputValueCaps;
            info.input_data       = hid_info.capabilities.NumberInputDataIndices;
            result.emplace_back( info );
        }
        SetupDiDestroyDeviceInfoList( hDevInfo );
        return result;
    }
    


};

#pragma warning(pop)
