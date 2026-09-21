//*****************************************************************************************************************
//! 
//! @file    wse_License.cpp
//! @brief   \~japanese Licenseの検証と読み取りの実装.
//! @brief   \~english  Implements licence validation and reading.
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
#include <wse/license/wse_License.h>

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

// プラットフォーム依存
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4242)  // ../
#pragma warning(disable: 4464)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
#pragma warning(disable: 4668)  // ../
#pragma warning(disable: 4820)  // ../
#pragma warning(disable: 5039)  // ../
#pragma warning(disable: 5045)  // ../
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
#pragma warning(disable: 5045)
#endif
namespace wse
{
    
    namespace license
    {
        static void current_time( uint16_t *p_year_out, uint8_t *p_month_out, uint8_t *p_date_out );

        static std::vector< uint8_t > loadLicence( const std::string& path_in, const std::string& name_in );


        static LicenseStatus analyzeLicence(
                      license::eType*         p_type_out
            ,         license::eVer*          p_ver_out
            ,         uint16_t*               p_masor_out
            ,         uint16_t*               p_minor_out
            ,         uint32_t*               p_patch_out
            ,   const std::vector< uint8_t >& licence_data_in
            ,   const std::string&            mac_address_in
        );
    }

    
    LicenseStatus License::load( const std::vector< uint8_t >& key_in, const std::string& path_in, const std::string& name_in )
    {
        DDLog() << "load";
        const std::string           mac_address     = license::mac_address();

        const std::vector<uint8_t> binary          = license::loadLicence( path_in, name_in );
        const std::vector<uint8_t> decoded_license = decodeLicense( binary, key_in );

        license::eType type = license::eType::Free;
        license::eVer  ver  = license::eVer::Admin;
        uint16_t       masor=0;
        uint16_t       minor=0;
        uint32_t       patch=0;
        const LicenseStatus analyzed =
            license::analyzeLicence( &type, &ver, &masor, &minor, &patch, decoded_license, mac_address );
        if( !analyzed.succeeded() )
        {
            return analyzed;
        }
        
        DDLog() << "type" << toString( type );
        DDLog() << "version" << toString( ver  ) << std::to_string( masor ) + "." + std::to_string( minor ) + "." + std::to_string( patch );

        setLicenseConfig( type, ver, masor, minor, patch );
        DDLog() << "success";
        return LicenseStatus::success();
    }


    
    void license::current_time( uint16_t *p_year_out, uint8_t *p_month_out, uint8_t *p_date_out )
    {
        // 現在の時刻を取得
        auto now = std::chrono::system_clock::now();
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

        // 時刻をtm構造体に変換
        std::tm local_tm;

        #ifdef _WIN32
            // Windowsではstd::localtime_sを使用（スレッドセーフ）
            localtime_s(&local_tm, &now_time_t);
        #else
            // POSIX系（Linux, macOS, Android, iOS, ChromeOSなど）ではstd::localtime_rを使用（スレッドセーフ）
            localtime_r(&now_time_t, &local_tm);
        #endif

        // 年, 月, 日を取得
        int tmp_year    = local_tm.tm_year + 1900;  // tm_year は 1900 年を基準にしているため 1900 を足す
        int tmp_month   = local_tm.tm_mon + 1;     // tm_mon は 0 から 11 なので 1 を足す
        int tmp_day     = local_tm.tm_mday;

        uint16_t &year  =*p_year_out ;
        uint8_t  &month =*p_month_out;
        uint8_t  &date  =*p_date_out ;
        year  = static_cast< uint16_t >( tmp_year  );
        month = static_cast< uint8_t  >( tmp_month );
        date  = static_cast< uint8_t  >( tmp_day   );

    }

    //!
    std::vector< uint8_t > license::loadLicence( const std::string& path_in, const std::string& name_in )
    {
        const std::string fullpath = ( path_in.empty() ) ? name_in : path_in + "/" + name_in;
        
        // パスを正規化
        std::string normalized_path = normalizePath( fullpath );

        // バイナリモードでファイルを開く
        std::ifstream file(normalized_path, std::ios::binary);

        if ( !file )
        {
            return std::vector< uint8_t >();
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
            return std::vector< uint8_t >();
        }
        return buffer;
    }


    //!
    LicenseStatus license::analyzeLicence(
                  license::eType*         p_type_out
        ,         license::eVer*          p_ver_out
        ,         uint16_t*               p_masor_out
        ,         uint16_t*               p_minor_out
        ,         uint32_t*               p_patch_out
        ,   const std::vector< uint8_t >& licence_data_in
        ,   const std::string&            mac_address_in
    )
    {
        license::eType& type   = *p_type_out ;
        license::eVer & ver    = *p_ver_out  ;
        uint16_t&       masor  = *p_masor_out;
        uint16_t&       minor  = *p_minor_out;
        uint32_t&       patch  = *p_patch_out;

        if( licence_data_in.empty() )
        {
            type  = license::eType::Free;
            ver   = license::eVer::Alpha;
            masor = 0;
            minor = 0;
            patch = 0;
            return LicenseStatus::success();
        }

        DLog() << licence_data_in.size();
        type  = static_cast< license::eType >( licence_data_in[ 0 ] );
        ver   = static_cast< license::eVer  >( licence_data_in[ 1 ] );

        // masor.
        masor = static_cast<uint16_t>( 
            ( ( static_cast<uint16_t>( static_cast< uint16_t >( ( licence_data_in[ 2 ] ) << 8 ) & 0xFF00 ) )
        +   ( static_cast<uint16_t>( static_cast< uint16_t >( ( licence_data_in[ 3 ] )      ) & 0x00FF ) ) ) );

        // minor.
        minor = static_cast<uint16_t>( 
            ( ( static_cast<uint16_t>( static_cast< uint16_t >( ( licence_data_in[ 4 ] ) << 8 ) & 0xFF00 ) )
        +   ( static_cast<uint16_t>( static_cast< uint16_t >( ( licence_data_in[ 5 ] )      ) & 0x00FF ) ) ) );
        
        // patch.
        patch = static_cast<uint32_t>( 
            ( ( static_cast<uint32_t>( static_cast< uint32_t >( ( licence_data_in[ 6 ] ) << 24 ) & 0xFF000000 ) )
          +   ( static_cast<uint32_t>( static_cast< uint32_t >( ( licence_data_in[ 7 ] ) << 16 ) & 0x00FF0000 ) )
          +   ( static_cast<uint32_t>( static_cast< uint32_t >( ( licence_data_in[ 8 ] ) <<  8 ) & 0x0000FF00 ) )
          +   ( static_cast<uint32_t>( static_cast< uint32_t >( ( licence_data_in[ 9 ] )       ) & 0x000000FF ) ) ) );
        
        DDLog() << "type" << toString( type );
        DDLog() << "version" << toString( ver  ) << std::to_string( masor ) + "." + std::to_string( minor ) + "." + std::to_string( patch );

        // 期限付きライセンス.
        if( type == license::eType::ExpirationDate )
        {
            DLog() << "ExpirationDate";
            // limit.
            const uint16_t limit_year   = static_cast< uint16_t >(
                ( ( static_cast< uint16_t >( static_cast< uint16_t >( ( licence_data_in[ 10 ] ) << 8 ) & 0xFF00 ) )
            +   ( static_cast< uint16_t >( static_cast< uint16_t >( ( licence_data_in[ 11 ] )      ) & 0x00FF ) ) ) );
            const uint8_t  limit_month = static_cast< uint8_t >( static_cast< uint8_t >( licence_data_in[ 12 ] ) );
            const uint8_t  limit_date  = static_cast< uint8_t >( static_cast< uint8_t >( licence_data_in[ 13 ] ) );
            
            // play.
            const uint16_t start_year   = static_cast< uint16_t >(
                ( ( static_cast< uint16_t >( static_cast< uint16_t >( ( licence_data_in[ 14 ] ) << 8 ) & 0xFF00 ) )
            +   ( static_cast< uint16_t >( static_cast< uint16_t >( ( licence_data_in[ 15 ] )      ) & 0x00FF ) ) ) );
            const uint8_t  start_month = static_cast< uint8_t >( licence_data_in[ 16 ] );
            const uint8_t  start_date  = static_cast< uint8_t >( licence_data_in[ 17 ] );

            // Local time
            uint16_t current_year ;
            uint8_t  current_month;
            uint8_t  current_date ;
            license::current_time( &current_year, &current_month, &current_date );


            const uint64_t limit   = static_cast< uint64_t >( limit_year   * 10000 ) + static_cast< uint64_t >( limit_month   * 100 ) + static_cast< uint64_t >( limit_date   );
            const uint64_t play    = static_cast< uint64_t >( start_year   * 10000 ) + static_cast< uint64_t >( start_month   * 100 ) + static_cast< uint64_t >( start_date   );
            const uint64_t current = static_cast< uint64_t >( current_year * 10000 ) + static_cast< uint64_t >( current_month * 100 ) + static_cast< uint64_t >( current_date );

            DLog() << "limit  " << std::to_string(limit  );
            DLog() << "start  " << std::to_string(play  );
            DLog() << "current" << std::to_string(current);


            // 有効期限切れの場合.
            if( current < play || current > limit )
            {
                type = license::eType::Free;
                return LicenseStatus::failure( LicenseError(
                    eLicenseErrorCategory::Verification, eLicenseErrorCode::LicenseExpired,
                    "the license is outside its validity period" ) );
            }
        }
            
        // デバイス依存ライセンス.
        else if( type == license::eType::DeviceDependent )
        {
            DLog() << "DeviceDependent";
            // マックアドレス不明.
            if( mac_address_in.empty() )
            {
                type  = license::eType::Free;
                ver   = license::eVer::Alpha;
                masor = 0;
                minor = 0;
                patch = 0;
                return LicenseStatus::failure( LicenseError(
                    eLicenseErrorCategory::Verification, eLicenseErrorCode::UnlicensedDevice,
                    "the device identity cannot be determined" ) );
            }
            else{}
            
            // mac_adress
            std::string license_mac_addr = "";
            for( size_t i = 18; i < licence_data_in.size(); ++i )
            {
                license_mac_addr += static_cast< char >( licence_data_in[i] );
            }
            
            DLog() << "this device    " << mac_address_in;
            DLog() << "license device " << license_mac_addr;
            if( license_mac_addr != mac_address_in )
            {
                type  = license::eType::Free;
                ver   = license::eVer::Alpha;
                masor = 0;
                minor = 0;
                patch = 0;
                return LicenseStatus::failure( LicenseError(
                    eLicenseErrorCategory::Verification, eLicenseErrorCode::UnlicensedDevice,
                    "the license does not permit this device" ) );
            }
        }

        return LicenseStatus::success();
    }
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
