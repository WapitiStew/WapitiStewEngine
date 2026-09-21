//*****************************************************************************************************************
//! 
//! @file    stew.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew
//!
//! 
//! @brief
//!     \~japanese 入力ユーザーインターフェースモジュールの共通インクルードヘッダ
//!     \~english  Common include header for Input User Interface module
//!
//! @details
//!     \~japanese
//!     @n 本ファイルは、入力系ユーザーインターフェース（UII）モジュールに関連するヘッダを一括でインクルードする。
//!     @n 入力デバイス（Keyboard）と依存情報（Metadata）をまとめて提供する。
//!     @n `IS_ENABLE_UII_WSE` マクロにより当該モジュールの有効化を宣言する。
//!
//!     \~english
//!     @n This file provides a unified include header for Input User Interface (UII) related components.
//!     @n It includes input device interfaces such as Keyboard and dependency metadata.
//!     @n Defines the `IS_ENABLE_UII_WSE` macro to enable the UII module.
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
#ifndef WANDERSTEWENGINE_INPUTUSERINTERFASE_HEADER_H
#define WANDERSTEWENGINE_INPUTUSERINTERFASE_HEADER_H

// define
#define IS_ENABLE_UII_WSE

// depand.
#include "depend/Metadata.h"

// device.
#include "device/Keyboard.h"

// utility.


#endif //WANDERSTEWENGINE_USERINPUTINTERFASE_HEADER_H
