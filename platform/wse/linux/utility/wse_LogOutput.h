//*****************************************************************************************************************
//!
//! @file    wse_LogOutput.h
//! @brief   \~japanese Linux向けログ出力関数を定義する。
//! @brief   \~english  Defines Linux log output functions.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-05, 2026   Add Linux logging support for portable WSE tests.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_PLATFORM_LINUX_UTILITY_LOGOUTPUT_H
#define WONDERSTEWENGINE_PLATFORM_LINUX_UTILITY_LOGOUTPUT_H

#include <cerrno>
#include <fstream>
#include <iostream>
#include <string>

#include <sys/stat.h>

namespace wse
{
    namespace log
    {
        inline void outputConsole( const std::string& str_in )
        {
            std::cout << str_in;
        }

        inline bool createDirectoryRecursive( const std::string& fullpath_in )
        {
            const std::size_t last_slash = fullpath_in.find_last_of( "/\\" );
            if( last_slash == std::string::npos )
            {
                return true;
            }

            const std::string directory = fullpath_in.substr( 0, last_slash );
            std::string current;
            for( std::size_t index = 0; index < directory.size(); ++index )
            {
                current += directory[index];
                if( directory[index] != '/' || current == "/" )
                {
                    continue;
                }
                if( ::mkdir( current.c_str(), 0755 ) != 0 && errno != EEXIST )
                {
                    return false;
                }
            }

            return directory.empty() ||
                   ::mkdir( directory.c_str(), 0755 ) == 0 ||
                   errno == EEXIST;
        }

        inline void outputLogFile( const std::string& message_in, const std::string& fullpath_in )
        {
            if( fullpath_in.empty() || !createDirectoryRecursive( fullpath_in ) )
            {
                return;
            }

            std::ofstream logfile( fullpath_in, std::ios::app | std::ios::binary );
            if( logfile )
            {
                logfile << message_in << '\n';
            }
        }
    }
}

#endif  // WONDERSTEWENGINE_PLATFORM_LINUX_UTILITY_LOGOUTPUT_H
