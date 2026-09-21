//*****************************************************************************************************************
//! 
//! @file    wse_Mesh.cpp
//! @brief   \~japanese Mesh2D_の明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of Mesh2D_.
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
#include <wse/data/wse_Mesh.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{

    template struct WSE_API Mesh_<  sint08_xy,  sint08_xy >;
    template struct WSE_API Mesh_<  sint16_xy,  sint16_xy >;
    template struct WSE_API Mesh_<  sint32_xy,  sint32_xy >;
    template struct WSE_API Mesh_<  sint64_xy,  sint64_xy >;
    template struct WSE_API Mesh_<  uint08_xy,  uint08_xy >;
    template struct WSE_API Mesh_<  uint16_xy,  uint16_xy >;
    template struct WSE_API Mesh_<  uint32_xy,  uint32_xy >;
    template struct WSE_API Mesh_<  uint64_xy,  uint64_xy >;
    template struct WSE_API Mesh_< float32_xy, float32_xy >;
    template struct WSE_API Mesh_< float64_xy, float64_xy >;

    template struct WSE_API Mesh_<  sint08_xy,  sint08_xyz >;
    template struct WSE_API Mesh_<  sint16_xy,  sint16_xyz >;
    template struct WSE_API Mesh_<  sint32_xy,  sint32_xyz >;
    template struct WSE_API Mesh_<  sint64_xy,  sint64_xyz >;
    template struct WSE_API Mesh_<  uint08_xy,  uint08_xyz >;
    template struct WSE_API Mesh_<  uint16_xy,  uint16_xyz >;
    template struct WSE_API Mesh_<  uint32_xy,  uint32_xyz >;
    template struct WSE_API Mesh_<  uint64_xy,  uint64_xyz >;
    template struct WSE_API Mesh_< float32_xy, float32_xyz >;
    template struct WSE_API Mesh_< float64_xy, float64_xyz >;

    template struct WSE_API Mesh_<  sint08_xyz,  sint08_xy >;
    template struct WSE_API Mesh_<  sint16_xyz,  sint16_xy >;
    template struct WSE_API Mesh_<  sint32_xyz,  sint32_xy >;
    template struct WSE_API Mesh_<  sint64_xyz,  sint64_xy >;
    template struct WSE_API Mesh_<  uint08_xyz,  uint08_xy >;
    template struct WSE_API Mesh_<  uint16_xyz,  uint16_xy >;
    template struct WSE_API Mesh_<  uint32_xyz,  uint32_xy >;
    template struct WSE_API Mesh_<  uint64_xyz,  uint64_xy >;
    template struct WSE_API Mesh_< float32_xyz, float32_xy >;
    template struct WSE_API Mesh_< float64_xyz, float64_xy >;

    template struct WSE_API Mesh_<  sint08_xyz,  sint08_xyz >;
    template struct WSE_API Mesh_<  sint16_xyz,  sint16_xyz >;
    template struct WSE_API Mesh_<  sint32_xyz,  sint32_xyz >;
    template struct WSE_API Mesh_<  sint64_xyz,  sint64_xyz >;
    template struct WSE_API Mesh_<  uint08_xyz,  uint08_xyz >;
    template struct WSE_API Mesh_<  uint16_xyz,  uint16_xyz >;
    template struct WSE_API Mesh_<  uint32_xyz,  uint32_xyz >;
    template struct WSE_API Mesh_<  uint64_xyz,  uint64_xyz >;
    template struct WSE_API Mesh_< float32_xyz, float32_xyz >;
    template struct WSE_API Mesh_< float64_xyz, float64_xyz >;


};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
