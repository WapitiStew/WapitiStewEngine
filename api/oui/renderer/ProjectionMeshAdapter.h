//*****************************************************************************************************************
//!
//! @file    ProjectionMeshAdapter.h
//! @brief   \~japanese Legacy Projection MeshをPortable Renderer Meshへ変換する.
//! @brief   \~english  Converts a legacy projection mesh to a portable renderer mesh.
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
#ifndef WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONMESHADAPTER_H
#define WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONMESHADAPTER_H

#include "../../dynamic.h"
#include "../../wse/data/wse_Mesh.h"
#include "RendererTypes.h"

namespace wse
{
namespace oui
{

//! \~japanese Pixel座標系Legacy Meshの互換変換. \~english Compatibility conversion for pixel-space legacy meshes.
class WSE_API ProjectionMeshAdapter final
{
  public:
    //!
    //! @brief Legacy 2D MeshをPortable TriangleListへ変換する.
    //! @param [in] mesh_in          Source／Destination Pixel座標を持つLegacy Mesh.
    //! @param [in] target_extent_in Destination座標をNDCへ正規化する出力Extent [pixel].
    //! @param [in] source_extent_in Source座標をUVへ正規化する入力Extent [pixel].
    //! @return Portable Mesh、または不正Extent／Meshを示すError.
    //!
    static RendererResult< sMeshDescription > createMesh(
          const wse::float64_mesh2d& mesh_in
        , const sRendererExtent2D     target_extent_in
        , const sRendererExtent2D     source_extent_in
    );
};

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_PROJECTIONMESHADAPTER_H
