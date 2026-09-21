//*****************************************************************************************************************
//!
//! @file    RendererTypes.h
//! @brief   \~japanese Backend非依存OUI Renderer ContractのData型を定義する.
//! @brief   \~english  Defines data types for the backend-independent OUI renderer contract.
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
#ifndef WONDERSTEWENGINE_OUI_RENDERER_RENDERERTYPES_H
#define WONDERSTEWENGINE_OUI_RENDERER_RENDERERTYPES_H

#include <utility>
#include "../../dynamic.h"
#include "RendererError.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wse
{
namespace oui
{

//! \~japanese Renderer Backend選択. \~english Renderer backend selection.
enum class eRendererBackend : std::uint8_t
{
      Automatic  = 0U //!< Platform既定Backend.
    , Direct3D12      //!< Windows Direct3D 12 Backend.
    , Vulkan12        //!< Vulkan 1.2 Backend.
};

//! \~japanese Portable Pixel format. \~english Portable pixel formats.
enum class eRendererPixelFormat : std::uint8_t
{
      Unknown      = 0U
    , R8Unorm
    , Rgba8Unorm
    , Bgra8Unorm
    , Rgba16Float
    , Rgba16Unorm  //!< \~japanese 16bit符号なし正規化. \~english 16-bit unsigned normalized.
};

//! \~japanese Texture用途Flag. \~english Texture usage flags.
enum class eTextureUsage : std::uint16_t
{
      None                = 0U
    , Sampled             = 1U << 0U
    , RenderTarget        = 1U << 1U
    , TransferSource      = 1U << 2U
    , TransferDestination = 1U << 3U
};

//! \~japanese Portable Texture状態. \~english Portable texture states.
enum class eTextureState : std::uint8_t
{
      Undefined           = 0U
    , Common
    , RenderTarget
    , ShaderResource
    , CopySource
    , CopyDestination
    , Present
};

//! \~japanese Mesh primitive topology. \~english Mesh primitive topology.
enum class ePrimitiveTopology : std::uint8_t
{
      TriangleList = 0U
    , TriangleStrip
};

//! \~japanese Texture Sampling filter. \~english Texture sampling filters.
enum class eTextureSamplingFilter : std::uint8_t
{
      Nearest = 0U
    , Linear
};

//! \~japanese Draw時のColor blend. \~english Color blending for draw commands.
enum class eColorBlendMode : std::uint8_t
{
      Replace = 0U
    , SourceAlpha
};

//! \~japanese Edge blendの減衰曲線. \~english Edge-blend attenuation curves.
enum class eEdgeBlendCurve : std::uint8_t
{
      Linear = 0U
    , Smoothstep
};

//! \~japanese Surface種類. \~english Surface types.
enum class eSurfaceType : std::uint8_t
{
      Offscreen     = 0U
    , Window
    , DirectDisplay
};

//! \~japanese Window Surfaceの表示状態. \~english Window-surface presentation modes.
enum class eSurfaceWindowMode : std::uint8_t
{
      NotApplicable = 0U
    , Windowed
    , BorderlessFullscreen
    , DisplayModeFullscreen
};

//! \~japanese Displayの回転. \~english Display rotation.
enum class eDisplayRotation : std::uint8_t
{
      Unknown = 0U
    , Identity
    , Rotate90
    , Rotate180
    , Rotate270
};

//! \~japanese Color attachmentのLoad動作. \~english Color attachment load operation.
enum class eAttachmentLoadOperation : std::uint8_t
{
      Load = 0U
    , Clear
    , Discard
};

//! \~japanese Color attachmentのStore動作. \~english Color attachment store operation.
enum class eAttachmentStoreOperation : std::uint8_t
{
      Store = 0U
    , Discard
};

//! \~japanese 2次元Extent. \~english Two-dimensional extent.
struct sRendererExtent2D
{
    std::uint32_t width; //!< Width [pixel].
    std::uint32_t height; //!< Height [pixel].

    //! @return \~japanese 空Extentの場合true. \~english True for an empty extent.
    WSE_API bool empty() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sRendererExtent2D(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
    )
        : width  ( width_in )
        , height ( height_in )
    {
    }
};

//! \~japanese 2次元領域. \~english Two-dimensional region.
struct sRendererRegion2D
{
    std::uint32_t      x; //!< Left [pixel].
    std::uint32_t      y; //!< Top [pixel].
    sRendererExtent2D  extent; //!< Region extent. Empty means the complete attachment.

    //! @brief Construct all members with explicit defaults.
    sRendererRegion2D(
          std::uint32_t x_in = 0U
        , std::uint32_t y_in = 0U
        , const sRendererExtent2D& extent_in = {}
    )
        : x      ( x_in )
        , y      ( y_in )
        , extent ( extent_in )
    {
    }
};

//! \~japanese CPU Frame layout. \~english CPU frame layout.
struct sRendererFrameDescription
{
    sRendererExtent2D    extent;
    eRendererPixelFormat format;
    std::size_t          row_pitch; //!< Row size [byte]. Zero selects the minimum packed pitch.

    //! @return \~japanese 1 PixelのByte数. 未定義Formatは0. \~english Bytes per pixel, or zero for unknown.
    WSE_API std::size_t bytesPerPixel() const noexcept;

    //! @return \~japanese 最小Row pitch [byte]. \~english Minimum row pitch in bytes.
    WSE_API std::size_t minimumRowPitch() const noexcept;

    //! @return \~japanese 有効Row pitch [byte]. \~english Effective row pitch in bytes.
    WSE_API std::size_t effectiveRowPitch() const noexcept;

    //! @return \~japanese Frame全体Size [byte]. Overflowまたは不正時は0.
    //!         \~english Total byte size, or zero on invalid input/overflow.
    WSE_API std::size_t memorySize() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sRendererFrameDescription(
          const sRendererExtent2D& extent_in = {}
        , eRendererPixelFormat format_in = eRendererPixelFormat::Unknown
        , std::size_t row_pitch_in = 0U
    )
        : extent    ( extent_in )
        , format    ( format_in )
        , row_pitch ( row_pitch_in )
    {
    }
};

//! \~japanese CPU FrameとLayout. \~english CPU frame and its layout.
struct sRendererFrame
{
    sRendererFrameDescription description;
    std::vector< std::uint8_t > data;

    //! @brief Construct all members with explicit defaults.
    sRendererFrame(
          const sRendererFrameDescription& description_in = {}
        , const std::vector< std::uint8_t >& data_in = {}
    )
        : description ( description_in )
        , data        ( data_in )
    {
    }
};

//! \~japanese Texture descriptor. \~english Texture descriptor.
struct sTextureDescription
{
    sRendererExtent2D    extent;
    eRendererPixelFormat format;
    eTextureUsage        usage;
    eTextureState        initial_state;
    std::uint32_t        mip_levels;

    //! @brief Construct all members with explicit defaults.
    sTextureDescription(
          const sRendererExtent2D& extent_in = {}
        , eRendererPixelFormat format_in = eRendererPixelFormat::Unknown
        , eTextureUsage usage_in = eTextureUsage::None
        , eTextureState initial_state_in = eTextureState::Undefined
        , std::uint32_t mip_levels_in = 1U
    )
        : extent        ( extent_in )
        , format        ( format_in )
        , usage         ( usage_in )
        , initial_state ( initial_state_in )
        , mip_levels    ( mip_levels_in )
    {
    }
};

//! \~japanese Projection用2D Vertex. \~english 2D vertex for projection.
struct sRendererVertex2D
{
    float position_x;
    float position_y;
    float texture_u;
    float texture_v;

    //! @brief Construct all members with explicit defaults.
    sRendererVertex2D(
          float position_x_in = 0.0F
        , float position_y_in = 0.0F
        , float texture_u_in = 0.0F
        , float texture_v_in = 0.0F
    )
        : position_x ( position_x_in )
        , position_y ( position_y_in )
        , texture_u  ( texture_u_in )
        , texture_v  ( texture_v_in )
    {
    }
};

//! \~japanese Backend非依存Mesh descriptor. \~english Backend-independent mesh descriptor.
struct sMeshDescription
{
    ePrimitiveTopology               topology;
    std::vector< sRendererVertex2D > vertices;
    std::vector< std::uint32_t >     indices;

    //! @brief Construct all members with explicit defaults.
    sMeshDescription(
          ePrimitiveTopology topology_in = ePrimitiveTopology::TriangleList
        , const std::vector< sRendererVertex2D >& vertices_in = {}
        , const std::vector< std::uint32_t >& indices_in = {}
    )
        : topology ( topology_in )
        , vertices ( vertices_in )
        , indices  ( indices_in )
    {
    }
};

//! \~japanese Linear RGBA clear color. \~english Linear RGBA clear color.
struct sRendererColor
{
    float red;
    float green;
    float blue;
    float alpha;

    //! @brief Construct all members with explicit defaults.
    sRendererColor(
          float red_in = 0.0F
        , float green_in = 0.0F
        , float blue_in = 0.0F
        , float alpha_in = 1.0F
    )
        : red   ( red_in )
        , green ( green_in )
        , blue  ( blue_in )
        , alpha ( alpha_in )
    {
    }
};

//! \~japanese Texture UV境界からのEdge blend幅. \~english Edge-blend widths from texture UV borders.
struct sEdgeBlendDescription
{
    float           left; //!< Normalized U width from the left edge.
    float           right; //!< Normalized U width from the right edge.
    float           top; //!< Normalized V width from the top edge.
    float           bottom; //!< Normalized V width from the bottom edge.
    eEdgeBlendCurve curve;

    //! @brief Construct all members with explicit defaults.
    sEdgeBlendDescription(
          float left_in = 0.0F
        , float right_in = 0.0F
        , float top_in = 0.0F
        , float bottom_in = 0.0F
        , eEdgeBlendCurve curve_in = eEdgeBlendCurve::Linear
    )
        : left   ( left_in )
        , right  ( right_in )
        , top    ( top_in )
        , bottom ( bottom_in )
        , curve  ( curve_in )
    {
    }
};

//! \~japanese Texture opaque handle. \~english Opaque texture handle.
struct sTextureHandle
{
    std::uint64_t value;
    std::uint32_t generation;

    //! @return \~japanese 有効Handleの場合true. \~english True for a valid handle.
    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sTextureHandle(
          std::uint64_t value_in = 0U
        , std::uint32_t generation_in = 0U
    )
        : value      ( value_in )
        , generation ( generation_in )
    {
    }
};

//! \~japanese Mesh opaque handle. \~english Opaque mesh handle.
struct sMeshHandle
{
    std::uint64_t value;
    std::uint32_t generation;

    //! @return \~japanese 有効Handleの場合true. \~english True for a valid handle.
    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sMeshHandle(
          std::uint64_t value_in = 0U
        , std::uint32_t generation_in = 0U
    )
        : value      ( value_in )
        , generation ( generation_in )
    {
    }
};

//! \~japanese Surface opaque handle. \~english Opaque surface handle.
struct sSurfaceHandle
{
    std::uint64_t value;
    std::uint32_t generation;

    //! @return \~japanese 有効Handleの場合true. \~english True for a valid handle.
    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sSurfaceHandle(
          std::uint64_t value_in = 0U
        , std::uint32_t generation_in = 0U
    )
        : value      ( value_in )
        , generation ( generation_in )
    {
    }
};

//! \~japanese Fence opaque handle. \~english Opaque fence handle.
struct sFenceHandle
{
    std::uint64_t value;
    std::uint32_t generation;

    //! @return \~japanese 有効Handleの場合true. \~english True for a valid handle.
    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sFenceHandle(
          std::uint64_t value_in = 0U
        , std::uint32_t generation_in = 0U
    )
        : value      ( value_in )
        , generation ( generation_in )
    {
    }
};

//! \~japanese TextureをMeshへ割り当てるDraw command. \~english Draw command binding a texture to a mesh.
struct sMeshDrawCommand
{
    sMeshHandle            mesh;
    sTextureHandle         source_texture;
    sTextureHandle         alpha_texture; //!< Optional alpha-map texture.
    eTextureSamplingFilter sampling_filter;
    eColorBlendMode        blend_mode;
    float                  opacity;
    sEdgeBlendDescription  edge_blend;

    //! @brief Construct all members with explicit defaults.
    sMeshDrawCommand(
          const sMeshHandle& mesh_in = {}
        , const sTextureHandle& source_texture_in = {}
        , const sTextureHandle& alpha_texture_in = {}
        , eTextureSamplingFilter sampling_filter_in = eTextureSamplingFilter::Nearest
        , eColorBlendMode blend_mode_in = eColorBlendMode::Replace
        , float opacity_in = 1.0F
        , const sEdgeBlendDescription& edge_blend_in = {}
    )
        : mesh            ( mesh_in )
        , source_texture  ( source_texture_in )
        , alpha_texture   ( alpha_texture_in )
        , sampling_filter ( sampling_filter_in )
        , blend_mode      ( blend_mode_in )
        , opacity         ( opacity_in )
        , edge_blend      ( edge_blend_in )
    {
    }
};

//! \~japanese Color attachment Render pass descriptor. \~english Color-attachment render-pass descriptor.
struct sRenderPassDescription
{
    sTextureHandle                  color_attachment;
    sRendererRegion2D               render_area;
    eAttachmentLoadOperation        load_operation;
    eAttachmentStoreOperation       store_operation;
    sRendererColor                  clear_color;
    std::vector< sMeshDrawCommand > draw_commands;
    eTextureState                   final_state;

    //! @brief Construct all members with explicit defaults.
    sRenderPassDescription(
          const sTextureHandle& color_attachment_in = {}
        , const sRendererRegion2D& render_area_in = {}
        , eAttachmentLoadOperation load_operation_in = eAttachmentLoadOperation::Clear
        , eAttachmentStoreOperation store_operation_in = eAttachmentStoreOperation::Store
        , const sRendererColor& clear_color_in = {}
        , const std::vector< sMeshDrawCommand >& draw_commands_in = {}
        , eTextureState final_state_in = eTextureState::RenderTarget
    )
        : color_attachment ( color_attachment_in )
        , render_area      ( render_area_in )
        , load_operation   ( load_operation_in )
        , store_operation  ( store_operation_in )
        , clear_color      ( clear_color_in )
        , draw_commands    ( draw_commands_in )
        , final_state      ( final_state_in )
    {
    }
};

//! \~japanese Desktop上のDisplay位置. \~english Display position in desktop coordinates.
struct sDisplayPosition2D
{
    std::int32_t x; //!< Left [pixel]. Negative values are valid.
    std::int32_t y; //!< Top [pixel]. Negative values are valid.

    //! @brief Construct all members with explicit defaults.
    sDisplayPosition2D(
          std::int32_t x_in = 0
        , std::int32_t y_in = 0
    )
        : x ( x_in )
        , y ( y_in )
    {
    }
};

//! \~japanese Display mode. \~english Display mode.
struct sDisplayMode
{
    sRendererExtent2D    extent;
    std::uint32_t        refresh_rate_numerator; //!< Zero means unspecified.
    std::uint32_t        refresh_rate_denominator;
    eRendererPixelFormat format;
    bool                 interlaced;

    //! @brief Construct all members with explicit defaults.
    sDisplayMode(
          const sRendererExtent2D& extent_in = {}
        , std::uint32_t refresh_rate_numerator_in = 0U
        , std::uint32_t refresh_rate_denominator_in = 1U
        , eRendererPixelFormat format_in = eRendererPixelFormat::Unknown
        , bool interlaced_in = false
    )
        : extent                   ( extent_in )
        , refresh_rate_numerator   ( refresh_rate_numerator_in )
        , refresh_rate_denominator ( refresh_rate_denominator_in )
        , format                   ( format_in )
        , interlaced               ( interlaced_in )
    {
    }
};

//! \~japanese Active DisplayのPortable descriptor. \~english Portable active-display descriptor.
struct sDisplayDescription
{
    std::string                 id; //!< Topology-scoped backend-qualified ID.
    std::string                 adapter_id; //!< Backend-qualified adapter ID.
    std::string                 adapter_name;
    std::string                 display_name;
    sDisplayPosition2D          position;
    sRendererExtent2D           desktop_extent; //!< Desktop-coordinate output extent.
    sDisplayMode                current_mode;
    std::vector< sDisplayMode > modes;
    eDisplayRotation            rotation;
    bool                        primary;
    bool                        renderer_compatible;
    bool                        modes_complete;

    //! @brief Construct all members with explicit defaults.
    sDisplayDescription(
          const std::string& id_in = {}
        , const std::string& adapter_id_in = {}
        , const std::string& adapter_name_in = {}
        , const std::string& display_name_in = {}
        , const sDisplayPosition2D& position_in = {}
        , const sRendererExtent2D& desktop_extent_in = {}
        , const sDisplayMode& current_mode_in = {}
        , const std::vector< sDisplayMode >& modes_in = {}
        , eDisplayRotation rotation_in = eDisplayRotation::Unknown
        , bool primary_in = false
        , bool renderer_compatible_in = false
        , bool modes_complete_in = false
    )
        : id                  ( id_in )
        , adapter_id          ( adapter_id_in )
        , adapter_name        ( adapter_name_in )
        , display_name        ( display_name_in )
        , position            ( position_in )
        , desktop_extent      ( desktop_extent_in )
        , current_mode        ( current_mode_in )
        , modes               ( modes_in )
        , rotation            ( rotation_in )
        , primary             ( primary_in )
        , renderer_compatible ( renderer_compatible_in )
        , modes_complete      ( modes_complete_in )
    {
    }
};

//! \~japanese Surface descriptor. \~english Surface descriptor.
struct sSurfaceDescription
{
    eSurfaceType         type;
    sRendererExtent2D    extent;
    eRendererPixelFormat format;
    std::uint32_t        buffer_count;
    bool                 vertical_sync;
    bool                 visible;
    std::string          title;
    std::string          display_id; //!< Required only for DirectDisplay.
    sDisplayMode         display_mode; //!< Required only for DirectDisplay.

    //! @brief Construct all members with explicit defaults.
    sSurfaceDescription(
          eSurfaceType type_in = eSurfaceType::Offscreen
        , const sRendererExtent2D& extent_in = {}
        , eRendererPixelFormat format_in = eRendererPixelFormat::Rgba8Unorm
        , std::uint32_t buffer_count_in = 1U
        , bool vertical_sync_in = true
        , bool visible_in = true
        , const std::string& title_in = "WSE Renderer"
        , const std::string& display_id_in = {}
        , const sDisplayMode& display_mode_in = {}
    )
        : type          ( type_in )
        , extent        ( extent_in )
        , format        ( format_in )
        , buffer_count  ( buffer_count_in )
        , vertical_sync ( vertical_sync_in )
        , visible       ( visible_in )
        , title         ( title_in )
        , display_id    ( display_id_in )
        , display_mode  ( display_mode_in )
    {
    }
};

//! \~japanese 現在のSurface状態. \~english Current surface state.
struct sSurfaceState
{
    eSurfaceType         type;
    sRendererExtent2D    extent;
    eSurfaceWindowMode   window_mode;
    std::string          display_id; //!< Non-empty in display-targeted modes.
    sDisplayMode         display_mode; //!< Valid only in display-mode fullscreen.

    //! @brief Construct all members with explicit defaults.
    sSurfaceState(
          eSurfaceType type_in = eSurfaceType::Offscreen
        , const sRendererExtent2D& extent_in = {}
        , eSurfaceWindowMode window_mode_in = eSurfaceWindowMode::NotApplicable
        , const std::string& display_id_in = {}
        , const sDisplayMode& display_mode_in = {}
    )
        : type         ( type_in )
        , extent       ( extent_in )
        , window_mode  ( window_mode_in )
        , display_id   ( display_id_in )
        , display_mode ( display_mode_in )
    {
    }
};

//! \~japanese Window Surface表示状態の変更要求. \~english Window-surface mode request.
struct sSurfaceWindowModeRequest
{
    eSurfaceWindowMode mode;
    std::string        display_id; //!< Required for display-targeted modes.
    sDisplayMode       display_mode; //!< Required for display-mode fullscreen.

    //! @brief Construct all members with explicit defaults.
    sSurfaceWindowModeRequest(
          eSurfaceWindowMode mode_in = eSurfaceWindowMode::Windowed
        , const std::string& display_id_in = {}
        , const sDisplayMode& display_mode_in = {}
    )
        : mode         ( mode_in )
        , display_id   ( display_id_in )
        , display_mode ( display_mode_in )
    {
    }
};

//! \~japanese Surface event処理結果. \~english Surface-event processing result.
struct sSurfaceEvents
{
    bool              alive;
    bool              extent_changed;
    sRendererExtent2D extent;
    bool              display_topology_changed;

    //! @brief Construct all members with explicit defaults.
    sSurfaceEvents(
          bool alive_in = true
        , bool extent_changed_in = false
        , const sRendererExtent2D& extent_in = {}
        , bool display_topology_changed_in = false
    )
        : alive                    ( alive_in )
        , extent_changed           ( extent_changed_in )
        , extent                   ( extent_in )
        , display_topology_changed ( display_topology_changed_in )
    {
    }
};

//! \~japanese Renderer初期化設定. \~english Renderer initialization configuration.
struct sRendererConfiguration
{
    eRendererBackend backend;
    bool             use_software_adapter;
    bool             prefer_display_adapter;
    bool             enable_validation;
    //! \~japanese 指名するAdapter名の一部。空なら自動選択。一致するAdapterが無ければ初期化は失敗する。
    //! \~english  Part of the adapter name to select; empty selects automatically. Initialization
    //!            fails when no adapter matches, rather than quietly using another one.
    std::string      adapter_name;

    //! @brief Construct all members with explicit defaults.
    sRendererConfiguration(
          eRendererBackend backend_in = eRendererBackend::Automatic
        , bool use_software_adapter_in = false
        , bool prefer_display_adapter_in = false
        , bool enable_validation_in = false
        , const std::string& adapter_name_in = {}
    )
        : backend                ( backend_in )
        , use_software_adapter   ( use_software_adapter_in )
        , prefer_display_adapter ( prefer_display_adapter_in )
        , enable_validation      ( enable_validation_in )
        , adapter_name           ( adapter_name_in )
    {
    }
};

//! \~japanese 初期化済みBackend能力. \~english Initialized backend capabilities.
struct sRendererCapabilities
{
    eRendererBackend backend;
    //! \~japanese 実際に選択したAdapterの名前. \~english Name of the adapter that was selected.
    std::string      adapter_name;
    std::uint32_t    maximum_texture_extent;
    bool             supports_offscreen;
    bool             supports_window;
    bool             supports_direct_display;
    bool             supports_mesh_rendering;
    bool             supports_display_enumeration;
    bool             supports_surface_resize;
    bool             supports_borderless_fullscreen;
    bool             supports_display_mode_fullscreen;
    bool             supports_interactive_resize;
    bool             supports_display_hotplug;

    //! @brief Construct all members with explicit defaults.
    sRendererCapabilities(
          eRendererBackend backend_in = eRendererBackend::Automatic
        , const std::string& adapter_name_in = {}
        , std::uint32_t maximum_texture_extent_in = 0U
        , bool supports_offscreen_in = false
        , bool supports_window_in = false
        , bool supports_direct_display_in = false
        , bool supports_mesh_rendering_in = false
        , bool supports_display_enumeration_in = false
        , bool supports_surface_resize_in = false
        , bool supports_borderless_fullscreen_in = false
        , bool supports_display_mode_fullscreen_in = false
        , bool supports_interactive_resize_in = false
        , bool supports_display_hotplug_in = false
    )
        : backend                          ( backend_in )
        , adapter_name                     ( adapter_name_in )
        , maximum_texture_extent           ( maximum_texture_extent_in )
        , supports_offscreen               ( supports_offscreen_in )
        , supports_window                  ( supports_window_in )
        , supports_direct_display          ( supports_direct_display_in )
        , supports_mesh_rendering          ( supports_mesh_rendering_in )
        , supports_display_enumeration     ( supports_display_enumeration_in )
        , supports_surface_resize          ( supports_surface_resize_in )
        , supports_borderless_fullscreen   ( supports_borderless_fullscreen_in )
        , supports_display_mode_fullscreen ( supports_display_mode_fullscreen_in )
        , supports_interactive_resize      ( supports_interactive_resize_in )
        , supports_display_hotplug         ( supports_display_hotplug_in )
    {
    }
};

//! @return \~japanese Bit flag結合値. \~english Combined bit flags.
constexpr eTextureUsage operator | ( const eTextureUsage left_in, const eTextureUsage right_in ) noexcept
{
    return static_cast< eTextureUsage >(
        static_cast< std::uint16_t >( left_in ) | static_cast< std::uint16_t >( right_in ) );
}

//! @return \~japanese 指定Flagをすべて含む場合true. \~english True when every requested flag is present.
constexpr bool hasTextureUsage( const eTextureUsage value_in, const eTextureUsage required_in ) noexcept
{
    return ( static_cast< std::uint16_t >( value_in ) & static_cast< std::uint16_t >( required_in ) ) ==
        static_cast< std::uint16_t >( required_in );
}

//! @return \~japanese Frame descriptor検証結果. \~english Frame descriptor validation result.
WSE_API RendererError validateRendererFrameDescription( const sRendererFrameDescription& description_in );

//! @return \~japanese Frame dataを含む検証結果. \~english Validation result including frame data.
WSE_API RendererError validateRendererFrame( const sRendererFrame& frame_in );

//! @return \~japanese Texture descriptor検証結果. \~english Texture descriptor validation result.
WSE_API RendererError validateTextureDescription( const sTextureDescription& description_in );

//! @return \~japanese Mesh descriptor検証結果. \~english Mesh descriptor validation result.
WSE_API RendererError validateMeshDescription( const sMeshDescription& description_in );

//! @return \~japanese Render pass descriptor検証結果. \~english Render-pass descriptor validation result.
WSE_API RendererError validateRenderPassDescription( const sRenderPassDescription& description_in );

//! @return \~japanese Surface descriptor検証結果. \~english Surface descriptor validation result.
WSE_API RendererError validateSurfaceDescription( const sSurfaceDescription& description_in );

//! @return \~japanese Surface state検証結果. \~english Surface-state validation result.
WSE_API RendererError validateSurfaceState( const sSurfaceState& state_in );

//! @return \~japanese Window mode要求検証結果. \~english Window-mode request validation result.
WSE_API RendererError validateSurfaceWindowModeRequest(
    const sSurfaceWindowModeRequest& request_in );

//! @return \~japanese Display mode検証結果. \~english Display-mode validation result.
WSE_API RendererError validateDisplayMode( const sDisplayMode& mode_in );

//! @return \~japanese Display descriptor検証結果. \~english Display-descriptor validation result.
WSE_API RendererError validateDisplayDescription( const sDisplayDescription& description_in );

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_RENDERERTYPES_H
