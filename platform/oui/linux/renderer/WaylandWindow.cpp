//*****************************************************************************************************************
//!
//! @file    WaylandWindow.cpp
//! @brief   \~japanese Vulkan OUI Backend用Wayland Window／Output管理を実装する.
//! @brief   \~english  Implements Wayland window and output management for the Vulkan OUI backend.
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

#include "WaylandWindow.h"

#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <utility>

namespace
{

void registryGlobal(
      void* const              p_data_in
    , wl_registry* const
    , const std::uint32_t      name_in
    , const char* const        p_interface_in
    , const std::uint32_t      version_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->addRegistryObject(
        name_in, p_interface_in, version_in );
}

void registryGlobalRemove(
      void* const         p_data_in
    , wl_registry* const
    , const std::uint32_t name_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->removeRegistryObject(
        name_in );
}

constexpr wl_registry_listener REGISTRY_LISTENER = {
      registryGlobal
    , registryGlobalRemove
};

void shellPing( void*, xdg_wm_base* const p_shell_in, const std::uint32_t serial_in )
{
    xdg_wm_base_pong( p_shell_in, serial_in );
}

constexpr xdg_wm_base_listener SHELL_LISTENER = { shellPing };

void outputGeometry(
      void* const         p_data_in
    , wl_output* const    p_output_in
    , const std::int32_t  x_in
    , const std::int32_t  y_in
    , const std::int32_t
    , const std::int32_t
    , const std::int32_t
    , const char* const   p_make_in
    , const char* const   p_model_in
    , const std::int32_t  transform_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->recordOutputGeometry(
        p_output_in, x_in, y_in, transform_in, p_make_in, p_model_in );
}

void outputMode(
      void* const         p_data_in
    , wl_output* const    p_output_in
    , const std::uint32_t flags_in
    , const std::int32_t  width_in
    , const std::int32_t  height_in
    , const std::int32_t  refresh_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->recordOutputMode(
        p_output_in, flags_in, width_in, height_in, refresh_in );
}

void outputDone( void*, wl_output* )
{
}

void outputScale( void*, wl_output*, std::int32_t )
{
}

void outputName(
      void* const      p_data_in
    , wl_output* const p_output_in
    , const char* const p_name_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->recordOutputName(
        p_output_in, p_name_in );
}

void outputDescription(
      void* const       p_data_in
    , wl_output* const  p_output_in
    , const char* const p_description_in
)
{
    static_cast< wse::oui::internal::WaylandConnection* >( p_data_in )->recordOutputDescription(
        p_output_in, p_description_in );
}

constexpr wl_output_listener OUTPUT_LISTENER = {
      outputGeometry
    , outputMode
    , outputDone
    , outputScale
    , outputName
    , outputDescription
};

void shellSurfaceConfigure(
      void* const        p_data_in
    , xdg_surface* const p_surface_in
    , const std::uint32_t serial_in
)
{
    xdg_surface_ack_configure( p_surface_in, serial_in );
    static_cast< wse::oui::internal::sWaylandWindow* >( p_data_in )->configured = true;
}

constexpr xdg_surface_listener XDG_SURFACE_LISTENER = { shellSurfaceConfigure };

void topLevelConfigure(
      void* const
        p_data_in
    , xdg_toplevel* const
    , const std::int32_t width_in
    , const std::int32_t height_in
    , wl_array* const
)
{
    if( width_in <= 0 || height_in <= 0 )
    {
        return;
    }
    auto* const p_window = static_cast< wse::oui::internal::sWaylandWindow* >( p_data_in );
    const wse::oui::sRendererExtent2D next_extent = {
          static_cast< std::uint32_t >( width_in )
        , static_cast< std::uint32_t >( height_in )
    };
    p_window->extent_changed = p_window->extent.width != next_extent.width ||
        p_window->extent.height != next_extent.height;
    p_window->extent = next_extent;
}

void topLevelClose( void* const p_data_in, xdg_toplevel* )
{
    static_cast< wse::oui::internal::sWaylandWindow* >( p_data_in )->alive = false;
}

void topLevelConfigureBounds( void*, xdg_toplevel*, std::int32_t, std::int32_t )
{
}

void topLevelCapabilities( void*, xdg_toplevel*, wl_array* )
{
}

constexpr xdg_toplevel_listener TOP_LEVEL_LISTENER = {
      topLevelConfigure
    , topLevelClose
    , topLevelConfigureBounds
    , topLevelCapabilities
};

wse::oui::eDisplayRotation toRotation( const std::int32_t transform_in ) noexcept
{
    switch( transform_in )
    {
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return wse::oui::eDisplayRotation::Identity;
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return wse::oui::eDisplayRotation::Rotate90;
        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return wse::oui::eDisplayRotation::Rotate180;
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return wse::oui::eDisplayRotation::Rotate270;
        default:
            return wse::oui::eDisplayRotation::Unknown;
    }
}

} // namespace

namespace wse
{
namespace oui
{
namespace internal
{

struct WaylandConnection::sOutput final
{
    std::uint32_t global_name;
    wl_output*    native;
    std::string   name;
    std::string   description;
    std::string   make;
    std::string   model;
    sDisplayPosition2D position;
    eDisplayRotation rotation;
    std::vector< sDisplayMode > modes;
    std::size_t current_mode;
    bool has_current_mode;

    std::string id() const
    {
        return "wayland:" + std::to_string( this->global_name );
    }

    //! @brief Construct all members with explicit defaults.
    sOutput(
          std::uint32_t global_name_in = 0U
        , wl_output * native_in = nullptr
        , const std::string& name_in = {}
        , const std::string& description_in = {}
        , const std::string& make_in = {}
        , const std::string& model_in = {}
        , const sDisplayPosition2D& position_in = {}
        , eDisplayRotation rotation_in = eDisplayRotation::Unknown
        , const std::vector< sDisplayMode >& modes_in = {}
        , std::size_t current_mode_in = 0U
        , bool has_current_mode_in = false
    )
        : global_name      ( global_name_in )
        , native           ( native_in )
        , name             ( name_in )
        , description      ( description_in )
        , make             ( make_in )
        , model            ( model_in )
        , position         ( position_in )
        , rotation         ( rotation_in )
        , modes            ( modes_in )
        , current_mode     ( current_mode_in )
        , has_current_mode ( has_current_mode_in )
    {
    }
};

WaylandConnection::WaylandConnection() noexcept
    : m_display             ( nullptr )
    , m_registry            ( nullptr )
    , m_compositor          ( nullptr )
    , m_shell               ( nullptr )
    , m_outputs             ()
    , m_topology_generation ( 0U )
{
}

WaylandConnection::~WaylandConnection()
{
    this->shutdown();
}

bool WaylandConnection::initialize( std::string* const p_error_out )
{
    if( this->available() )
    {
        return true;
    }
    this->m_display = wl_display_connect( nullptr );
    if( this->m_display == nullptr )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "WAYLAND_DISPLAY is unavailable.";
        }
        return false;
    }
    this->m_registry = wl_display_get_registry( this->m_display );
    if( this->m_registry == nullptr )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Failed to obtain the Wayland registry.";
        }
        this->shutdown();
        return false;
    }
    wl_registry_add_listener( this->m_registry, &REGISTRY_LISTENER, this );
    if( wl_display_roundtrip( this->m_display ) < 0 ||
        wl_display_roundtrip( this->m_display ) < 0 )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Failed to synchronize the Wayland registry.";
        }
        this->shutdown();
        return false;
    }
    if( this->m_compositor == nullptr || this->m_shell == nullptr )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland compositor or xdg-shell is unavailable.";
        }
        this->shutdown();
        return false;
    }
    return true;
}

void WaylandConnection::shutdown() noexcept
{
    for( auto& output : this->m_outputs )
    {
        if( output->native != nullptr )
        {
            wl_output_destroy( output->native );
        }
    }
    this->m_outputs.clear();
    if( this->m_shell != nullptr )
    {
        xdg_wm_base_destroy( this->m_shell );
        this->m_shell = nullptr;
    }
    if( this->m_compositor != nullptr )
    {
        wl_compositor_destroy( this->m_compositor );
        this->m_compositor = nullptr;
    }
    if( this->m_registry != nullptr )
    {
        wl_registry_destroy( this->m_registry );
        this->m_registry = nullptr;
    }
    if( this->m_display != nullptr )
    {
        wl_display_disconnect( this->m_display );
        this->m_display = nullptr;
    }
    this->m_topology_generation = 0U;
}

bool WaylandConnection::available() const noexcept
{
    return this->m_display != nullptr && this->m_registry != nullptr &&
        this->m_compositor != nullptr && this->m_shell != nullptr;
}

wl_display* WaylandConnection::nativeDisplay() const noexcept
{
    return this->m_display;
}

WaylandConnection::sOutput* WaylandConnection::findOutputRecord(
    const std::string& id_in ) noexcept
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [&id_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->id() == id_in;
          }
    );
    return found == this->m_outputs.end() ? nullptr : found->get();
}

const WaylandConnection::sOutput* WaylandConnection::findOutputRecord(
    const std::string& id_in ) const noexcept
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [&id_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->id() == id_in;
          }
    );
    return found == this->m_outputs.end() ? nullptr : found->get();
}

std::vector< sDisplayDescription > WaylandConnection::enumerateDisplays(
      const std::string& adapter_id_in
    , const std::string& adapter_name_in
) const
{
    std::vector< sDisplayDescription > result;
    result.reserve( this->m_outputs.size() );
    for( const auto& output : this->m_outputs )
    {
        if( output->modes.empty() )
        {
            continue;
        }
        sDisplayDescription description;
        description.id = output->id();
        description.adapter_id = adapter_id_in;
        description.adapter_name = adapter_name_in;
        description.display_name = !output->description.empty() ? output->description :
            ( !output->name.empty() ? output->name : output->make + " " + output->model );
        if( description.display_name.empty() )
        {
            description.display_name = description.id;
        }
        description.position = output->position;
        description.modes = output->modes;
        const std::size_t current = output->has_current_mode ? output->current_mode : 0U;
        description.current_mode = description.modes[ current ];
        description.desktop_extent = description.current_mode.extent;
        description.rotation = output->rotation;
        description.primary = result.empty();
        description.renderer_compatible = true;
        description.modes_complete = true;
        result.emplace_back( std::move( description ) );
    }
    return result;
}

wl_output* WaylandConnection::findOutput( const std::string& id_in ) const noexcept
{
    const sOutput* const p_output = this->findOutputRecord( id_in );
    return p_output == nullptr ? nullptr : p_output->native;
}

std::unique_ptr< sWaylandWindow > WaylandConnection::createWindow(
      std::string* const      p_error_out
    , const std::string&      title_in
    , const sRendererExtent2D extent_in
    , const bool              visible_in
)
{
    (void)visible_in;
    if( !this->available() )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland is not initialized.";
        }
        return nullptr;
    }
    auto window = std::make_unique< sWaylandWindow >();
    window->extent = extent_in;
    window->topology_generation = this->m_topology_generation;
    window->surface = wl_compositor_create_surface( this->m_compositor );
    if( window->surface != nullptr )
    {
        window->shell_surface = xdg_wm_base_get_xdg_surface( this->m_shell, window->surface );
    }
    if( window->shell_surface != nullptr )
    {
        xdg_surface_add_listener(
            window->shell_surface, &XDG_SURFACE_LISTENER, window.get() );
        window->top_level = xdg_surface_get_toplevel( window->shell_surface );
    }
    if( window->top_level == nullptr )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Failed to create an xdg-shell top-level window.";
        }
        this->destroyWindow( &window );
        return nullptr;
    }
    xdg_toplevel_add_listener( window->top_level, &TOP_LEVEL_LISTENER, window.get() );
    xdg_toplevel_set_title( window->top_level, title_in.c_str() );
    xdg_toplevel_set_app_id( window->top_level, "wse-renderer" );
    xdg_surface_set_window_geometry(
          window->shell_surface
        , 0
        , 0
        , static_cast< std::int32_t >( extent_in.width )
        , static_cast< std::int32_t >( extent_in.height )
    );
    wl_surface_commit( window->surface );
    for( int attempt = 0; attempt < 4 && !window->configured; ++attempt )
    {
        if( wl_display_roundtrip( this->m_display ) < 0 )
        {
            if( p_error_out != nullptr )
            {
                *p_error_out = "Wayland connection failed during initial configure.";
            }
            this->destroyWindow( &window );
            return nullptr;
        }
    }
    if( !window->configured )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland compositor did not configure the window.";
        }
        this->destroyWindow( &window );
        return nullptr;
    }
    window->extent_changed = false;
    return window;
}

void WaylandConnection::destroyWindow(
    std::unique_ptr< sWaylandWindow >* const p_window_inout ) noexcept
{
    if( p_window_inout == nullptr || *p_window_inout == nullptr )
    {
        return;
    }
    sWaylandWindow* const p_window = p_window_inout->get();
    if( p_window->top_level != nullptr )
    {
        xdg_toplevel_destroy( p_window->top_level );
    }
    if( p_window->shell_surface != nullptr )
    {
        xdg_surface_destroy( p_window->shell_surface );
    }
    if( p_window->surface != nullptr )
    {
        wl_surface_destroy( p_window->surface );
    }
    p_window_inout->reset();
}

bool WaylandConnection::setFullscreen(
      sWaylandWindow* const p_window_inout
    , std::string* const    p_error_out
    , const std::string&    display_id_in
    , const bool            fullscreen_in
)
{
    if( p_window_inout == nullptr || p_window_inout->top_level == nullptr )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland window is invalid.";
        }
        return false;
    }
    if( fullscreen_in )
    {
        wl_output* const p_output = this->findOutput( display_id_in );
        if( p_output == nullptr )
        {
            if( p_error_out != nullptr )
            {
                *p_error_out = "Requested Wayland output no longer exists.";
            }
            return false;
        }
        xdg_toplevel_set_fullscreen( p_window_inout->top_level, p_output );
    }
    else
    {
        xdg_toplevel_unset_fullscreen( p_window_inout->top_level );
    }
    p_window_inout->configured = false;
    wl_surface_commit( p_window_inout->surface );
    if( wl_display_roundtrip( this->m_display ) < 0 )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland fullscreen transition failed.";
        }
        return false;
    }
    p_window_inout->fullscreen = fullscreen_in;
    return true;
}

bool WaylandConnection::requestExtent(
      sWaylandWindow* const   p_window_inout
    , std::string* const      p_error_out
    , const sRendererExtent2D extent_in
)
{
    if( p_window_inout == nullptr || p_window_inout->shell_surface == nullptr ||
        p_window_inout->fullscreen )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Only a valid windowed Wayland surface can be resized.";
        }
        return false;
    }
    xdg_surface_set_window_geometry(
          p_window_inout->shell_surface
        , 0
        , 0
        , static_cast< std::int32_t >( extent_in.width )
        , static_cast< std::int32_t >( extent_in.height )
    );
    p_window_inout->extent_changed =
        p_window_inout->extent.width != extent_in.width ||
        p_window_inout->extent.height != extent_in.height;
    p_window_inout->extent = extent_in;
    wl_surface_commit( p_window_inout->surface );
    if( wl_display_roundtrip( this->m_display ) < 0 )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland resize synchronization failed.";
        }
        return false;
    }
    return true;
}

bool WaylandConnection::poll(
      sWaylandWindow* const p_window_inout
    , sSurfaceEvents* const p_events_out
    , std::string* const    p_error_out
)
{
    if( p_window_inout == nullptr || p_events_out == nullptr || !this->available() )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland event poll arguments are invalid.";
        }
        return false;
    }
    if( wl_display_roundtrip( this->m_display ) < 0 )
    {
        if( p_error_out != nullptr )
        {
            *p_error_out = "Wayland connection failed while polling events.";
        }
        return false;
    }
    p_events_out->alive = p_window_inout->alive;
    p_events_out->extent_changed = p_window_inout->extent_changed;
    p_events_out->extent = p_window_inout->extent;
    p_events_out->display_topology_changed =
        p_window_inout->topology_generation != this->m_topology_generation;
    p_window_inout->extent_changed = false;
    p_window_inout->topology_generation = this->m_topology_generation;
    return true;
}

void WaylandConnection::addRegistryObject(
      const std::uint32_t name_in
    , const char* const   p_interface_in
    , const std::uint32_t version_in
)
{
    if( p_interface_in == nullptr )
    {
        return;
    }
    if( std::strcmp( p_interface_in, wl_compositor_interface.name ) == 0 )
    {
        this->m_compositor = static_cast< wl_compositor* >(
            wl_registry_bind(
                  this->m_registry
                , name_in
                , &wl_compositor_interface
                , std::min< std::uint32_t >( version_in, 4U )
            ) );
        return;
    }
    if( std::strcmp( p_interface_in, xdg_wm_base_interface.name ) == 0 )
    {
        this->m_shell = static_cast< xdg_wm_base* >(
            wl_registry_bind( this->m_registry, name_in, &xdg_wm_base_interface, 1U ) );
        if( this->m_shell != nullptr )
        {
            xdg_wm_base_add_listener( this->m_shell, &SHELL_LISTENER, this );
        }
        return;
    }
    if( std::strcmp( p_interface_in, wl_output_interface.name ) == 0 )
    {
        auto output = std::make_unique< sOutput >();
        output->global_name = name_in;
        output->native = static_cast< wl_output* >(
            wl_registry_bind(
                  this->m_registry
                , name_in
                , &wl_output_interface
                , std::min< std::uint32_t >( version_in, 4U )
            ) );
        if( output->native != nullptr )
        {
            wl_output_add_listener( output->native, &OUTPUT_LISTENER, this );
            this->m_outputs.emplace_back( std::move( output ) );
            ++this->m_topology_generation;
        }
    }
}

void WaylandConnection::removeRegistryObject( const std::uint32_t name_in )
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [name_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->global_name == name_in;
          }
    );
    if( found == this->m_outputs.end() )
    {
        return;
    }
    if( ( *found )->native != nullptr )
    {
        wl_output_destroy( ( *found )->native );
    }
    this->m_outputs.erase( found );
    ++this->m_topology_generation;
}

void WaylandConnection::recordOutputGeometry(
      void* const        p_output_in
    , const std::int32_t x_in
    , const std::int32_t y_in
    , const std::int32_t transform_in
    , const char* const  p_make_in
    , const char* const  p_model_in
)
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [p_output_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->native == p_output_in;
          }
    );
    if( found == this->m_outputs.end() )
    {
        return;
    }
    ( *found )->position = { x_in, y_in };
    ( *found )->rotation = toRotation( transform_in );
    ( *found )->make = p_make_in == nullptr ? std::string() : p_make_in;
    ( *found )->model = p_model_in == nullptr ? std::string() : p_model_in;
}

void WaylandConnection::recordOutputMode(
      void* const         p_output_in
    , const std::uint32_t flags_in
    , const std::int32_t  width_in
    , const std::int32_t  height_in
    , const std::int32_t  refresh_millihertz_in
)
{
    if( width_in <= 0 || height_in <= 0 )
    {
        return;
    }
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [p_output_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->native == p_output_in;
          }
    );
    if( found == this->m_outputs.end() )
    {
        return;
    }
    sDisplayMode mode;
    mode.extent = {
          static_cast< std::uint32_t >( width_in )
        , static_cast< std::uint32_t >( height_in )
    };
    const std::uint32_t refresh = refresh_millihertz_in > 0
        ? static_cast< std::uint32_t >( refresh_millihertz_in ) : 0U;
    const std::uint32_t divisor = refresh == 0U ? 1U : std::gcd( refresh, 1000U );
    mode.refresh_rate_numerator = refresh / divisor;
    mode.refresh_rate_denominator = 1000U / divisor;
    mode.format = eRendererPixelFormat::Bgra8Unorm;
    sOutput* const p_output = found->get();
    p_output->modes.emplace_back( mode );
    if( ( flags_in & WL_OUTPUT_MODE_CURRENT ) != 0U )
    {
        p_output->current_mode = p_output->modes.size() - 1U;
        p_output->has_current_mode = true;
    }
}

void WaylandConnection::recordOutputName(
      void* const       p_output_in
    , const char* const p_name_in
)
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [p_output_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->native == p_output_in;
          }
    );
    if( found != this->m_outputs.end() )
    {
        ( *found )->name = p_name_in == nullptr ? std::string() : p_name_in;
    }
}

void WaylandConnection::recordOutputDescription(
      void* const       p_output_in
    , const char* const p_description_in
)
{
    const auto found = std::find_if(
          this->m_outputs.begin()
        , this->m_outputs.end()
        , [p_output_in]( const std::unique_ptr< sOutput >& output_in )
          {
              return output_in->native == p_output_in;
          }
    );
    if( found != this->m_outputs.end() )
    {
        ( *found )->description = p_description_in == nullptr
            ? std::string() : p_description_in;
    }
}

} // namespace internal
} // namespace oui
} // namespace wse
