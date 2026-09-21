//*****************************************************************************************************************
//! 
//! @file    wse_Wait.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 時間待機処理を提供するユーティリティ。
//!     \~english  Utility for precise wait/sleep operations.
//!
//! @details
//!     \~japanese
//!         本ヘッダでは、ナノ秒〜秒単位の待機処理を静的関数として提供する。
//!         マルチスレッド環境やタイミング制御用途に利用可能。
//!
//!     \~english
//!         This header provides static functions for wait/sleep operations
//!         ranging from nanoseconds to seconds, useful for multithreaded
//!         environments and precise timing control.
//!
//! @note
//!     \~japanese Windows APIやC++標準ライブラリに基づいた実装を想定。
//!     \~english  Expected to be implemented based on Windows API or C++ standard library.
//!
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
#ifndef WONDERSTEWENGINE_UTILITY_WAIT_H
#define WONDERSTEWENGINE_UTILITY_WAIT_H


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4668) 
#pragma warning(disable: 4464) 
#endif
#include "../depend/wse_STD.h"
#include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)  // Dynamicライブラリでユーザー側に公開されないメンバーが含まれる
#endif
namespace  wse
{
    //!
    //! @class Wait
    //!
    //! @brief
    //!     \~japanese 単位時間ごとの待機処理を提供するユーティリティクラス。
    //!     \~english  Utility class for wait operations in specific time units.
    //!
    //! @details
    //!     \~japanese
    //!         秒、ミリ秒、マイクロ秒、ナノ秒単位でスレッドをブロックする静的関数を提供。
    //!         一般的なポーリングやフレームレート制御、通信タイミング制御などに利用される。
    //!
    //!     \~english
    //!         Provides static functions for blocking the thread in seconds,
    //!         milliseconds, microseconds, or nanoseconds. Useful for polling,
    //!         frame rate regulation, or communication timing.
    //! 
    class WSE_API Wait
    {
        //!
        //! @brief
        //!     \~japanese 指定された秒数だけ待機する。
        //!     \~english  Wait for the specified number of seconds.
        //!
        //! @param[in] time_in  Wait time in seconds.
        //!
        public: static void sec( const uint64_t time_in );

        //!
        //! @brief
        //!     \~japanese 指定されたミリ秒数だけ待機する。
        //!     \~english  Wait for the specified number of milliseconds.
        //!
        //! @param[in] time_in  Wait time in milliseconds.
        //!
        public: static void msec( const uint64_t time_in );

        //!
        //! @brief
        //!     \~japanese 指定されたマイクロ秒数だけ待機する。
        //!     \~english  Wait for the specified number of microseconds.
        //!
        //! @param[in] time_in  Wait time in microseconds.
        //!
        public: static void usec( const uint64_t time_in );

        //!
        //! @brief
        //!     \~japanese 指定されたナノ秒数だけ待機する。
        //!     \~english  Wait for the specified number of nanoseconds.
        //!
        //! @param[in] time_in  Wait time in nanoseconds.
        //!
        public: static void nsec( const uint64_t time_in );

    };

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif  //WONDERSTEWENGINE_UTILITY_WAIT_H
