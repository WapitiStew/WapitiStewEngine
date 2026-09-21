//*****************************************************************************************************************
//! 
//! @file    stew.h
//! @par      Character Code: UTF-8N
//! @par      Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese WonderStewEngine の共通インクルードヘッダ
//!     \~english  Common include header for WonderStewEngine
//!
//! @details
//!     \~japanese
//!     @n このファイルは、WonderStewEngine の基礎を構成する主要なモジュールを一括でインクルードするヘッダファイル。
//!     @n 依存モジュール（Typedef、Enum、Constant など）、ユーティリティ（Log、Timer、Wait など）、
//!     @n データ構造（Point、Range、Size、Matrix、Image など）、およびライセンス制御関連を統合している。
//!     @n 外部ファイルでは本ヘッダのみをインクルードすることで、エンジンの中核機能を利用できる。
//!
//!     \~english
//!     @n This file serves as a unified include header for the core modules of WonderStewEngine.
//!     @n It covers dependency definitions (Typedef, Enum, Constant), utility components (Log, Timer, Wait),
//!     @n data structures (Point, Range, Size, Matrix, Image), and license management components.
//!     @n Including this single file allows external code to access the engine's core features.
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
#ifndef WONDERSTEWENGINE_HEADER
#define WONDERSTEWENGINE_HEADER


// Depend
#include "depend/wse_STD.h"
#include "depend/wse_Enum.h"
#include "depend/wse_Typedef.h"
#include "depend/wse_Constant.h"

// License
#include "license/wse_License.h"
#include "license/wse_LicenseKey.h"
#include "license/wse_LicenseWriter.h"

// Error.
#include "error/CoreError.h"
#include "error/LicenseError.h"

// Utility.
#include "utility/wse_Log.h"
#include "utility/wse_Math.h"
#include "utility/wse_PixelFormat.h"
#include "utility/wse_StringTool.h"
#include "utility/wse_Timer.h"
#include "utility/wse_Wait.h"
#include "utility/wse_DevicePickup.h"
#include "utility/wse_DataCast.h"


// Data
#include "data/wse_Map.h"
#include "data/wse_Matrix.h"
#include "data/wse_Point2D.h"
#include "data/wse_Point3D.h"
#include "data/wse_Point4D.h"
#include "data/wse_Range1D.h"
#include "data/wse_Range2D.h"
#include "data/wse_Range3D.h"
#include "data/wse_Size.h"
#include "data/wse_Homography.h"

#include "data/wse_Pixel.h"
#include "data/wse_Image.h"
#include "data/wse_ImageInterleaved.h"
#include "data/wse_ImageTransform.h"
#include "data/wse_ImageDemosaic.h"

#include "data/wse_TiePoint.h"
#include "data/wse_Mesh.h"
#include "data/wse_Cell.h"
#include "data/wse_DeviceInfo.h"






#endif   //WONDERSTEWENGINE_HEADER




