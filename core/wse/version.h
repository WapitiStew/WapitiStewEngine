//*****************************************************************************************************************
//! 
//! @file    version.h
//! @brief   \~japanese 数値比較できるWSE Version定数を定義する.
//! @brief   \~english  Defines the numerically comparable WSE version constants.
//! @author  WapitiStew
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2021/06/01   Create New WapitiStew
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
#ifndef WONDERSTEWENGINE_LIB_CORE_VERSION_DEFINE
#define WONDERSTEWENGINE_LIB_CORE_VERSION_DEFINE

namespace wse
{
    // Version 1.x.x
    #define WSEVERSION_1_0_0 ( static_cast< uint64_t >( 100000000000000 ) )
    #define WSEVERSION_1_1_0 ( static_cast< uint64_t >( 100000100000000 ) )
    #define WSEVERSION_1_2_0 ( static_cast< uint64_t >( 100000200000000 ) )
    #define WSEVERSION_1_3_0 ( static_cast< uint64_t >( 100000300000000 ) )
    
    // Version 2.x.x
    #define WSEVERSION_2_0_0 ( static_cast< uint64_t >( 200000000000000 ) )
    #define WSEVERSION_2_1_0 ( static_cast< uint64_t >( 200000100000000 ) )
    #define WSEVERSION_2_2_0 ( static_cast< uint64_t >( 200000200000000 ) )
    #define WSEVERSION_2_3_0 ( static_cast< uint64_t >( 200000300000000 ) )
}
#endif  //WONDERSTEWENGINE_PROJECTOR_LIB_HEADER
