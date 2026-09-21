//*****************************************************************************************************************
//! 
//! @file    wse_DataCast.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 画像データのフォーマット変換処理を提供するユーティリティヘッダ。
//!     \~english  Utility header that provides image format conversion functions.
//!
//! @details
//!     \~japanese
//!         異なるピクセルフォーマット間での画像変換をテンプレート関数として提供する。
//!         主にImage_クラスを用いた、RGBやアルファチャンネルの処理を行う。
//!
//!     \~english
//!         This header provides template-based image format conversion functions
//!         between different pixel formats using the Image_ class.
//!
//! @note
//!     \~japanese OpenCV非依存で軽量な変換処理を提供。
//!     \~english  Provides lightweight conversions without OpenCV dependency.
//!
//! @note
//!     \~japanese
//!         `castData`はChannel数とBit深度をIndex順のまま変換し、Channel順序は決して入れ替えない。
//!      @n RGBとBGRの入れ替えには`wse::convertChannelOrder`(wse_ImageInterleaved.h)を使用する。
//!     \~english
//!         `castData` changes the channel count and the bit depth by index and never reorders
//!         channels. Use `wse::convertChannelOrder` in wse_ImageInterleaved.h to swap RGB and BGR.
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
#ifndef WONDERSTEWENGINE_UTILITY_DATACAST_H
#define WONDERSTEWENGINE_UTILITY_DATACAST_H


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4668) 
#pragma warning(disable: 4464) 
#endif
#include "../depend/wse_STD.h"
#include "../data/wse_Image.h"
#include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
/*!
@class  Timer
@author  WapitiStew

タイマークラス.
*/

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)  // Dynamicライブラリでユーザー側に公開されないメンバーが含まれる
#endif
namespace  wse
{

    //!
    //! @class castData
    //!
    //! @brief
    //!     \~japanese ピクセルフォーマット変換を行う静的ユーティリティクラス。
    //!     \~english  Static utility class for pixel format conversion.
    //!
    //! @details
    //!     \~japanese
    //!         本クラスは異なるピクセルフォーマット（ePixFormat）間の変換処理を提供する。
    //!         全てのメソッドはテンプレート関数であり、インスタンスを必要としない。
    //!
    //!     \~english
    //!         This class provides conversion between different pixel formats (ePixFormat).
    //!         All functions are static templates and do not require instantiation.
    //!
    //! @note
    //!     \~japanese 変換はwse::Image_クラスを対象とする。
    //!     \~english  Operates on wse::Image_ objects.
    //!
    class WSE_API castData
    {
        //!
        //! @brief
        //!     \~japanese 画像フォーマットを別の形式へ変換する。
        //!     \~english  Convert image format to another pixel format.
        //!
        //! @tparam _dPf Destination pixel format.
        //! @tparam _sPf Source pixel format.
        //! @param[in] src_in 変換元画像。
        //! @return 変換後の画像。
        //!
        public : 
        template< ePixFormat _dPf, ePixFormat _sPf > static Image_< _dPf > image( const Image_< _sPf >& src_in );

        //!
        //! @brief
        //!     \~japanese アルファチャンネル付きの4ch画像へ変換する。
        //!     \~english  Convert image to 4-channel alpha image.
        //!
        //! @tparam _dPf Destination pixel format.
        //! @tparam _sPf Source pixel format.
        //! @param[in] src_in 変換元画像。
        //! @return 変換後の4ch画像。
        //!
        public : 
        template< ePixFormat _dPf, ePixFormat _sPf > static Image_< _dPf > to4chAlphamap( const Image_< _sPf >& src_in );

    };

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif  //WONDERSTEWENGINE_UTILITY_DATACAST_H
