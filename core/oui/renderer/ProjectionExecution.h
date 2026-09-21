// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
// Private implementation shared by the facade and deterministic fault tests; never installed.
#pragma once
#include "../../../api/oui/renderer/ProjectionPipeline.h"
#include <exception>
#include <utility>

namespace wse::oui::detail
{
//! @brief Projection LayerをGeneric Render passへ変換する.
//! @param [in] description_in      Projection descriptor.
//! @param [in] color_attachment_in 描画先Texture.
//! @param [in] render_area_in      描画領域.
//! @param [in] final_state_in      描画後Texture state.
//! @return Generic Render pass.
inline wse::oui::sRenderPassDescription makeProjectionRenderPass(
      const wse::oui::sProjectionPassDescription& description_in
    , const wse::oui::sTextureHandle               color_attachment_in
    , const wse::oui::sRendererRegion2D&           render_area_in
    , const wse::oui::eTextureState                final_state_in
)
{
    wse::oui::sRenderPassDescription render_pass;
    render_pass.color_attachment = color_attachment_in;
    render_pass.render_area      = render_area_in;
    render_pass.load_operation   = wse::oui::eAttachmentLoadOperation::Clear;
    render_pass.store_operation  = wse::oui::eAttachmentStoreOperation::Store;
    render_pass.clear_color      = description_in.clear_color;
    render_pass.final_state      = final_state_in;
    render_pass.draw_commands.reserve( description_in.layers.size() );
    for( const wse::oui::sProjectionLayerDescription& layer : description_in.layers )
    {
        render_pass.draw_commands.emplace_back( wse::oui::sMeshDrawCommand{
              layer.mesh
            , layer.source_texture
            , layer.alpha_texture
            , layer.sampling_filter
            , wse::oui::eColorBlendMode::SourceAlpha
            , layer.opacity
            , layer.edge_blend
        } );
    }
    return render_pass;
}

//! @brief Supersample中間Textureを最終Targetへ縮小するFullscreen Meshを生成する.
//! @return Fullscreen Triangle-list mesh.
inline wse::oui::sMeshDescription makeDownsampleMesh()
{
    wse::oui::sMeshDescription mesh;
    mesh.vertices = {
          { -1.0F,  1.0F, 0.0F, 0.0F }
        , {  1.0F,  1.0F, 1.0F, 0.0F }
        , {  1.0F, -1.0F, 1.0F, 1.0F }
        , { -1.0F, -1.0F, 0.0F, 1.0F }
    };
    mesh.indices = { 0U, 1U, 2U, 0U, 2U, 3U };
    return mesh;
}


// Owns the obligation to release each acquired handle exactly once. A failed destroy
// may leave the native resource with Renderer until shutdown; do not retry blindly.
template <typename Backend>
class ProjectionTemporaries final
{
public:
    explicit ProjectionTemporaries( Backend& renderer_inout ) noexcept : renderer( renderer_inout ) {}
    ProjectionTemporaries( const ProjectionTemporaries& ) = delete;
    ProjectionTemporaries& operator=( const ProjectionTemporaries& ) = delete;
    ~ProjectionTemporaries() noexcept { discard(); }

    void discard() noexcept
    {
        try { (void)release(); } catch (...) { /* Preserve the primary operation failure. */ }
    }

    RendererStatus release()
    {
        RendererStatus mesh_status = RendererStatus::success();
        RendererStatus texture_status = RendererStatus::success();
        std::exception_ptr first_exception;
        // Exchange before calling out, so an exception cannot cause a second destroy.
        const auto old_mesh = std::exchange( mesh, {} );
        const auto old_texture = std::exchange( texture, {} );
        try { if ( old_mesh.valid() ) mesh_status = renderer.destroyMesh( old_mesh ); }
        catch (...) { first_exception = std::current_exception(); }
        try { if ( old_texture.valid() ) texture_status = renderer.destroyTexture( old_texture ); }
        catch (...) { if ( !first_exception ) first_exception = std::current_exception(); }
        if ( first_exception ) std::rethrow_exception( first_exception );
        return mesh_status.succeeded() ? std::move( texture_status ) : std::move( mesh_status );
    }

    sMeshHandle mesh{};
    sTextureHandle texture{};
private:
    Backend& renderer;
};

template <typename Backend>
RendererResult<sFenceHandle> executeProjection(
    Backend* p_renderer_inout, const sProjectionPassDescription& description_in )
{
    if ( p_renderer_inout == nullptr )
        return RendererResult<sFenceHandle>::failure( RendererError(
            eRendererErrorCategory::Validation, eRendererErrorCode::InvalidArgument,
            "Projection renderer pointer must be non-null." ) );
    const auto validation = validateProjectionPassDescription( description_in );
    if ( !validation.ok() ) return RendererResult<sFenceHandle>::failure( validation );
    if ( description_in.supersample_scale == 1U )
        return p_renderer_inout->executeRenderPass( makeProjectionRenderPass(
            description_in, description_in.color_attachment, description_in.render_area,
            description_in.final_state ) );

    const sRendererExtent2D extent{ description_in.render_area.extent.width * 2U,
                                   description_in.render_area.extent.height * 2U };
    sTextureDescription texture_description;
    texture_description.extent = extent;
    texture_description.format = eRendererPixelFormat::Rgba8Unorm;
    texture_description.usage = eTextureUsage::RenderTarget | eTextureUsage::Sampled;
    texture_description.initial_state = eTextureState::RenderTarget;

    // Finish allocating every CPU descriptor before acquiring temporary GPU handles.
    const auto mesh_description = makeDownsampleMesh();
    auto supersample_pass = makeProjectionRenderPass(
        description_in, {}, { 0U, 0U, extent }, eTextureState::ShaderResource );
    sRenderPassDescription downsample_pass;
    downsample_pass.color_attachment = description_in.color_attachment;
    downsample_pass.render_area = description_in.render_area;
    downsample_pass.load_operation = eAttachmentLoadOperation::Clear;
    downsample_pass.store_operation = eAttachmentStoreOperation::Store;
    downsample_pass.clear_color = description_in.clear_color;
    downsample_pass.final_state = description_in.final_state;
    downsample_pass.draw_commands.emplace_back( sMeshDrawCommand{
        {}, {}, {}, eTextureSamplingFilter::Linear, eColorBlendMode::Replace, 1.0F, {} } );

    ProjectionTemporaries<Backend> owned( *p_renderer_inout );
    const auto texture = p_renderer_inout->createTexture( texture_description );
    if ( !texture.succeeded() ) return RendererResult<sFenceHandle>::failure( texture.error() );
    owned.texture = texture.value();
    const auto mesh = p_renderer_inout->createMesh( mesh_description );
    if ( !mesh.succeeded() ) return RendererResult<sFenceHandle>::failure( mesh.error() );
    owned.mesh = mesh.value();
    supersample_pass.color_attachment = owned.texture;
    downsample_pass.draw_commands.front().mesh = owned.mesh;
    downsample_pass.draw_commands.front().source_texture = owned.texture;
    const auto first = p_renderer_inout->executeRenderPass( supersample_pass );
    if ( !first.succeeded() ) return first;
    const auto second = p_renderer_inout->executeRenderPass( downsample_pass );
    if ( !second.succeeded() ) return second;
    const auto cleanup = owned.release();
    if ( !cleanup.succeeded() ) return RendererResult<sFenceHandle>::failure( cleanup.error() );
    return second;
}
} // namespace wse::oui::detail
