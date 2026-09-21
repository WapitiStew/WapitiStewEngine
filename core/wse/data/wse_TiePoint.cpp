//*****************************************************************************************************************
//! 
//! @file    wse_TiePoint.cpp
//! @brief   \~japanese TiePoint_の明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of TiePoint_.
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
#include <wse/data/wse_TiePoint.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{
    template struct WSE_API TiePoint_<  float32_xy,  float32_xy >;
    template struct WSE_API TiePoint_<  float64_xy,  float64_xy >;
    template struct WSE_API TiePoint_<  float32_xy, float32_xyz >;
    template struct WSE_API TiePoint_<  float64_xy, float64_xyz >;
    template struct WSE_API TiePoint_< float32_xyz,  float32_xy >;
    template struct WSE_API TiePoint_< float64_xyz,  float64_xy >;
    template struct WSE_API TiePoint_< float32_xyz, float32_xyz >;
    template struct WSE_API TiePoint_< float64_xyz, float64_xyz >;


};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
