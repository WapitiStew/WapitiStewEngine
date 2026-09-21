//*****************************************************************************************************************
//! 
//! @file    wse_Wait.cpp
//! @brief   \~japanese 現在Threadを指定時間だけ眠らせるWaitの実装.
//! @brief   \~english  Implements Wait, which sleeps the current thread for the given duration.
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
#include "../common.h"

#include <chrono>
#include <thread>



/*!
@class  Timer
@author  WapitiStew

タイマークラス.
*/

namespace  wse
{
    void Wait::sec( const uint64_t time_in )
    {
        std::this_thread::sleep_for( std::chrono::seconds( time_in ) );
    }

    void Wait::msec( const uint64_t time_in )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( time_in ) );
    }

    void Wait::usec( const uint64_t time_in )
    {
        std::this_thread::sleep_for( std::chrono::microseconds( time_in ) );
    }

    void Wait::nsec( const uint64_t time_in )
    {
        std::this_thread::sleep_for( std::chrono::nanoseconds( time_in ) );
    }

};
