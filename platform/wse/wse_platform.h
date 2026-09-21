//*****************************************************************************************************************
//! 
//! @file    wse_platform.h
//! @brief   \~japanese Platform判定を一箇所で行い、OS別のLicenseとLog出力Headerへ振り分ける.
//! @brief   \~english  Decides the platform once and routes to the per-OS licence and log-output headers.
//! @author  WapitiStew
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2021/06/01   Create New WapitiStew
//!   Aug-05, 2026   Add Linux log output include for portable tests.
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
#ifndef WONDERSTEWENGINE_PLATFORM_DEPENDCODE_H
#define WONDERSTEWENGINE_PLATFORM_DEPENDCODE_H



#if defined(_WIN32) || defined(_WIN64) || defined( WINDOWS )          // Windows

#include "win/license/mac_addless.h"
#include "win/utility/wse_LogOutput.h"

#elif ( defined(__APPLE__) && defined(__MACH__) ) || defined(MACOS)   // macOS

#include "mac/license/mac_addless.h"

#elif defined(__linux__) || defined(LINUX)                            // Linux

#include "linux/license/mac_addless.h"
#include "linux/utility/wse_LogOutput.h"

#elif defined(__chromeos__) || defined(CHROMEOS)                      // ChromeOS

#include "chrome/license/mac_addless.h"

#elif defined(__ANDROID__) || defined(ANDROID)                        // Android

#include "android/license/mac_addless.h"

#elif defined(__ios__) || defined(IOS)                                // iOS

#include "ios/license/mac_addless.h"

#else
    #error "Unsupported platform"
#endif




#endif  //WONDERSTEWENGINE_PLATFORM_DEPENDCODE_H
