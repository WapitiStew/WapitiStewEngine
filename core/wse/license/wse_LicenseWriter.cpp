//*****************************************************************************************************************
//! 
//! @file    wse_LicenseWriter.cpp
//! @brief   \~japanese License fileの生成と保存の実装.
//! @brief   \~english  Implements licence-file generation and saving.
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
#include <wse/license/wse_LicenseWriter.h>
#include <wse/license/wse_LicenseKey.h>

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
        #include "../../../platform/wse/wse_platform.h"
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
#include <stdexcept>

// プラットフォーム依存
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4464)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
#pragma warning(disable: 4668)  // ../
#pragma warning(disable: 4820)  // ../
#pragma warning(disable: 5039)  // ../
#pragma warning(disable: 5204)  // ../
    #include <windows.h>
#pragma warning(pop)

#elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__) || defined(__CHROMEOS__) || defined(__IOS__)
    #include <unistd.h>
    #include <time.h>
#endif

#include "wse_LicenceAdmin.h"



#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
namespace wse
{
    
    namespace license
    {
        static LicenseStatus writeLicence(
                const std::string&              path_in
            ,   const std::string&              name_in
            ,   const std::vector< uint8_t >&   licence_data_in
        );


        static std::vector< uint8_t > createLicense(
                const license::eType          type_in
            ,   const license::eVer           ver_in
            ,   const uint16_t                masor_in
            ,   const uint16_t                minor_in
            ,   const uint32_t                patch_in
            ,   const uint16_t                limit_year_in
            ,   const uint8_t                 limit_month_in
            ,   const uint8_t                 limit_date_in
            ,   const uint16_t                start_year_in
            ,   const uint8_t                 start_month_in
            ,   const uint8_t                 start_date_in
            ,   const std::string&            mac_address_in
        );

        static license::eType toLicenseAdmin( const LicenseWriter::eLicenseType type_in )
        {
            switch( type_in )
            {
                case LicenseWriter::eLicenseType::Free             : return license::eType::Free;
                case LicenseWriter::eLicenseType::ExpirationDate   : return license::eType::ExpirationDate ;
                case LicenseWriter::eLicenseType::DeviceDependent  : return license::eType::DeviceDependent;
                default : break;
            }
            throw std::invalid_argument( "the license type value is outside the enumeration" );
        }
        static license::eVer  toLicenseAdmin( const LicenseWriter::eLicenseVersion  version_in )
        {
            switch( version_in )
            {
                case LicenseWriter::eLicenseVersion::Alpha   : return license::eVer::Alpha ;
                case LicenseWriter::eLicenseVersion::Beta    : return license::eVer::Beta  ;
                case LicenseWriter::eLicenseVersion::Stable  : return license::eVer::Stable;
                case LicenseWriter::eLicenseVersion::Admin  : return license::eVer::Admin;
                default : break;
            }
            throw std::invalid_argument( "the license version value is outside the enumeration" );
        }


    }
    
    LicenseStatus LicenseWriter::save( const std::vector< uint8_t >&key_in, const sConfig& config_in )
    {
        const std::string            mac_address     = license::mac_address();

        license::eType license_type    = license::toLicenseAdmin( config_in.type )   ;
        license::eVer  license_version = license::toLicenseAdmin( config_in.version );

        const std::vector< uint8_t > data = license::createLicense( 
                license_type, license_version, config_in.masor, config_in.minor, config_in.patch
            ,   config_in.limit_year, config_in.limit_month, config_in.limit_date
            ,   config_in.start_year, config_in.start_month, config_in.start_date
            ,   mac_address
        );

        const std::vector< uint8_t > encrypted_data = encryptLicense( data, key_in );

        return license::writeLicence( config_in.path, config_in.name, encrypted_data );
    }
    
    std::vector< uint8_t > license::createLicense( 
            const license::eType          type_in 
        ,   const license::eVer           ver_in  
        ,   const uint16_t                masor_in
        ,   const uint16_t                minor_in
        ,   const uint32_t                patch_in
        ,   const uint16_t                limit_year_in
        ,   const uint8_t                 limit_month_in
        ,   const uint8_t                 limit_date_in
        ,   const uint16_t                start_year_in
        ,   const uint8_t                 start_month_in
        ,   const uint8_t                 start_date_in
        ,   const std::string&            mac_address_in
    )
    {
        std::vector< uint8_t > license_data;

        // type.
        license_data.emplace_back( static_cast< uint8_t >( type_in ) );

        // version.
        license_data.emplace_back( static_cast< uint8_t >( ver_in  ) );

        // masor.
        license_data.emplace_back( static_cast<uint8_t>((masor_in >>  8) & 0xFF));
        license_data.emplace_back( static_cast<uint8_t>( masor_in        & 0xFF)); // 最下位バイト
        
        // minor.
        license_data.emplace_back( static_cast<uint8_t>((minor_in >>  8) & 0xFF));
        license_data.emplace_back( static_cast<uint8_t>( minor_in        & 0xFF)); // 最下位バイト

        // patch.
        license_data.emplace_back( static_cast<uint8_t>(( patch_in >> 24) & 0xFF));
        license_data.emplace_back( static_cast<uint8_t>(( patch_in >> 16) & 0xFF));
        license_data.emplace_back( static_cast<uint8_t>(( patch_in >>  8) & 0xFF));
        license_data.emplace_back( static_cast<uint8_t>(  patch_in        & 0xFF)); // 最下位バイト

        // year.
        license_data.emplace_back( static_cast< uint8_t >((limit_year_in >>  8) & 0xFF));
        license_data.emplace_back( static_cast< uint8_t >( limit_year_in        & 0xFF)); // 最下位バイト

        // month.
        license_data.emplace_back( static_cast< uint8_t >( limit_month_in ) );

        // date.
        license_data.emplace_back( static_cast< uint8_t >( limit_date_in  ) );

        
        // year.
        license_data.emplace_back( static_cast< uint8_t >((start_year_in >>  8) & 0xFF));
        license_data.emplace_back( static_cast< uint8_t >( start_year_in        & 0xFF)); // 最下位バイト

        // month.
        license_data.emplace_back( static_cast< uint8_t >( start_month_in ) );

        // date.
        license_data.emplace_back( static_cast< uint8_t >( start_date_in  ) );


        // mac_adress
        for( const char data : mac_address_in )
        {
            license_data.emplace_back( static_cast<uint8_t>( data ) );
        }

        return license_data;
    }
    
    


    LicenseStatus license::writeLicence( 
                const std::string&              path_in
            ,   const std::string&              name_in
            ,   const std::vector< uint8_t >&   licence_data_in
    )
    {
        const std::string fullpath = ( path_in.empty() ) ? name_in : path_in + "/" + name_in;

        // ファイルの存在確認
        
        // パスを正規化
        std::string normalized_path = normalizePath( fullpath );
        std::ifstream inFile(normalized_path);
        if(inFile.good()) 
        {  // ファイルが存在する場合
            inFile.close();   // 明示的にクローズ
            std::remove(normalized_path.c_str());
        }
    
        // 新規作成して書き込み
        std::ofstream licence_file( normalized_path, std::ios::out );
        if (!licence_file)
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Io, eLicenseErrorCode::FileOpenFailed,
                "the license file cannot be opened" ) );
        }

        std::string license_content;
        for( const uint8_t data : licence_data_in )
        {
            license_content += static_cast< char >( data );
        }

        licence_file << license_content;

        return LicenseStatus::success();
    }

}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
