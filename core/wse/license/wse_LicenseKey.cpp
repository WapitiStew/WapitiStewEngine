//*****************************************************************************************************************
//! 
//! @file    wse_LicenseKey.cpp
//! @brief   \~japanese License keyの生成と保存の実装.
//! @brief   \~english  Implements licence-key generation and saving.
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
#include <wse/license/wse_LicenseKey.h>

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
        #include "../../../platform/wse/wse_platform.h"
        #include "../../../api/wse/utility/wse_Log.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <iostream>
#include <chrono>
#include <ctime>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>

#include "wse_LicenceAdmin.h"



#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
namespace wse
{





    LicenseStatus Licensekey::genrate( const std::string& path_in, const std::string& name_in )
    {
        std::vector< uint8_t > keydata = {
                static_cast< uint8_t >( std::rand() % ( 225 )  )
            ,   static_cast< uint8_t >( std::rand() % ( 185 )  )
            ,   static_cast< uint8_t >( std::rand() % ( 128 )  )
            ,   static_cast< uint8_t >( std::rand() % (  55 )  )
            ,   static_cast< uint8_t >( std::rand() % ( 111 )  )
            ,   static_cast< uint8_t >( std::rand() % ( 255 )  )
        };

        const std::string fullpath = ( path_in.empty() ) ? name_in : path_in + "/" + name_in;

        // ファイルの存在確認
        // パスを正規化
        std::string normalized_path = normalizePath( fullpath );
        std::ifstream inFile(normalized_path);
        DLog() << "path_in" << normalized_path;
        if(inFile.good()) 
        {  // ファイルが存在する場合
            inFile.close();   // 明示的にクローズ
            std::remove(normalized_path.c_str());
        }
    
        // 新規作成して書き込み
        DLog() << "create";
        std::ofstream licence_file( normalized_path, std::ios::out );
        if (!licence_file)
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Io, eLicenseErrorCode::FileOpenFailed,
                "the license key file cannot be opened" ) );
        }

        std::string license_content;
        DLog() << "contents";
        for( const uint8_t data : keydata )
        {
            license_content += static_cast< char >( data );
        }
        
        DLog() << "write";
        licence_file << license_content;

        licence_file.close();

        return LicenseStatus::success();
    }
    LicenseResult< std::vector< uint8_t > > Licensekey::load   ( const std::string& path_in, const std::string& name_in )
    {
        DDLog() << "load";
        
        const std::string fullpath = ( path_in.empty() ) ? name_in : path_in + "/" + name_in;
        
        // パスを正規化
        std::string normalized_path = normalizePath( fullpath );
        DDLog() << "load" << fullpath;
        // バイナリモードでファイルを開く
        std::ifstream file(normalized_path, std::ios::binary);
        if ( !file )
        {
            return LicenseResult< std::vector< uint8_t > >::failure( LicenseError(
                eLicenseErrorCategory::Io, eLicenseErrorCode::FileOpenFailed,
                "the license key file cannot be opened" ) );
        }

        // ファイルのサイズを取得
        file.seekg(0, std::ios::end);
        std::streamsize size = static_cast<int64_t>( file.tellg() );
        file.seekg(0, std::ios::beg);

        // バッファをサイズに合わせてリサイズ
        std::vector< uint8_t > buffer( static_cast<size_t>(size), 0 );

        // ファイルの内容をバッファに読み込む
        if (!file.read( reinterpret_cast< char* >( buffer.data() ), size ) ) 
        {
            return LicenseResult< std::vector< uint8_t > >::failure( LicenseError(
                eLicenseErrorCategory::Io, eLicenseErrorCode::ReadFailed,
                "the license key file cannot be read" ) );
        }
        DDLog() << "success";
        return LicenseResult< std::vector< uint8_t > >::success( buffer );
    }

}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
