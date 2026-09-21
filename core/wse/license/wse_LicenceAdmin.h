//*****************************************************************************************************************
//! 
//! @file    wse_LicenceAdmin.h
//! @brief   \~japanese License種別（Type／Version）と管理Objectの内部宣言.
//! @brief   \~english  Internal declarations of the licence kinds (type, version) and the admin object.
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
#ifndef  WONDERSTEWENGINE_LICENCE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_RAKAKARUIAIHANOKI_OHIWAHURAK
#define  WONDERSTEWENGINE_LICENCE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_RAKAKARUIAIHANOKI_OHIWAHURAK

#include <wse/stew.h>

#include <stdexcept>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)  // ../
#endif
namespace wse
{
    namespace license
    {
        enum class eType : uint8_t
        {
                Free            = 0
            ,   ExpirationDate  = 1
            ,   DeviceDependent = 2
        };
        enum class eVer : uint8_t
        {
                Alpha  = 0
            ,   Beta   = 1
            ,   Stable = 2
            ,   Admin = 3
        };

        static inline std::string toString( const eType type_in )
        {
            switch( type_in )
            {
                case eType::Free            : return "Free           ";
                case eType::ExpirationDate  : return "ExpirationDate ";
                case eType::DeviceDependent : return "DeviceDependent";
                default: throw std::invalid_argument( "the license type value is outside the enumeration" );
            }
        }

        static inline std::string toString( const eVer ver_in )
        {
            switch( ver_in )
            {
                case eVer::Alpha  : return "Alpha";
                case eVer::Beta   : return "Beta";
                case eVer::Stable : return "Stable";
                case eVer::Admin  : return "Admin";
                default: throw std::invalid_argument( "the license version value is outside the enumeration" );
            }
        }


    }
    
        
    std::vector< uint8_t > encryptLicense( const std::vector< uint8_t >& data_in, const std::vector< uint8_t >& key_in );
    std::vector< uint8_t > decodeLicense ( const std::vector< uint8_t >& data_in, const std::vector< uint8_t >& key_in );


    LicenseStatus checkLicence( const license::eVer ver_in, const uint64_t version_num_in );

    bool isAlphaLicense ( void );

    bool isBetaLicense  ( void );

    bool isStableLicense( void );

    bool isAdminLicense ( void );


    void setLicenseConfig(
            const license::eType type_in
        ,   const license::eVer  version_in
        ,   const uint16_t       masor_in
        ,   const uint16_t       minor_in
        ,   const uint32_t       patch_in
    );

}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif //WONDERSTEWENGINE_LICENCE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
