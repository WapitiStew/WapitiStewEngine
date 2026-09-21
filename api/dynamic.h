//*****************************************************************************************************************
//! 
//! @file    dynamic.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//! 
//! @brief
//!   \~japanese ダイナミックライブラリとして公開するためのマクロを定義するファイル.
//!   \~english File that defines macros for exporting as a dynamic library.
//!
//! 
//! @details
//!   \~japanese
//!     開発者は以下の設定をすることでライブラリを開発できる.
//!         VisualStudio    プロジェクトプロパティ > C/C++ > プリプロセッサ > プリプロセッサの定義に"__WSE_EXPORTS__"を追記.
//!     ユーザーはプリプロセッサの定義は不要.
//!   \~english
//!     Developers can build the library by applying the following setting:
//!         In Visual Studio: Project Properties > C/C++ > Preprocessor > add "__WSE_EXPORTS__" to the Preprocessor Definitions.
//!     End users do not need to define any preprocessor symbols.
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
#ifndef WONDER_STEW_ENGINE_LIBRARY_DYNAMICLIB_DEFINE
#define WONDER_STEW_ENGINE_LIBRARY_DYNAMICLIB_DEFINE


namespace wse
{
    // MyLibraryAPI.h
    #if defined WSE_STATIC
      #define WSE_API
    #elif defined _WIN32 || defined _WIN64 || defined __CYGWIN__
      #ifdef __WSE_EXPORTS__
        #define WSE_API __declspec(dllexport)
      #else
        #define WSE_API __declspec(dllimport)
      #endif

    #else
      #ifdef __WSE_EXPORTS__
        #define WSE_API __attribute__ ((visibility ("default")))
      #else
        #define WSE_API
      #endif
    #endif

};

#endif   /*WONDERSTEWENGINE_LIBRARY_HEADER*/



