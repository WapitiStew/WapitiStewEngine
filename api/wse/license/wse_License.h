//*****************************************************************************************************************
//! 
//! @file    wse_License.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese ライセンス情報の読み込み機能を提供するヘッダファイル。
//!     \~english  Header file providing license loading functionality.
//!
//! @details
//!     \~japanese 指定されたキーとファイルパスを用いてライセンスファイルを読み込み、認証処理を行う。
//!     \~english  Loads and verifies a license file using the specified key and file path.
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
#ifndef  WONDERSTEWENGINE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
#define  WONDERSTEWENGINE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
#include "../depend/wse_STD.h"
#include "../depend/wse_Constant.h"
#include "../error/LicenseError.h"
#include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace wse
{
    //!
    //! @class License
    //! @brief
    //!     \~japanese ライセンスファイルの読み込み・認証を行うクラス。
    //!     \~english  Class for loading and verifying license files.
    //!
    //! @details
    //!     \~japanese 本クラスは、バイナリ形式のライセンスキーを指定し、
    //!         オプションとしてファイルパスやファイル名を設定することで、ライセンスの読み込みを行う。
    //!         読み込んだライセンスはアプリケーションの認証に使用される。
    //!
    //!     \~english  This class loads and verifies license files using a binary key.
    //!         Optionally accepts a path and name for the license file. The license data is used for authentication.
    //!
    class WSE_API License
    {
        //! @brief
        //!     \~japanese ライセンスファイルを読み込む。
        //!     \~english  Load license file.
        //!
        //! @param[in] key_in  Binary license key.
        //! @param[in] path_in Optional file path.
        //! @param[in] name_in Optional file name. Defaults to DEFAULT_LICENSE_NAME.
        //!
        //! @return
        //!     \~japanese 成功`LicenseStatus`. 期限外は`Verification/LicenseExpired`,
        //!                非対応Deviceは`Verification/UnlicensedDevice`で失敗する. License Fileの
        //!                不在は失敗ではなくFree Licenseとして成功する.
        //!     \~english  The `LicenseStatus`. An out-of-period license fails with
        //!                `Verification/LicenseExpired` and an unpermitted device with
        //!                `Verification/UnlicensedDevice`. A missing license file is not a
        //!                failure; it loads as the Free license.
        //!
        //! @exception std::logic_error
        //!     \~japanese すでにLicenseをLoad済みの場合.
        //!     \~english  When a license is already loaded.
        //!
        public: static LicenseStatus load( 
                const std::vector< uint8_t >& key_in
            ,   const std::string& path_in = ""
            ,   const std::string& name_in =DEFAULT_LICENSE_NAME
        );
    };
}

#endif //WONDERSTEWENGINE_WSELICENCE_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
