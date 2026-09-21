//*****************************************************************************************************************
//!
//! @file    RendererTypes.cpp
//! @brief   \~japanese Backend非依存OUI Renderer descriptor検証を実装する.
//! @brief   \~english  Implements backend-independent OUI renderer descriptor validation.
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

#include "../../../api/oui/renderer/RendererTypes.h"

#include <cmath>
#include <limits>

namespace
{

wse::oui::RendererError invalidDescription( const std::string& message_in )
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Validation
        , wse::oui::eRendererErrorCode::InvalidDescription
        , message_in
    );
}

bool isKnownFormat( const wse::oui::eRendererPixelFormat format_in ) noexcept
{
    switch( format_in )
    {
        case wse::oui::eRendererPixelFormat::R8Unorm:
        case wse::oui::eRendererPixelFormat::Rgba8Unorm:
        case wse::oui::eRendererPixelFormat::Bgra8Unorm:
        case wse::oui::eRendererPixelFormat::Rgba16Float:
        case wse::oui::eRendererPixelFormat::Rgba16Unorm:
            return true;
        default:
            return false;
    }
}

bool isKnownTextureState( const wse::oui::eTextureState state_in ) noexcept
{
    switch( state_in )
    {
        case wse::oui::eTextureState::Undefined:
        case wse::oui::eTextureState::Common:
        case wse::oui::eTextureState::RenderTarget:
        case wse::oui::eTextureState::ShaderResource:
        case wse::oui::eTextureState::CopySource:
        case wse::oui::eTextureState::CopyDestination:
        case wse::oui::eTextureState::Present:
            return true;
        default:
            return false;
    }
}

bool isKnownSamplingFilter( const wse::oui::eTextureSamplingFilter filter_in ) noexcept
{
    return filter_in == wse::oui::eTextureSamplingFilter::Nearest ||
        filter_in == wse::oui::eTextureSamplingFilter::Linear;
}

bool isKnownBlendMode( const wse::oui::eColorBlendMode mode_in ) noexcept
{
    return mode_in == wse::oui::eColorBlendMode::Replace ||
        mode_in == wse::oui::eColorBlendMode::SourceAlpha;
}

bool isKnownEdgeBlendCurve( const wse::oui::eEdgeBlendCurve curve_in ) noexcept
{
    return curve_in == wse::oui::eEdgeBlendCurve::Linear ||
        curve_in == wse::oui::eEdgeBlendCurve::Smoothstep;
}

bool isKnownDisplayRotation( const wse::oui::eDisplayRotation rotation_in ) noexcept
{
    switch( rotation_in )
    {
        case wse::oui::eDisplayRotation::Unknown:
        case wse::oui::eDisplayRotation::Identity:
        case wse::oui::eDisplayRotation::Rotate90:
        case wse::oui::eDisplayRotation::Rotate180:
        case wse::oui::eDisplayRotation::Rotate270:
            return true;
        default:
            return false;
    }
}

bool isKnownSurfaceWindowMode( const wse::oui::eSurfaceWindowMode mode_in ) noexcept
{
    return mode_in == wse::oui::eSurfaceWindowMode::NotApplicable ||
        mode_in == wse::oui::eSurfaceWindowMode::Windowed ||
        mode_in == wse::oui::eSurfaceWindowMode::BorderlessFullscreen ||
        mode_in == wse::oui::eSurfaceWindowMode::DisplayModeFullscreen;
}

bool isUnspecifiedDisplayMode( const wse::oui::sDisplayMode& mode_in ) noexcept
{
    return mode_in.extent.empty() && mode_in.refresh_rate_numerator == 0U &&
        mode_in.refresh_rate_denominator == 1U &&
        mode_in.format == wse::oui::eRendererPixelFormat::Unknown && !mode_in.interlaced;
}

bool isSameDisplayMode(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in
) noexcept
{
    return left_in.extent.width == right_in.extent.width &&
        left_in.extent.height == right_in.extent.height &&
        left_in.format == right_in.format &&
        left_in.interlaced == right_in.interlaced &&
        static_cast< std::uint64_t >( left_in.refresh_rate_numerator ) *
            right_in.refresh_rate_denominator ==
        static_cast< std::uint64_t >( right_in.refresh_rate_numerator ) *
            left_in.refresh_rate_denominator;
}

bool isNormalizedEdgeBlendWidth( const float width_in ) noexcept
{
    return std::isfinite( width_in ) && width_in >= 0.0F && width_in <= 1.0F;
}

bool isNormalizedColor( const wse::oui::sRendererColor& color_in ) noexcept
{
    return std::isfinite( color_in.red ) && color_in.red >= 0.0F && color_in.red <= 1.0F &&
        std::isfinite( color_in.green ) && color_in.green >= 0.0F && color_in.green <= 1.0F &&
        std::isfinite( color_in.blue ) && color_in.blue >= 0.0F && color_in.blue <= 1.0F &&
        std::isfinite( color_in.alpha ) && color_in.alpha >= 0.0F && color_in.alpha <= 1.0F;
}

} // namespace

namespace wse
{
namespace oui
{

bool sRendererExtent2D::empty() const noexcept
{
    return this->width == 0U || this->height == 0U;
}

std::size_t sRendererFrameDescription::bytesPerPixel() const noexcept
{
    switch( this->format )
    {
        case eRendererPixelFormat::R8Unorm:
            return 1U;
        case eRendererPixelFormat::Rgba8Unorm:
        case eRendererPixelFormat::Bgra8Unorm:
            return 4U;
        case eRendererPixelFormat::Rgba16Float:
        case eRendererPixelFormat::Rgba16Unorm:
            return 8U;
        default:
            return 0U;
    }
}

std::size_t sRendererFrameDescription::minimumRowPitch() const noexcept
{
    const std::size_t pixel_size = this->bytesPerPixel();
    if( pixel_size == 0U || this->extent.width == 0U ||
        this->extent.width > ( std::numeric_limits< std::size_t >::max )() / pixel_size )
    {
        return 0U;
    }
    return static_cast< std::size_t >( this->extent.width ) * pixel_size;
}

std::size_t sRendererFrameDescription::effectiveRowPitch() const noexcept
{
    return this->row_pitch == 0U ? this->minimumRowPitch() : this->row_pitch;
}

std::size_t sRendererFrameDescription::memorySize() const noexcept
{
    const std::size_t pitch = this->effectiveRowPitch();
    if( pitch == 0U || this->extent.height == 0U ||
        this->extent.height > ( std::numeric_limits< std::size_t >::max )() / pitch )
    {
        return 0U;
    }
    return static_cast< std::size_t >( this->extent.height ) * pitch;
}

bool sTextureHandle::valid() const noexcept
{
    return this->value != 0U && this->generation != 0U;
}

bool sMeshHandle::valid() const noexcept
{
    return this->value != 0U && this->generation != 0U;
}

bool sSurfaceHandle::valid() const noexcept
{
    return this->value != 0U && this->generation != 0U;
}

bool sFenceHandle::valid() const noexcept
{
    return this->value != 0U && this->generation != 0U;
}

RendererError validateRendererFrameDescription( const sRendererFrameDescription& description_in )
{
    if( description_in.extent.empty() )
    {
        return invalidDescription( "Frame extent must be non-zero." );
    }
    if( !isKnownFormat( description_in.format ) )
    {
        return invalidDescription( "Frame pixel format is unknown." );
    }
    const std::size_t minimum_pitch = description_in.minimumRowPitch();
    if( minimum_pitch == 0U || description_in.effectiveRowPitch() < minimum_pitch ||
        description_in.memorySize() == 0U )
    {
        return invalidDescription( "Frame row pitch or memory size is invalid." );
    }
    return RendererError();
}

RendererError validateRendererFrame( const sRendererFrame& frame_in )
{
    const RendererError description_error =
        validateRendererFrameDescription( frame_in.description );
    if( !description_error.ok() )
    {
        return description_error;
    }
    if( frame_in.data.size() != frame_in.description.memorySize() )
    {
        return invalidDescription( "Frame data size does not match its description." );
    }
    return RendererError();
}

RendererError validateTextureDescription( const sTextureDescription& description_in )
{
    if( description_in.extent.empty() )
    {
        return invalidDescription( "Texture extent must be non-zero." );
    }
    if( !isKnownFormat( description_in.format ) )
    {
        return invalidDescription( "Texture pixel format is unknown." );
    }
    constexpr std::uint16_t ALL_TEXTURE_USAGE_BITS =
        static_cast< std::uint16_t >( eTextureUsage::Sampled ) |
        static_cast< std::uint16_t >( eTextureUsage::RenderTarget ) |
        static_cast< std::uint16_t >( eTextureUsage::TransferSource ) |
        static_cast< std::uint16_t >( eTextureUsage::TransferDestination );
    const std::uint16_t usage_bits = static_cast< std::uint16_t >( description_in.usage );
    if( description_in.usage == eTextureUsage::None || description_in.mip_levels == 0U ||
        ( usage_bits & static_cast< std::uint16_t >( ~ALL_TEXTURE_USAGE_BITS ) ) != 0U )
    {
        return invalidDescription( "Texture usage bits and mip level count must be valid." );
    }
    if( !isKnownTextureState( description_in.initial_state ) )
    {
        return invalidDescription( "Texture initial state is unknown." );
    }
    if( description_in.initial_state == eTextureState::RenderTarget &&
        !hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget ) )
    {
        return invalidDescription( "RenderTarget state requires RenderTarget usage." );
    }
    if( description_in.initial_state == eTextureState::ShaderResource &&
        !hasTextureUsage( description_in.usage, eTextureUsage::Sampled ) )
    {
        return invalidDescription( "ShaderResource state requires Sampled usage." );
    }
    if( description_in.initial_state == eTextureState::CopySource &&
        !hasTextureUsage( description_in.usage, eTextureUsage::TransferSource ) )
    {
        return invalidDescription( "CopySource state requires TransferSource usage." );
    }
    if( description_in.initial_state == eTextureState::CopyDestination &&
        !hasTextureUsage( description_in.usage, eTextureUsage::TransferDestination ) )
    {
        return invalidDescription( "CopyDestination state requires TransferDestination usage." );
    }
    return RendererError();
}

RendererError validateMeshDescription( const sMeshDescription& description_in )
{
    if( description_in.vertices.empty() || description_in.indices.empty() )
    {
        return invalidDescription( "Mesh vertices and indices must be non-empty." );
    }
    if( description_in.topology == ePrimitiveTopology::TriangleList )
    {
        if( description_in.indices.size() % 3U != 0U )
        {
            return invalidDescription( "Triangle-list index count must be divisible by three." );
        }
    }
    else if( description_in.topology == ePrimitiveTopology::TriangleStrip )
    {
        if( description_in.indices.size() < 3U )
        {
            return invalidDescription( "Triangle-strip index count must be at least three." );
        }
    }
    else
    {
        return invalidDescription( "Mesh topology is unknown." );
    }

    for( const sRendererVertex2D& vertex : description_in.vertices )
    {
        if( !std::isfinite( vertex.position_x ) || !std::isfinite( vertex.position_y ) ||
            !std::isfinite( vertex.texture_u ) || !std::isfinite( vertex.texture_v ) )
        {
            return invalidDescription( "Mesh contains a non-finite vertex value." );
        }
    }
    for( const std::uint32_t index : description_in.indices )
    {
        if( index >= description_in.vertices.size() )
        {
            return invalidDescription( "Mesh index is outside the vertex array." );
        }
    }
    return RendererError();
}

RendererError validateRenderPassDescription( const sRenderPassDescription& description_in )
{
    if( !description_in.color_attachment.valid() )
    {
        return invalidDescription( "Render pass color attachment handle is invalid." );
    }
    if( description_in.load_operation != eAttachmentLoadOperation::Load &&
        description_in.load_operation != eAttachmentLoadOperation::Clear &&
        description_in.load_operation != eAttachmentLoadOperation::Discard )
    {
        return invalidDescription( "Render pass load operation is unknown." );
    }
    if( description_in.store_operation != eAttachmentStoreOperation::Store &&
        description_in.store_operation != eAttachmentStoreOperation::Discard )
    {
        return invalidDescription( "Render pass store operation is unknown." );
    }
    if( description_in.load_operation == eAttachmentLoadOperation::Clear &&
        !isNormalizedColor( description_in.clear_color ) )
    {
        return invalidDescription( "Clear color must contain finite normalized values." );
    }
    if( !isKnownTextureState( description_in.final_state ) ||
        description_in.final_state == eTextureState::Undefined )
    {
        return invalidDescription( "Render pass final state is invalid." );
    }
    if( !description_in.render_area.extent.empty() )
    {
        const std::uint32_t maximum = ( std::numeric_limits< std::uint32_t >::max )();
        if( description_in.render_area.x > maximum - description_in.render_area.extent.width ||
            description_in.render_area.y > maximum - description_in.render_area.extent.height )
        {
            return invalidDescription( "Render area overflows its coordinate range." );
        }
    }
    for( const sMeshDrawCommand& draw_command : description_in.draw_commands )
    {
        if( !draw_command.mesh.valid() || !draw_command.source_texture.valid() )
        {
            return invalidDescription( "Mesh draw handles must be valid." );
        }
        if( draw_command.source_texture.value == description_in.color_attachment.value &&
            draw_command.source_texture.generation == description_in.color_attachment.generation )
        {
            return invalidDescription( "A render attachment cannot sample itself." );
        }
        const bool alpha_texture_empty = draw_command.alpha_texture.value == 0U &&
            draw_command.alpha_texture.generation == 0U;
        if( !alpha_texture_empty && !draw_command.alpha_texture.valid() )
        {
            return invalidDescription( "Optional alpha-map handle is incomplete." );
        }
        if( draw_command.alpha_texture.valid() &&
            draw_command.alpha_texture.value == description_in.color_attachment.value &&
            draw_command.alpha_texture.generation == description_in.color_attachment.generation )
        {
            return invalidDescription( "A render attachment cannot be its own alpha map." );
        }
        if( !isKnownSamplingFilter( draw_command.sampling_filter ) ||
            !isKnownBlendMode( draw_command.blend_mode ) )
        {
            return invalidDescription( "Mesh draw sampling or blend mode is unknown." );
        }
        if( !std::isfinite( draw_command.opacity ) ||
            draw_command.opacity < 0.0F || draw_command.opacity > 1.0F )
        {
            return invalidDescription( "Mesh draw opacity must be a finite normalized value." );
        }
        if( !isKnownEdgeBlendCurve( draw_command.edge_blend.curve ) ||
            !isNormalizedEdgeBlendWidth( draw_command.edge_blend.left ) ||
            !isNormalizedEdgeBlendWidth( draw_command.edge_blend.right ) ||
            !isNormalizedEdgeBlendWidth( draw_command.edge_blend.top ) ||
            !isNormalizedEdgeBlendWidth( draw_command.edge_blend.bottom ) )
        {
            return invalidDescription(
                "Mesh draw edge-blend curve and widths must be finite normalized values." );
        }
    }
    return RendererError();
}

RendererError validateSurfaceDescription( const sSurfaceDescription& description_in )
{
    if( description_in.extent.empty() || !isKnownFormat( description_in.format ) )
    {
        return invalidDescription( "Surface extent and pixel format must be valid." );
    }
    if( description_in.buffer_count == 0U || description_in.buffer_count > 3U )
    {
        return invalidDescription( "Surface buffer count must be between one and three." );
    }
    switch( description_in.type )
    {
        case eSurfaceType::Offscreen:
            if( description_in.buffer_count != 1U || !description_in.display_id.empty() ||
                !isUnspecifiedDisplayMode( description_in.display_mode ) )
            {
                return invalidDescription(
                    "Offscreen surfaces require one buffer and no display target." );
            }
            break;
        case eSurfaceType::Window:
            if( description_in.buffer_count < 2U || description_in.title.empty() ||
                !description_in.display_id.empty() ||
                !isUnspecifiedDisplayMode( description_in.display_mode ) )
            {
                return invalidDescription(
                    "Window surfaces require two or three buffers, a title, and no initial display target." );
            }
            break;
        case eSurfaceType::DirectDisplay:
            if( description_in.buffer_count < 2U || description_in.title.empty() ||
                description_in.display_id.empty() || !description_in.visible )
            {
                return invalidDescription(
                    "Direct-display surfaces require two or three buffers, a title, a display ID, and visibility." );
            }
            if( !validateDisplayMode( description_in.display_mode ).ok() ||
                description_in.extent.width != description_in.display_mode.extent.width ||
                description_in.extent.height != description_in.display_mode.extent.height ||
                description_in.format != description_in.display_mode.format )
            {
                return invalidDescription(
                    "Direct-display extent and format must match a valid display mode." );
            }
            break;
        default:
            return invalidDescription( "Surface type is unknown." );
    }
    return RendererError();
}

RendererError validateSurfaceState( const sSurfaceState& state_in )
{
    if( state_in.extent.empty() || !isKnownSurfaceWindowMode( state_in.window_mode ) )
    {
        return invalidDescription( "Surface state extent or window mode is invalid." );
    }
    if( state_in.type == eSurfaceType::Window )
    {
        if( state_in.window_mode == eSurfaceWindowMode::NotApplicable )
        {
            return invalidDescription( "Window surfaces require a window mode." );
        }
        const bool display_targeted =
            state_in.window_mode == eSurfaceWindowMode::BorderlessFullscreen ||
            state_in.window_mode == eSurfaceWindowMode::DisplayModeFullscreen;
        if( display_targeted != !state_in.display_id.empty() )
        {
            return invalidDescription(
                "Only display-targeted surface state carries a display ID." );
        }
        if( state_in.window_mode == eSurfaceWindowMode::DisplayModeFullscreen )
        {
            return validateDisplayMode( state_in.display_mode );
        }
        if( !isUnspecifiedDisplayMode( state_in.display_mode ) )
        {
            return invalidDescription(
                "Windowed and borderless surface state cannot carry a display mode." );
        }
        return RendererError();
    }
    if( state_in.type == eSurfaceType::DirectDisplay )
    {
        if( state_in.window_mode != eSurfaceWindowMode::DisplayModeFullscreen ||
            state_in.display_id.empty() )
        {
            return invalidDescription(
                "Direct-display state requires display-mode fullscreen and a display ID." );
        }
        return validateDisplayMode( state_in.display_mode );
    }
    if( state_in.type != eSurfaceType::Offscreen )
    {
        return invalidDescription( "Surface state type is unknown." );
    }
    if( state_in.window_mode != eSurfaceWindowMode::NotApplicable || !state_in.display_id.empty() ||
        !isUnspecifiedDisplayMode( state_in.display_mode ) )
    {
        return invalidDescription( "Non-window surfaces cannot carry window state." );
    }
    return RendererError();
}

RendererError validateSurfaceWindowModeRequest( const sSurfaceWindowModeRequest& request_in )
{
    if( request_in.mode == eSurfaceWindowMode::Windowed )
    {
        return request_in.display_id.empty() &&
                isUnspecifiedDisplayMode( request_in.display_mode ) ? RendererError() :
            invalidDescription( "Windowed mode must not specify a display target." );
    }
    if( request_in.mode == eSurfaceWindowMode::BorderlessFullscreen )
    {
        return !request_in.display_id.empty() &&
                isUnspecifiedDisplayMode( request_in.display_mode ) ? RendererError() :
            invalidDescription( "Borderless fullscreen requires only a display ID." );
    }
    if( request_in.mode == eSurfaceWindowMode::DisplayModeFullscreen )
    {
        if( request_in.display_id.empty() )
        {
            return invalidDescription( "Display-mode fullscreen requires a display ID." );
        }
        return validateDisplayMode( request_in.display_mode );
    }
    return invalidDescription( "Surface window-mode request is invalid." );
}

RendererError validateDisplayMode( const sDisplayMode& mode_in )
{
    if( mode_in.extent.empty() || !isKnownFormat( mode_in.format ) )
    {
        return invalidDescription( "Display mode extent and pixel format must be valid." );
    }
    if( mode_in.refresh_rate_denominator == 0U )
    {
        return invalidDescription(
            "Display mode refresh-rate denominator must be non-zero." );
    }
    return RendererError();
}

RendererError validateDisplayDescription( const sDisplayDescription& description_in )
{
    if( description_in.id.empty() || description_in.adapter_id.empty() ||
        description_in.adapter_name.empty() || description_in.display_name.empty() )
    {
        return invalidDescription( "Display and adapter identities must be non-empty." );
    }
    if( !isKnownDisplayRotation( description_in.rotation ) )
    {
        return invalidDescription( "Display rotation value is invalid." );
    }
    if( description_in.desktop_extent.empty() )
    {
        return invalidDescription( "Display desktop extent must be non-empty." );
    }
    const RendererError current_mode_error = validateDisplayMode( description_in.current_mode );
    if( !current_mode_error.ok() )
    {
        return current_mode_error;
    }
    if( description_in.modes.empty() )
    {
        return invalidDescription( "Display mode list must contain the current mode." );
    }

    bool contains_current_mode = false;
    for( const sDisplayMode& mode : description_in.modes )
    {
        const RendererError mode_error = validateDisplayMode( mode );
        if( !mode_error.ok() )
        {
            return mode_error;
        }
        contains_current_mode = contains_current_mode ||
            isSameDisplayMode( mode, description_in.current_mode );
    }
    if( !contains_current_mode )
    {
        return invalidDescription( "Display mode list does not contain the current mode." );
    }
    return RendererError();
}

} // namespace oui
} // namespace wse
