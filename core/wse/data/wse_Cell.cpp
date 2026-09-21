//*****************************************************************************************************************
//! 
//! @file    wse_Cell.cpp
//! @brief   \~japanese Cell_の全数値型に対する明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of Cell_ for every numeric type.
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
#include <wse/data/wse_Cell.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{
    template struct WSE_API Cell2D_< S8> ;
    template struct WSE_API Cell2D_<S16> ;
    template struct WSE_API Cell2D_<S32> ;
    template struct WSE_API Cell2D_<S64> ;
    template struct WSE_API Cell2D_< U8> ;
    template struct WSE_API Cell2D_<U16> ;
    template struct WSE_API Cell2D_<U32> ;
    template struct WSE_API Cell2D_<U64> ;
    template struct WSE_API Cell2D_<F32> ;
    template struct WSE_API Cell2D_<F64> ;

    template struct WSE_API Cell3D_< S8> ;
    template struct WSE_API Cell3D_<S16> ;
    template struct WSE_API Cell3D_<S32> ;
    template struct WSE_API Cell3D_<S64> ;
    template struct WSE_API Cell3D_< U8> ;
    template struct WSE_API Cell3D_<U16> ;
    template struct WSE_API Cell3D_<U32> ;
    template struct WSE_API Cell3D_<U64> ;
    template struct WSE_API Cell3D_<F32> ;
    template struct WSE_API Cell3D_<F64> ;

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
