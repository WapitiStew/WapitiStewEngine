//*****************************************************************************************************************
//! 
//! @file    wse_Math.h
//! @brief   文字列の汎用処理を記載.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
//!
//! @brief
//!     \~japanese 基本的な数学関数を提供するテンプレートユーティリティ。
//!     \~english  Template utility for basic math operations.
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
#ifndef WONDERSTEWENGINE_UTILITY_MATH_H
#define WONDERSTEWENGINE_UTILITY_MATH_H

// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)  // 使用されていないインライン関数の削除
#endif
// C/C++
#include <cstdint>
#include <cmath>

namespace wse
{
    //!
    //! @brief
    //!     \~japanese 2つの数値のうち大きい方を返す。
    //!     \~english  Return the larger of two values.
    //!
    //! @tparam _Tp Arithmetic type (e.g., int, float, double).
    //! @param[in] v1_in First value.
    //! @param[in] v2_in Second value.
    //! @return Larger of the two values.
    //!
    template<typename _Tp, typename Enable = std::enable_if_t<std::is_arithmetic<_Tp>::value> >
    inline static _Tp Maximum(const _Tp v1_in, const _Tp v2_in)
    {
        return (v1_in > v2_in) ? v1_in : v2_in;
    }

    //!
    //! @brief
    //!     \~japanese 2つの数値のうち小さい方を返す。
    //!     \~english  Return the smaller of two values.
    //!
    //! @tparam _Tp Arithmetic type (e.g., int, float, double).
    //! @param[in] v1_in First value.
    //! @param[in] v2_in Second value.
    //! @return Smaller of the two values.
    //!
    template<typename _Tp, typename Enable = std::enable_if_t<std::is_arithmetic<_Tp>::value> >
    inline static _Tp Minimum(const _Tp v1_in, const _Tp v2_in)
    {
        return (v1_in < v2_in) ? v1_in : v2_in;
    }

}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_UTILITY_MATH_H


