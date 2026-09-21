//*****************************************************************************************************************
//! 
//! @file    mac_addless.h
//! @brief   \~japanese License識別に使うMAC Addressを取得する関数を宣言する.
//! @brief   \~english  Declares the function that reads the MAC address used for licence identification.
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
#include <string>

namespace wse
{
namespace license
{
    std::string mac_address( void );
}
}
