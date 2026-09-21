//*****************************************************************************************************************
//!
//! @file    RendererBackend.h
//! @brief   \~japanese OUI Renderer facade内部のBackend Interfaceを定義する.
//! @brief   \~english  Defines the internal backend interface for the OUI renderer facade.
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
#ifndef WONDERSTEWENGINE_CORE_OUI_RENDERER_RENDERERBACKEND_H
#define WONDERSTEWENGINE_CORE_OUI_RENDERER_RENDERERBACKEND_H

#include "../../../api/oui/renderer/RendererTypes.h"

#include <cstdint>
#include <memory>

namespace wse
{
namespace oui
{
namespace internal
{

//! \~japanese Platform実装が満たす内部Renderer Interface. \~english Internal renderer interface for platform implementations.
class RendererBackend
{
  public:
    virtual ~RendererBackend() = default;

    virtual RendererStatus initialize( const sRendererConfiguration& configuration_in ) = 0;
    virtual void shutdown() noexcept = 0;
    virtual bool isInitialized() const noexcept = 0;
    virtual sRendererCapabilities getCapabilities() const noexcept = 0;
    virtual RendererResult< std::vector< sDisplayDescription > > enumerateDisplays() const = 0;

    virtual RendererResult< sTextureHandle > createTexture(
        const sTextureDescription& description_in ) = 0;
    virtual RendererStatus destroyTexture( const sTextureHandle texture_in ) = 0;
    virtual RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle texture_in
        , const sRendererFrame& frame_in
    ) = 0;

    virtual RendererResult< sMeshHandle > createMesh(
        const sMeshDescription& description_in ) = 0;
    virtual RendererStatus updateMesh(
          const sMeshHandle        mesh_in
        , const sMeshDescription& description_in
    ) = 0;
    virtual RendererStatus destroyMesh( const sMeshHandle mesh_in ) = 0;

    virtual RendererResult< sSurfaceHandle > createSurface(
        const sSurfaceDescription& description_in ) = 0;
    virtual RendererResult< sTextureHandle > getSurfaceTexture(
        const sSurfaceHandle surface_in ) const = 0;
    virtual RendererResult< sSurfaceState > getSurfaceState(
        const sSurfaceHandle surface_in ) const = 0;
    virtual RendererStatus resizeSurface(
          const sSurfaceHandle     surface_in
        , const sRendererExtent2D extent_in
    ) = 0;
    virtual RendererStatus setSurfaceWindowMode(
          const sSurfaceHandle            surface_in
        , const sSurfaceWindowModeRequest& request_in
    ) = 0;
    virtual RendererResult< sSurfaceEvents > pollSurfaceEvents(
        const sSurfaceHandle surface_in ) = 0;
    virtual RendererStatus destroySurface( const sSurfaceHandle surface_in ) = 0;
    virtual RendererResult< bool > processSurfaceEvents(
        const sSurfaceHandle surface_in ) = 0;
    virtual RendererResult< sFenceHandle > presentSurface(
        const sSurfaceHandle surface_in ) = 0;

    virtual RendererResult< sFenceHandle > executeRenderPass(
        const sRenderPassDescription& description_in ) = 0;
    virtual RendererStatus waitFence(
          const sFenceHandle  fence_in
        , const std::uint32_t timeout_ms_in
    ) = 0;
    virtual RendererResult< sRendererFrame > readTexture(
          const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    ) = 0;
};

//!
//! @brief Platform Renderer Backendを生成する.
//! @param [out] p_error_out      生成失敗時のPortable Error.
//! @param [in]  configuration_in Backend選択設定.
//! @return Backend Instance. 失敗時nullptr.
//!
std::unique_ptr< RendererBackend > createPlatformRendererBackend(
          RendererError*                p_error_out
    , const sRendererConfiguration& configuration_in
);

} // namespace internal
} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_CORE_OUI_RENDERER_RENDERERBACKEND_H
