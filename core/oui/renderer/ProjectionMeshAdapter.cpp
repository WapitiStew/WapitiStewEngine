//*****************************************************************************************************************
//!
//! @file    ProjectionMeshAdapter.cpp
//! @brief   \~japanese Legacy Projection MeshのPortable変換を実装する.
//! @brief   \~english  Implements portable conversion of legacy projection meshes.
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

#include "../../../api/oui/renderer/ProjectionMeshAdapter.h"

#include <limits>
#include <utility>
#include <vector>

namespace
{

wse::oui::RendererResult< wse::oui::sMeshDescription > invalidMesh(
    const std::string& message_in )
{
    return wse::oui::RendererResult< wse::oui::sMeshDescription >::failure( wse::oui::RendererError(
              wse::oui::eRendererErrorCategory::Validation
            , wse::oui::eRendererErrorCode::InvalidDescription
            , message_in
        )
    );
}

} // namespace

namespace wse
{
namespace oui
{

RendererResult< sMeshDescription > ProjectionMeshAdapter::createMesh(
      const wse::float64_mesh2d& mesh_in
    , const sRendererExtent2D     target_extent_in
    , const sRendererExtent2D     source_extent_in
)
{
    if( target_extent_in.width < 2U || target_extent_in.height < 2U ||
        source_extent_in.width < 2U || source_extent_in.height < 2U )
    {
        return invalidMesh( "Projection source and target extents must be at least 2 x 2." );
    }
    if( mesh_in.mesh_width() == 0U || mesh_in.mesh_height() == 0U )
    {
        return invalidMesh( "Projection mesh must contain at least one grid cell." );
    }

    const std::vector< wse::float64_xy > source_points = mesh_in.getSrcList();
    const std::vector< wse::float64_xy > target_points = mesh_in.getDstList();
    if( source_points.size() != target_points.size() ||
        source_points.size() > ( std::numeric_limits< std::uint32_t >::max )() )
    {
        return invalidMesh( "Projection mesh point arrays are inconsistent or too large." );
    }

    // Legacy Pixel座標をD3D／Vulkan共通のNDCおよびUVへ正規化する.
    sMeshDescription result;
    result.vertices.reserve( source_points.size() );
    const double target_x_scale = 2.0 / static_cast< double >( target_extent_in.width - 1U );
    const double target_y_scale = -2.0 / static_cast< double >( target_extent_in.height - 1U );
    const double source_x_scale = 1.0 / static_cast< double >( source_extent_in.width - 1U );
    const double source_y_scale = 1.0 / static_cast< double >( source_extent_in.height - 1U );
    for( std::size_t index = 0U; index < source_points.size(); ++index )
    {
        result.vertices.emplace_back( sRendererVertex2D{
              static_cast< float >( target_points[ index ].x * target_x_scale - 1.0 )
            , static_cast< float >( target_points[ index ].y * target_y_scale + 1.0 )
            , static_cast< float >( source_points[ index ].x * source_x_scale )
            , static_cast< float >( source_points[ index ].y * source_y_scale )
        } );
    }

    // 各Legacy Gridを同じWindingの2 Triangleへ展開する.
    const std::size_t mesh_width  = mesh_in.mesh_width();
    const std::size_t mesh_height = mesh_in.mesh_height();
    if( mesh_width > ( std::numeric_limits< std::size_t >::max )() / mesh_height )
    {
        return invalidMesh( "Projection mesh cell count overflows host size limits." );
    }
    const std::size_t cell_count = mesh_width * mesh_height;
    if( cell_count > result.indices.max_size() / 6U )
    {
        return invalidMesh( "Projection mesh index count exceeds host size limits." );
    }
    result.indices.reserve( cell_count * 6U );
    for( std::size_t row = 0U; row < mesh_in.mesh_height(); ++row )
    {
        for( std::size_t column = 0U; column < mesh_in.mesh_width(); ++column )
        {
            const std::size_t left_top_index = row * mesh_in.vertex_width() + column;
            const std::size_t left_bottom_index =
                ( row + 1U ) * mesh_in.vertex_width() + column;
            const std::uint32_t left_top =
                static_cast< std::uint32_t >( left_top_index );
            const std::uint32_t right_top =
                static_cast< std::uint32_t >( left_top_index + 1U );
            const std::uint32_t right_bottom =
                static_cast< std::uint32_t >( left_bottom_index + 1U );
            const std::uint32_t left_bottom =
                static_cast< std::uint32_t >( left_bottom_index );
            result.indices.insert( result.indices.end(), {
                  left_top, right_top, right_bottom
                , left_top, right_bottom, left_bottom
            } );
        }
    }

    const RendererError validation_error = validateMeshDescription( result );
    if( !validation_error.ok() )
    {
        return RendererResult< sMeshDescription >::failure( validation_error );
    }
    return RendererResult< sMeshDescription >::success( std::move( result ) );
}

} // namespace oui
} // namespace wse
