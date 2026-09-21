//*****************************************************************************************************************
//!
//! @file    Renderer.cpp
//! @brief   \~japanese Backend非依存OUI Renderer facadeを実装する.
//! @brief   \~english  Implements the backend-independent OUI renderer facade.
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

#include "../../../api/oui/renderer/Renderer.h"

#include "../../../api/oui/renderer/RendererFrameOps.h"
#include "RendererBackend.h"

#include <limits>
#include <utility>

namespace
{

wse::oui::RendererError notInitializedError()
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Lifecycle
        , wse::oui::eRendererErrorCode::NotInitialized
        , "Renderer is not initialized."
    );
}

wse::oui::RendererError invalidHandleError( const std::string& resource_name_in )
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Validation
        , wse::oui::eRendererErrorCode::InvalidArgument
        , resource_name_in + " handle is invalid."
    );
}

wse::oui::RendererError invalidExtentError()
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Validation
        , wse::oui::eRendererErrorCode::InvalidDescription
        , "Surface extent must be non-empty."
    );
}

bool isSameConfiguration(
      const wse::oui::sRendererConfiguration& left_in
    , const wse::oui::sRendererConfiguration& right_in
) noexcept
{
    return left_in.backend == right_in.backend &&
        left_in.use_software_adapter == right_in.use_software_adapter &&
        left_in.prefer_display_adapter == right_in.prefer_display_adapter &&
        left_in.enable_validation == right_in.enable_validation &&
        left_in.adapter_name == right_in.adapter_name;
}

bool isKnownBackend( const wse::oui::eRendererBackend backend_in ) noexcept
{
    return backend_in == wse::oui::eRendererBackend::Automatic ||
        backend_in == wse::oui::eRendererBackend::Direct3D12 ||
        backend_in == wse::oui::eRendererBackend::Vulkan12;
}

} // namespace

namespace wse
{
namespace oui
{

class Renderer::Impl final
{
    //! @brief Construct all members with explicit defaults.
public:
    Impl()
        : backend       ()
        , configuration ()
    {
    }
private:

  public:
    std::unique_ptr< internal::RendererBackend > backend;
    sRendererConfiguration configuration;
};

Renderer::Renderer()
    : m_impl ( std::make_unique< Impl >() )
{
}

Renderer::~Renderer()
{
    this->shutdown();
}

Renderer::Renderer( Renderer&& source_in ) noexcept = default;

Renderer& Renderer::operator = ( Renderer&& source_in ) noexcept = default;

RendererStatus Renderer::initialize( const sRendererConfiguration& configuration_in )
{
    if( !isKnownBackend( configuration_in.backend ) )
    {
        return RendererStatus::failure( RendererError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidArgument
                , "Renderer backend value is unknown."
            )
        );
    }
    // Naming an adapter and asking for the software adapter are two different requests, and one
    // backend used to answer the second while quietly dropping the first. A name is never ignored,
    // so the two together are refused rather than silently resolved one way.
    if( configuration_in.use_software_adapter && !configuration_in.adapter_name.empty() )
    {
        return RendererStatus::failure( RendererError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidArgument
                , "An adapter name and the software adapter cannot both be requested."
            )
        );
    }
    if( this->m_impl == nullptr )
    {
        this->m_impl = std::make_unique< Impl >();
    }
    if( this->isInitialized() )
    {
        if( isSameConfiguration( this->m_impl->configuration, configuration_in ) )
        {
            return RendererStatus::success();
        }
        return RendererStatus::failure( RendererError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::AlreadyInitialized
                , "Renderer is already initialized with a different configuration."
            )
        );
    }

    // Copy potentially allocating configuration before publishing a live backend.
    auto configuration = configuration_in;
    RendererError factory_error;
    this->m_impl->backend = internal::createPlatformRendererBackend(
          &factory_error
        , configuration_in
    );
    if( this->m_impl->backend == nullptr )
    {
        return RendererStatus::failure( std::move( factory_error ) );
    }

    RendererStatus result = this->m_impl->backend->initialize( configuration_in );
    if( !result.succeeded() )
    {
        this->m_impl->backend.reset();
        return result;
    }
    this->m_impl->configuration = std::move( configuration );
    return result;
}

void Renderer::shutdown() noexcept
{
    if( this->m_impl != nullptr && this->m_impl->backend != nullptr )
    {
        this->m_impl->backend->shutdown();
        this->m_impl->backend.reset();
    }
}

bool Renderer::isInitialized() const noexcept
{
    return this->m_impl != nullptr && this->m_impl->backend != nullptr &&
        this->m_impl->backend->isInitialized();
}

RendererResult< sRendererCapabilities > Renderer::getCapabilities() const
{
    if( !this->isInitialized() )
    {
        return RendererResult< sRendererCapabilities >::failure( notInitializedError() );
    }
    return RendererResult< sRendererCapabilities >::success( this->m_impl->backend->getCapabilities() );
}

RendererResult< std::vector< sDisplayDescription > > Renderer::enumerateDisplays() const
{
    if( !this->isInitialized() )
    {
        return RendererResult< std::vector< sDisplayDescription > >::failure( notInitializedError() );
    }
    return this->m_impl->backend->enumerateDisplays();
}

RendererResult< sTextureHandle > Renderer::createTexture( const sTextureDescription& description_in )
{
    const RendererError validation_error = validateTextureDescription( description_in );
    if( !validation_error.ok() )
    {
        return RendererResult< sTextureHandle >::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sTextureHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->createTexture( description_in );
}

RendererStatus Renderer::destroyTexture( const sTextureHandle texture_in )
{
    if( !texture_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Texture" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->destroyTexture( texture_in );
}

RendererResult< sFenceHandle > Renderer::uploadTexture(
      const sTextureHandle texture_in
    , const sRendererFrame& frame_in
)
{
    if( !texture_in.valid() )
    {
        return RendererResult< sFenceHandle >::failure( invalidHandleError( "Texture" ) );
    }
    const RendererError validation_error = validateRendererFrame( frame_in );
    if( !validation_error.ok() )
    {
        return RendererResult< sFenceHandle >::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sFenceHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->uploadTexture( texture_in, frame_in );
}

RendererResult< sMeshHandle > Renderer::createMesh( const sMeshDescription& description_in )
{
    const RendererError validation_error = validateMeshDescription( description_in );
    if( !validation_error.ok() )
    {
        return RendererResult< sMeshHandle >::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sMeshHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->createMesh( description_in );
}

RendererStatus Renderer::updateMesh(
      const sMeshHandle        mesh_in
    , const sMeshDescription& description_in
)
{
    if( !mesh_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Mesh" ) );
    }
    const RendererError validation_error = validateMeshDescription( description_in );
    if( !validation_error.ok() )
    {
        return RendererStatus::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->updateMesh( mesh_in, description_in );
}

RendererStatus Renderer::destroyMesh( const sMeshHandle mesh_in )
{
    if( !mesh_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Mesh" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->destroyMesh( mesh_in );
}

RendererResult< sSurfaceHandle > Renderer::createSurface( const sSurfaceDescription& description_in )
{
    const RendererError validation_error = validateSurfaceDescription( description_in );
    if( !validation_error.ok() )
    {
        return RendererResult< sSurfaceHandle >::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sSurfaceHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->createSurface( description_in );
}

RendererResult< sTextureHandle > Renderer::getSurfaceTexture( const sSurfaceHandle surface_in ) const
{
    if( !surface_in.valid() )
    {
        return RendererResult< sTextureHandle >::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sTextureHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->getSurfaceTexture( surface_in );
}

RendererResult< sSurfaceState > Renderer::getSurfaceState( const sSurfaceHandle surface_in ) const
{
    if( !surface_in.valid() )
    {
        return RendererResult< sSurfaceState >::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sSurfaceState >::failure( notInitializedError() );
    }
    return this->m_impl->backend->getSurfaceState( surface_in );
}

RendererStatus Renderer::resizeSurface(
      const sSurfaceHandle     surface_in
    , const sRendererExtent2D extent_in
)
{
    if( !surface_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Surface" ) );
    }
    if( extent_in.empty() )
    {
        return RendererStatus::failure( invalidExtentError() );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->resizeSurface( surface_in, extent_in );
}

RendererStatus Renderer::setSurfaceWindowMode(
      const sSurfaceHandle            surface_in
    , const sSurfaceWindowModeRequest& request_in
)
{
    if( !surface_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Surface" ) );
    }
    const RendererError validation_error = validateSurfaceWindowModeRequest( request_in );
    if( !validation_error.ok() )
    {
        return RendererStatus::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->setSurfaceWindowMode( surface_in, request_in );
}

RendererResult< sSurfaceEvents > Renderer::pollSurfaceEvents(
    const sSurfaceHandle surface_in )
{
    if( !surface_in.valid() )
    {
        return RendererResult< sSurfaceEvents >::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sSurfaceEvents >::failure( notInitializedError() );
    }
    return this->m_impl->backend->pollSurfaceEvents( surface_in );
}

RendererStatus Renderer::destroySurface( const sSurfaceHandle surface_in )
{
    if( !surface_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->destroySurface( surface_in );
}

RendererResult< bool > Renderer::processSurfaceEvents( const sSurfaceHandle surface_in )
{
    if( !surface_in.valid() )
    {
        return RendererResult< bool >::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< bool >::failure( notInitializedError() );
    }
    return this->m_impl->backend->processSurfaceEvents( surface_in );
}

RendererResult< sFenceHandle > Renderer::presentSurface( const sSurfaceHandle surface_in )
{
    if( !surface_in.valid() )
    {
        return RendererResult< sFenceHandle >::failure( invalidHandleError( "Surface" ) );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sFenceHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->presentSurface( surface_in );
}

RendererResult< sFenceHandle > Renderer::executeRenderPass( const sRenderPassDescription& description_in )
{
    const RendererError validation_error = validateRenderPassDescription( description_in );
    if( !validation_error.ok() )
    {
        return RendererResult< sFenceHandle >::failure( validation_error );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sFenceHandle >::failure( notInitializedError() );
    }
    return this->m_impl->backend->executeRenderPass( description_in );
}

RendererStatus Renderer::waitFence(
      const sFenceHandle  fence_in
    , const std::uint32_t timeout_ms_in
)
{
    if( !fence_in.valid() )
    {
        return RendererStatus::failure( invalidHandleError( "Fence" ) );
    }
    if( timeout_ms_in == ( std::numeric_limits< std::uint32_t >::max )() )
    {
        return RendererStatus::failure( RendererError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidArgument
                , "Fence timeout must be finite."
            )
        );
    }
    if( !this->isInitialized() )
    {
        return RendererStatus::failure( notInitializedError() );
    }
    return this->m_impl->backend->waitFence( fence_in, timeout_ms_in );
}

RendererResult< sRendererFrame > Renderer::readTexture(
      const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in
)
{
    if( !texture_in.valid() )
    {
        return RendererResult< sRendererFrame >::failure( invalidHandleError( "Texture" ) );
    }
    if( timeout_ms_in == ( std::numeric_limits< std::uint32_t >::max )() )
    {
        return RendererResult< sRendererFrame >::failure( RendererError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidArgument
                , "Readback timeout must be finite."
            )
        );
    }
    if( !this->isInitialized() )
    {
        return RendererResult< sRendererFrame >::failure( notInitializedError() );
    }
    return this->m_impl->backend->readTexture( texture_in, timeout_ms_in );
}


namespace
{

//! \~japanese ImageをFrameへ写してからUploadする. \~english Maps an image onto a frame, then uploads.
template< typename ImageType >
RendererResult< sFenceHandle > uploadImage(
      Renderer* const      p_renderer_inout
    , const sTextureHandle texture_in
    , const ImageType&     image_in )
{
    Renderer& renderer_inout = *p_renderer_inout;

    const RendererResult< sRendererFrame > frame = toRendererFrame( image_in );
    if( !frame.succeeded() )
    {
        return RendererResult< sFenceHandle >::failure( frame.error() );
    }
    return renderer_inout.uploadTexture( texture_in, frame.value() );
}

//! \~japanese Readbackした後にImageへ写す. \~english Reads back, then maps onto an image.
template< typename ImageType >
RendererStatus readImage(
      Renderer* const      p_renderer_inout
    , ImageType* const     p_image_out
    , const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in )
{
    const RendererResult< sRendererFrame > frame =
        p_renderer_inout->readTexture( texture_in, timeout_ms_in );
    if( !frame.succeeded() )
    {
        return RendererStatus::failure( frame.error() );
    }
    return toImage( p_image_out, frame.value() );
}

} // namespace


RendererResult< sFenceHandle > Renderer::uploadTexture(
      const sTextureHandle  texture_in
    , const wse::img1c08_t& image_in )
{
    return uploadImage( this, texture_in, image_in );
}

RendererResult< sFenceHandle > Renderer::uploadTexture(
      const sTextureHandle  texture_in
    , const wse::img4c08_t& image_in )
{
    return uploadImage( this, texture_in, image_in );
}

RendererResult< sFenceHandle > Renderer::uploadTexture(
      const sTextureHandle       texture_in
    , const wse::img4c08_bgra_t& image_in )
{
    return uploadImage( this, texture_in, image_in );
}

RendererResult< sFenceHandle > Renderer::uploadTexture(
      const sTextureHandle  texture_in
    , const wse::img4c16_t& image_in )
{
    return uploadImage( this, texture_in, image_in );
}


RendererStatus Renderer::readTexture(
      wse::img1c08_t*      p_image_out
    , const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in )
{
    return readImage( this, p_image_out, texture_in, timeout_ms_in );
}

RendererStatus Renderer::readTexture(
      wse::img4c08_t*      p_image_out
    , const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in )
{
    return readImage( this, p_image_out, texture_in, timeout_ms_in );
}

RendererStatus Renderer::readTexture(
      wse::img4c08_bgra_t* p_image_out
    , const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in )
{
    return readImage( this, p_image_out, texture_in, timeout_ms_in );
}

RendererStatus Renderer::readTexture(
      wse::img4c16_t*      p_image_out
    , const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in )
{
    return readImage( this, p_image_out, texture_in, timeout_ms_in );
}

} // namespace oui
} // namespace wse
