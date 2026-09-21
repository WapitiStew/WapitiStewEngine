//*****************************************************************************************************************
//! 
//! @file    wse_LicenseKey.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New  WapitiStew.
//!
//!
//! @brief
//!     \~japanese ライセンスキーの生成および読み込みを提供するヘッダファイル。
//!     \~english  Header file providing license key generation and loading functionality.
//!
//!
//! @details
//!     \~japanese 本ファイルは、ライセンス認証に使用されるバイナリキーの生成および読み込みに関する機能を提供する。
//!         ファイル名やパスは省略可能であり、デフォルト値が用いられる。
//!     \~english  This file provides functionality for generating and loading binary license keys used in license authentication.
//!         The file name and path parameters are optional and default values are used.
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
#ifndef  WONDERSTEWENGINE_WSELICENCE_KEY_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
#define  WONDERSTEWENGINE_WSELICENCE_KEY_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI

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
    //! @class Licensekey
    //! @brief
    //!     \~japanese ライセンスキーの生成および読み込みを行うクラス。
    //!     \~english  Class for generating and loading license keys.
    //!
    //! @details
    //!     \~japanese 本クラスは、バイナリ形式のライセンスキーを生成し、既存のキーをファイルから読み込むためのインターフェースを提供する。
    //!         デフォルトのパスおよびファイル名が使用可能であり、簡易的な認証処理に適している。
    //!     \~english  This class provides an interface for generating and loading binary license keys.
    //!         Default path and file name values are supported for simplified authentication processing.
    //!
    class WSE_API Licensekey
    {

        //!
        //! @brief
        //!     \~japanese ライセンスキーを生成しファイルに保存する。
        //!     \~english  Generate license key and save to file.
        //!
        //! @param[in] path_in Optional file path.
        //! @param[in] name_in Optional file name. Defaults to DEFAULT_LICENSE_KEY_NAME.
        //!
        //! @return
        //!     \~japanese 成功`LicenseStatus`. 開けないFileは`Io/FileOpenFailed`で失敗する.
        //!     \~english  The `LicenseStatus`; a file that cannot be opened fails with `Io/FileOpenFailed`.
        //!
        public: static LicenseStatus genrate( const std::string& path_in = "", const std::string& name_in = DEFAULT_LICENSE_KEY_NAME );

        //! 
        //! @brief
        //!     \~japanese ライセンスキーファイルを読み込みバイナリデータとして返す。
        //!     \~english  Load license key file and return binary data.
        //!
        //! @param[in] path_in Optional file path.
        //! @param[in] name_in Optional file name. Defaults to DEFAULT_LICENSE_KEY_NAME.
        //!
        //! @return
        //!     \~japanese Binary keyを運ぶ`LicenseResult`. 開けないFileは`Io/FileOpenFailed`,
        //!                読込失敗は`Io/ReadFailed`で失敗する.
        //!     \~english  The `LicenseResult` carrying the binary key. A file that cannot be
        //!                opened fails with `Io/FileOpenFailed`; a failed read with `Io/ReadFailed`.
        //!
        public: static LicenseResult< std::vector<uint8_t> > load   ( const std::string& path_in = "", const std::string& name_in = DEFAULT_LICENSE_KEY_NAME );
    };
}

#endif //WONDERSTEWENGINE_WSELICENCE_KEY_AKABOYASAIHIAIRM_YIZAKAYS_KARUIAIHANOKI
