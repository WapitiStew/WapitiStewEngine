//*****************************************************************************************************************
//! 
//! @file    wse_LicenseWriter.h
//! @brief   ライセンス情報を書き込むするクラス.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @brief
//!     \~japanese ライセンス設定を定義し、バイナリファイルに保存するためのクラスを提供するヘッダファイル。
//!     \~english  Header file providing license configuration definitions and functionality for saving them as binary files.
//!
//!
//! @details
//!     \~japanese 本ファイルは、ライセンス種別、バージョン、期間などを設定し、暗号化されたキーと共にファイルとして出力する機能を提供する。
//!     \~english  This file provides functionality to configure license type, version, and validity period, and save them with an encrypted key.
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
#ifndef  WONDERSTEWENGINE_WSELICENCE_WRITER_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
#define  WONDERSTEWENGINE_WSELICENCE_WRITER_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
#include <utility>
#include "../depend/wse_STD.h"
#include "../depend/wse_Constant.h"
#include "../error/LicenseError.h"
#include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4820)  // 
#endif
namespace wse
{

    //!
    //! @class LicenseWriter
    //! @brief
    //!     \~japanese ライセンス情報の定義および書き込みを行うクラス。
    //!     \~english  Class for defining and writing license information.
    //!
    //! @details
    //!     \~japanese ライセンス種別やバージョン、期限などの構成情報を設定し、バイナリファイルに保存するための機能を提供する。
    //!     \~english  Provides functionality to configure license type, version, and expiration, and to save the license as a binary file.
    //!
    class WSE_API LicenseWriter
    {

        //!
        //! @brief
        //!     \~japanese ライセンスの種類を示す列挙体。
        //!     \~english  Enumeration representing license types.
        //!
        public: enum class eLicenseType : uint8_t
        {
                Free            = 0     //!< \~japanese 無償ライセンス            \~english Free license.
            ,   ExpirationDate  = 1     //!< \~japanese 有効期限付きライセンス    \~english License with expiration date.
            ,   DeviceDependent = 2     //!< \~japanese デバイス依存ライセンス     \~english Device-dependent license.
        };

        //!
        //! @brief
        //!     \~japanese ライセンスのバージョンを示す列挙体。
        //!     \~english  Enumeration representing license version levels.
        //!
        public: enum class eLicenseVersion : uint8_t
        {
                Alpha  = 0       //!< \~japanese アルファ版  \~english Alpha version.
            ,   Beta   = 1       //!< \~japanese ベータ版    \~english Beta version.
            ,   Stable = 2       //!< \~japanese 安定版      \~english Stable version.
            ,   Admin  = 3       //!< \~japanese 管理者用     \~english Administrator version.
        };

        //!
        //! @brief
        //!     \~japanese ライセンス構成を保持する構造体。
        //!     \~english  Structure holding license configuration information.
        //!
        struct sConfig
        {
            eLicenseType type;           //!< \~japanese ライセンスタイプ         \~english License type.
            eLicenseVersion version;           //!< \~japanese ライセンスバージョン     \~english License version.
            uint16_t masor;                     //!< \~japanese メジャーバージョン番号   \~english Major version.
            uint16_t minor;                     //!< \~japanese マイナーバージョン番号   \~english Minor version.
            uint32_t patch;                     //!< \~japanese パッチバージョン番号     \~english Patch version.
            uint16_t limit_year;                     //!< \~japanese 有効期限（年）           \~english Expiration year.
            uint8_t  limit_month;                     //!< \~japanese 有効期限（月）           \~english Expiration month.
            uint8_t  limit_date;                     //!< \~japanese 有効期限（日）           \~english Expiration date.
            uint16_t start_year;                     //!< \~japanese 開始日（年）             \~english Start year.
            uint8_t  start_month;                     //!< \~japanese 開始日（月）             \~english Start month.
            uint8_t  start_date;                     //!< \~japanese 開始日（日）             \~english Start date.
            std::string path;                    //!< \~japanese 出力パス                 \~english Output path.
            std::string name;  //!< \~japanese 出力ファイル名           \~english Output file name.

            //! @brief Construct all members with explicit defaults.
            sConfig(
                  eLicenseType type_in = eLicenseType::Free
                , eLicenseVersion version_in = eLicenseVersion::Admin
                , uint16_t masor_in = 0
                , uint16_t minor_in = 0
                , uint32_t patch_in = 0
                , uint16_t limit_year_in = 0
                , uint8_t limit_month_in = 0
                , uint8_t limit_date_in = 0
                , uint16_t start_year_in = 0
                , uint8_t start_month_in = 0
                , uint8_t start_date_in = 0
                , const std::string& path_in = ""
                , const std::string& name_in = DEFAULT_LICENSE_NAME
            )
                : type        ( type_in )
                , version     ( version_in )
                , masor       ( masor_in )
                , minor       ( minor_in )
                , patch       ( patch_in )
                , limit_year  ( limit_year_in )
                , limit_month ( limit_month_in )
                , limit_date  ( limit_date_in )
                , start_year  ( start_year_in )
                , start_month ( start_month_in )
                , start_date  ( start_date_in )
                , path        ( path_in )
                , name        ( name_in )
            {
            }
        };

        //!
        //! @brief
        //!     \~japanese 指定されたキーとライセンス構成情報を元にライセンスファイルを保存する。
        //!     \~english  Save license file using the given key and license configuration.
        //!
        //! @param[in] key_in    \~japanese 暗号化キー                  \~english Encryption key.
        //! @param[in] config_in \~japanese ライセンス構成情報          \~english License configuration.
        //!
        //! @return
        //!     \~japanese 成功`LicenseStatus`. 開けないFileは`Io/FileOpenFailed`で失敗する.
        //!     \~english  The `LicenseStatus`; a file that cannot be opened fails with `Io/FileOpenFailed`.
        //!
        //! @exception std::invalid_argument
        //!     \~japanese `sConfig`の`type`／`version`が列挙値の範囲外の場合.
        //!     \~english  When the `sConfig` `type` / `version` value is outside the enumeration.
        //!
        public: static LicenseStatus save( const std::vector< uint8_t >&key_in, const sConfig& config_in );


    };
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_WSELICENCE_WRITER_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
