//*****************************************************************************************************************
//!
//! @file    ProjectionPipeline.h
//! @brief   \~japanese Portable Projection描画Facadeを定義する.
//! @brief   \~english  Defines the portable projection drawing facade.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
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
#ifndef WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONPIPELINE_H
#define WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONPIPELINE_H

#include <utility>
#include "../../dynamic.h"
#include "Renderer.h"

#include <vector>

namespace wse
{
namespace oui
{

//! \~japanese Projection 1 Layerの描画設定. \~english Drawing description for one projection layer.
struct sProjectionLayerDescription
{
    sMeshHandle            mesh;
    sTextureHandle         source_texture;
    sTextureHandle         alpha_texture; //!< Optional alpha map. Its alpha channel is used.
    eTextureSamplingFilter sampling_filter;
    float                  opacity;
    sEdgeBlendDescription  edge_blend; //!< Optional UV-edge attenuation.

    //! @brief Construct all members with explicit defaults.
    sProjectionLayerDescription(
          const sMeshHandle& mesh_in = {}
        , const sTextureHandle& source_texture_in = {}
        , const sTextureHandle& alpha_texture_in = {}
        , eTextureSamplingFilter sampling_filter_in = eTextureSamplingFilter::Linear
        , float opacity_in = 1.0F
        , const sEdgeBlendDescription& edge_blend_in = {}
    )
        : mesh            ( mesh_in )
        , source_texture  ( source_texture_in )
        , alpha_texture   ( alpha_texture_in )
        , sampling_filter ( sampling_filter_in )
        , opacity         ( opacity_in )
        , edge_blend      ( edge_blend_in )
    {
    }
};

//! \~japanese Projection 1 Targetの描画設定. \~english Drawing description for one projection target.
struct sProjectionPassDescription
{
    sTextureHandle                           color_attachment;
    sRendererRegion2D                        render_area;
    sRendererColor                           clear_color;
    std::vector< sProjectionLayerDescription > layers;
    eTextureState                            final_state;
    std::uint32_t                            supersample_scale; //!< 1: disabled, 2: legacy AA.

    //! @brief Construct all members with explicit defaults.
    sProjectionPassDescription(
          const sTextureHandle& color_attachment_in = {}
        , const sRendererRegion2D& render_area_in = {}
        , const sRendererColor& clear_color_in = {}
        , const std::vector< sProjectionLayerDescription >& layers_in = {}
        , eTextureState final_state_in = eTextureState::RenderTarget
        , std::uint32_t supersample_scale_in = 1U
    )
        : color_attachment  ( color_attachment_in )
        , render_area       ( render_area_in )
        , clear_color       ( clear_color_in )
        , layers            ( layers_in )
        , final_state       ( final_state_in )
        , supersample_scale ( supersample_scale_in )
    {
    }
};

//! \~japanese Projection descriptor検証結果. \~english Projection descriptor validation result.
WSE_API RendererError validateProjectionPassDescription(
    const sProjectionPassDescription& description_in );

//! \~japanese Renderer上でProjection Layerを合成するFacade. \~english Facade for projection layers on Renderer.
class WSE_API ProjectionPipeline final
{
  public:
    //!
    //! @brief Projection passをRendererへ送信する.
    //! @param [in,out] p_renderer_inout Portable Renderer. NullはError.
    //! @param [in]     description_in   Target、Layer、Clearおよび最終State.
    //! @return 送信完了FenceまたはValidation／Renderer Error.
    //! @note \~japanese 一時Handleは例外時も解放を試行する。解放失敗で残った資源はRendererが所有する。
    //! @note \~english Temporary handles are released on a best-effort basis even on exceptions; Renderer owns any retained resources.
    //! @note \~japanese C++例外は伝播し得る。失敗はAttachmentの巻戻しを保証しない。
    //! @note \~english C++ exceptions may propagate. Failure does not guarantee attachment rollback.
    //!
    static RendererResult< sFenceHandle > execute(
              Renderer*                   p_renderer_inout
        , const sProjectionPassDescription& description_in
    );
};

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONPIPELINE_H
