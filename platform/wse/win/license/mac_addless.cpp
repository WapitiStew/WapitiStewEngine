//*****************************************************************************************************************
//! 
//! @file    mac_addless.cpp
//! @brief   \~japanese Windows IP Helper APIでLicense識別用のMAC Addressを取得する.
//! @brief   \~english  Reads the licence-identification MAC address through the Windows IP Helper API.
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
    #pragma warning(disable: 4464)  // 
#include "../../../../api/wse/utility/wse_Log.h"
#pragma warning(pop)

#include "mac_addless.h"

#pragma warning(push)
#pragma warning(disable: 101)  // 警告番号 102 を無効化
#pragma warning(disable: 102)  // 警告番号 102 を無効化
#pragma warning(disable: 4242)
#pragma warning(disable: 4244)  // ../
#pragma warning(disable: 4365)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
#pragma warning(disable: 4464)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
#pragma warning(disable: 4668)  // ../
#pragma warning(disable: 4820)  // ../
#pragma warning(disable: 5039)  // ../
#pragma warning(disable: 5204)  // ../
        #include <windows.h>
        #include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

namespace wse
{
namespace license
{
    std::string mac_address( void )
    {
        // 複数のアダプタ情報を保持するためのバッファを用意
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD bufLen = sizeof(adapterInfo);

        // アダプタ情報の取得
        DWORD status = GetAdaptersInfo(adapterInfo, &bufLen);
        if (status != ERROR_SUCCESS) {
            return "";
        }

        // 最初に見つかった有効なアダプタのMACアドレスを取得
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        while (pAdapterInfo) {
            if (pAdapterInfo->AddressLength == 6) {
                char macStr[18];
                std::snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                              pAdapterInfo->Address[0], pAdapterInfo->Address[1],
                              pAdapterInfo->Address[2], pAdapterInfo->Address[3],
                              pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
                return std::string(macStr);
            }
            pAdapterInfo = pAdapterInfo->Next;
        }
        return "";
    }


}
}
#pragma warning(pop)
