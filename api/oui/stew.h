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
//!     \~japanese 出力ユーザーインターフェースモジュールの共通インクルードヘッダ
//!     \~english  Common include header for Output User Interface module
//!
//! @details
//!     \~japanese
//!     @n 本ファイルは、出力系ユーザーインターフェース（UII）モジュールに関連するヘッダを一括でインクルードする。
//!     @n 描画系クラス（ProjectionRenderer, ScreenRenderer, WindowRenderer 等）と GPU インターフェース、テクスチャ機能が含まれる。
//!     @n また、OS依存のディスプレイ処理も `osDisplay.h` によりサポートされている。
//!     @n `IS_ENABLE_UII_WSE` マクロにより当該モジュールの有効化を宣言する。
//!
//!     \~english
//!     @n This file provides a unified include header for Output User Interface (UII) related components.
//!     @n It includes the portable Renderer and its projection pipeline,
//!     @n GPU utility interfaces and RenderTexture2D handling,
//!     @n as well as OS-specific display support via `osDisplay.h`.
//!     @n Defines the `IS_ENABLE_UII_WSE` macro to enable the UII module
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
#ifndef WANDERSTEWENGINE_OUTPUTUSERINTERFASE_HEADER_H
#define WANDERSTEWENGINE_OUTPUTUSERINTERFASE_HEADER_H

// define
#define IS_ENABLE_UII_WSE

// renderer.
#include "binding/Projection.h"
#include "renderer/Renderer.h"
#include "renderer/ProjectionMeshAdapter.h"
#include "renderer/ProjectionPipeline.h"

// device.



#endif //WANDERSTEWENGINE_USERINPUTINTERFASE_HEADER_H
