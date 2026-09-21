//*****************************************************************************************************************
//! 
//! @file    wse_STD.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 標準C++ライブラリのヘッダファイルを一括でインクルードするためのヘッダ。
//!     \~english  Header to collectively include standard C++ library headers.
//!
//! @details
//!     \~japanese よく使用されるC++標準ライブラリをまとめて読み込むことで、共通処理の記述を簡素化する。
//!     \~english  This file simplifies inclusion of frequently used standard C++ headers for shared functionality.
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
#ifndef WONDERSTEWENGINE_DEPEND_STD_INCLUD
#define WONDERSTEWENGINE_DEPEND_STD_INCLUD


// Warning Disable
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4365)  // 符号ありなしの引数
#pragma warning(disable: 4711)  // 勝手にインライン化した時のwarning
#endif

// C/C++
#include <cstdint>
#include <type_traits>
#include <string>
#include <array>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif   // WONDERSTEWENGINE_DEPEND_STD_INCLUD


