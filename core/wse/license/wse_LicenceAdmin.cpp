//*****************************************************************************************************************
//! 
//! @file    wse_LicenceAdmin.cpp
//! @brief   \~japanese License dataの暗号化・復号と、読み込んだLicense種別の保持・照会の実装.
//! @brief   \~english  Implements licence-data encryption/decryption and holds and answers the loaded licence kind.
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
#include "wse_LicenceAdmin.h"

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4242)  // 
    #pragma warning(disable: 4244)  // 
    #pragma warning(disable: 4514)  // 
    #pragma warning(disable: 4820)  // 
#endif
namespace wse
{
    //namespace license
    //{
    //    static constexpr eType    LIBRARY_TYPE      = eType::Free;
    //    static constexpr eVer     LIBRARY_VERSION   = eVer::Alpha;
    //    static constexpr uint32_t MASOR_NUM = 0;
    //    static constexpr uint32_t MINOR_NUM = 0;
    //    static constexpr uint32_t PATCH_NUM = 0;
    //}

    class LicenceAdmin
    {
        private: license::eType m_type      ;
        private: license::eVer  m_version   ;
        private: uint16_t       m_masor   ;
        private: uint16_t       m_minor     ;
        private: uint32_t       m_patch     ;
        private: bool           m_is_already ;


        public: license::eType type     ( void ) const { return this->m_type     ; }
        public: license::eVer  version  ( void ) const 
        { 
            return this->m_version  ; 
        }
        public: uint64_t       version_num( void )const 
        { 
            return static_cast< uint64_t >( this->m_masor ) * 10000000000000 
                 + static_cast< uint64_t >( this->m_minor ) * 100000000 
                 + static_cast< uint64_t >( this->m_patch );
        }


        public: uint32_t       patch    ( void ) const { return this->m_patch    ; }
        public: bool           is_already( void ) const { return this->m_is_already ; }



        public :  LicenceAdmin( void );
        public : ~LicenceAdmin( void ){}

        public : LicenceAdmin             ( const LicenceAdmin&  obj_in    ) = delete;
        public : LicenceAdmin             (       LicenceAdmin&& obj_inout ) = delete;
        public : LicenceAdmin& operator = ( const LicenceAdmin&  obj_in    ) = delete;
        public : LicenceAdmin& operator = (       LicenceAdmin&& obj_inout ) = delete;

        public : void set(
                const license::eType type_in
            ,   const license::eVer  version_in
            ,   const uint16_t       masor_in
            ,   const uint16_t       minor_in
            ,   const uint32_t       patch_in
        );

        public : LicenseStatus checkAlpha( const uint64_t version_in )const;
        public : LicenseStatus checkBeta ( const uint64_t version_in )const;
        public : LicenseStatus checkStable( const uint64_t version_in );
        public : LicenseStatus checkAdmin( const uint64_t version_in );
    
    };
    LicenceAdmin g_licence_admin;
    
    
    std::vector< uint8_t > encryptLicense( const std::vector< uint8_t >& data_in, const std::vector< uint8_t >& key_in )
    {
        std::vector< uint8_t > encrypted_data;

        size_t key_i = 0;
        for( const uint8_t d : data_in )
        {
            encrypted_data.emplace_back( ( d ^ key_in[key_i] ) );
            ++key_i;
            if( key_i == key_in.size() ){ key_i = 0; }
        }
        return encrypted_data;
    }

    std::vector< uint8_t > decodeLicense( const std::vector< uint8_t >& data_in, const std::vector< uint8_t >& key_in )
    {
        std::vector< uint8_t > encrypted_data;

        size_t key_i = 0;
        for( const uint8_t d : data_in )
        {
            encrypted_data.emplace_back( ( d ) ^ key_in[key_i] );
            ++key_i;
            if( key_i == key_in.size() ){ key_i = 0; }
        }
        return encrypted_data;
    }


    LicenseStatus checkLicence( const license::eVer ver_in, const uint64_t version_num_in )
    {
        switch( ver_in )
        {
            // A向け機能,
            case license::eVer::Alpha :
            {
                return g_licence_admin.checkAlpha( version_num_in );
            }
            // B向け機能,
            case license::eVer::Beta :
            {
                return g_licence_admin.checkBeta( version_num_in );
            }
            // Stable向け機能,
            case license::eVer::Stable :
            {
                return g_licence_admin.checkStable( version_num_in );
            }
            // Admin向け機能,
            case license::eVer::Admin :
            {
                return g_licence_admin.checkStable( version_num_in );
            }
            default:
            break;
        }
        return LicenseStatus::success();
    }

    
    bool isAlphaLicense (  ){ return g_licence_admin.version() == license::eVer::Alpha ; }
    bool isBetaLicense  (  ){ return g_licence_admin.version() == license::eVer::Beta  ; }
    bool isStableLicense(  ){ return g_licence_admin.version() == license::eVer::Stable; }
    bool isAdminLicense (  ){ return g_licence_admin.version() == license::eVer::Admin ; }




    void setLicenseConfig( 
            const license::eType type_in
        ,   const license::eVer  version_in
        ,   const uint16_t       masor_in
        ,   const uint16_t       minor_in
        ,   const uint32_t       patch_in
    )
    {
        g_licence_admin.set( type_in, version_in, masor_in, minor_in, patch_in );
    }


    LicenceAdmin::LicenceAdmin( void )
        : m_type       ( license::eType::Free )
        , m_version    ( license::eVer ::Alpha )
        , m_masor      ( 0 )
        , m_minor      ( 0 )
        , m_patch      ( 0 )
        , m_is_already ( false )
    {
    }
    void LicenceAdmin::set  ( 
            const license::eType type_in
        ,   const license::eVer  version_in
        ,   const uint16_t       masor_in
        ,   const uint16_t       minor_in
        ,   const uint32_t       patch_in
    )
    {
        if( this->is_already() )
        {
            throw std::logic_error( "a license is already loaded" );
        }

        this->m_type    = type_in    ;
        this->m_version = version_in ;
        this->m_masor = masor_in   ;
        this->m_minor   = minor_in   ;
        this->m_patch   = patch_in   ;
        this->m_is_already = true;

    }
    

    LicenseStatus LicenceAdmin::checkAlpha( const uint64_t version_in )const
    {

        // 無償版なら例外を投げる.このコードは有償版でのみ使用する.
        if( g_licence_admin.type() == license::eType::Free )
        { 
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableLicense,
                "the license does not permit this feature" ) );
        }

        // 有償版の場合バージョンチェックする.Alpha版はメジャーバージョンのみチェック.
        const uint64_t admin_version = static_cast< uint64_t >( this->m_masor ) * 10000000000000U;
        if( admin_version < version_in )
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableVersion,
                "the license does not permit this version" ) );
        }
    
        return LicenseStatus::success();
    }
    LicenseStatus LicenceAdmin::checkBeta ( const uint64_t version_in )const
    {
        // 無償版なら例外を投げる.このコードは有償版でのみ使用する.
        if( g_licence_admin.type() == license::eType::Free )
        { 
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableLicense,
                "the license does not permit this feature" ) );
        }

        // 有償版の場合バージョンチェックする.Alpha版はメジャーバージョンのみチェック.
        const uint64_t admin_version = static_cast< uint64_t >( this->m_masor ) * 10000000000000U +static_cast< uint64_t >( this->m_minor ) * 100000000U;
        if( admin_version < version_in )
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableVersion,
                "the license does not permit this version" ) );
        }
    
        return LicenseStatus::success();
    }
    LicenseStatus LicenceAdmin::checkStable  ( const uint64_t version_in )
    {
        // 無償版なら例外を投げる.このコードは有償版でのみ使用する.
        if( g_licence_admin.type() == license::eType::Free )
        { 
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableLicense,
                "the license does not permit this feature" ) );
        }
        
        // ライブラリ開発者向け.
        if( g_licence_admin.version() == license::eVer::Admin ){ return LicenseStatus::success(); }

        // 有償版の場合バージョンチェックする.
        if( g_licence_admin.version_num() < version_in )
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableVersion,
                "the license does not permit this version" ) );
        }
    
        return LicenseStatus::success();
    }

    LicenseStatus LicenceAdmin::checkAdmin  ( const uint64_t version_in )
    {
        const LicenseStatus stable = checkStable( version_in );
        if( !stable.succeeded() )
        {
            return stable;
        }
        // 有償版の場合バージョンチェックする.
        if( g_licence_admin.version() != license::eVer::Admin )
        {
            return LicenseStatus::failure( LicenseError(
                eLicenseErrorCategory::Verification, eLicenseErrorCode::UnusableVersion,
                "the license does not permit this version" ) );
        }
    
        return LicenseStatus::success();
    }
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
