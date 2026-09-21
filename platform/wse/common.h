//*****************************************************************************************************************
//! 
//! @file    common.h
//! @brief   \~japanese Platform実装が共通で使う公開Core型とUtilityをまとめて読み込む.
//! @brief   \~english  Pulls in the public Core types and utilities every platform implementation shares.
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
#ifndef WONDERSTEWENGINE_LIB_PLATFORM_COMMON_PATH
#define WONDERSTEWENGINE_LIB_PLATFORM_COMMON_PATH


// #include "../../api/wse/stew.h" // This is not present in the snippet

#pragma warning(push)
#pragma warning(disable: 4464)  // ../
// Platform
#include "../wse/wse_platform.h"

// Depend
#include "../../api/wse/depend/wse_Constant.h"
#include "../../api/wse/depend/wse_Enum.h"
#include "../../api/wse/depend/wse_STD.h"
#include "../../api/wse/depend/wse_Typedef.h"


// Utility.
#include "../../api/wse/utility/wse_Practiser.h"
#include "../../api/wse/utility/wse_Timer.h"
#include "../../api/wse/utility/wse_Wait.h"
#include "../../api/wse/utility/wse_Log.h"


// Data
#include "../../api/wse/data/wse_Map.h"
#include "../../api/wse/data/wse_Matrix.h"
#include "../../api/wse/data/wse_Point2D.h"
#include "../../api/wse/data/wse_Point3D.h"
#include "../../api/wse/data/wse_Point4D.h"
#include "../../api/wse/data/wse_Range1D.h"
#include "../../api/wse/data/wse_Range2D.h"
#include "../../api/wse/data/wse_Range3D.h"
#include "../../api/wse/data/wse_Size.h"
#include "../../api/wse/data/wse_Homography.h"
#include "../../api/wse/data/wse_Pixel.h"
#include "../../api/wse/data/wse_Image.h"

#pragma warning(pop)


#endif  //WONDERSTEWENGINE_PROJECTOR_LIB_HEADER
