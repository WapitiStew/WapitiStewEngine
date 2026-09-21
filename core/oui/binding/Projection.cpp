//*****************************************************************************************************************
//!
//! @file    Projection.cpp
//! @brief   \~japanese 高水準・Handle非公開のProjectionの実装.
//! @brief   \~english  High-level, handle-free Projection implementation.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#include "../../../api/oui/binding/Projection.h"

#include <limits>
#include <new>
#include <string>
#include <utility>

namespace wse
{
namespace oui
{
namespace
{

RendererError validationError( const std::string& message_in )
{
    return RendererError( eRendererErrorCategory::Validation,
        eRendererErrorCode::InvalidDescription, message_in );
}

template<typename T>
RendererResult<T> failure( const RendererError& error_in )
{
    return RendererResult<T>::failure( error_in );
}

bool imageSize(
      std::size_t* const  p_size_out
    , const std::uint32_t width_in
    , const std::uint32_t height_in
    , const std::size_t   bytes_per_pixel_in )
{
    std::size_t& size_out = *p_size_out;

    if ( width_in == 0U || height_in == 0U || bytes_per_pixel_in == 0U ) return false;
    const std::size_t width = static_cast<std::size_t>( width_in );
    const std::size_t height = static_cast<std::size_t>( height_in );
    if ( width > std::numeric_limits<std::size_t>::max() / bytes_per_pixel_in ) return false;
    const std::size_t row = width * bytes_per_pixel_in;
    if ( height > std::numeric_limits<std::size_t>::max() / row ) return false;
    size_out = row * height;
    return true;
}

// The whole request is validated before any GPU resource exists. A managed caller sends plain
// byte arrays, so a size that disagrees with the declared extent has to be caught here — the
// upload below would otherwise read past the buffer the caller actually provided.
RendererError validateRequest( const sProjectionRenderRequest& request_in )
{
    if ( request_in.output_width == 0U || request_in.output_height == 0U )
        return validationError( "Projection output extent must be non-zero." );
    if ( request_in.timeout_ms == 0U )
        return validationError( "Projection timeout must be non-zero." );
    if ( request_in.layers.empty() )
        return validationError( "Projection requires at least one layer." );
    for ( const auto& layer : request_in.layers )
    {
        std::size_t rgba_size = 0U;
        std::size_t alpha_size = 0U;
        if ( !imageSize( &rgba_size, layer.width, layer.height, 4U )
             || layer.rgba.size() != rgba_size )
            return validationError( "Projection layer RGBA data does not match its extent." );
        if ( !layer.alpha.empty()
             && ( !imageSize( &alpha_size, layer.width, layer.height, 1U )
                  || layer.alpha.size() != alpha_size ) )
            return validationError( "Projection layer alpha data does not match its extent." );
        sMeshDescription mesh;
        mesh.vertices = layer.vertices;
        mesh.indices = layer.indices;
        const RendererError mesh_error = validateMeshDescription( mesh );
        if ( !mesh_error.ok() ) return mesh_error;
    }
    return RendererError();
}

// Creates a texture, uploads the bytes, and waits for the copy to finish, so the returned
// handle is safe to sample immediately and the caller's byte buffer is free to disappear.
RendererResult<sTextureHandle> createUploadedTexture( Renderer& renderer_in,
    const std::uint32_t width_in, const std::uint32_t height_in,
    const eRendererPixelFormat format_in, const std::vector<std::uint8_t>& bytes_in,
    const std::uint32_t timeout_ms_in )
{
    sTextureDescription description;
    description.extent = { width_in, height_in };
    description.format = format_in;
    description.usage = eTextureUsage::Sampled | eTextureUsage::TransferDestination;
    description.initial_state = eTextureState::CopyDestination;
    auto texture = renderer_in.createTexture( description );
    if ( !texture.succeeded() ) return texture;

    sRendererFrame frame;
    frame.description.extent = description.extent;
    frame.description.format = format_in;
    frame.data = bytes_in;
    const auto upload = renderer_in.uploadTexture( texture.value(), frame );
    if ( !upload.succeeded() ) return failure<sTextureHandle>( upload.error() );
    const auto wait = renderer_in.waitFence( upload.value(), timeout_ms_in );
    if ( !wait.succeeded() ) return failure<sTextureHandle>( wait.error() );
    return texture;
}

} // namespace

RendererResult<sProjectionRenderFrame> renderProjection(
    const sProjectionRenderRequest& request_in )
{
    try
    {
        const RendererError request_error = validateRequest( request_in );
        if ( !request_error.ok() ) return failure<sProjectionRenderFrame>( request_error );

        Renderer renderer;
        sRendererConfiguration configuration;
        configuration.backend = request_in.backend;
        configuration.use_software_adapter = request_in.use_software_adapter;
        configuration.enable_validation = request_in.enable_validation;
        configuration.adapter_name = request_in.adapter_name;
        const auto initialized = renderer.initialize( configuration );
        if ( !initialized.succeeded() )
            return failure<sProjectionRenderFrame>( initialized.error() );

        sSurfaceDescription surface_description;
        surface_description.extent = { request_in.output_width, request_in.output_height };
        surface_description.format = eRendererPixelFormat::Rgba8Unorm;
        const auto surface = renderer.createSurface( surface_description );
        if ( !surface.succeeded() ) return failure<sProjectionRenderFrame>( surface.error() );
        const auto target = renderer.getSurfaceTexture( surface.value() );
        if ( !target.succeeded() ) return failure<sProjectionRenderFrame>( target.error() );

        sProjectionPassDescription pass;
        pass.color_attachment = target.value();
        pass.render_area.extent = { request_in.output_width, request_in.output_height };
        pass.clear_color = request_in.clear_color;
        pass.final_state = eTextureState::CopySource;
        pass.supersample_scale = request_in.supersample_scale;

        for ( const auto& layer : request_in.layers )
        {
            const auto source = createUploadedTexture( renderer, layer.width, layer.height,
                eRendererPixelFormat::Rgba8Unorm, layer.rgba, request_in.timeout_ms );
            if ( !source.succeeded() ) return failure<sProjectionRenderFrame>( source.error() );

            sTextureHandle alpha_handle;
            if ( !layer.alpha.empty() )
            {
                const auto alpha = createUploadedTexture( renderer, layer.width, layer.height,
                    eRendererPixelFormat::R8Unorm, layer.alpha, request_in.timeout_ms );
                if ( !alpha.succeeded() ) return failure<sProjectionRenderFrame>( alpha.error() );
                alpha_handle = alpha.value();
            }

            sMeshDescription mesh_description;
            mesh_description.vertices = layer.vertices;
            mesh_description.indices = layer.indices;
            const auto mesh = renderer.createMesh( mesh_description );
            if ( !mesh.succeeded() ) return failure<sProjectionRenderFrame>( mesh.error() );

            sProjectionLayerDescription projection_layer;
            projection_layer.mesh = mesh.value();
            projection_layer.source_texture = source.value();
            projection_layer.alpha_texture = alpha_handle;
            projection_layer.sampling_filter = layer.sampling_filter;
            projection_layer.opacity = layer.opacity;
            projection_layer.edge_blend = layer.edge_blend;
            pass.layers.emplace_back( projection_layer );
        }

        const auto submitted = ProjectionPipeline::execute( &renderer, pass );
        if ( !submitted.succeeded() ) return failure<sProjectionRenderFrame>( submitted.error() );
        const auto waited = renderer.waitFence( submitted.value(), request_in.timeout_ms );
        if ( !waited.succeeded() ) return failure<sProjectionRenderFrame>( waited.error() );
        auto readback = renderer.readTexture( target.value(), request_in.timeout_ms );
        if ( !readback.succeeded() ) return failure<sProjectionRenderFrame>( readback.error() );

        sProjectionRenderFrame output;
        output.width = readback.value().description.extent.width;
        output.height = readback.value().description.extent.height;
        output.row_pitch = readback.value().description.effectiveRowPitch();
        // Which adapter drew the frame is only knowable from the renderer that drew it, and
        // the renderer goes out of scope here, so the name is taken while it is still open.
        const auto capabilities = renderer.getCapabilities();
        if ( capabilities.succeeded() ) output.adapter_name = capabilities.value().adapter_name;
        output.data = wse::binding::FrameBuffer( std::move( readback.value().data ) );
        return RendererResult<sProjectionRenderFrame>::success( std::move( output ) );
    }
    catch ( const std::bad_alloc& )
    {
        return failure<sProjectionRenderFrame>( RendererError(
            eRendererErrorCategory::Resource, eRendererErrorCode::ResourceExhausted,
            "Projection memory allocation failed." ) );
    }
    catch ( const std::exception& )
    {
        // A Core image allocation reports through its own exception; it must not cross this
        // boundary, because the facade contract is a structured result.
        return failure<sProjectionRenderFrame>( RendererError(
            eRendererErrorCategory::Resource, eRendererErrorCode::ResourceExhausted,
            "Projection memory allocation failed." ) );
    }
}

} // namespace oui
} // namespace wse
