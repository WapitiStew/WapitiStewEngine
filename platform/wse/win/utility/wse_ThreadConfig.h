//*****************************************************************************************************************
//! 
//! @file    wse_ThreadConfig.h
//! @brief   \~japanese Windows Thread優先度設定のUtilityを定義する.
//! @brief   \~english  Defines the Windows thread-priority utility.
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
#ifndef WONDERSTEWENGINE_PLATFORM_UTILITY_THREAD_CONFIG_H
#define WONDERSTEWENGINE_PLATFORM_UTILITY_THREAD_CONFIG_H

#pragma warning(push)
#pragma warning(disable: 4244)  // ../
#pragma warning(disable: 4464)  // ../
#pragma warning(disable: 4505)  // ../
#pragma warning(disable: 4514)  // ../
#pragma warning(disable: 4702)  // ../
#pragma warning(disable: 4820)  // ../
#include <windows.h>
#include <thread>
#include "../../../../api/wse/stew.h"

namespace wse
{
    //!
    //! @brief スレッドの優先度を最高にする.
    //! @param [out] p_thread_in スレッドポインタ.
    //!
    static void setHighestThreadPriority( std::thread* p_thread_in )
    {
        // Windowsのスレッドハンドルを取得
        HANDLE handle = p_thread_in->native_handle();

        // 優先度を変更する（例：最高の優先度）
        if( SetThreadPriority( handle, THREAD_PRIORITY_HIGHEST ) ) 
        {
            DDLog() << "SetThreadPriority HIGHEST";
        }
    }
};

#pragma warning(pop)

#endif //WONDERSTEWENGINE_PLATFORM_UTILITY_THREAD_CONFIG_H