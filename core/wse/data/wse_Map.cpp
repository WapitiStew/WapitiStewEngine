//*****************************************************************************************************************
//! 
//! @file    wse_Map.cpp
//! @brief   \~japanese Map_の明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of Map_.
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
#include <wse/data/wse_Map.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5045)  // /Qspectre オプションが有効になっていることで、Spectre 脆弱性対策用のコードがコンパイル時に自動的に挿入される
#endif

namespace wse
{
    template class WSE_API Map< S8>;
    template class WSE_API Map<S16>;
    template class WSE_API Map<S32>;
    template class WSE_API Map<S64>;
    template class WSE_API Map< U8>;
    template class WSE_API Map<U16>;
    template class WSE_API Map<U32>;
    template class WSE_API Map<U64>;
    template class WSE_API Map<F32>;
    template class WSE_API Map<F64>;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
