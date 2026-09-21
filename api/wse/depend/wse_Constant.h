//*****************************************************************************************************************
//! 
//! @file    wse_Constant.h
//! @brief   ライブラリ内で標準的に使用する定数パラメータ.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @brief
//!     \~japanese 数値定数や定義済みの変換係数を提供する定数定義ヘッダ。
//!     \~english  Header defining numerical constants and conversion factors.
//!
//! @details
//!     \~japanese 数学定数（円周率、ネイピア数、黄金比）や、
//!                 単位変換（cm⇔inch, radian⇔degree）などの値を提供する。
//!     \~english  Provides mathematical constants (e.g., pi, e, golden ratio) and unit conversion values.
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
#ifndef WONDERSTEWENGINE_DEPEND_CONSTANT_H
#define WONDERSTEWENGINE_DEPEND_CONSTANT_H

// Warning Disable
#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4244)  // ../
    #pragma warning(disable: 4464)  // 
#pragma warning(disable: 4505)  // '内部リンケージ含む参照されていない関数が削除
#pragma warning(disable: 4514)  // 使用されていないインライン関数の削除
#endif

#include "wse_STD.h"
#include "wse_Typedef.h"

namespace wse
{
    //! \~japanese 円周率 \~english Pi
    static constexpr float32_t float32_pi      = 3.14159265359f;                                        //!< \~japanese 単精度の円周率 \~english Single-precision Pi
    static constexpr float64_t float64_pi      = 3.14159265358979323846264338327950288419716939937510;  //!< \~japanese 倍精度の円周率 \~english Double-precision Pi

    //! \~japanese 浮動小数点のゼロ定義 \~english Floating point zero threshold
    static constexpr float32_t float32_zero    = 1.192092896e-07f;        //!< \~japanese 単精度のゼロ定義（最小の正の非ゼロ値） \~english Single-precision zero threshold
    static constexpr float64_t float64_zero    = 2.2204460492503131e-016; //!< \~japanese 倍精度のゼロ定義（最小の正の非ゼロ値） \~english Double-precision zero threshold

    //! \~japanese ネイピア数 \~english Napier's constant (e)
    static constexpr float32_t float32_e       = 2.71828182845f;                                        //!< \~japanese 単精度のネイピア数 \~english Single-precision Napier's constant (e)
    static constexpr float64_t float64_e       = 2.71828182845904523536028747135266249775724709369995;  //!< \~japanese 倍精度のネイピア数 \~english Double-precision Napier's constant (e)

    //! \~japanese 黄金比 \~english Golden ratio
    static constexpr float32_t float32_fai     = 0.61803398874f;                                        //!< \~japanese 単精度の黄金比 \~english Single-precision Golden ratio
    static constexpr float64_t float64_fai     = 0.61803398874989484820458683436563811772030917980576;  //!< \~japanese 倍精度の黄金比 \~english Double-precision Golden ratio

    //! \~japanese 単位変換定数 \~english Unit conversion constants
    static constexpr float32_t float32_inch2cm = 0.3937f;               //!< \~japanese 単精度: inch → cm 変換係数 \~english Single-precision: inch to cm conversion
    static constexpr float64_t float64_inch2cm = 0.3937;                //!< \~japanese 倍精度: inch → cm 変換係数 \~english Double-precision: inch to cm conversion
    static constexpr float32_t float32_cm2inch = 2.54f;                 //!< \~japanese 単精度: cm → inch 変換係数 \~english Single-precision: cm to inch conversion
    static constexpr float64_t float64_cm2inch = 2.54;                  //!< \~japanese 倍精度: cm → inch 変換係数 \~english Double-precision: cm to inch conversion
    static constexpr float32_t float32_rad2deg = 180.0f / float32_pi;   //!< \~japanese 単精度: rad → deg 変換係数 \~english Single-precision: rad to deg conversion
    static constexpr float64_t float64_rad2deg = 180.0  / float64_pi;   //!< \~japanese 倍精度: rad → deg 変換係数 \~english Double-precision: rad to deg conversion
    static constexpr float32_t float32_deg2rad = float32_pi / 180.0f;   //!< \~japanese 単精度: deg → rad 変換係数 \~english Single-precision: deg to rad conversion
    static constexpr float64_t float64_deg2rad = float64_pi / 180.0;    //!< \~japanese 倍精度: deg → rad 変換係数 \~english Double-precision: deg to rad conversion


    //! \~japanese ライセンスキーのデフォルトファイル名 \~english Default license key file name
    static const std::string DEFAULT_LICENSE_KEY_NAME = "license_key.wse";
    static const std::string DEFAULT_LICENSE_NAME     = "license.wse";

    //!
    //! @brief
    //!     \~japanese 改行コードを取得する。
    //!     \~english  Get platform-specific line ending string.
    //!
    //! @return Line ending string for the current platform.
    //!
    static std::string END_LINE( void )
    {
        #if defined _WIN64
        return { "\r\n" };
        #elif defined _WIN32
        return { "\r\n" };
        #else
        return { "\n" };
        #endif
    
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif   //WONDERSTEWENGINE_DEPEND_CONSTANT_H