//*****************************************************************************************************************
//! 
//! @file    STD.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//! @brief   
//!     \~japanese  内部で使用するC/C++標準のヘッダファイルをincludeする.
//!     \~english   Includes C/C++ standard header files used internally.
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
#ifndef WONDERSTEWENGINE_TURTLECAMERALIB_DEPEND_STD_INCLUDED
#define WONDERSTEWENGINE_TURTLECAMERALIB_DEPEND_STD_INCLUDED


// Warning Disable
#pragma warning(push)
#pragma warning(disable: 4365)  // 符号ありなしの引数
#pragma warning(disable: 4711)  // 勝手にインライン化した時のwarning

// C/C++
#include <cstddef>
#include <cstdint>
#include <vector>
#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>

#pragma warning(pop)

#endif   //WONDERSTEWENGINE_TURTLECAMERALIB_DEPEND_STD_INCLUDED


