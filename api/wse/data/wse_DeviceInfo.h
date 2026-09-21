//*****************************************************************************************************************
//! 
//! @file    wse_DeviceInfo.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese デバイス情報（シリアル、ネットワーク、モニターなど）を格納する構造体を定義するヘッダファイル.
//!     \~english  Header defining structs that store device information (serial, network, monitor, etc.).
//!
//!
//! @details
//!     \~japanese
//!         本ファイルでは、接続された各種デバイス（シリアル、ネットワーク、モニター、入力デバイス等）の情報を格納する
//!      @n 構造体群を定義する。構造体は外部ライブラリやOSから取得した情報をラップして使用可能な形式に整形する。
//!
//!     \~english
//!         This file defines structs to hold information about connected devices, such as serial, network,
//!      @n monitors, and input devices. These structs wrap OS or external library data into accessible formats.
//!
//!
//! @note
//!     \~japanese 各構造体は取得データ格納用の単純なデータホルダであり、メソッドは持たない。
//!     \~english  Each struct is a plain data holder with no member methods.
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

#ifndef WONDERSTEWENGINE_DATA_DEVICE_INFO_H
#define WONDERSTEWENGINE_DATA_DEVICE_INFO_H


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#endif
// include files
#include <utility>
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "../depend/wse_Typedef.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "wse_Point2D.h"
#include "wse_Size.h"

// External Library




namespace wse
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)
#endif


    //!
    //! @struct sSerialInfo
    //!
    //! @brief
    //!     \~japanese シリアルデバイスの情報を保持する構造体.
    //!     \~english  Struct to hold serial device information.
    //!
    //! @details
    //!     \~japanese デバイス名、ハードウェアID、ポート情報などのメタ情報を含む.
    //!     \~english  Includes metadata like device name, hardware ID, and port information.
    //!
    struct WSE_API sSerialInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.           \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.             \~japanese ハードウェアID.
        public : std::string        stable_id; //!< \~english Opaque persistent OS ID; empty when unavailable. \~japanese OSが提供する永続ID。取得不能時は空.
        public : std::string        manufacturer; //!< \~english Manufacturer name.       \~japanese 製造元.
        public : std::string        port_name; //!< \~english Port name.               \~japanese ポート番号.
        public : std::string        driver; //!< \~english Driver.                  \~japanese ドライバー.
        public : eSerialDevice      device_type; //!< \~english Device type. \~japanese デバイスタイプ.
        public : eSerialState       state; //!< \~english Device state. \~japanese 状態.

        //! @brief Construct all members with explicit defaults.
        sSerialInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& stable_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& port_name_in = ""
            , const std::string& driver_in = ""
            , eSerialDevice device_type_in = eSerialDevice::Unknown
            , eSerialState state_in = eSerialState ::Unknown
        )
            : friendly_name ( friendly_name_in )
            , hardware_id   ( hardware_id_in )
            , stable_id     ( stable_id_in )
            , manufacturer  ( manufacturer_in )
            , port_name     ( port_name_in )
            , driver        ( driver_in )
            , device_type   ( device_type_in )
            , state         ( state_in )
        {
        }
    };

    //!
    //! @struct sNetworkInfo
    //!
    //! @brief
    //!     \~japanese ネットワークインターフェース情報を保持する構造体.
    //!     \~english  Struct to hold network interface information.
    //!
    //! @details
    //!     \~japanese IPv4/IPv6アドレス、MACアドレス、接続種別などを含む.
    //!     \~english  Contains IPv4/IPv6 addresses, MAC address, and connection type.
    //!
    struct WSE_API sNetworkInfo
    {
        public : std::string              friendly_name;  //!< \~english Friendly name.         \~japanese フレンドリーネーム.
        public : std::string              hardware_id;  //!< \~english Hardware ID.           \~japanese ハードウェアID.
        public : std::string              manufacturer;  //!< \~english Manufacturer.          \~japanese 製造元.
        public : std::string              device_id;  //!< \~english Device ID.             \~japanese デバイスID.
        public : std::string              adapter_name;  //!< \~english Adapter name.          \~japanese アダプター名称.
        public : std::vector<std::string> ip4_addresses;  //!< \~english IPv4 addresses. \~japanese IP4アドレス.
        public : std::vector<std::string> ip6_addresses;  //!< \~english IPv6 addresses. \~japanese IP6アドレス.
        public : std::string              mac_address;  //!< \~english MAC address.           \~japanese MACアドレス.
        public : eNetworkConnection       connection;  //!< \~english Connection type. \~japanese Ethernet / Wi-Fi.

        //! @brief Construct all members with explicit defaults.
        sNetworkInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& device_id_in = ""
            , const std::string& adapter_name_in = ""
            , const std::vector<std::string>& ip4_addresses_in = std::vector<std::string>()
            , const std::vector<std::string>& ip6_addresses_in = std::vector<std::string>()
            , const std::string& mac_address_in = ""
            , eNetworkConnection connection_in = eNetworkConnection::Unknown
        )
            : friendly_name ( friendly_name_in )
            , hardware_id   ( hardware_id_in )
            , manufacturer  ( manufacturer_in )
            , device_id     ( device_id_in )
            , adapter_name  ( adapter_name_in )
            , ip4_addresses ( ip4_addresses_in )
            , ip6_addresses ( ip6_addresses_in )
            , mac_address   ( mac_address_in )
            , connection    ( connection_in )
        {
        }
    };

    //!
    //! @struct sMonitorInfo
    //!
    //! @brief
    //!     \~japanese モニター情報を保持する構造体.
    //!     \~english  Struct to hold monitor information.
    //!
    //! @details
    //!     \~japanese モニター名、位置、サイズ、リフレッシュレート、色深度などを含む.
    //!     \~english  Includes monitor name, position, size, refresh rate, and color depth.
    //!
    struct WSE_API sMonitorInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.       \~japanese フレンドリーネーム.
        public : std::string        friendly_name2; //!< \~english Secondary name.      \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.         \~japanese ハードウェアID.
        public : std::string        key; //!< \~english Registry key.        \~japanese キー.
        public : sint64_xy          position; //!< \~english Monitor position.  \~japanese 位置.
        public : sint64_size        size; //!< \~english Monitor size.      \~japanese サイズ.
        public : uint16_t           color_depth;  //!< \~english Color depth (bits).   \~japanese 色深度.
        public : uint16_t           refresh_rate;  //!< \~english Refresh rate (Hz).    \~japanese リフレッシュレート.
        public : bool               is_primary; //!< \~english Primary monitor flag. \~japanese プライマリモニタかどうか.

        //! @brief Construct all members with explicit defaults.
        sMonitorInfo(
              const std::string& friendly_name_in = ""
            , const std::string& friendly_name2_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& key_in = ""
            , const sint64_xy& position_in = sint64_xy()
            , const sint64_size& size_in = sint64_size()
            , uint16_t color_depth_in = 0
            , uint16_t refresh_rate_in = 0
            , bool is_primary_in = false
        )
            : friendly_name  ( friendly_name_in )
            , friendly_name2 ( friendly_name2_in )
            , hardware_id    ( hardware_id_in )
            , key            ( key_in )
            , position       ( position_in )
            , size           ( size_in )
            , color_depth    ( color_depth_in )
            , refresh_rate   ( refresh_rate_in )
            , is_primary     ( is_primary_in )
        {
        }
    };

    //!
    //! @struct sKeyboardInfo
    //!
    //! @brief
    //!     \~japanese キーボードデバイスの情報を保持する構造体.
    //!     \~english  Struct to hold keyboard device information.
    //!
    //! @details
    //!     \~japanese 入力状態、ホットキー設定、キー数などの情報を含む.
    //!     \~english  Contains input state, hotkey settings, and number of keys.
    //!
    struct WSE_API sKeyboardInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.       \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.         \~japanese ハードウェアID.
        public : std::string        manufacturer; //!< \~english Manufacturer.        \~japanese 製造元.
        public : std::string        device_path; //!< \~english Device path.         \~japanese デバイスパス.
        public : uint16_t           key_state;  //!< \~english Current key state.   \~japanese キー状態.
        public : uint16_t           hot_key_stting;  //!< \~english Hotkey setting.      \~japanese ホットキー設定.
        public : uint16_t           key_num;  //!< \~english Number of keys.      \~japanese キー数.
        public : uint16_t           key_index;  //!< \~english Key index.           \~japanese キーインデックス.

        //! @brief Construct all members with explicit defaults.
        sKeyboardInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& device_path_in = ""
            , uint16_t key_state_in = 0
            , uint16_t hot_key_stting_in = 0
            , uint16_t key_num_in = 0
            , uint16_t key_index_in = 0
        )
            : friendly_name  ( friendly_name_in )
            , hardware_id    ( hardware_id_in )
            , manufacturer   ( manufacturer_in )
            , device_path    ( device_path_in )
            , key_state      ( key_state_in )
            , hot_key_stting ( hot_key_stting_in )
            , key_num        ( key_num_in )
            , key_index      ( key_index_in )
        {
        }
    };

    //!
    //! @struct sMouseInfo
    //!
    //! @brief
    //!     \~japanese マウスデバイスの情報を保持する構造体.
    //!     \~english  Struct to hold mouse device information.
    //!
    //! @details
    //!     \~japanese ボタン数、スクロール位置、入出力状態などの情報を含む.
    //!     \~english  Contains button count, scroll position, and I/O state.
    //!
    struct WSE_API sMouseInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.       \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.         \~japanese ハードウェアID.
        public : std::string        manufacturer; //!< \~english Manufacturer.        \~japanese 製造元.
        public : std::string        device_path; //!< \~english Device path.         \~japanese デバイスパス.
        public : uint16_t           input_state;  //!< \~english Input state.         \~japanese 入力状態.
        public : uint16_t           output_state;  //!< \~english Output state.        \~japanese 出力状態.
        public : uint16_t           internal_setting;  //!< \~english Internal setting.     \~japanese 内部設定.
        public : uint16_t           button_num;  //!< \~english Number of buttons.    \~japanese ボタン数.
        public : uint16_t           scroll_position;  //!< \~english Scroll position.      \~japanese スクロール位置.
        public : uint16_t           wheel_position;  //!< \~english Wheel position.       \~japanese ホイール位置.

        //! @brief Construct all members with explicit defaults.
        sMouseInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& device_path_in = ""
            , uint16_t input_state_in = 0
            , uint16_t output_state_in = 0
            , uint16_t internal_setting_in = 0
            , uint16_t button_num_in = 0
            , uint16_t scroll_position_in = 0
            , uint16_t wheel_position_in = 0
        )
            : friendly_name    ( friendly_name_in )
            , hardware_id      ( hardware_id_in )
            , manufacturer     ( manufacturer_in )
            , device_path      ( device_path_in )
            , input_state      ( input_state_in )
            , output_state     ( output_state_in )
            , internal_setting ( internal_setting_in )
            , button_num       ( button_num_in )
            , scroll_position  ( scroll_position_in )
            , wheel_position   ( wheel_position_in )
        {
        }
    };

    //!
    //! @struct sTouchscreenInfo
    //!
    //! @brief
    //!     \~japanese タッチスクリーンデバイスの情報を保持する構造体.
    //!     \~english  Struct to hold touchscreen device information.
    //!
    //! @details
    //!     \~japanese 入力状態、ポジション、インデックスなどの情報を含む.
    //!     \~english  Includes input state, position, and index information.
    //!
    struct WSE_API sTouchscreenInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.         \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.           \~japanese ハードウェアID.
        public : std::string        manufacturer; //!< \~english Manufacturer.          \~japanese 製造元.
        public : std::string        device_path; //!< \~english Device path.           \~japanese デバイスパス.
        public : uint16_t           input_state;  //!< \~english Input state.           \~japanese 入力状態.
        public : uint16_t           internal_setting;  //!< \~english Internal setting.      \~japanese 内部設定.
        public : uint16_t           input_num;  //!< \~english Input count.           \~japanese 入力数.
        public : uint16_t           input_position;  //!< \~english Input position.        \~japanese 入力位置.
        public : uint16_t           input_index;  //!< \~english Input index.           \~japanese 入力インデックス.

        //! @brief Construct all members with explicit defaults.
        sTouchscreenInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& device_path_in = ""
            , uint16_t input_state_in = 0
            , uint16_t internal_setting_in = 0
            , uint16_t input_num_in = 0
            , uint16_t input_position_in = 0
            , uint16_t input_index_in = 0
        )
            : friendly_name    ( friendly_name_in )
            , hardware_id      ( hardware_id_in )
            , manufacturer     ( manufacturer_in )
            , device_path      ( device_path_in )
            , input_state      ( input_state_in )
            , internal_setting ( internal_setting_in )
            , input_num        ( input_num_in )
            , input_position   ( input_position_in )
            , input_index      ( input_index_in )
        {
        }
    };

    //!
    //! @struct sGamepadInfo
    //!
    //! @brief
    //!     \~japanese ゲームパッドの情報を保持する構造体.
    //!     \~english  Struct to hold gamepad device information.
    //!
    //! @details
    //!     \~japanese ボタン数、スティック数、入出力状態などの情報を含む.
    //!     \~english  Contains button/stick count, I/O states, and other metadata.
    //!
    struct WSE_API sGamepadInfo
    {
        public : std::string        friendly_name; //!< \~english Friendly name.         \~japanese フレンドリーネーム.
        public : std::string        hardware_id; //!< \~english Hardware ID.           \~japanese ハードウェアID.
        public : std::string        manufacturer; //!< \~english Manufacturer.          \~japanese 製造元.
        public : std::string        device_path; //!< \~english Device path.           \~japanese デバイスパス.
        public : uint16_t           input_state;  //!< \~english Input state.           \~japanese 入力状態.
        public : uint16_t           output_state;  //!< \~english Output state.          \~japanese 出力状態.
        public : uint16_t           internal_setting;  //!< \~english Internal setting.      \~japanese 内部設定.
        public : uint16_t           button_num;  //!< \~english Number of buttons.      \~japanese ボタン数.
        public : uint16_t           stick_num;  //!< \~english Number of sticks.       \~japanese スティック数.
        public : uint16_t           input_data;  //!< \~english Input data.             \~japanese 入力データ.

        //! @brief Construct all members with explicit defaults.
        sGamepadInfo(
              const std::string& friendly_name_in = ""
            , const std::string& hardware_id_in = ""
            , const std::string& manufacturer_in = ""
            , const std::string& device_path_in = ""
            , uint16_t input_state_in = 0
            , uint16_t output_state_in = 0
            , uint16_t internal_setting_in = 0
            , uint16_t button_num_in = 0
            , uint16_t stick_num_in = 0
            , uint16_t input_data_in = 0
        )
            : friendly_name    ( friendly_name_in )
            , hardware_id      ( hardware_id_in )
            , manufacturer     ( manufacturer_in )
            , device_path      ( device_path_in )
            , input_state      ( input_state_in )
            , output_state     ( output_state_in )
            , internal_setting ( internal_setting_in )
            , button_num       ( button_num_in )
            , stick_num        ( stick_num_in )
            , input_data       ( input_data_in )
        {
        }
    };

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
};


#endif //WONDERSTEWENGINE_DATA_COLOR_H
