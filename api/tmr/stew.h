//*****************************************************************************************************************
//! 
//! @file    stew.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//! 
//! @brief
//!     \~japanese カメラ制御ライブラリ TurtleCameraLib の共通インクルードヘッダ
//!     \~english  Common include header for the TurtleCameraLib camera control library
//!
//! 
//! @details
//!     \~japanese
//!     @n 本ファイルはPublic TmrのPortable CameraとCalibration契約を一括で提供する。
//!     @n 高水準WebCamera Facadeも本Headerから利用できる。
//!     @n Access-controlled機種拡張は直接Headerを使用する。
//!     @n `IS_ENABLE_TMR_WSE` マクロにより WSE 機能の有効化フラグも定義されている。
//!
//!     \~english
//!     @n This file provides the public portable Tmr camera and calibration contracts.
//!     @n The high-level WebCamera facade is also available through this header.
//!     @n Access-controlled device extensions use direct headers.
//!     @n Defines the `IS_ENABLE_TMR_WSE` macro to enable WSE functionality.
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
#ifndef WONDERSTEWENINE_TURTLECAMERALIB_HEADER_H
#define WONDERSTEWENINE_TURTLECAMERALIB_HEADER_H

// define
#define IS_ENABLE_TMR_WSE

// Portable public camera contract.
#include "camera/CameraError.h"
#include "camera/CameraTypes.h"
#include "camera/Camera.h"
#include "camera/CameraFrameOps.h"
#include "device/WebCamera.h"
#include "calibration/CameraCalibration.h"


#endif //WONDERSTEWENINE_TURTLECAMERALIB_HEADER_H
