//*****************************************************************************************************************
//!
//! @file    WaylandWindow.h
//! @brief   \~japanese Vulkan OUI Backend用Wayland Window／Output管理を定義する.
//! @brief   \~english  Defines Wayland window and output management for the Vulkan OUI backend.
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
#ifndef WONDERSTEWENGINE_PLATFORM_OUI_LINUX_RENDERER_WAYLANDWINDOW_H
#define WONDERSTEWENGINE_PLATFORM_OUI_LINUX_RENDERER_WAYLANDWINDOW_H

#include <utility>
#include "../../../../api/oui/renderer/RendererTypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct wl_compositor;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;

namespace wse
{
namespace oui
{
namespace internal
{

//! \~japanese 1個のWayland Top-level Window状態. \~english State for one Wayland top-level window.
struct sWaylandWindow final
{
    wl_surface*   surface;
    xdg_surface*  shell_surface;
    xdg_toplevel* top_level;
    sRendererExtent2D extent;
    bool configured;
    bool alive;
    bool extent_changed;
    bool fullscreen;
    std::uint64_t topology_generation;

    //! @brief Construct all members with explicit defaults.
    sWaylandWindow(
          wl_surface * surface_in = nullptr
        , xdg_surface * shell_surface_in = nullptr
        , xdg_toplevel * top_level_in = nullptr
        , const sRendererExtent2D& extent_in = {}
        , bool configured_in = false
        , bool alive_in = true
        , bool extent_changed_in = false
        , bool fullscreen_in = false
        , std::uint64_t topology_generation_in = 0U
    )
        : surface             ( surface_in )
        , shell_surface       ( shell_surface_in )
        , top_level           ( top_level_in )
        , extent              ( extent_in )
        , configured          ( configured_in )
        , alive               ( alive_in )
        , extent_changed      ( extent_changed_in )
        , fullscreen          ( fullscreen_in )
        , topology_generation ( topology_generation_in )
    {
    }
};

//! \~japanese Wayland ConnectionとOutput topologyを所有する. \~english Owns a Wayland connection and output topology.
class WaylandConnection final
{
  private:
    struct sOutput;

    wl_display*    m_display;
    wl_registry*   m_registry;
    wl_compositor* m_compositor;
    xdg_wm_base*   m_shell;
    std::vector< std::unique_ptr< sOutput > > m_outputs;
    std::uint64_t  m_topology_generation;

    sOutput* findOutputRecord( const std::string& id_in ) noexcept;
    const sOutput* findOutputRecord( const std::string& id_in ) const noexcept;

  public:
    WaylandConnection() noexcept;
    ~WaylandConnection();

    WaylandConnection( const WaylandConnection& ) = delete;
    WaylandConnection& operator = ( const WaylandConnection& ) = delete;

    bool initialize( std::string* p_error_out );
    void shutdown() noexcept;
    bool available() const noexcept;
    wl_display* nativeDisplay() const noexcept;

    std::vector< sDisplayDescription > enumerateDisplays(
        const std::string& adapter_id_in,
        const std::string& adapter_name_in ) const;
    wl_output* findOutput( const std::string& id_in ) const noexcept;

    std::unique_ptr< sWaylandWindow > createWindow(
          std::string*                p_error_out
        , const std::string&          title_in
        , const sRendererExtent2D     extent_in
        , bool                        visible_in
    );
    void destroyWindow( std::unique_ptr< sWaylandWindow >* p_window_inout ) noexcept;
    bool setFullscreen(
          sWaylandWindow*    p_window_inout
        , std::string*       p_error_out
        , const std::string& display_id_in
        , bool               fullscreen_in
    );
    bool requestExtent(
          sWaylandWindow*        p_window_inout
        , std::string*           p_error_out
        , sRendererExtent2D      extent_in
    );
    bool poll(
          sWaylandWindow* p_window_inout
        , sSurfaceEvents* p_events_out
        , std::string*    p_error_out
    );

    // Native callbacks are public only so the C Wayland listener tables can call them.
    void addRegistryObject(
          std::uint32_t name_in
        , const char*   interface_in
        , std::uint32_t version_in
    );
    void removeRegistryObject( std::uint32_t name_in );
    void recordOutputGeometry(
          void*        p_output_in
        , std::int32_t x_in
        , std::int32_t y_in
        , std::int32_t transform_in
        , const char*  make_in
        , const char*  model_in
    );
    void recordOutputMode(
          void*        p_output_in
        , std::uint32_t flags_in
        , std::int32_t width_in
        , std::int32_t height_in
        , std::int32_t refresh_millihertz_in
    );
    void recordOutputName( void* p_output_in, const char* name_in );
    void recordOutputDescription( void* p_output_in, const char* description_in );
};

} // namespace internal
} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_PLATFORM_OUI_LINUX_RENDERER_WAYLANDWINDOW_H
