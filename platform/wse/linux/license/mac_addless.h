//*****************************************************************************************************************
//!
//! @file    mac_addless.h
//! @brief   \~japanese Linux向けDevice license識別子の取得契約を定義する.
//! @brief   \~english  Defines Linux device-license identity acquisition.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create the Linux platform adapter contract.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#pragma once

#ifndef WONDERSTEWENGINE_PLATFORM_LINUX_LICENSE_MAC_ADDRESS_H
#define WONDERSTEWENGINE_PLATFORM_LINUX_LICENSE_MAC_ADDRESS_H

#include <string>

namespace wse
{
    namespace license
    {
        //!
        //! @brief Loopback以外で最初の6-byte hardware addressを取得する.
        //! @author WapitiStew.
        //! @return Lower-case colon-separated address. 取得不能時は空文字列.
        //!
        std::string mac_address( void );
    }
}

#endif  // WONDERSTEWENGINE_PLATFORM_LINUX_LICENSE_MAC_ADDRESS_H
