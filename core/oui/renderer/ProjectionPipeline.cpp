//*****************************************************************************************************************
//!
//! @file    ProjectionPipeline.cpp
//! @brief   \~japanese Portable Projection描画Facadeを実装する.
//! @brief   \~english  Implements the portable projection drawing facade.
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

#include "../../../api/oui/renderer/ProjectionPipeline.h"

#include "ProjectionExecution.h"

#include <limits>

namespace
{

//! @brief Projection descriptor用Validation Errorを生成する.
//! @param [in] message_in Error message.
//! @return InvalidDescription Error.
wse::oui::RendererError invalidProjectionDescription( const std::string& message_in )
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Validation
        , wse::oui::eRendererErrorCode::InvalidDescription
        , message_in
    );
}

} // namespace

namespace wse
{
namespace oui
{

//! @brief Projection pass descriptorを検証する.
//! @author WapitiStew.
RendererError validateProjectionPassDescription(
    const sProjectionPassDescription& description_in )
{
    if( !description_in.color_attachment.valid() )
    {
        return invalidProjectionDescription( "Projection color attachment handle is invalid." );
    }
    if( description_in.layers.empty() )
    {
        return invalidProjectionDescription( "Projection pass requires at least one layer." );
    }
    if( description_in.supersample_scale != 1U &&
        description_in.supersample_scale != 2U )
    {
        return invalidProjectionDescription(
            "Projection supersample scale must be one or two." );
    }
    if( description_in.supersample_scale == 2U )
    {
        if( description_in.render_area.extent.empty() )
        {
            return invalidProjectionDescription(
                "Supersampled projection requires an explicit render-area extent." );
        }
        const std::uint32_t maximum = ( std::numeric_limits< std::uint32_t >::max )();
        if( description_in.render_area.extent.width > maximum / 2U ||
            description_in.render_area.extent.height > maximum / 2U )
        {
            return invalidProjectionDescription(
                "Supersampled projection extent overflows its coordinate range." );
        }
    }

    // Generic Renderer validatorをProjection固有の固定Blend設定にも適用する.
    const sRenderPassDescription render_pass = detail::makeProjectionRenderPass(
          description_in
        , description_in.color_attachment
        , description_in.render_area
        , description_in.final_state
    );
    return validateRenderPassDescription( render_pass );
}

//! @brief Projection passをRendererへ送信する.
//! @author WapitiStew.
RendererResult< sFenceHandle > ProjectionPipeline::execute(
          Renderer*                    p_renderer_inout
    , const sProjectionPassDescription& description_in
)
{
    return detail::executeProjection( p_renderer_inout, description_in );
}

} // namespace oui
} // namespace wse
