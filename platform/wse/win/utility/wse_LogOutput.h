//*****************************************************************************************************************
//! 
//! @file    wse_LogOutput.h
//! @brief   \~japanese ConsoleとLog fileへの出力Primitiveを宣言する. Windows実装が対になる.
//! @brief   \~english  Declares the console and log-file output primitives implemented for Windows.
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
#ifndef WONDERSTEWENGINE_PLATFORM_UTILITY_LOGOUTPUT_H
#define WONDERSTEWENGINE_PLATFORM_UTILITY_LOGOUTPUT_H


namespace wse
{
    namespace log
    {
        void outputConsole( const std::string& str_in );
        void outputLogFile( const std::string& str_in, const std::string& file_in );
    }
}

#endif   //WONDERSTEWENGINE_DEPEND_EXCEPTION_H

