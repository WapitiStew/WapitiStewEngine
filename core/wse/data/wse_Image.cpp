//*****************************************************************************************************************
//! 
//! @file    wse_Image.cpp
//! @brief   \~japanese Image_の明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of Image_.
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
#include <wse/data/wse_Image.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{
    template class WSE_API Map< Pixel_< U8,  1,  8 > >;
    template class WSE_API Map< Pixel_< U16, 1, 10 > >;
    template class WSE_API Map< Pixel_< U16, 1, 12 > >;
    template class WSE_API Map< Pixel_< U16, 1, 14 > >;
    template class WSE_API Map< Pixel_< U16, 1, 16 > >;
    template class WSE_API Map< Pixel_< U32, 1, 32 > >;
    template class WSE_API Map< Pixel_< U64, 1, 64 > >;
    template class WSE_API Map< Pixel_< U8,  3,  8 > >;
    template class WSE_API Map< Pixel_< U16, 3, 10 > >;
    template class WSE_API Map< Pixel_< U16, 3, 12 > >;
    template class WSE_API Map< Pixel_< U16, 3, 14 > >;
    template class WSE_API Map< Pixel_< U16, 3, 16 > >;
    template class WSE_API Map< Pixel_< U32, 3, 32 > >;
    template class WSE_API Map< Pixel_< U64, 3, 64 > >;
    template class WSE_API Map< Pixel_< U8,  4,  8 > >;
    template class WSE_API Map< Pixel_< U16, 4, 10 > >;
    template class WSE_API Map< Pixel_< U16, 4, 12 > >;
    template class WSE_API Map< Pixel_< U16, 4, 14 > >;
    template class WSE_API Map< Pixel_< U16, 4, 16 > >;
    template class WSE_API Map< Pixel_< U32, 4, 32 > >;
    template class WSE_API Map< Pixel_< U64, 4, 64 > >;

    template class WSE_API Image_< ePixFormat::CH1D8   >;
    template class WSE_API Image_< ePixFormat::CH1D10  >;
    template class WSE_API Image_< ePixFormat::CH1D12  >;
    template class WSE_API Image_< ePixFormat::CH1D14  >;
    template class WSE_API Image_< ePixFormat::CH1D16  >;
    template class WSE_API Image_< ePixFormat::CH1D32  >;
    template class WSE_API Image_< ePixFormat::CH1D64  >;
    template class WSE_API Image_< ePixFormat::CH2D8   >;
    template class WSE_API Image_< ePixFormat::CH2D10  >;
    template class WSE_API Image_< ePixFormat::CH2D12  >;
    template class WSE_API Image_< ePixFormat::CH2D14  >;
    template class WSE_API Image_< ePixFormat::CH2D16  >;
    template class WSE_API Image_< ePixFormat::CH2D32  >;
    template class WSE_API Image_< ePixFormat::CH2D64  >;
    template class WSE_API Image_< ePixFormat::CH3D8   >;
    template class WSE_API Image_< ePixFormat::CH3D10  >;
    template class WSE_API Image_< ePixFormat::CH3D12  >;
    template class WSE_API Image_< ePixFormat::CH3D14  >;
    template class WSE_API Image_< ePixFormat::CH3D16  >;
    template class WSE_API Image_< ePixFormat::CH3D32  >;
    template class WSE_API Image_< ePixFormat::CH3D64  >;
    template class WSE_API Image_< ePixFormat::CH4D8   >;
    template class WSE_API Image_< ePixFormat::CH4D10  >;
    template class WSE_API Image_< ePixFormat::CH4D12  >;
    template class WSE_API Image_< ePixFormat::CH4D14  >;
    template class WSE_API Image_< ePixFormat::CH4D16  >;
    template class WSE_API Image_< ePixFormat::CH4D32  >;
    template class WSE_API Image_< ePixFormat::CH4D64  >;

    // Channel順序違いのFormat。基底のMapは対応するCHxDyy版と同一のため、追加の実体化は要らない。
    template class WSE_API Image_< ePixFormat::BGR3D8   >;
    template class WSE_API Image_< ePixFormat::BGR3D16  >;
    template class WSE_API Image_< ePixFormat::BGRA4D8  >;
    template class WSE_API Image_< ePixFormat::BGRA4D16 >;

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
