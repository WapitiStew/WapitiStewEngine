//*****************************************************************************************************************
//! 
//! @file    wse_Typedef.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New      WapitiStew.
//!
//!
//! @brief
//!     \~japanese 基本的な型エイリアス（整数・浮動小数点・文字列）を定義するヘッダファイル。
//!     \~english  Header file defining basic type aliases (integers, floating points, strings).
//!
//! @details
//!     \~japanese このファイルは、プラットフォーム（x86/x64/gcc）ごとに共通の型名で整数型・浮動小数点型・文字列型を扱えるようにする。
//!     \~english  This file provides platform-dependent but consistent type aliases for integer, floating point, and string types.
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
#ifndef WONDERSTEWENGINE_DEPEND_TYPEDEF
#define WONDERSTEWENGINE_DEPEND_TYPEDEF


#include <stdint.h>

namespace wse
{
    // Visual C++ x64.
	#if defined _WIN64
    typedef  int8_t     S8;         //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   S16;         //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   S32;         //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   S64;         //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef uint8_t     U8;         //!< Unsigned Integer  8 bit : 0 to 255
    typedef uint16_t   U16;         //!< Unsigned Integer 16 bit : 0 to 65535
    typedef uint32_t   U32;         //!< Unsigned Integer 32 bit : 0 to 4,294,967,295
    typedef uint64_t   U64;         //!< Unsigned Integer 64 bit : 0 to 18,446,744,073,709,551,615
    typedef    float   F32;         //!< Floating Points 32 bit
    typedef    double  F64;         //!< Floating Points 64 bit
    typedef char       CHR;         //!< Charactor
    typedef char*      STR;         //!< String
    typedef  int8_t    sint8_t;     //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   sint16_t;    //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   sint32_t;    //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   sint64_t;    //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef  int8_t    sint08_t;    //!< Signed Integer  8 bit   : -128 to 127
    typedef uint8_t    uint08_t;    //!< Unsigned Integer  8 bit : 0 to 255
    typedef    float   float32_t;   //!< Floating Points 32 bit
    typedef    double  float64_t;   //!< Floating Points 64 bit


    // Visual C++ x86.
    #elif defined _WIN32
    typedef  int8_t     S8;         //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   S16;         //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   S32;         //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   S64;         //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef uint8_t     U8;         //!< Unsigned Integer  8 bit : 0 to 255
    typedef uint16_t   U16;         //!< Unsigned Integer 16 bit : 0 to 65535
    typedef uint32_t   U32;         //!< Unsigned Integer 32 bit : 0 to 4,294,967,295
    typedef uint64_t   U64;         //!< Unsigned Integer 64 bit : 0 to 18,446,744,073,709,551,615
    typedef    float   F32;         //!< Floating Points 32 bit
    typedef    double  F64;         //!< Floating Points 64 bit
    typedef char       CHR;         //!< Charactor
    typedef char* STR;              //!< String
    typedef  int8_t    sint8_t;     //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   sint16_t;    //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   sint32_t;    //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   sint64_t;    //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef  int8_t    sint08_t;    //!< Signed Integer  8 bit   : -128 to 127
    typedef uint8_t    uint08_t;    //!< Unsigned Integer  8 bit : 0 to 255
    typedef    float   float32_t;   //!< Floating Points 32 bit
    typedef    double  float64_t;   //!< Floating Points 64 bit

    // gcc.
    #elif defined __GNUC__
    typedef  int8_t     S8;         //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   S16;         //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   S32;         //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   S64;         //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef uint8_t     U8;         //!< Unsigned Integer  8 bit : 0 to 255
    typedef uint16_t   U16;         //!< Unsigned Integer 16 bit : 0 to 65535
    typedef uint32_t   U32;         //!< Unsigned Integer 32 bit : 0 to 4,294,967,295
    typedef uint64_t   U64;         //!< Unsigned Integer 64 bit : 0 to 18,446,744,073,709,551,615
    typedef    float   F32;         //!< Floating Points 32 bit
    typedef    double  F64;         //!< Floating Points 64 bit
    typedef char       CHR;         //!< Charactor
    typedef char*      STR;         //!< String
    typedef  int8_t    sint8_t;     //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   sint16_t;    //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   sint32_t;    //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   sint64_t;    //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef  int8_t    sint08_t;    //!< Signed Integer  8 bit   : -128 to 127
    typedef uint8_t    uint08_t;    //!< Unsigned Integer  8 bit : 0 to 255
    typedef    float   float32_t;   //!< Floating Points 32 bit
    typedef    double  float64_t;   //!< Floating Points 64 bit

    // other.
    #else
    typedef  int8_t     S8;         //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   S16;         //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   S32;         //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   S64;         //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef uint8_t     U8;         //!< Unsigned Integer  8 bit : 0 to 255
    typedef uint16_t   U16;         //!< Unsigned Integer 16 bit : 0 to 65535
    typedef uint32_t   U32;         //!< Unsigned Integer 32 bit : 0 to 4,294,967,295
    typedef uint64_t   U64;         //!< Unsigned Integer 64 bit : 0 to 18,446,744,073,709,551,615
    typedef    float   F32;         //!< Floating Points 32 bit
    typedef    double  F64;         //!< Floating Points 64 bit
    typedef char       CHR;         //!< Charactor
    typedef char*      STR;         //!< String
    typedef  int8_t    sint8_t;     //!< Signed Integer  8 bit   : -128 to 127
    typedef  int16_t   sint16_t;    //!< Signed Integer 16 bit   : -32,768 to 2,767
    typedef  int32_t   sint32_t;    //!< Signed Integer 32 bit   : -2,147,483,648 to 2,147,483,647
    typedef  int64_t   sint64_t;    //!< Signed Integer 64 bit   : -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
    typedef  int8_t    sint08_t;    //!< Signed Integer  8 bit   : -128 to 127
    typedef uint8_t    uint08_t;    //!< Unsigned Integer  8 bit : 0 to 255
    typedef    float   float32_t;   //!< Floating Points 32 bit
    typedef    double  float64_t;   //!< Floating Points 64 bit

    #endif
}

#endif   //WONDERSTEWENGINE_DEPEND_TYPEDEF
