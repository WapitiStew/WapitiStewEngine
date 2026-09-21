//*****************************************************************************************************************
//! 
//! @file    wse_Enum.h
//! @brief   公開する列挙子をまとめて記載する.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @brief
//!     \~japanese エラーコードやフォーマットなど、共通列挙体の定義。
//!     \~english  Common enum definitions such as error codes and formats.
//!
//!
//! @details
//!     \~japanese ライブラリ全体で使用されるエラーコード、ピクセルフォーマット、通信形式などの定義を提供します。
//!     \~english  Provides definitions for error codes, pixel formats, and communication types used throughout the library.
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
#ifndef WONDERSTEWENGINE_DEPEND_ENUMCLASS_H
#define WONDERSTEWENGINE_DEPEND_ENUMCLASS_H

#include "wse_STD.h"

namespace wse
{

    //!
    //! @brief
    //!     \~japanese Channel 0がBであるColor Formatを示すBit。
    //!     \~english  Bit that marks a colour format whose channel zero is blue.
    //!
    //! @details
    //!     \~japanese
    //!         Channel数とBit深度を表す既存値は0x1000未満に収まる。このBitを立てた値は、
    //!      @n 同じChannel数とBit深度でChannel順序だけが異なるFormatを表す。
    //!     \~english
    //!         Every value that carries a channel count and a bit depth stays below 0x1000, so this
    //!         bit marks a format with the same channel count and bit depth but a different order.
    //!
    constexpr uint16_t COLOR_ORDER_BGR_FLAG = 0x1000U;

    //!
    //! @enum ePixFormat
    //! @brief
    //!     \~japanese ピクセルフォーマット定義。
    //!     \~english  Pixel format definitions.
    //!
    //! @details
    //!  CHxDyy
    //!   x = Channel Num
    //!   y = Bit depth
    //! @n
    //!     \~japanese
    //!         `CHxDyy`はChannel数とBit深度だけを表し、Channel順序はRGBとみなす。
    //!      @n Channel 0がBであるFormatは`BGR`／`BGRA`の名前を持ち、対応する`CHxDyy`に
    //!      @n `COLOR_ORDER_BGR_FLAG`を立てた値になる。
    //!     \~english
    //!         `CHxDyy` carries only a channel count and a bit depth, and means RGB order. A format
    //!         whose channel zero is blue is named `BGR`/`BGRA` and equals its `CHxDyy` counterpart
    //!         with `COLOR_ORDER_BGR_FLAG` set.
    //!
    enum class ePixFormat : uint16_t
    {
            CH1D8   =   0
        ,   CH1D10  =   1
        ,   CH1D12  =   2
        ,   CH1D14  =   3
        ,   CH1D16  =   4
        ,   CH1D32  =   5
        ,   CH1D64  =   6
        ,   CH1D08   =  CH1D8

        ,   CH2D8   =  10
        ,   CH2D10  =  11
        ,   CH2D12  =  12
        ,   CH2D14  =  13
        ,   CH2D16  =  14
        ,   CH2D32  =  15
        ,   CH2D64  =  16
        ,   CH2D08   =  CH2D8

        ,   CH3D8   =  20
        ,   CH3D10  =  21
        ,   CH3D12  =  22
        ,   CH3D14  =  23
        ,   CH3D16  =  24
        ,   CH3D32  =  25
        ,   CH3D64  =  26
        ,   CH3D08   =  CH3D8

        ,   CH4D8   =  30
        ,   CH4D10  =  31
        ,   CH4D12  =  32
        ,   CH4D14  =  33
        ,   CH4D16  =  34
        ,   CH4D32  =  35
        ,   CH4D64  =  36
        ,   CH4D08   =  CH4D8

        // \~japanese Channel順序を明示する別名。値は対応するCHxDyyと同一である。
        // \~english  Explicit channel-order aliases. Each equals its CHxDyy counterpart.
        ,   RGB3D8   =  CH3D8    //!< \~japanese 8bit RGB.       \~english Eight-bit RGB.
        ,   RGB3D16  =  CH3D16   //!< \~japanese 16bit RGB.      \~english Sixteen-bit RGB.
        ,   RGBA4D8  =  CH4D8    //!< \~japanese 8bit RGBA.      \~english Eight-bit RGBA.
        ,   RGBA4D16 =  CH4D16   //!< \~japanese 16bit RGBA.     \~english Sixteen-bit RGBA.
        ,   RGB3D08  =  RGB3D8
        ,   RGBA4D08 =  RGBA4D8

        // \~japanese Channel 0がBであるFormat。Channel数とBit深度は対応するCHxDyyと等しい。
        // \~english  Formats whose channel zero is blue; the channel count and bit depth are shared.
        ,   BGR3D8   =  CH3D8  | COLOR_ORDER_BGR_FLAG   //!< \~japanese 8bit BGR.   \~english Eight-bit BGR.
        ,   BGR3D16  =  CH3D16 | COLOR_ORDER_BGR_FLAG   //!< \~japanese 16bit BGR.  \~english Sixteen-bit BGR.
        ,   BGRA4D8  =  CH4D8  | COLOR_ORDER_BGR_FLAG   //!< \~japanese 8bit BGRA.  \~english Eight-bit BGRA.
        ,   BGRA4D16 =  CH4D16 | COLOR_ORDER_BGR_FLAG   //!< \~japanese 16bit BGRA. \~english Sixteen-bit BGRA.
        ,   BGR3D08  =  BGR3D8
        ,   BGRA4D08 =  BGRA4D8
    };

    //!
    //! @enum eColorChannelOrder
    //! @brief
    //!     \~japanese Color画像のChannel順序。
    //!     \~english  Channel order of a colour image.
    //!
    enum class eColorChannelOrder : uint8_t
    {
          Rgb = 0U  //!< \~japanese Channel 0がR.  \~english Channel zero is red.
        , Bgr       //!< \~japanese Channel 0がB.  \~english Channel zero is blue.
    };


    //! 
    //! @enum eDOR
    //! @brief
    //!     \~japanese 回転軸の定義。
    //!     \~english  Direction of rotation.
    //!
    enum class eDOR : uint8_t
    {
            Roll   = 0          //!< ロール回転.
        ,   Pitch  = 1          //!< ピッチ回転.
        ,   Yow    = 2          //!< ヨー回転.

        ,   X_Axis = Pitch      //!< X軸周り.
        ,   Y_Axis = Yow        //!< Y軸周り.
        ,   Z_Axis = Roll       //!< Z軸周り.
    };
    

    //! 
    //! @enum eSerialDevice
    //! @brief
    //!     \~japanese シリアル通信規格の定義。
    //!     \~english  Serial communication standard types.
    //!
    enum class eSerialDevice : uint8_t
    {
            Unknown     = 0
        ,   RS232       = 1    //!< RS232C.
        ,   RS422       = 2    //!< RS422.
        ,   RS485       = 3    //!< RS485.
        ,   USBtoSerial = 4    //!< USB.
        ,   UART        = 5    //!< UART.
        ,   TTL         = 6    //!< TTL.
        ,   IrDA        = 7    //!< IrDA.
    };
    
    //! 
    //! @enum eSerialState
    //! @brief
    //!     \~japanese シリアル通信状態の定義。
    //!     \~english  Serial communication states.
    //!
    enum class eSerialState : uint8_t
{
        Unknown         = 0    //!< \~japanese 不明な状態   \~english Unknown state.
    ,   Connected       = 1    //!< \~japanese 接続中       \~english Device is connected and communication is available.
    ,   Disconnected    = 2    //!< \~japanese 切断         \~english Device is disconnected or not connected.
    ,   Idle            = 3    //!< \~japanese 待機中       \~english Device is idle, no data transmission occurring.
    ,   Transmitting    = 4    //!< \~japanese 送信中       \~english Device is transmitting data.
    ,   Receiving       = 5    //!< \~japanese 受信中       \~english Device is receiving data.
    ,   Error           = 6    //!< \~japanese エラー       \~english Communication or hardware error occurred.
    ,   Timeout         = 6    //!< \~japanese タイムアウト \~english Timeout occurred during communication.
    ,   Ready           = 6    //!< \~japanese 準備完了     \~english Device initialized and ready for communication.
    ,   Paused          = 6    //!< \~japanese 一時停止     \~english Communication temporarily paused.
    ,   Echo            = 6    //!< \~japanese エコー       \~english Device echoes received data.
};

    //! 
    //! @enum eNetworkConnection
    //! @brief
    //!     \~japanese ネットワーク接続種別の定義。
    //!     \~english  Network connection types.
    //!
    enum class eNetworkConnection : uint8_t
    {
            Unknown             = 0    //!< \~japanese 不明な接続                    \~english Unknown connection type.
        ,   Ethernet            = 1    //!< \~japanese イーサネット（有線）          \~english Ethernet (wired).
        ,   Bluetooth           = 3    //!< \~japanese Bluetooth（無線）             \~english Bluetooth (wireless).
        ,   WiMAX               = 4    //!< \~japanese WiMAX（無線）                 \~english WiMAX (wireless).
        ,   VPN                 = 5    //!< \~japanese 仮想プライベートネットワーク  \~english VPN.
        ,   VirtualEthernet     = 6    //!< \~japanese 仮想イーサネット              \~english Virtual Ethernet.
        ,   Wifi_4              = 10   //!< \~japanese Wi-Fi 4（無線）               \~english Wi-Fi 4 (wireless).
        ,   Wifi_5              = 11   //!< \~japanese Wi-Fi 5（無線）               \~english Wi-Fi 5 (wireless).
        ,   Wifi_6              = 12   //!< \~japanese Wi-Fi 6（無線）               \~english Wi-Fi 6 (wireless).
        ,   Wifi_6E             = 13   //!< \~japanese Wi-Fi 6E（無線）              \~english Wi-Fi 6E (wireless).
    };

    //! 
    //! @enum eMonitorConnection
    //! @brief
    //!     \~japanese モニター接続端子の定義。
    //!     \~english  Monitor connection types.
    //!
    enum class eMonitorConnection : uint8_t
    {
            Unknown        = 0    //!< \~japanese 不明な接続方式  \~english Unknown connection type.
        ,   HDMI           = 1    //!< \~japanese HDMI            \~english HDMI (High-Definition Multimedia Interface).
        ,   VGA            = 2    //!< \~japanese VGA             \~english VGA (Analog RGB).
        ,   DVI            = 3    //!< \~japanese DVI             \~english DVI (Digital Visual Interface).
        ,   DisplayPort    = 4    //!< \~japanese DisplayPort     \~english DisplayPort.
        ,   USB_C          = 5    //!< \~japanese USB-C           \~english USB-C.
        ,   Thunderbolt    = 6    //!< \~japanese Thunderbolt     \~english Thunderbolt.
    };

}

#endif   //WONDERSTEWENGINE_DEPEND_ENUMCLASS_H
