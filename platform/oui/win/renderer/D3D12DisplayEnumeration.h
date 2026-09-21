//*****************************************************************************************************************
//!
//! @file    D3D12DisplayEnumeration.h
//! @brief   \~japanese Direct3D 12向けDisplay列挙の内部Interfaceを定義する.
//! @brief   \~english  Defines the internal display-enumeration interface for Direct3D 12.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
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
#ifndef WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_D3D12DISPLAYENUMERATION_H
#define WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_D3D12DISPLAYENUMERATION_H

#include <utility>
#include "../../../../api/oui/renderer/RendererTypes.h"

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

namespace wse
{
namespace oui
{
namespace internal
{

//! \~japanese Portable descriptorとNative outputの対応. \~english Portable/native display target pair.
struct sD3D12DisplayTarget
{
    sDisplayDescription                 description;
    Microsoft::WRL::ComPtr< IDXGIOutput > output;
    std::wstring                        device_name;

    //! @brief Construct all members with explicit defaults.
    sD3D12DisplayTarget(
          const sDisplayDescription& description_in = {}
        , const Microsoft::WRL::ComPtr< IDXGIOutput >& output_in = {}
        , const std::wstring& device_name_in = {}
    )
        : description ( description_in )
        , output      ( output_in )
        , device_name ( device_name_in )
    {
    }
};

//! @return \~japanese Desktop接続中DisplayのSnapshot. \~english Snapshot of desktop-attached displays.
RendererResult< std::vector< sDisplayDescription > > enumerateD3D12Displays(
      IDXGIFactory6* factory_in
    , IDXGIAdapter1* renderer_adapter_in
);

//! @return \~japanese Renderer adapter上のDisplay target. \~english Display target on the renderer adapter.
RendererResult< sD3D12DisplayTarget > findD3D12DisplayTarget(
      IDXGIFactory6* factory_in
    , IDXGIAdapter1* renderer_adapter_in
    , const std::string& display_id_in
);

} // namespace internal
} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_PLATFORM_OUI_WIN_RENDERER_D3D12DISPLAYENUMERATION_H
