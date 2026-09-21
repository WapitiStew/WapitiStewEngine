//*****************************************************************************************************************
//! 
//! @file    wse_StringTool.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 数値やバイナリデータを文字列に変換するユーティリティ関数群。
//!     \~english  Utility functions for converting numeric and binary data to strings.
//!
//! @details
//!     \~japanese
//!         数値の桁揃えや16進数・2進数変換、パスの正規化など、主に文字列化に関する機能を提供する。
//!         テンプレートによる型対応や、float/double専用の桁数指定整形も含む。
//!
//!     \~english
//!         Provides string formatting utilities for numbers, hexadecimal/binary conversion, and path normalization.
//!         Includes type-generic templates and fixed-digit formatting for float/double values.
//!
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
#ifndef WONDERSTEWENGINE_UTILITY_STRINGTOOL_H
#define WONDERSTEWENGINE_UTILITY_STRINGTOOL_H

// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)  // 使用されていないインライン関数の削除
#endif
// C/C++
#include <cstdint>
#include <string>
#include <sstream>
#include <cmath>
#include <bitset>

#include "wse_Math.h"

namespace wse
{
    namespace strtool
    {
        //!
        //! @brief
        //!     \~japanese 固定桁数で小数付き数値を文字列に変換する。
        //!     \~english  Format floating-point number as fixed-width string.
        //!
        //! @param[in] value_in  Numeric value.
        //! @param[in] digit_in  Total digit count.
        //! @return Formatted string.
        //!
        inline static std::string toString_Digit( const double value_in, int32_t digit_in )
        {
            const int32_t integer_abs = std::abs( static_cast< int32_t >( std::floor( value_in ) ) );
            int32_t integer_digits = ( integer_abs == 0 ) ? 1 : static_cast< int32_t >( std::log10( integer_abs ) ) + 1;
            int32_t max_decimal_digits = Maximum( 0, digit_in - integer_digits );
            int total_width = integer_digits + max_decimal_digits + 2;
            std::ostringstream oss;
            oss << std::fixed << std::setw( total_width ) << std::setprecision( max_decimal_digits ) << value_in;
            return oss.str();
        }

        //!
        //! @brief
        //!     \~japanese 整数型を文字列に変換する。
        //!     \~english  Convert integral type to string.
        //!
        //! @tparam T Integral type.
        //! @param[in] value_in Integer value.
        //! @return Converted string.
        //!
        template<typename T>
        typename std::enable_if<std::is_integral<T>::value, std::string>::type
        inline to_string_numeric(T value_in) {
            typedef typename std::conditional<(sizeof(T) < sizeof(int)), int, T>::type cast_type;
            return std::to_string(static_cast<cast_type>(value_in));
        }
    }

    //!
    //! @brief
    //!     \~japanese 任意型の値を符号付きで文字列に変換。
    //!     \~english  Convert value to string with optional leading space.
    //!
    //! @tparam _Tp Numeric type.
    //! @param[in] value_in Value to convert.
    //! @return Converted string.
    //!
    template<typename _Tp>
    inline static std::string toString( const _Tp value_in )
    {
        return ( value_in < 0 ) ? strtool::to_string_numeric( value_in )
            : " " + strtool::to_string_numeric( value_in );
    }

    //!
    //! @brief
    //!     \~japanese floatを8桁で文字列変換。
    //!     \~english  Convert float to fixed-width string (8 digits).
    //!
    //! @param[in] value_in Float value.
    //! @return Converted string.
    //!
    inline static std::string toString( const float value_in )
    {
        const int32_t STRING_DIGITS = 8;
        return strtool::toString_Digit( value_in, STRING_DIGITS );
    }

    //!
    //! @brief
    //!     \~japanese doubleを16桁で文字列変換。
    //!     \~english  Convert double to fixed-width string (16 digits).
    //!
    //! @param[in] value_in Double value.
    //! @return Converted string.
    //!
    inline static std::string toString( const double value_in )
    {
        const int32_t STRING_DIGITS = 16;
        return strtool::toString_Digit( value_in, STRING_DIGITS );
    }

    //!
    //! @brief
    //!     \~japanese 任意の整数型を16進数文字列に変換。
    //!     \~english  Convert integral value to hexadecimal string.
    //!
    //! @tparam T Integral type.
    //! @param[in] buffer_in Value to convert.
    //! @return Hex string, "0x" prefixed and zero-padded to the width of T.
    //!
    template<typename T, std::enable_if_t<(sizeof(T) == 1) && std::is_integral<T>::value, int> = 0>
    inline static std::string toHex(const T buffer_in)
    {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(2)
           << std::hex << std::nouppercase
           << static_cast<unsigned int>(buffer_in);
        return ss.str();
    }

    //
    // Multi-byte overload of the same logical function; documented above.
    //
    template<typename T, std::enable_if_t<(sizeof(T) > 1) && std::is_integral<T>::value, int> = 0>
    inline static std::string toHex(const T buffer_in)
    {
        constexpr int width = sizeof(T) * 2;
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(width)
           << std::hex << std::nouppercase
           << buffer_in;
        return ss.str();
    }

    //!
    //! @brief
    //!     \~japanese 整数ベクタを16進数文字列へ変換。
    //!     \~english  Convert vector of integers to hex string.
    //!
    //! @tparam T Integral type.
    //! @param[in] values_in Vector.
    //! @return Space-separated hex string.
    //!
    template<typename T, std::enable_if_t< std::is_integral<T>::value, int > = 0>
    static std::string toHex( const std::vector< T >& values_in )
    {
        std::string str = "";
        for( size_t i = 0; i < values_in.size(); ++i )
        {
            str += toHex( values_in[i] );
            if( i < ( values_in.size() - 1 ) )
            {
                str += " ";
            }
        }
        return str;
    }

    //!
    //! @brief
    //!     \~japanese 1バイトの値を2進数文字列に変換。
    //!     \~english  Convert 1-byte value to binary string.
    //!
    //! @param[in] data_in Byte value.
    //! @return Binary string.
    //!
    inline static std::string toBin( const unsigned char data_in )
    {
        std::stringstream ss;
        ss << std::bitset< 8 >( data_in );
        return "0b" + ss.str();
    }

    //!
    //! @brief
    //!     \~japanese パス区切りをプラットフォーム用に正規化。
    //!     \~english  Normalize path separators.
    //!
    //! @param[in] path_in File path.
    //! @return Normalized path string.
    //!
    inline static std::string normalizePath( const std::string& path_in )
    {
        std::string normalized_path = path_in;
        for (auto& c : normalized_path) {
            if (c == '\\') {
                c = '/';
            }
        }
        return normalized_path;
    }

} // namespace wse

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif //WONDERSTEWENGINE_UTILITY_STRINGTOOL_H


