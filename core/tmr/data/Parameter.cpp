//*****************************************************************************************************************
//! 
//! @file    Parameter.cpp
//! @brief   \~japanese Parameter<>の全数値型（WindowsではLegacyのlongを含む）に対する明示的Template実体化.
//! @brief   \~english  Explicit template instantiations of Parameter<> for every numeric type, including the
//!                     legacy long on Windows.
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
#include "../../../api/tmr/data/Parameter.h"

namespace wse
{
namespace tmr
{
    template class WSE_API Parameter< S8>;
    template class WSE_API Parameter<S16>;
    template class WSE_API Parameter<S32>;
    template class WSE_API Parameter<S64>;
    template class WSE_API Parameter< U8>;
    template class WSE_API Parameter<U16>;
    template class WSE_API Parameter<U32>;
    template class WSE_API Parameter<U64>;
    template class WSE_API Parameter<F32>;
    template class WSE_API Parameter<F64>;
#if defined( _WIN32 )
    // MSVC's long is distinct from int32_t; the historical WebCamera API returns Parameter<long>.
    template class WSE_API Parameter<long>;
#endif

}
}
