//*****************************************************************************************************************
//! 
//! @file    wse_LogOutput.cpp
//! @brief   \~japanese ConsoleへのUTF-8出力と、階層を作りながらのLog file追記をWindows APIで実装する.
//! @brief   \~english  Implements UTF-8 console output and directory-creating log-file appends with the Windows API.
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

#include <iostream>
#include <fstream>
#include <string>



#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#pragma warning(push)
#pragma warning(disable: 4244)  // ../
#pragma warning(disable: 4464)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
#pragma warning(disable: 4668)  // ../
#pragma warning(disable: 4820)  // ../
#pragma warning(disable: 5039)  // ../
#pragma warning(disable: 5204)  // ../
        #include <windows.h>
#pragma warning(pop)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <errno.h>
    #include <unistd.h>
#endif


#pragma warning(push)
#pragma warning(disable: 4505)  // 
namespace wse
{
    namespace log
    {
        void outputConsole( const std::string& str_in )
        {
            // WSEの文字列は全てUTF-8なので、Code pageを合わせてからでないと日本語Logが化ける.
            SetConsoleOutputCP(CP_UTF8);
            std::cout << str_in;
        }
        

        // ディレクトリを作成する関数 (クロスプラットフォーム対応)
        static bool createDirectory(const std::string& path_in) 
        {
            if ( CreateDirectoryA(path_in.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS ) 
            {
                return true; // 成功または既に存在
            }
            return false; // 失敗
        }

        // ファイルが存在するか確認する関数
        static bool fileExists(const std::string& path_in)
        {
            return GetFileAttributesA( path_in.c_str() ) != INVALID_FILE_ATTRIBUTES;
        }

        // ディレクトリを再帰的に作成する関数（ファイルがある階層まで作成）
        static bool createDirectoryRecursive(const std::string& path_in) 
        {
            size_t lastSlash = path_in.find_last_of("/\\");
            if (lastSlash == std::string::npos) 
            {
                return true; // 階層がない場合は何もしない
            }

            std::string dirPath = path_in.substr(0, lastSlash); // ファイルを除いたディレクトリ部分
            size_t pos = 0;
            std::string currentPath;

            while ( ( pos = dirPath.find_first_of("/\\", pos) ) != std::string::npos ) 
            {
                currentPath = dirPath.substr(0, pos);
                ++pos; // 次の位置へ進む
                if (!CreateDirectoryA(currentPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }
            if (!CreateDirectoryA(dirPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                return false;
            }
            return true;
        }

        // ファイルに追記または新規作成する
        void outputLogFile(const std::string&message_in , const std::string&fullpath_in )
        {
            size_t last_slash = fullpath_in.find_last_of("/\\");
            if (last_slash == std::string::npos) {
                std::cerr << "Invalid path: " << fullpath_in << std::endl;
                return;
            }

            std::string dir_path = fullpath_in.substr(0, last_slash); // ディレクトリ部分
            std::string filePath = fullpath_in; // ファイル名を含むパス

            if (!createDirectoryRecursive(fullpath_in))
            {
                return;
            }

            std::ofstream logfile( fullpath_in, std::ios::app | std::ios::binary );
            if (!logfile) 
            {
                return;
            }

            logfile << message_in << "\n";
            logfile.close();
        }

    }
}
#pragma warning(pop)

#endif   //WONDERSTEWENGINE_DEPEND_EXCEPTION_H

