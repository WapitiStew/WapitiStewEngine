//*****************************************************************************************************************
//!
//! @file    RendererFrameOps.h
//! @brief   \~japanese Renderer FrameとCore Imageの相互変換を定義する.
//! @brief   \~english  Defines the conversion between a renderer frame and a Core image.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @details
//!     \~japanese
//!         Renderer Pixel FormatをCoreのChannel数・Bit深度・Channel順序へ写す責務だけを持つ。
//!      @n 画像演算そのものはCoreの`wse_ImageTransform.h`などが担う。
//!      @n `Bgra8Unorm`はChannel順序を保ったまま`BGRA4D8`へ写り、Alphaは4 Channel目として残る。
//!      @n `Rgba16Float`は半精度浮動小数点であり、Coreの`Pixel_`が整数型に限られるため対応しない。
//!     \~english
//!         Carries only the responsibility of mapping a renderer pixel format onto a Core channel
//!         count, bit depth, and channel order. The image operations themselves live in Core.
//!      @n `Bgra8Unorm` keeps its channel order and maps onto `BGRA4D8`; the alpha survives as the
//!      @n fourth channel.
//!      @n `Rgba16Float` is half precision and Core's `Pixel_` is limited to an integer type, so it
//!      @n is not supported.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#pragma once
#ifndef WONDERSTEWENGINE_OUI_RENDERER_RENDERERFRAMEOPS_H
#define WONDERSTEWENGINE_OUI_RENDERER_RENDERERFRAMEOPS_H

#include "../../dynamic.h"
#include "../../wse/data/wse_Image.h"
#include "../../wse/data/wse_ImageInterleaved.h"
#include "RendererError.h"
#include "RendererTypes.h"

namespace wse
{
namespace oui
{

//!
//! @brief Renderer Pixel FormatをCoreのPixel Formatへ写す.
//!
//! @details
//!     \~japanese
//!         `R8Unorm`は`CH1D8`、`Rgba8Unorm`は`CH4D8`、`Bgra8Unorm`は`BGRA4D8`、
//!      @n `Rgba16Unorm`は`CH4D16`へ写る。いずれもAlphaを保持する。
//!      @n `Rgba16Float`と`Unknown`は`false`を返す。
//!     \~english
//!         `R8Unorm` maps onto `CH1D8`, `Rgba8Unorm` onto `CH4D8`, `Bgra8Unorm` onto `BGRA4D8`, and
//!         `Rgba16Unorm` onto `CH4D16`. Every one of them keeps its alpha.
//!      @n `Rgba16Float` and `Unknown` report false.
//!
//! @param [out] p_format_out 対応するCore Format. 写せない場合は変更しない.
//! @param [in]  format_in    写すRenderer Format.
//! @retval false 写せないFormat.
//!
WSE_API bool coreFormatOf(
      wse::ePixFormat*     p_format_out
    , eRendererPixelFormat format_in ) noexcept;

//!
//! @brief Renderer FrameをCoreの`wse::Image_`へ変換する.
//!
//! @details
//!     \~japanese
//!         宛先の型がFormatを選ぶ。Frameが宛先の型へ写らない場合は`UnsupportedFormat`を返し、
//!      @n `p_image_out`は変更しない。Row Pitchは畳み込まれ、結果は常に詰まった配置になる。
//!     \~english
//!         The destination type selects the format. A frame that does not map onto that type gives
//!         `UnsupportedFormat` and leaves `p_image_out` untouched. The row pitch is folded away, so
//!         the result is always packed.
//!
//! @param [out] p_image_out 変換結果.
//! @param [in]  frame_in    変換元Frame.
//!
//! \~
//! @{
WSE_API RendererStatus toImage( wse::img1c08_t*      p_image_out, const sRendererFrame& frame_in );
WSE_API RendererStatus toImage( wse::img4c08_t*      p_image_out, const sRendererFrame& frame_in );
WSE_API RendererStatus toImage( wse::img4c08_bgra_t* p_image_out, const sRendererFrame& frame_in );
WSE_API RendererStatus toImage( wse::img4c16_t*      p_image_out, const sRendererFrame& frame_in );
//! @}

//!
//! @brief Coreの`wse::Image_`をRenderer Frameへ変換する.
//!
//! @details
//!     \~japanese
//!         元の型がRenderer Formatを選ぶ。結果の`row_pitch`は0であり、Backendが自らの整列規則で
//!      @n 詰まった行を選ぶ。
//!     \~english
//!         The source type selects the renderer format. The result carries a zero `row_pitch`, which
//!         lets the backend pick the packed row under its own alignment rule.
//!
//! @param [in] image_in 変換元Image.
//!
//! \~
//! @{
WSE_API RendererResult< sRendererFrame > toRendererFrame( const wse::img1c08_t&      image_in );
WSE_API RendererResult< sRendererFrame > toRendererFrame( const wse::img4c08_t&      image_in );
WSE_API RendererResult< sRendererFrame > toRendererFrame( const wse::img4c08_bgra_t& image_in );
WSE_API RendererResult< sRendererFrame > toRendererFrame( const wse::img4c16_t&      image_in );
//! @}

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_RENDERERFRAMEOPS_H
