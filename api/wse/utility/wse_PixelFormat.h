//*****************************************************************************************************************
//! 
//! @file    wse_PixelFormat.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese  列挙子を実用的なデータに変換したりする処理をまとめるユーティリティ。
//!     \~english  Utility providing helper functions for pixel format operations.
//!
//! @details
//!     \~japanese
//!         ピクセルフォーマット（ePixFormat）からチャンネル数やビット深度などを取得する。
//!         フォーマット定義に対応しており、無効な型に対しては例外をスローする。
//!
//!     \~english
//!         Retrieves number of channels or bit depth from pixel formats (ePixFormat).
//!         Throws an exception for invalid or unknown formats.
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
#ifndef WONDERSTEWENGINE_UTILITY_PIXELFORMAT_H
#define WONDERSTEWENGINE_UTILITY_PIXELFORMAT_H

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4464)  // 
#endif
        #include <stdexcept>
#include "../depend/wse_STD.h"
        #include "../depend/wse_Enum.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace wse
{
    //!
    //! @brief
    //!     \~japanese Channel順序のBitを外し、Channel数とBit深度だけを表す値へ正規化する。
    //!     \~english  Strips the channel-order bit, giving the canonical channel-and-depth format.
    //!
    //! @param[in] type_in Pixel format.
    //! @return Canonical pixel format.
    //!
    inline static constexpr ePixFormat canonicalFormat( const ePixFormat type_in ) noexcept
    {
        return static_cast< ePixFormat >(
            static_cast< std::uint16_t >(
                static_cast< std::uint16_t >( type_in )
              & static_cast< std::uint16_t >( ~COLOR_ORDER_BGR_FLAG ) ) );
    }

    //!
    //! @brief
    //!     \~japanese ピクセルフォーマットのChannel順序を取得する。
    //!     \~english  Get the channel order a pixel format carries.
    //!
    //! @param[in] type_in Pixel format.
    //! @return Channel order. A one- or two-channel format reports Rgb.
    //!
    inline static constexpr eColorChannelOrder getChannelOrder( const ePixFormat type_in ) noexcept
    {
        return ( static_cast< std::uint16_t >( type_in ) & COLOR_ORDER_BGR_FLAG ) != 0U
             ? eColorChannelOrder::Bgr
             : eColorChannelOrder::Rgb;
    }

    //!
    //! @brief
    //!     \~japanese Channel順序を明示するFormatかどうかを判定する。
    //!     \~english  Whether the format is an explicit channel-order variant.
    //!
    //! @param[in] type_in Pixel format.
    //! @retval true  Channel 0がBであるFormat.
    //! @retval false 正規のCHxDyy Format.
    //!
    inline static constexpr bool isColorOrderVariant( const ePixFormat type_in ) noexcept
    {
        return canonicalFormat( type_in ) != type_in;
    }

    //!
    //! @brief
    //!     \~japanese ピクセルフォーマットからチャンネル数を取得する。
    //!     \~english  Get the number of channels from the pixel format.
    //!
    //! @param[in] type_in Pixel format.
    //! @return Number of channels.
    //! @throw std::invalid_argument When an invalid format is given.
    //!
    inline static constexpr std::uint8_t getDataNum( const ePixFormat type_in )
    {
        switch( canonicalFormat( type_in ) )
        {
            case ePixFormat::CH1D8  :
            case ePixFormat::CH1D10 :
            case ePixFormat::CH1D12 :
            case ePixFormat::CH1D14 :
            case ePixFormat::CH1D16 :
            case ePixFormat::CH1D32 :
            case ePixFormat::CH1D64 :
            return 1;

            case ePixFormat::CH2D8  :
            case ePixFormat::CH2D10 :
            case ePixFormat::CH2D12 :
            case ePixFormat::CH2D14 :
            case ePixFormat::CH2D16 :
            case ePixFormat::CH2D32 :
            case ePixFormat::CH2D64 :
            return 2;

            case ePixFormat::CH3D8  :
            case ePixFormat::CH3D10 :
            case ePixFormat::CH3D12 :
            case ePixFormat::CH3D14 :
            case ePixFormat::CH3D16 :
            case ePixFormat::CH3D32 :
            case ePixFormat::CH3D64 :
            return 3;

            case ePixFormat::CH4D8  :
            case ePixFormat::CH4D10 :
            case ePixFormat::CH4D12 :
            case ePixFormat::CH4D14 :
            case ePixFormat::CH4D16 :
            case ePixFormat::CH4D32 :
            case ePixFormat::CH4D64 :
            return 4;
            // Color-order aliases were canonicalized; reject every other value below.
            default: break;
        }
        throw std::invalid_argument( "unknown pixel format" );
    }
    //!
    //! @brief
    //!     \~japanese ピクセルフォーマットからビット深度を取得する。
    //!     \~english  Get bit depth from the pixel format.
    //!
    //! @param[in] type_in Pixel format.
    //! @return Bit depth.
    //! @throw std::invalid_argument When an invalid format is given.
    //!
    inline static constexpr uint8_t getBitDepth( const ePixFormat type_in )
    {
        switch( canonicalFormat( type_in ) )
        {
            case ePixFormat::CH1D8  : return  8;
            case ePixFormat::CH1D10 : return 10;
            case ePixFormat::CH1D12 : return 12;
            case ePixFormat::CH1D14 : return 14;
            case ePixFormat::CH1D16 : return 16;
            case ePixFormat::CH1D32 : return 32;
            case ePixFormat::CH1D64 : return 64;

            case ePixFormat::CH2D8  : return  8;
            case ePixFormat::CH2D10 : return 10;
            case ePixFormat::CH2D12 : return 12;
            case ePixFormat::CH2D14 : return 14;
            case ePixFormat::CH2D16 : return 16;
            case ePixFormat::CH2D32 : return 32;
            case ePixFormat::CH2D64 : return 64;

            case ePixFormat::CH3D8  : return  8;
            case ePixFormat::CH3D10 : return 10;
            case ePixFormat::CH3D12 : return 12;
            case ePixFormat::CH3D14 : return 14;
            case ePixFormat::CH3D16 : return 16;
            case ePixFormat::CH3D32 : return 32;
            case ePixFormat::CH3D64 : return 64;

            case ePixFormat::CH4D8  : return  8;
            case ePixFormat::CH4D10 : return 10;
            case ePixFormat::CH4D12 : return 12;
            case ePixFormat::CH4D14 : return 14;
            case ePixFormat::CH4D16 : return 16;
            case ePixFormat::CH4D32 : return 32;
            case ePixFormat::CH4D64 : return 64;
            // Color-order aliases were canonicalized; reject every other value below.
            default: break;
        }
        throw std::invalid_argument( "unknown pixel format" );
    }


}

#endif   //WONDERSTEWENGINE_UTILITY_PIXELFORMAT_H

