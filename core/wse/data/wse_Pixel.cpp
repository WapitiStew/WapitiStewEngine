//*****************************************************************************************************************
//! 
//! @file    wse_Pixel.cpp
//! @brief   \~japanese Pixel型の明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of the pixel types.
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
#include "../TemplateExport.h"
#include <wse/data/wse_Pixel.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{

    template struct WSE_INSTANTIATION_API Pixel_<  U8, 1,  8, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 1, 10, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 1, 12, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 1, 14, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 1, 16, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U32, 1, 32, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U64, 1, 64, void >;

    template struct WSE_INSTANTIATION_API Pixel_<  U8, 3,  8, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 3, 10, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 3, 12, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 3, 14, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 3, 16, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U32, 3, 32, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U64, 3, 64, void >;

    template struct WSE_INSTANTIATION_API Pixel_<  U8, 4,  8, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 4, 10, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 4, 12, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 4, 14, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U16, 4, 16, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U32, 4, 32, void >;
    template struct WSE_INSTANTIATION_API Pixel_< U64, 4, 64, void >;

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
