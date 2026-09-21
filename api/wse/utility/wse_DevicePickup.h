//*****************************************************************************************************************
//! 
//! @file    wse_DevicePickup.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 接続されている各種デバイスの情報を取得するユーティリティ。
//!     \~english  Utility for retrieving connected device information.
//!
//! @details
//!     \~japanese
//!         本ヘッダは、シリアルデバイス、ネットワークデバイス、モニタ、キーボード、マウス、タッチパネル、
//!         ゲームパッドなどのハードウェアに関する一覧取得インタフェースを提供する。
//!
//!     \~english
//!         This header provides interfaces to retrieve lists of various connected hardware devices such as
//!         serial, network, monitor, keyboard, mouse, touchscreen, and gamepad.
//!
//! @note
//!     \~japanese それぞれの関数は構造体ベースで情報を返す。
//!     \~english  Each function returns device information using predefined structures.
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
#pragma once

#ifndef WONDERSTEWENGINE_UTILITY_DEVICE_PICKUP_H
#define WONDERSTEWENGINE_UTILITY_DEVICE_PICKUP_H


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#endif
// include files
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "../depend/wse_Typedef.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "../data/wse_DeviceInfo.h"

// External Library

namespace wse
{

    //!
    //! @class pickupDeviceInfo
    //!
    //! @brief
    //!     \~japanese 各種デバイス情報を取得する静的ユーティリティクラス。
    //!     \~english  Static utility class to retrieve various device information.
    //!
    //! @details
    //!     \~japanese
    //!         システムに接続されているハードウェアのうち、シリアル、ネットワーク、モニター、
    //!         入力デバイス（キーボード・マウス・タッチパネル・ゲームパッド）に関する情報を
    //!         静的関数で一覧取得できる。
    //!
    //!     \~english
    //!         Provides static functions to retrieve hardware information from the system,
    //!         including serial devices, network interfaces, monitors, and input devices
    //!         (keyboard, mouse, touchscreen, gamepad).
    //!
    class WSE_API pickupDeviceInfo
    {
        //!
        //! @brief
        //!     \~japanese シリアルデバイス一覧を取得する。
        //!     \~english  Retrieve list of serial devices.
        //!
        //! @return List of sSerialInfo.
        //!
        public : static std::vector< sSerialInfo > Serials();

        //!
        //! @brief
        //!     \~japanese ネットワークデバイス一覧を取得する。
        //!     \~english  Retrieve list of network devices.
        //!
        //! @return List of sNetworkInfo.
        //!
        public : static std::vector< sNetworkInfo > Networks();

        //!
        //! @brief
        //!     \~japanese モニターデバイス一覧を取得する。
        //!     \~english  Retrieve list of monitor devices.
        //!
        //! @return List of sMonitorInfo.
        //!
        public : static std::vector< sMonitorInfo > Monitors();

        //!
        //! @brief
        //!     \~japanese キーボードデバイス一覧を取得する。
        //!     \~english  Retrieve list of keyboard devices.
        //!
        //! @return List of sKeyboardInfo.
        //!
        public : static std::vector< sKeyboardInfo > Keyboards();

        //!
        //! @brief
        //!     \~japanese マウスデバイス一覧を取得する。
        //!     \~english  Retrieve list of mouse devices.
        //!
        //! @return List of sMouseInfo.
        //!
        public : static std::vector< sMouseInfo > Mouses();

        //!
        //! @brief
        //!     \~japanese タッチパネルデバイス一覧を取得する。
        //!     \~english  Retrieve list of touchscreen devices.
        //!
        //! @return List of sTouchscreenInfo.
        //!
        public : static std::vector< sTouchscreenInfo > Touchscreens();

        //!
        //! @brief
        //!     \~japanese ゲームパッドデバイス一覧を取得する。
        //!     \~english  Retrieve list of gamepad devices.
        //!
        //! @return List of sGamepadInfo.
        //!
        public : static std::vector< sGamepadInfo > Gamepads();
    };
};


#endif // WONDERSTEWENGINE_UTILITY_DEVICE_PICKUP_H
