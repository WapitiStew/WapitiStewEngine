//*****************************************************************************************************************
//!
//! @file    wse_capi_oui.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese OUI Projection描画の平坦C ABI実装.
//! @brief   \~english  Implementation of the flat C ABI for OUI projection rendering.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/capi/wse_capi_oui.h>

#include "capi_internal.h"

#include <cstring>
#include <string>

#include <wse/binding/OuiErrorAdapter.h>
#include <oui/binding/Projection.h>

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

namespace
{

//! \~japanese C ABIのLayer記述をOUIのRequest Layerへ写す. Pointerは呼出中のみ参照する.
//! \~english  Maps one C ABI layer onto an OUI request layer; pointers are read during the call only.
wse_capi_status buildLayer(
      wse::oui::sProjectionImageLayer* const p_layer_out
    , const wse_capi_projection_layer&       source_in )
{
    wse::oui::sProjectionImageLayer& layer_out = *p_layer_out;

    if ( source_in.rgba == nullptr || source_in.rgba_size == 0U )
    {
        return wse::capi::makeInvalidArgument( "Each layer requires a non-empty rgba buffer." );
    }
    if ( source_in.vertices == nullptr || source_in.vertex_count == 0U )
    {
        return wse::capi::makeInvalidArgument( "Each layer requires at least one vertex." );
    }
    if ( source_in.indices == nullptr || source_in.index_count == 0U )
    {
        return wse::capi::makeInvalidArgument( "Each layer requires at least one index." );
    }
    if ( source_in.alpha == nullptr && source_in.alpha_size != 0U )
    {
        return wse::capi::makeInvalidArgument( "alpha_size must be zero when alpha is null." );
    }

    layer_out.width = source_in.width;
    layer_out.height = source_in.height;
    layer_out.rgba.assign( source_in.rgba, source_in.rgba + source_in.rgba_size );
    if ( source_in.alpha != nullptr )
    {
        layer_out.alpha.assign( source_in.alpha, source_in.alpha + source_in.alpha_size );
    }

    layer_out.vertices.resize( source_in.vertex_count );
    for ( std::size_t index = 0U; index < source_in.vertex_count; ++index )
    {
        const wse_capi_projection_vertex& vertex = source_in.vertices[ index ];
        layer_out.vertices[ index ].position_x = vertex.position_x;
        layer_out.vertices[ index ].position_y = vertex.position_y;
        layer_out.vertices[ index ].texture_u = vertex.texture_u;
        layer_out.vertices[ index ].texture_v = vertex.texture_v;
    }
    layer_out.indices.assign( source_in.indices, source_in.indices + source_in.index_count );

    layer_out.sampling_filter =
        static_cast<wse::oui::eTextureSamplingFilter>( source_in.sampling_filter );
    layer_out.opacity = source_in.opacity;
    layer_out.edge_blend.left = source_in.edge_blend.left;
    layer_out.edge_blend.right = source_in.edge_blend.right;
    layer_out.edge_blend.top = source_in.edge_blend.top;
    layer_out.edge_blend.bottom = source_in.edge_blend.bottom;
    layer_out.edge_blend.curve =
        static_cast<wse::oui::eEdgeBlendCurve>( source_in.edge_blend.curve );
    return wse::capi::makeSuccess();
}

} // namespace

extern "C"
{

wse_capi_status WSE_CAPI_CALL wse_capi_render_projection(
      wse_capi_projection_frame*         p_frame_out
    , wse_capi_frame_buffer*             p_data_out
    , const wse_capi_projection_request* request_in )
{
    return wse::capi::guard( [request_in, p_frame_out, p_data_out]() -> wse_capi_status
    {
        if ( request_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "request_in must not be null." );
        }
        if ( p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_frame_out must not be null." );
        }
        if ( p_data_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_data_out must not be null." );
        }
        if ( request_in->layers == nullptr || request_in->layer_count == 0U )
        {
            return wse::capi::makeInvalidArgument( "A projection needs at least one layer." );
        }

        wse::oui::sProjectionRenderRequest native;
        native.output_width = request_in->output_width;
        native.output_height = request_in->output_height;
        native.clear_color.red = request_in->clear_color.red;
        native.clear_color.green = request_in->clear_color.green;
        native.clear_color.blue = request_in->clear_color.blue;
        native.clear_color.alpha = request_in->clear_color.alpha;
        native.supersample_scale = request_in->supersample_scale;
        native.backend = static_cast<wse::oui::eRendererBackend>( request_in->backend );
        native.use_software_adapter = request_in->use_software_adapter != 0;
        native.enable_validation = request_in->enable_validation != 0;
        // A null pointer and an empty string both mean "choose for me".
        native.adapter_name = request_in->adapter_name == nullptr
            ? std::string() : std::string( request_in->adapter_name );
        native.timeout_ms = request_in->timeout_milliseconds;

        native.layers.resize( request_in->layer_count );
        for ( std::size_t index = 0U; index < request_in->layer_count; ++index )
        {
            const wse_capi_status layer_status =
                buildLayer( &native.layers[ index ], request_in->layers[ index ] );
            if ( layer_status.category != static_cast<std::int32_t>( WSE_CAPI_ERROR_NONE ) )
            {
                return layer_status;
            }
        }

        // Renderer, surface, texture, mesh, fence, and readback objects stay owned inside OUI.
        const wse::oui::RendererResult<wse::oui::sProjectionRenderFrame> result =
            wse::oui::renderProjection( native );
        if ( !result.succeeded() )
        {
            return wse::capi::fromBindingError( wse::binding::fromOuiError( result.error() ) );
        }

        const wse::oui::sProjectionRenderFrame& frame = result.value();
        p_frame_out->width = frame.width;
        p_frame_out->height = frame.height;
        p_frame_out->row_pitch = frame.row_pitch;
        // The name is held inline, so it is truncated rather than allowed to run past its room;
        // the terminator is written either way.
        const std::size_t name_size = frame.adapter_name.size()
            < WSE_CAPI_ADAPTER_NAME_CAPACITY - 1U
            ? frame.adapter_name.size() : WSE_CAPI_ADAPTER_NAME_CAPACITY - 1U;
        if ( name_size != 0U )
        {
            std::memcpy( p_frame_out->adapter_name, frame.adapter_name.data(), name_size );
        }
        p_frame_out->adapter_name[ name_size ] = '\0';
        *p_data_out = new wse_capi_frame_buffer_t( frame.data );
        return wse::capi::makeSuccess();
    } );
}

} // extern "C"
