//*****************************************************************************************************************
//!
//! @file    D3D12RendererBackend.cpp
//! @brief   \~japanese Backend非依存OUI ContractのDirect3D 12実装を提供する.
//! @brief   \~english  Provides the Direct3D 12 implementation of the backend-independent OUI contract.
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

#include "../../../../core/oui/renderer/RendererBackend.h"
#include "../../../../core/oui/renderer/RendererIdentity.h"
#include "D3D12DisplayEnumeration.h"
#include "Win32WindowTransition.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using Microsoft::WRL::ComPtr;

constexpr std::uint32_t SHUTDOWN_TIMEOUT_MS = 30000U;
constexpr std::uint32_t DRAW_FLAG_USE_ALPHA_MAP = 1U << 0U;
constexpr std::uint32_t DRAW_FLAG_LINEAR_SAMPLING = 1U << 1U;
constexpr std::uint32_t DRAW_FLAG_ALPHA_RED_CHANNEL = 1U << 2U;
constexpr std::uint32_t DRAW_FLAG_USE_EDGE_BLEND = 1U << 3U;
constexpr std::uint32_t DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND = 1U << 4U;
constexpr std::uint32_t DRAW_DESCRIPTORS_PER_COMMAND = 2U;
constexpr std::uint32_t DRAW_PARAMETER_COUNT = 6U;

constexpr const char* VERTEX_SHADER_SOURCE = R"(
struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main( float2 position : POSITION, float2 uv : TEXCOORD0 )
{
    VertexOutput output;
    output.position = float4( position, 0.0F, 1.0F );
    output.uv = uv;
    return output;
}
)";

constexpr const char* PIXEL_SHADER_SOURCE = R"(
Texture2D<float4> source_texture : register(t0);
Texture2D<float4> alpha_texture : register(t1);
SamplerState nearest_sampler : register(s0);
SamplerState linear_sampler : register(s1);

cbuffer DrawParameters : register(b0)
{
    uint draw_flags;
    float draw_opacity;
    float edge_blend_left;
    float edge_blend_right;
    float edge_blend_top;
    float edge_blend_bottom;
};

float applyEdgeCurve( float value_in )
{
    const float normalized = saturate( value_in );
    const bool use_smoothstep = ( draw_flags & 16U ) != 0U;
    return use_smoothstep
        ? normalized * normalized * ( 3.0F - 2.0F * normalized )
        : normalized;
}

float4 main( float4 position : SV_POSITION, float2 uv : TEXCOORD0 ) : SV_TARGET
{
    const bool use_alpha_map = ( draw_flags & 1U ) != 0U;
    const bool use_linear_sampling = ( draw_flags & 2U ) != 0U;
    float4 source_color;
    float4 alpha_color;
    if( use_linear_sampling )
    {
        source_color = source_texture.Sample( linear_sampler, uv );
        alpha_color = alpha_texture.Sample( linear_sampler, uv );
    }
    else
    {
        source_color = source_texture.Sample( nearest_sampler, uv );
        alpha_color = alpha_texture.Sample( nearest_sampler, uv );
    }
    const bool alpha_red_channel = ( draw_flags & 4U ) != 0U;
    const float alpha_sample = alpha_red_channel ? alpha_color.r : alpha_color.a;
    const float alpha_factor = use_alpha_map ? alpha_sample : 1.0F;
    float edge_factor = 1.0F;
    if( ( draw_flags & 8U ) != 0U )
    {
        if( edge_blend_left > 0.0F )
        {
            edge_factor *= applyEdgeCurve( uv.x / edge_blend_left );
        }
        if( edge_blend_right > 0.0F )
        {
            edge_factor *= applyEdgeCurve( ( 1.0F - uv.x ) / edge_blend_right );
        }
        if( edge_blend_top > 0.0F )
        {
            edge_factor *= applyEdgeCurve( uv.y / edge_blend_top );
        }
        if( edge_blend_bottom > 0.0F )
        {
            edge_factor *= applyEdgeCurve( ( 1.0F - uv.y ) / edge_blend_bottom );
        }
    }
    source_color.a *= alpha_factor * edge_factor * draw_opacity;
    return source_color;
}
)";

struct sWindowEventState
{
    bool                       alive;
    bool                       extent_changed;
    wse::oui::sRendererExtent2D extent;
    bool                       minimized;
    bool                       display_topology_changed;

    //! @brief Construct all members with explicit defaults.
    sWindowEventState(
          bool alive_in = true
        , bool extent_changed_in = false
        , const wse::oui::sRendererExtent2D& extent_in = {}
        , bool minimized_in = false
        , bool display_topology_changed_in = false
    )
        : alive                    ( alive_in )
        , extent_changed           ( extent_changed_in )
        , extent                   ( extent_in )
        , minimized                ( minimized_in )
        , display_topology_changed ( display_topology_changed_in )
    {
    }
};

LRESULT CALLBACK rendererWindowProcedure(
      const HWND   window_in
    , const UINT   message_in
    , const WPARAM word_parameter_in
    , const LPARAM long_parameter_in
)
{
    sWindowEventState* p_state = reinterpret_cast< sWindowEventState* >(
        GetWindowLongPtrW( window_in, GWLP_USERDATA ) );
    if( message_in == WM_NCCREATE )
    {
        const CREATESTRUCTW* const p_create =
            reinterpret_cast< const CREATESTRUCTW* >( long_parameter_in );
        p_state = static_cast< sWindowEventState* >( p_create->lpCreateParams );
        SetWindowLongPtrW(
            window_in, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( p_state ) );
    }
    if( p_state != nullptr && message_in == WM_SIZE )
    {
        p_state->minimized = word_parameter_in == SIZE_MINIMIZED;
        if( !p_state->minimized )
        {
            p_state->extent = {
                  static_cast< std::uint32_t >( LOWORD( long_parameter_in ) )
                , static_cast< std::uint32_t >( HIWORD( long_parameter_in ) )
            };
            p_state->extent_changed = !p_state->extent.empty();
        }
    }
    if( p_state != nullptr &&
        ( message_in == WM_DISPLAYCHANGE || message_in == WM_DEVICECHANGE ) )
    {
        p_state->display_topology_changed = true;
    }
    if( message_in == WM_CLOSE )
    {
        if( p_state != nullptr )
        {
            p_state->alive = false;
        }
        DestroyWindow( window_in );
        return 0;
    }
    if( message_in == WM_NCDESTROY )
    {
        if( p_state != nullptr )
        {
            p_state->alive = false;
        }
        SetWindowLongPtrW( window_in, GWLP_USERDATA, 0 );
    }
    return DefWindowProcW( window_in, message_in, word_parameter_in, long_parameter_in );
}

// Error and native-type conversion helpers.
wse::oui::RendererError makeError(
      const wse::oui::eRendererErrorCategory category_in
    , const wse::oui::eRendererErrorCode     code_in
    , const std::string&                     message_in
    , const HRESULT                          native_code_in = S_OK
)
{
    return wse::oui::RendererError(
          category_in
        , code_in
        , message_in
        , static_cast< std::int64_t >( native_code_in )
    );
}

wse::oui::RendererError backendError( const std::string& message_in, const HRESULT result_in )
{
    const bool device_lost = result_in == DXGI_ERROR_DEVICE_REMOVED ||
        result_in == DXGI_ERROR_DEVICE_RESET;
    return makeError(
          device_lost ? wse::oui::eRendererErrorCategory::Backend
                      : wse::oui::eRendererErrorCategory::Resource
        , device_lost ? wse::oui::eRendererErrorCode::DeviceLost
                      : wse::oui::eRendererErrorCode::BackendFailure
        , message_in
        , result_in
    );
}

wse::oui::RendererError displayChangeError(
      const std::string& message_in
    , const LONG         result_in
)
{
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Backend
        , wse::oui::eRendererErrorCode::BackendFailure
        , message_in
        , static_cast< std::int64_t >( result_in )
    );
}

wse::oui::RendererError transitionWithRollbackError(
      const wse::oui::RendererError& transition_error_in
    , const wse::oui::RendererError& rollback_error_in
)
{
    if( rollback_error_in.ok() )
    {
        return transition_error_in;
    }
    return wse::oui::RendererError(
          wse::oui::eRendererErrorCategory::Backend
        , wse::oui::eRendererErrorCode::BackendFailure
        , transition_error_in.message() + " Rollback also failed: " + rollback_error_in.message()
        , rollback_error_in.nativeCode()
    );
}

wse::oui::RendererError unsupportedError(
      const wse::oui::eRendererErrorCode code_in
    , const std::string&                 message_in
)
{
    return makeError(
          wse::oui::eRendererErrorCategory::Unsupported
        , code_in
        , message_in
    );
}

wse::oui::RendererError resourceNotFoundError( const std::string& resource_in )
{
    return makeError(
          wse::oui::eRendererErrorCategory::Resource
        , wse::oui::eRendererErrorCode::ResourceNotFound
        , resource_in + " handle was not found."
    );
}

DXGI_FORMAT toNativeFormat( const wse::oui::eRendererPixelFormat format_in ) noexcept
{
    switch( format_in )
    {
        case wse::oui::eRendererPixelFormat::R8Unorm:
            return DXGI_FORMAT_R8_UNORM;
        case wse::oui::eRendererPixelFormat::Rgba8Unorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case wse::oui::eRendererPixelFormat::Bgra8Unorm:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case wse::oui::eRendererPixelFormat::Rgba16Float:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case wse::oui::eRendererPixelFormat::Rgba16Unorm:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

bool sameDisplayMode(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in
) noexcept
{
    return left_in.extent.width == right_in.extent.width &&
        left_in.extent.height == right_in.extent.height &&
        left_in.refresh_rate_numerator == right_in.refresh_rate_numerator &&
        left_in.refresh_rate_denominator == right_in.refresh_rate_denominator &&
        left_in.format == right_in.format && left_in.interlaced == right_in.interlaced;
}

D3D12_RESOURCE_STATES toNativeState( const wse::oui::eTextureState state_in ) noexcept
{
    switch( state_in )
    {
        case wse::oui::eTextureState::RenderTarget:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case wse::oui::eTextureState::ShaderResource:
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case wse::oui::eTextureState::CopySource:
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case wse::oui::eTextureState::CopyDestination:
            return D3D12_RESOURCE_STATE_COPY_DEST;
        case wse::oui::eTextureState::Present:
            return D3D12_RESOURCE_STATE_PRESENT;
        case wse::oui::eTextureState::Undefined:
        case wse::oui::eTextureState::Common:
        default:
            return D3D12_RESOURCE_STATE_COMMON;
    }
}

D3D12_HEAP_PROPERTIES makeHeapProperties( const D3D12_HEAP_TYPE type_in ) noexcept
{
    D3D12_HEAP_PROPERTIES properties = {};
    properties.Type                  = type_in;
    properties.CreationNodeMask      = 1U;
    properties.VisibleNodeMask       = 1U;
    return properties;
}

D3D12_RESOURCE_BARRIER makeTransition(
      ID3D12Resource* const      p_resource_in
    , const D3D12_RESOURCE_STATES before_in
    , const D3D12_RESOURCE_STATES after_in
) noexcept
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = p_resource_in;
    barrier.Transition.StateBefore = before_in;
    barrier.Transition.StateAfter  = after_in;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

} // namespace

namespace wse
{
namespace oui
{
namespace internal
{

// Backend state and resource ownership.
class D3D12RendererBackend final : public RendererBackend
{
  private:
    struct sTextureRecord
    {
        ComPtr< ID3D12Resource >       resource;
        ComPtr< ID3D12DescriptorHeap > render_target_heap;
        ComPtr< ID3D12DescriptorHeap > shader_resource_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE    render_target_handle;
        sTextureDescription            description;
        eTextureState                  state;
        std::uint64_t                  surface_owner;

        //! @brief Construct all members with explicit defaults.
        sTextureRecord(
              const ComPtr< ID3D12Resource >& resource_in = {}
            , const ComPtr< ID3D12DescriptorHeap >& render_target_heap_in = {}
            , const ComPtr< ID3D12DescriptorHeap >& shader_resource_heap_in = {}
            , const D3D12_CPU_DESCRIPTOR_HANDLE& render_target_handle_in = {}
            , const sTextureDescription& description_in = {}
            , eTextureState state_in = eTextureState::Common
            , std::uint64_t surface_owner_in = 0U
        )
            : resource             ( resource_in )
            , render_target_heap   ( render_target_heap_in )
            , shader_resource_heap ( shader_resource_heap_in )
            , render_target_handle ( render_target_handle_in )
            , description          ( description_in )
            , state                ( state_in )
            , surface_owner        ( surface_owner_in )
        {
        }
    };

    struct sMeshRecord
    {
        ComPtr< ID3D12Resource > vertex_buffer;
        ComPtr< ID3D12Resource > index_buffer;
        D3D12_VERTEX_BUFFER_VIEW vertex_view;
        D3D12_INDEX_BUFFER_VIEW  index_view;
        ePrimitiveTopology       topology;
        std::uint32_t            index_count;

        //! @brief Construct all members with explicit defaults.
        sMeshRecord(
              const ComPtr< ID3D12Resource >& vertex_buffer_in = {}
            , const ComPtr< ID3D12Resource >& index_buffer_in = {}
            , const D3D12_VERTEX_BUFFER_VIEW& vertex_view_in = {}
            , const D3D12_INDEX_BUFFER_VIEW& index_view_in = {}
            , ePrimitiveTopology topology_in = ePrimitiveTopology::TriangleList
            , std::uint32_t index_count_in = 0U
        )
            : vertex_buffer ( vertex_buffer_in )
            , index_buffer  ( index_buffer_in )
            , vertex_view   ( vertex_view_in )
            , index_view    ( index_view_in )
            , topology      ( topology_in )
            , index_count   ( index_count_in )
        {
        }
    };

    struct sSurfaceRecord
    {
        sSurfaceDescription description;
        std::vector< sTextureHandle > textures;
        ComPtr< IDXGISwapChain3 >     swap_chain;
        HWND                          window;
        std::unique_ptr< sWindowEventState > window_events;
        eSurfaceWindowMode            window_mode;
        std::string                   display_id;
        sDisplayMode                  display_mode;
        sDisplayMode                  restore_display_mode;
        std::wstring                  display_device_name;
        DEVMODEW                      restore_native_display_mode;
        bool                          has_native_display_mode;
        sWin32WindowedState           windowed_state;
        sRendererExtent2D             windowed_extent;
        bool                          has_windowed_state;

        //! @brief Construct all members with explicit defaults.
        sSurfaceRecord(
              const sSurfaceDescription& description_in = {}
            , const std::vector< sTextureHandle >& textures_in = {}
            , const ComPtr< IDXGISwapChain3 >& swap_chain_in = {}
            , const HWND& window_in = nullptr
            , std::unique_ptr< sWindowEventState > window_events_in = {}
            , eSurfaceWindowMode window_mode_in = eSurfaceWindowMode::NotApplicable
            , const std::string& display_id_in = {}
            , const sDisplayMode& display_mode_in = {}
            , const sDisplayMode& restore_display_mode_in = {}
            , const std::wstring& display_device_name_in = {}
            , const DEVMODEW& restore_native_display_mode_in = {}
            , bool has_native_display_mode_in = false
            , const sWin32WindowedState& windowed_state_in = {}
            , const sRendererExtent2D& windowed_extent_in = {}
            , bool has_windowed_state_in = false
        )
            : description                 ( description_in )
            , textures                    ( textures_in )
            , swap_chain                  ( swap_chain_in )
            , window                      ( window_in )
            , window_events               ( std::move( window_events_in ) )
            , window_mode                 ( window_mode_in )
            , display_id                  ( display_id_in )
            , display_mode                ( display_mode_in )
            , restore_display_mode        ( restore_display_mode_in )
            , display_device_name         ( display_device_name_in )
            , restore_native_display_mode ( restore_native_display_mode_in )
            , has_native_display_mode     ( has_native_display_mode_in )
            , windowed_state              ( windowed_state_in )
            , windowed_extent             ( windowed_extent_in )
            , has_windowed_state          ( has_windowed_state_in )
        {
        }
    };

    struct sPendingSubmission
    {
        std::uint64_t                    fence_value;
        ComPtr< ID3D12CommandAllocator > allocator;
        ComPtr< ID3D12GraphicsCommandList > command_list;
        std::vector< ComPtr< ID3D12Resource > > retained_resources;
        std::vector< ComPtr< ID3D12DescriptorHeap > > retained_descriptor_heaps;

        //! @brief Construct all members with explicit defaults.
        sPendingSubmission(
              std::uint64_t fence_value_in = 0U
            , const ComPtr< ID3D12CommandAllocator >& allocator_in = {}
            , const ComPtr< ID3D12GraphicsCommandList >& command_list_in = {}
            , const std::vector< ComPtr< ID3D12Resource > >& retained_resources_in = {}
            , const std::vector< ComPtr< ID3D12DescriptorHeap > >& retained_descriptor_heaps_in = {}
        )
            : fence_value               ( fence_value_in )
            , allocator                 ( allocator_in )
            , command_list              ( command_list_in )
            , retained_resources        ( retained_resources_in )
            , retained_descriptor_heaps ( retained_descriptor_heaps_in )
        {
        }
    };

    ComPtr< IDXGIFactory6 >              m_factory;
    ComPtr< IDXGIAdapter1 >              m_adapter;
    ComPtr< ID3D12Device >               m_device;
    ComPtr< ID3D12CommandQueue >         m_queue;
    ComPtr< ID3D12Fence >                m_fence;
    ComPtr< ID3D12RootSignature >        m_root_signature;
    ComPtr< ID3D12PipelineState >        m_rgba_pipeline;
    ComPtr< ID3D12PipelineState >        m_bgra_pipeline;
    ComPtr< ID3D12PipelineState >        m_rgba_alpha_pipeline;
    ComPtr< ID3D12PipelineState >        m_bgra_alpha_pipeline;
    HANDLE                               m_fence_event;
    HINSTANCE                            m_module_instance;
    ATOM                                 m_window_class;
    std::wstring                         m_window_class_name;
    std::uint32_t m_handle_generation = 0U;
    std::uint64_t                        m_next_resource_id;
    std::uint64_t                        m_last_fence_value;
    bool                                 m_initialized;
    sRendererCapabilities                m_capabilities;
    std::unordered_map< std::uint64_t, sTextureRecord > m_textures;
    std::unordered_map< std::uint64_t, sMeshRecord >    m_meshes;
    std::unordered_map< std::uint64_t, sSurfaceRecord > m_surfaces;
    std::vector< sPendingSubmission >    m_pending_submissions;

    RendererError selectAdapter(
          bool               use_software_adapter_in
        , bool               prefer_display_adapter_in
        , const std::string& adapter_name_in
    );
    RendererError initializeMeshPipeline();
    RendererError ensureWindowClass();
    RendererResult< sSurfaceHandle > createOffscreenSurface(
        const sSurfaceDescription& description_in );
    RendererResult< sSurfaceHandle > createWindowSurface(
        const sSurfaceDescription& description_in );
    RendererResult< sSurfaceHandle > createDirectDisplaySurface(
        const sSurfaceDescription& description_in );
    RendererError waitForGpuIdle();
    RendererError resizeWindowSurfaceBuffers(
          sSurfaceRecord* const    p_surface_inout
        , const std::uint64_t      surface_id_in
        , const sRendererExtent2D  extent_in
        , bool                     force_rebuild_in = false
    );
    RendererError leaveDisplayModeFullscreen(
          sSurfaceRecord* p_surface_inout
        , std::uint64_t   surface_id_in
    );
    RendererError enterDisplayModeFullscreen(
          sSurfaceRecord*                     p_surface_inout
        , const std::uint64_t                 surface_id_in
        , const sSurfaceWindowModeRequest&    request_in
    );
    ID3D12PipelineState* findPipelineState(
          const eRendererPixelFormat format_in
        , const eColorBlendMode      blend_mode_in
    ) const noexcept;
    RendererError createCommandList(
              ComPtr< ID3D12CommandAllocator >*    p_allocator_out
        ,     ComPtr< ID3D12GraphicsCommandList >* p_command_list_out
    );
    RendererResult< sFenceHandle > submit(
        ComPtr< ID3D12CommandAllocator >    allocator_in
        , ComPtr< ID3D12GraphicsCommandList > command_list_in
        , std::vector< ComPtr< ID3D12Resource > > retained_resources_in = {}
        , std::vector< ComPtr< ID3D12DescriptorHeap > > retained_descriptor_heaps_in = {}
    );
    void collectCompletedSubmissions() noexcept;
    RendererStatus waitFenceValue(
          const std::uint64_t fence_value_in
        , const std::uint32_t timeout_ms_in
    );
    sTextureRecord* findTexture( const sTextureHandle texture_in ) noexcept;
    const sTextureRecord* findTexture( const sTextureHandle texture_in ) const noexcept;
    sMeshRecord* findMesh( const sMeshHandle mesh_in ) noexcept;
    const sMeshRecord* findMesh( const sMeshHandle mesh_in ) const noexcept;
    RendererError createMeshRecord(
          sMeshRecord*             p_record_out
        , const sMeshDescription& description_in
    );

  public:
    D3D12RendererBackend() noexcept;
    ~D3D12RendererBackend() override;

    RendererStatus initialize( const sRendererConfiguration& configuration_in ) override;
    void shutdown() noexcept override;
    bool isInitialized() const noexcept override;
    sRendererCapabilities getCapabilities() const noexcept override;
    RendererResult< std::vector< sDisplayDescription > > enumerateDisplays() const override;

    RendererResult< sTextureHandle > createTexture(
        const sTextureDescription& description_in ) override;
    RendererStatus destroyTexture( const sTextureHandle texture_in ) override;
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle texture_in
        , const sRendererFrame& frame_in
    ) override;

    RendererResult< sMeshHandle > createMesh(
        const sMeshDescription& description_in ) override;
    RendererStatus updateMesh(
          const sMeshHandle        mesh_in
        , const sMeshDescription& description_in
    ) override;
    RendererStatus destroyMesh( const sMeshHandle mesh_in ) override;

    RendererResult< sSurfaceHandle > createSurface(
        const sSurfaceDescription& description_in ) override;
    RendererResult< sTextureHandle > getSurfaceTexture(
        const sSurfaceHandle surface_in ) const override;
    RendererResult< sSurfaceState > getSurfaceState(
        const sSurfaceHandle surface_in ) const override;
    RendererStatus resizeSurface(
          const sSurfaceHandle     surface_in
        , const sRendererExtent2D extent_in
    ) override;
    RendererStatus setSurfaceWindowMode(
          const sSurfaceHandle            surface_in
        , const sSurfaceWindowModeRequest& request_in
    ) override;
    RendererResult< sSurfaceEvents > pollSurfaceEvents(
        const sSurfaceHandle surface_in ) override;
    RendererStatus destroySurface( const sSurfaceHandle surface_in ) override;
    RendererResult< bool > processSurfaceEvents(
        const sSurfaceHandle surface_in ) override;
    RendererResult< sFenceHandle > presentSurface(
        const sSurfaceHandle surface_in ) override;

    RendererResult< sFenceHandle > executeRenderPass(
        const sRenderPassDescription& description_in ) override;
    RendererStatus waitFence(
          const sFenceHandle  fence_in
        , const std::uint32_t timeout_ms_in
    ) override;
    RendererResult< sRendererFrame > readTexture(
          const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    ) override;
};

// Lifecycle and adapter selection.
D3D12RendererBackend::D3D12RendererBackend() noexcept
    : m_factory             ()
    , m_adapter             ()
    , m_device              ()
    , m_queue               ()
    , m_fence               ()
    , m_root_signature      ()
    , m_rgba_pipeline       ()
    , m_bgra_pipeline       ()
    , m_rgba_alpha_pipeline ()
    , m_bgra_alpha_pipeline ()
    , m_fence_event         ( nullptr )
    , m_module_instance     ( GetModuleHandleW( nullptr ) )
    , m_window_class        ( 0U )
    , m_window_class_name   ()
    , m_next_resource_id    ( 1U )
    , m_last_fence_value    ( 0U )
    , m_initialized         ( false )
    , m_capabilities        ()
    , m_textures            ()
    , m_meshes              ()
    , m_surfaces            ()
    , m_pending_submissions ()
{
}

D3D12RendererBackend::~D3D12RendererBackend()
{
    this->shutdown();
}

//! @brief Adapter名がRequestを満たすかを判定する. 大文字小文字を区別しない部分一致である.
namespace
{
std::string toUtf8Lower( const std::wstring& text_in )
{
    std::string result;
    result.reserve( text_in.size() );
    for( const wchar_t character : text_in )
    {
        // Adapter名は実質ASCIIであるため、非ASCIIは照合対象から外す。
        if( character < 128 )
            result.push_back( static_cast< char >( std::tolower( static_cast< int >( character ) ) ) );
    }
    return result;
}

std::string toLower( const std::string& text_in )
{
    std::string result = text_in;
    for( char& character : result )
        character = static_cast< char >( std::tolower( static_cast< unsigned char >( character ) ) );
    return result;
}
} // namespace

RendererError D3D12RendererBackend::selectAdapter(
      const bool         use_software_adapter_in
    , const bool         prefer_display_adapter_in
    , const std::string& adapter_name_in
)
{
    const std::string requested = toLower( adapter_name_in );
    if( use_software_adapter_in )
    {
        ComPtr< IDXGIAdapter > warp_adapter;
        const HRESULT result = this->m_factory->EnumWarpAdapter(
            IID_PPV_ARGS( warp_adapter.ReleaseAndGetAddressOf() ) );
        if( FAILED( result ) )
        {
            return backendError( "Failed to enumerate the software adapter.", result );
        }
        const HRESULT query_result = warp_adapter.As( &this->m_adapter );
        if( FAILED( query_result ) )
        {
            return backendError( "Failed to query the software adapter interface.", query_result );
        }
        return RendererError();
    }

    ComPtr< IDXGIAdapter1 > first_compatible_adapter;
    for( UINT adapter_index = 0U; ; ++adapter_index )
    {
        ComPtr< IDXGIAdapter1 > candidate;
        const HRESULT enumerate_result = this->m_factory->EnumAdapterByGpuPreference(
              adapter_index
            , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
            , IID_PPV_ARGS( candidate.ReleaseAndGetAddressOf() )
        );
        if( enumerate_result == DXGI_ERROR_NOT_FOUND )
        {
            break;
        }
        if( FAILED( enumerate_result ) )
        {
            return backendError( "Failed to enumerate a hardware adapter.", enumerate_result );
        }

        DXGI_ADAPTER_DESC1 description = {};
        candidate->GetDesc1( &description );
        if( ( description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) != 0U )
        {
            continue;
        }
        if( !requested.empty()
            && toUtf8Lower( description.Description ).find( requested ) == std::string::npos )
        {
            continue;
        }
        if( SUCCEEDED( D3D12CreateDevice(
                candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof( ID3D12Device ), nullptr ) ) )
        {
            if( first_compatible_adapter == nullptr )
            {
                first_compatible_adapter = candidate;
            }
            ComPtr< IDXGIOutput > output;
            const bool has_output = SUCCEEDED(
                candidate->EnumOutputs( 0U, output.ReleaseAndGetAddressOf() ) );
            if( !prefer_display_adapter_in || has_output )
            {
                this->m_adapter = std::move( candidate );
                return RendererError();
            }
        }
    }

    if( first_compatible_adapter != nullptr )
    {
        this->m_adapter = std::move( first_compatible_adapter );
        return RendererError();
    }

    if( !requested.empty() )
    {
        // 指名したAdapterが無いまま別のAdapterを使うほうが危険であるため、明示的に失敗させる。
        return RendererError(
              eRendererErrorCategory::Resource
            , eRendererErrorCode::ResourceNotFound
            , "No adapter matches the requested adapter name." );
    }

    return makeError(
          eRendererErrorCategory::Backend
        , eRendererErrorCode::ResourceExhausted
        , "No compatible Direct3D 12 hardware adapter is available."
    );
}

RendererError D3D12RendererBackend::initializeMeshPipeline()
{
    const UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    ComPtr< ID3DBlob > vertex_shader;
    ComPtr< ID3DBlob > pixel_shader;
    ComPtr< ID3DBlob > compiler_error;

    // Portable Vertex layoutを固定ShaderへCompileする.
    HRESULT result = D3DCompile(
          VERTEX_SHADER_SOURCE
        , std::strlen( VERTEX_SHADER_SOURCE )
        , "WSE portable vertex shader"
        , nullptr
        , nullptr
        , "main"
        , "vs_5_0"
        , compile_flags
        , 0U
        , vertex_shader.ReleaseAndGetAddressOf()
        , compiler_error.ReleaseAndGetAddressOf()
    );
    if( FAILED( result ) )
    {
        const std::string detail = compiler_error == nullptr ? std::string() :
            std::string(
                  static_cast< const char* >( compiler_error->GetBufferPointer() )
                , compiler_error->GetBufferSize()
            );
        return backendError( "Failed to compile the portable vertex shader. " + detail, result );
    }
    compiler_error.Reset();
    result = D3DCompile(
          PIXEL_SHADER_SOURCE
        , std::strlen( PIXEL_SHADER_SOURCE )
        , "WSE portable pixel shader"
        , nullptr
        , nullptr
        , "main"
        , "ps_5_0"
        , compile_flags
        , 0U
        , pixel_shader.ReleaseAndGetAddressOf()
        , compiler_error.ReleaseAndGetAddressOf()
    );
    if( FAILED( result ) )
    {
        const std::string detail = compiler_error == nullptr ? std::string() :
            std::string(
                  static_cast< const char* >( compiler_error->GetBufferPointer() )
                , compiler_error->GetBufferSize()
            );
        return backendError( "Failed to compile the portable pixel shader. " + detail, result );
    }

    // Source／Alpha Texture、Draw parameter、Nearest／Linear Samplerを定義する.
    D3D12_DESCRIPTOR_RANGE descriptor_range = {};
    descriptor_range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptor_range.NumDescriptors                    = DRAW_DESCRIPTORS_PER_COMMAND;
    descriptor_range.BaseShaderRegister                = 0U;
    descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER root_parameters[ 2U ] = {};
    root_parameters[ 0U ].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[ 0U ].DescriptorTable.NumDescriptorRanges = 1U;
    root_parameters[ 0U ].DescriptorTable.pDescriptorRanges   = &descriptor_range;
    root_parameters[ 0U ].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root_parameters[ 1U ].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_parameters[ 1U ].Constants.ShaderRegister = 0U;
    root_parameters[ 1U ].Constants.Num32BitValues = DRAW_PARAMETER_COUNT;
    root_parameters[ 1U ].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC samplers[ 2U ] = {};
    samplers[ 0U ].Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[ 0U ].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[ 0U ].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[ 0U ].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[ 0U ].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    samplers[ 0U ].MaxLOD           = D3D12_FLOAT32_MAX;
    samplers[ 0U ].ShaderRegister   = 0U;
    samplers[ 0U ].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[ 1U ]                   = samplers[ 0U ];
    samplers[ 1U ].Filter            = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[ 1U ].ShaderRegister    = 1U;
    D3D12_ROOT_SIGNATURE_DESC root_description = {};
    root_description.NumParameters     = static_cast< UINT >( std::size( root_parameters ) );
    root_description.pParameters       = root_parameters;
    root_description.NumStaticSamplers = static_cast< UINT >( std::size( samplers ) );
    root_description.pStaticSamplers   = samplers;
    root_description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr< ID3DBlob > serialized_root;
    ComPtr< ID3DBlob > serialization_error;
    result = D3D12SerializeRootSignature(
          &root_description
        , D3D_ROOT_SIGNATURE_VERSION_1
        , serialized_root.ReleaseAndGetAddressOf()
        , serialization_error.ReleaseAndGetAddressOf()
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to serialize the portable root signature.", result );
    }
    result = this->m_device->CreateRootSignature(
          0U
        , serialized_root->GetBufferPointer()
        , serialized_root->GetBufferSize()
        , IID_PPV_ARGS( this->m_root_signature.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the portable root signature.", result );
    }

    // RGBA8／BGRA8用Pipeline stateを同一Shaderから生成する.
    const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
          { "POSITION", 0U, DXGI_FORMAT_R32G32_FLOAT, 0U, 0U,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U }
        , { "TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT, 0U, 8U,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U }
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_description = {};
    pipeline_description.pRootSignature        = this->m_root_signature.Get();
    pipeline_description.VS                    = {
        vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    pipeline_description.PS                    = {
        pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
    pipeline_description.BlendState.RenderTarget[ 0U ].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline_description.SampleMask            = D3D12_DEFAULT_SAMPLE_MASK;
    pipeline_description.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    pipeline_description.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    pipeline_description.RasterizerState.DepthClipEnable       = TRUE;
    pipeline_description.RasterizerState.ConservativeRaster    =
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    pipeline_description.DepthStencilState.DepthEnable         = FALSE;
    pipeline_description.DepthStencilState.StencilEnable       = FALSE;
    pipeline_description.InputLayout = {
        input_layout, static_cast< UINT >( std::size( input_layout ) ) };
    pipeline_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_description.NumRenderTargets      = 1U;
    pipeline_description.SampleDesc.Count      = 1U;
    pipeline_description.RTVFormats[ 0U ]      = DXGI_FORMAT_R8G8B8A8_UNORM;
    result = this->m_device->CreateGraphicsPipelineState(
          &pipeline_description
        , IID_PPV_ARGS( this->m_rgba_pipeline.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the RGBA8 mesh pipeline.", result );
    }
    pipeline_description.RTVFormats[ 0U ] = DXGI_FORMAT_B8G8R8A8_UNORM;
    result = this->m_device->CreateGraphicsPipelineState(
          &pipeline_description
        , IID_PPV_ARGS( this->m_bgra_pipeline.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the BGRA8 mesh pipeline.", result );
    }

    // Projection Layer用Straight-alpha Blend pipelineを生成する.
    D3D12_RENDER_TARGET_BLEND_DESC& blend_description =
        pipeline_description.BlendState.RenderTarget[ 0U ];
    blend_description.BlendEnable           = TRUE;
    blend_description.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend_description.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend_description.BlendOp                = D3D12_BLEND_OP_ADD;
    blend_description.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend_description.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend_description.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    pipeline_description.RTVFormats[ 0U ]    = DXGI_FORMAT_R8G8B8A8_UNORM;
    result = this->m_device->CreateGraphicsPipelineState(
          &pipeline_description
        , IID_PPV_ARGS( this->m_rgba_alpha_pipeline.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the RGBA8 alpha mesh pipeline.", result );
    }
    pipeline_description.RTVFormats[ 0U ] = DXGI_FORMAT_B8G8R8A8_UNORM;
    result = this->m_device->CreateGraphicsPipelineState(
          &pipeline_description
        , IID_PPV_ARGS( this->m_bgra_alpha_pipeline.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the BGRA8 alpha mesh pipeline.", result );
    }
    return RendererError();
}

RendererError D3D12RendererBackend::ensureWindowClass()
{
    if( this->m_window_class != 0U )
    {
        return RendererError();
    }
    this->m_window_class_name = L"WseRendererWindow_" +
        std::to_wstring( reinterpret_cast< std::uintptr_t >( this ) );
    WNDCLASSEXW window_class = {};
    window_class.cbSize        = sizeof( window_class );
    window_class.lpfnWndProc    = &rendererWindowProcedure;
    window_class.hInstance      = this->m_module_instance;
    window_class.hCursor        = LoadCursorW( nullptr, IDC_ARROW );
    window_class.lpszClassName  = this->m_window_class_name.c_str();
    this->m_window_class = RegisterClassExW( &window_class );
    if( this->m_window_class == 0U )
    {
        return backendError(
              "Failed to register the portable renderer window class."
            , HRESULT_FROM_WIN32( GetLastError() )
        );
    }
    return RendererError();
}

ID3D12PipelineState* D3D12RendererBackend::findPipelineState(
      const eRendererPixelFormat format_in
    , const eColorBlendMode      blend_mode_in
) const noexcept
{
    if( format_in == eRendererPixelFormat::Rgba8Unorm )
    {
        return blend_mode_in == eColorBlendMode::SourceAlpha
            ? this->m_rgba_alpha_pipeline.Get() : this->m_rgba_pipeline.Get();
    }
    if( format_in == eRendererPixelFormat::Bgra8Unorm )
    {
        return blend_mode_in == eColorBlendMode::SourceAlpha
            ? this->m_bgra_alpha_pipeline.Get() : this->m_bgra_pipeline.Get();
    }
    return nullptr;
}

RendererStatus D3D12RendererBackend::initialize( const sRendererConfiguration& configuration_in )
{
    if( this->m_initialized )
    {
        return RendererStatus::success();
    }
    this->m_handle_generation = reserveRendererGeneration();
    if( this->m_handle_generation == 0U )
    {
        return RendererStatus::failure( makeError(
              eRendererErrorCategory::Resource, eRendererErrorCode::ResourceExhausted,
              "Renderer lifetime identities are exhausted." ) );
    }


    if( configuration_in.enable_validation )
    {
        ComPtr< ID3D12Debug > debug_layer;
        const HRESULT debug_result = D3D12GetDebugInterface(
            IID_PPV_ARGS( debug_layer.ReleaseAndGetAddressOf() ) );
        if( FAILED( debug_result ) )
        {
            return RendererStatus::failure( unsupportedError(
                      eRendererErrorCode::UnsupportedOperation
                    , "Direct3D 12 validation was requested but is unavailable."
                )
            );
        }
        debug_layer->EnableDebugLayer();
    }

    HRESULT result = CreateDXGIFactory1( IID_PPV_ARGS( this->m_factory.ReleaseAndGetAddressOf() ) );
    if( FAILED( result ) )
    {
        return RendererStatus::failure( backendError( "Failed to create the DXGI factory.", result ) );
    }

    const RendererError adapter_error = this->selectAdapter(
          configuration_in.use_software_adapter
        , configuration_in.prefer_display_adapter
        , configuration_in.adapter_name
    );
    if( !adapter_error.ok() )
    {
        this->shutdown();
        return RendererStatus::failure( adapter_error );
    }

    result = D3D12CreateDevice(
          this->m_adapter.Get()
        , D3D_FEATURE_LEVEL_11_0
        , IID_PPV_ARGS( this->m_device.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the Direct3D 12 device.", result ) );
    }

    D3D12_COMMAND_QUEUE_DESC queue_description = {};
    queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = this->m_device->CreateCommandQueue(
          &queue_description
        , IID_PPV_ARGS( this->m_queue.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the command queue.", result ) );
    }

    result = this->m_device->CreateFence(
          0U
        , D3D12_FENCE_FLAG_NONE
        , IID_PPV_ARGS( this->m_fence.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the renderer fence.", result ) );
    }

    this->m_fence_event = CreateEvent( nullptr, FALSE, FALSE, nullptr );
    if( this->m_fence_event == nullptr )
    {
        const HRESULT event_error = HRESULT_FROM_WIN32( GetLastError() );
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the fence event.", event_error ) );
    }

    const RendererError pipeline_error = this->initializeMeshPipeline();
    if( !pipeline_error.ok() )
    {
        this->shutdown();
        return RendererStatus::failure( pipeline_error );
    }

    this->m_capabilities.backend                 = eRendererBackend::Direct3D12;
    if( this->m_adapter != nullptr )
    {
        DXGI_ADAPTER_DESC1 selected_description = {};
        this->m_adapter->GetDesc1( &selected_description );
        const std::wstring selected_name( selected_description.Description );
        std::string ascii_name;
        ascii_name.reserve( selected_name.size() );
        for( const wchar_t character : selected_name )
        {
            if( character < 128 )
                ascii_name.push_back( static_cast< char >( character ) );
        }
        this->m_capabilities.adapter_name = ascii_name;
    }
    this->m_capabilities.maximum_texture_extent  = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    this->m_capabilities.supports_offscreen      = true;
    this->m_capabilities.supports_window         = true;
    this->m_capabilities.supports_direct_display = true;
    this->m_capabilities.supports_mesh_rendering = true;
    this->m_capabilities.supports_display_enumeration = true;
    this->m_capabilities.supports_surface_resize = true;
    this->m_capabilities.supports_borderless_fullscreen = true;
    this->m_capabilities.supports_display_mode_fullscreen = true;
    this->m_capabilities.supports_interactive_resize = true;
    this->m_capabilities.supports_display_hotplug = true;
    this->m_initialized = true;
    return RendererStatus::success();
}

void D3D12RendererBackend::shutdown() noexcept
{
    if( this->m_queue != nullptr && this->m_fence != nullptr && this->m_fence_event != nullptr )
    {
        const std::uint64_t shutdown_fence = ++this->m_last_fence_value;
        if( SUCCEEDED( this->m_queue->Signal( this->m_fence.Get(), shutdown_fence ) ) )
        {
            this->waitFenceValue( shutdown_fence, SHUTDOWN_TIMEOUT_MS );
        }
    }

    this->m_pending_submissions.clear();
    for( auto& surface_pair : this->m_surfaces )
    {
        if( surface_pair.second.window_mode == eSurfaceWindowMode::DisplayModeFullscreen )
        {
            this->leaveDisplayModeFullscreen( &surface_pair.second, surface_pair.first );
        }
        if( surface_pair.second.window != nullptr && IsWindow( surface_pair.second.window ) )
        {
            DestroyWindow( surface_pair.second.window );
        }
    }
    this->m_surfaces.clear();
    this->m_meshes.clear();
    this->m_textures.clear();
    if( this->m_window_class != 0U )
    {
        UnregisterClassW( this->m_window_class_name.c_str(), this->m_module_instance );
        this->m_window_class = 0U;
        this->m_window_class_name.clear();
    }
    if( this->m_fence_event != nullptr )
    {
        CloseHandle( this->m_fence_event );
        this->m_fence_event = nullptr;
    }
    this->m_fence.Reset();
    this->m_queue.Reset();
    this->m_bgra_alpha_pipeline.Reset();
    this->m_rgba_alpha_pipeline.Reset();
    this->m_bgra_pipeline.Reset();
    this->m_rgba_pipeline.Reset();
    this->m_root_signature.Reset();
    this->m_device.Reset();
    this->m_adapter.Reset();
    this->m_factory.Reset();
    this->m_capabilities = {};
    this->m_handle_generation = 0U;
    this->m_next_resource_id = 1U;
    this->m_last_fence_value = 0U;
    this->m_initialized = false;
}

bool D3D12RendererBackend::isInitialized() const noexcept
{
    return this->m_initialized;
}

sRendererCapabilities D3D12RendererBackend::getCapabilities() const noexcept
{
    return this->m_capabilities;
}

RendererResult< std::vector< sDisplayDescription > >
D3D12RendererBackend::enumerateDisplays() const
{
    return enumerateD3D12Displays( this->m_factory.Get(), this->m_adapter.Get() );
}

// Command submission and fence completion.
RendererError D3D12RendererBackend::createCommandList(
          ComPtr< ID3D12CommandAllocator >*    p_allocator_out
    ,     ComPtr< ID3D12GraphicsCommandList >* p_command_list_out
)
{
    if( p_allocator_out == nullptr || p_command_list_out == nullptr )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidArgument
            , "Command-list output pointers must be non-null."
        );
    }
    HRESULT result = this->m_device->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT
        , IID_PPV_ARGS( p_allocator_out->ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create a command allocator.", result );
    }
    result = this->m_device->CreateCommandList(
          0U
        , D3D12_COMMAND_LIST_TYPE_DIRECT
        , p_allocator_out->Get()
        , nullptr
        , IID_PPV_ARGS( p_command_list_out->ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create a command list.", result );
    }
    return RendererError();
}

RendererResult< sFenceHandle > D3D12RendererBackend::submit(
    ComPtr< ID3D12CommandAllocator >    allocator_in
    , ComPtr< ID3D12GraphicsCommandList > command_list_in
    , std::vector< ComPtr< ID3D12Resource > > retained_resources_in
    , std::vector< ComPtr< ID3D12DescriptorHeap > > retained_descriptor_heaps_in
)
{
    const HRESULT close_result = command_list_in->Close();
    if( FAILED( close_result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to close the command list.", close_result )
        );
    }

    ID3D12CommandList* command_lists[] = { command_list_in.Get() };
    this->m_queue->ExecuteCommandLists( 1U, command_lists );
    const std::uint64_t fence_value = ++this->m_last_fence_value;
    const HRESULT signal_result = this->m_queue->Signal( this->m_fence.Get(), fence_value );
    if( FAILED( signal_result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to signal the renderer fence.", signal_result )
        );
    }

    sPendingSubmission pending;
    pending.fence_value       = fence_value;
    pending.allocator         = std::move( allocator_in );
    pending.command_list      = std::move( command_list_in );
    pending.retained_resources = std::move( retained_resources_in );
    pending.retained_descriptor_heaps = std::move( retained_descriptor_heaps_in );
    this->m_pending_submissions.emplace_back( std::move( pending ) );
    this->collectCompletedSubmissions();

    return RendererResult< sFenceHandle >::success(
        sFenceHandle{ fence_value, this->m_handle_generation } );
}

void D3D12RendererBackend::collectCompletedSubmissions() noexcept
{
    if( this->m_fence == nullptr )
    {
        return;
    }
    const std::uint64_t completed_value = this->m_fence->GetCompletedValue();
    this->m_pending_submissions.erase(
          std::remove_if(
                this->m_pending_submissions.begin()
              , this->m_pending_submissions.end()
              , [completed_value]( const sPendingSubmission& pending_in )
                {
                    return pending_in.fence_value <= completed_value;
                }
            )
        , this->m_pending_submissions.end()
    );
}

RendererStatus D3D12RendererBackend::waitFenceValue(
      const std::uint64_t fence_value_in
    , const std::uint32_t timeout_ms_in
)
{
    if( this->m_fence->GetCompletedValue() >= fence_value_in )
    {
        this->collectCompletedSubmissions();
        return RendererStatus::success();
    }

    const HRESULT event_result = this->m_fence->SetEventOnCompletion(
        fence_value_in, this->m_fence_event );
    if( FAILED( event_result ) )
    {
        return RendererStatus::failure( backendError( "Failed to register the fence event.", event_result ) );
    }
    const DWORD wait_result = WaitForSingleObject( this->m_fence_event, timeout_ms_in );
    if( wait_result == WAIT_TIMEOUT )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Timeout
                , eRendererErrorCode::TimedOut
                , "Renderer fence wait timed out."
            )
        );
    }
    if( wait_result != WAIT_OBJECT_0 )
    {
        return RendererStatus::failure( backendError(
                  "Renderer fence wait failed."
                , HRESULT_FROM_WIN32( GetLastError() )
            )
        );
    }
    this->collectCompletedSubmissions();
    return RendererStatus::success();
}

D3D12RendererBackend::sTextureRecord* D3D12RendererBackend::findTexture(
    const sTextureHandle texture_in ) noexcept
{
    if( texture_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto iterator = this->m_textures.find( texture_in.value );
    return iterator == this->m_textures.end() ? nullptr : &iterator->second;
}

const D3D12RendererBackend::sTextureRecord* D3D12RendererBackend::findTexture(
    const sTextureHandle texture_in ) const noexcept
{
    if( texture_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto iterator = this->m_textures.find( texture_in.value );
    return iterator == this->m_textures.end() ? nullptr : &iterator->second;
}

D3D12RendererBackend::sMeshRecord* D3D12RendererBackend::findMesh(
    const sMeshHandle mesh_in ) noexcept
{
    if( mesh_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto iterator = this->m_meshes.find( mesh_in.value );
    return iterator == this->m_meshes.end() ? nullptr : &iterator->second;
}

const D3D12RendererBackend::sMeshRecord* D3D12RendererBackend::findMesh(
    const sMeshHandle mesh_in ) const noexcept
{
    if( mesh_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto iterator = this->m_meshes.find( mesh_in.value );
    return iterator == this->m_meshes.end() ? nullptr : &iterator->second;
}

// Texture allocation and ownership.
RendererResult< sTextureHandle > D3D12RendererBackend::createTexture(
    const sTextureDescription& description_in )
{
    if( description_in.mip_levels != 1U )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "The current Direct3D 12 adapter supports one mip level."
            )
        );
    }
    if( description_in.extent.width > this->m_capabilities.maximum_texture_extent ||
        description_in.extent.height > this->m_capabilities.maximum_texture_extent )
    {
        return RendererResult< sTextureHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceExhausted
                , "Texture extent exceeds the backend limit."
            )
        );
    }
    if( description_in.initial_state == eTextureState::Present )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Present state is reserved for textures owned by a window surface."
            )
        );
    }

    const DXGI_FORMAT native_format = toNativeFormat( description_in.format );
    if( native_format != DXGI_FORMAT_R8_UNORM &&
        native_format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        native_format != DXGI_FORMAT_B8G8R8A8_UNORM &&
        native_format != DXGI_FORMAT_R16G16B16A16_UNORM )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "The current Direct3D 12 adapter supports R8, RGBA8, BGRA8 and RGBA16 UNORM textures."
            )
        );
    }
    // R8とRGBA16 UNORMはSampled Sourceとして扱う。Render TargetのPipelineは8bit RGBA／BGRAのみである。
    if( ( native_format == DXGI_FORMAT_R8_UNORM ||
          native_format == DXGI_FORMAT_R16G16B16A16_UNORM ) &&
        hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget ) )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "R8 and RGBA16 UNORM textures are supported as non-render-target resources."
            )
        );
    }

    D3D12_RESOURCE_DESC resource_description = {};
    resource_description.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_description.Width              = description_in.extent.width;
    resource_description.Height             = description_in.extent.height;
    resource_description.DepthOrArraySize   = 1U;
    resource_description.MipLevels          = 1U;
    resource_description.Format             = native_format;
    resource_description.SampleDesc.Count   = 1U;
    resource_description.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_description.Flags = hasTextureUsage(
        description_in.usage, eTextureUsage::RenderTarget )
            ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            : D3D12_RESOURCE_FLAG_NONE;

    D3D12_CLEAR_VALUE clear_value = {};
    clear_value.Format   = native_format;
    clear_value.Color[3] = 1.0F;
    const D3D12_HEAP_PROPERTIES heap_properties = makeHeapProperties( D3D12_HEAP_TYPE_DEFAULT );

    sTextureRecord record;
    const eTextureState portable_initial_state =
        description_in.initial_state == eTextureState::Undefined
            ? eTextureState::Common
            : description_in.initial_state;
    const D3D12_CLEAR_VALUE* const p_clear_value =
        hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget )
            ? &clear_value
            : nullptr;
    HRESULT result = this->m_device->CreateCommittedResource(
          &heap_properties
        , D3D12_HEAP_FLAG_NONE
        , &resource_description
        , toNativeState( portable_initial_state )
        , p_clear_value
        , IID_PPV_ARGS( record.resource.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return RendererResult< sTextureHandle >::failure( backendError( "Failed to create a portable texture.", result ) );
    }

    // Usageに対応するViewをTexture recordへ保持する.
    if( hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget ) )
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
        heap_description.NumDescriptors = 1U;
        heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        result = this->m_device->CreateDescriptorHeap(
              &heap_description
            , IID_PPV_ARGS( record.render_target_heap.ReleaseAndGetAddressOf() )
        );
        if( FAILED( result ) )
        {
            return RendererResult< sTextureHandle >::failure( backendError( "Failed to create a render-target descriptor heap.", result ) );
        }
        record.render_target_handle =
            record.render_target_heap->GetCPUDescriptorHandleForHeapStart();
        this->m_device->CreateRenderTargetView(
              record.resource.Get()
            , nullptr
            , record.render_target_handle
        );
    }
    if( hasTextureUsage( description_in.usage, eTextureUsage::Sampled ) )
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
        heap_description.NumDescriptors = 1U;
        heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_description.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        result = this->m_device->CreateDescriptorHeap(
              &heap_description
            , IID_PPV_ARGS( record.shader_resource_heap.ReleaseAndGetAddressOf() )
        );
        if( FAILED( result ) )
        {
            return RendererResult< sTextureHandle >::failure( backendError( "Failed to create a shader-resource descriptor heap.", result ) );
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC view_description = {};
        view_description.Format                    = native_format;
        view_description.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        view_description.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view_description.Texture2D.MipLevels       = 1U;
        this->m_device->CreateShaderResourceView(
              record.resource.Get()
            , &view_description
            , record.shader_resource_heap->GetCPUDescriptorHandleForHeapStart()
        );
    }

    const std::uint64_t resource_id = this->m_next_resource_id++;
    record.description = description_in;
    record.state       = portable_initial_state;
    this->m_textures.emplace( resource_id, std::move( record ) );
    return RendererResult< sTextureHandle >::success(
        sTextureHandle{ resource_id, this->m_handle_generation } );
}

RendererStatus D3D12RendererBackend::destroyTexture( const sTextureHandle texture_in )
{
    sTextureRecord* const p_record = this->findTexture( texture_in );
    if( p_record == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Texture" ) );
    }
    if( p_record->surface_owner != 0U )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::ResourceInUse
                , "Texture is owned by a surface and must be released through that surface."
            )
        );
    }
    this->m_textures.erase( texture_in.value );
    return RendererStatus::success();
}

RendererResult< sFenceHandle > D3D12RendererBackend::uploadTexture(
      const sTextureHandle texture_in
    , const sRendererFrame& frame_in
)
{
    sTextureRecord* const p_texture = this->findTexture( texture_in );
    if( p_texture == nullptr )
    {
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Texture" ) );
    }
    if( !hasTextureUsage( p_texture->description.usage, eTextureUsage::TransferDestination ) ||
        !hasTextureUsage( p_texture->description.usage, eTextureUsage::Sampled ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Texture upload requires TransferDestination and Sampled usage."
            )
        );
    }
    if( frame_in.description.extent.width != p_texture->description.extent.width ||
        frame_in.description.extent.height != p_texture->description.extent.height ||
        frame_in.description.format != p_texture->description.format )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Upload frame extent and format must match the destination texture."
            )
        );
    }

    // Native copy footprintに合わせたUpload bufferを生成する.
    const D3D12_RESOURCE_DESC texture_description = p_texture->resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 upload_size = 0U;
    this->m_device->GetCopyableFootprints(
          &texture_description
        , 0U
        , 1U
        , 0U
        , &footprint
        , nullptr
        , nullptr
        , &upload_size
    );
    D3D12_RESOURCE_DESC upload_description = {};
    upload_description.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_description.Width              = upload_size;
    upload_description.Height             = 1U;
    upload_description.DepthOrArraySize   = 1U;
    upload_description.MipLevels          = 1U;
    upload_description.SampleDesc.Count   = 1U;
    upload_description.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const D3D12_HEAP_PROPERTIES upload_heap = makeHeapProperties( D3D12_HEAP_TYPE_UPLOAD );
    ComPtr< ID3D12Resource > upload_buffer;
    HRESULT result = this->m_device->CreateCommittedResource(
          &upload_heap
        , D3D12_HEAP_FLAG_NONE
        , &upload_description
        , D3D12_RESOURCE_STATE_GENERIC_READ
        , nullptr
        , IID_PPV_ARGS( upload_buffer.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to create the texture upload buffer.", result ) );
    }

    void* p_mapped_data = nullptr;
    const D3D12_RANGE read_range = { 0U, 0U };
    result = upload_buffer->Map( 0U, &read_range, &p_mapped_data );
    if( FAILED( result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to map the texture upload buffer.", result ) );
    }
    const std::size_t source_pitch = frame_in.description.effectiveRowPitch();
    const std::size_t packed_pitch = frame_in.description.minimumRowPitch();
    std::uint8_t* const p_destination = static_cast< std::uint8_t* >( p_mapped_data );
    for( std::uint32_t row = 0U; row < frame_in.description.extent.height; ++row )
    {
        std::copy_n(
              frame_in.data.data() + static_cast< std::size_t >( row ) * source_pitch
            , packed_pitch
            , p_destination + static_cast< std::size_t >( row ) * footprint.Footprint.RowPitch
        );
    }
    const D3D12_RANGE written_range = { 0U, static_cast< SIZE_T >( upload_size ) };
    upload_buffer->Unmap( 0U, &written_range );

    // Copy後はProjectionからSample可能なStateへ遷移する.
    ComPtr< ID3D12CommandAllocator > allocator;
    ComPtr< ID3D12GraphicsCommandList > command_list;
    const RendererError command_error = this->createCommandList( &allocator, &command_list );
    if( !command_error.ok() )
    {
        return RendererResult< sFenceHandle >::failure( command_error );
    }
    if( p_texture->state != eTextureState::CopyDestination )
    {
        const D3D12_RESOURCE_BARRIER barrier = makeTransition(
              p_texture->resource.Get()
            , toNativeState( p_texture->state )
            , D3D12_RESOURCE_STATE_COPY_DEST
        );
        command_list->ResourceBarrier( 1U, &barrier );
    }
    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource       = upload_buffer.Get();
    source.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource        = p_texture->resource.Get();
    destination.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0U;
    command_list->CopyTextureRegion( &destination, 0U, 0U, 0U, &source, nullptr );
    const D3D12_RESOURCE_BARRIER sample_barrier = makeTransition(
          p_texture->resource.Get()
        , D3D12_RESOURCE_STATE_COPY_DEST
        , D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    command_list->ResourceBarrier( 1U, &sample_barrier );

    std::vector< ComPtr< ID3D12Resource > > retained_resources = {
          p_texture->resource
        , upload_buffer
    };
    RendererResult< sFenceHandle > submit_result = this->submit(
          std::move( allocator )
        , std::move( command_list )
        , std::move( retained_resources )
    );
    if( submit_result.succeeded() )
    {
        p_texture->state = eTextureState::ShaderResource;
    }
    return submit_result;
}

RendererError D3D12RendererBackend::createMeshRecord(
      sMeshRecord* const          p_record_out
    , const sMeshDescription& description_in
)
{
    if( p_record_out == nullptr )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidArgument
            , "Mesh record output pointer must be non-null."
        );
    }
    const std::size_t max_view_size = ( std::numeric_limits< UINT >::max )();
    if( description_in.vertices.size() > max_view_size / sizeof( sRendererVertex2D ) ||
        description_in.indices.size() > max_view_size / sizeof( std::uint32_t ) ||
        description_in.indices.size() > ( std::numeric_limits< UINT >::max )() )
    {
        return makeError(
              eRendererErrorCategory::Resource
            , eRendererErrorCode::ResourceExhausted
            , "Mesh data exceeds Direct3D 12 view limits."
        );
    }

    const std::size_t vertex_size = description_in.vertices.size() * sizeof( sRendererVertex2D );
    const std::size_t index_size  = description_in.indices.size() * sizeof( std::uint32_t );

    // 更新単位となるVertex／Index dataをUpload heapへ配置する.
    const D3D12_HEAP_PROPERTIES upload_heap = makeHeapProperties( D3D12_HEAP_TYPE_UPLOAD );
    D3D12_RESOURCE_DESC buffer_description = {};
    buffer_description.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer_description.Height             = 1U;
    buffer_description.DepthOrArraySize   = 1U;
    buffer_description.MipLevels          = 1U;
    buffer_description.SampleDesc.Count   = 1U;
    buffer_description.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    sMeshRecord record;
    buffer_description.Width = vertex_size;
    HRESULT result = this->m_device->CreateCommittedResource(
          &upload_heap
        , D3D12_HEAP_FLAG_NONE
        , &buffer_description
        , D3D12_RESOURCE_STATE_GENERIC_READ
        , nullptr
        , IID_PPV_ARGS( record.vertex_buffer.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the mesh vertex buffer.", result );
    }
    buffer_description.Width = index_size;
    result = this->m_device->CreateCommittedResource(
          &upload_heap
        , D3D12_HEAP_FLAG_NONE
        , &buffer_description
        , D3D12_RESOURCE_STATE_GENERIC_READ
        , nullptr
        , IID_PPV_ARGS( record.index_buffer.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the mesh index buffer.", result );
    }

    void* p_vertex_data = nullptr;
    void* p_index_data  = nullptr;
    const D3D12_RANGE empty_read_range = { 0U, 0U };
    result = record.vertex_buffer->Map( 0U, &empty_read_range, &p_vertex_data );
    if( FAILED( result ) )
    {
        return backendError( "Failed to map the mesh vertex buffer.", result );
    }
    std::memcpy( p_vertex_data, description_in.vertices.data(), vertex_size );
    const D3D12_RANGE vertex_write_range = { 0U, vertex_size };
    record.vertex_buffer->Unmap( 0U, &vertex_write_range );
    result = record.index_buffer->Map( 0U, &empty_read_range, &p_index_data );
    if( FAILED( result ) )
    {
        return backendError( "Failed to map the mesh index buffer.", result );
    }
    std::memcpy( p_index_data, description_in.indices.data(), index_size );
    const D3D12_RANGE index_write_range = { 0U, index_size };
    record.index_buffer->Unmap( 0U, &index_write_range );

    record.vertex_view.BufferLocation = record.vertex_buffer->GetGPUVirtualAddress();
    record.vertex_view.SizeInBytes    = static_cast< UINT >( vertex_size );
    record.vertex_view.StrideInBytes  = sizeof( sRendererVertex2D );
    record.index_view.BufferLocation  = record.index_buffer->GetGPUVirtualAddress();
    record.index_view.SizeInBytes     = static_cast< UINT >( index_size );
    record.index_view.Format          = DXGI_FORMAT_R32_UINT;
    record.topology                   = description_in.topology;
    record.index_count                = static_cast< std::uint32_t >( description_in.indices.size() );

    *p_record_out = std::move( record );
    return RendererError();
}

RendererResult< sMeshHandle > D3D12RendererBackend::createMesh(
    const sMeshDescription& description_in )
{
    sMeshRecord record;
    const RendererError record_error = this->createMeshRecord( &record, description_in );
    if( !record_error.ok() )
    {
        return RendererResult< sMeshHandle >::failure( record_error );
    }

    const std::uint64_t resource_id = this->m_next_resource_id++;
    this->m_meshes.emplace( resource_id, std::move( record ) );
    return RendererResult< sMeshHandle >::success( sMeshHandle{ resource_id, this->m_handle_generation } );
}

RendererStatus D3D12RendererBackend::updateMesh(
      const sMeshHandle        mesh_in
    , const sMeshDescription& description_in
)
{
    sMeshRecord* const p_record = this->findMesh( mesh_in );
    if( p_record == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Mesh" ) );
    }

    // Replacementを完全に構築してから交換し、失敗時は既存Meshを変更しない.
    sMeshRecord replacement;
    const RendererError record_error =
        this->createMeshRecord( &replacement, description_in );
    if( !record_error.ok() )
    {
        return RendererStatus::failure( record_error );
    }
    *p_record = std::move( replacement );
    return RendererStatus::success();
}

RendererStatus D3D12RendererBackend::destroyMesh( const sMeshHandle mesh_in )
{
    if( this->findMesh( mesh_in ) == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Mesh" ) );
    }
    this->m_meshes.erase( mesh_in.value );
    return RendererStatus::success();
}

// Portable surface resources.
RendererResult< sSurfaceHandle > D3D12RendererBackend::createSurface(
    const sSurfaceDescription& description_in )
{
    if( description_in.type == eSurfaceType::Offscreen )
    {
        return this->createOffscreenSurface( description_in );
    }
    if( description_in.type == eSurfaceType::Window )
    {
        return this->createWindowSurface( description_in );
    }
    return this->createDirectDisplaySurface( description_in );
}

RendererResult< sSurfaceHandle > D3D12RendererBackend::createOffscreenSurface(
    const sSurfaceDescription& description_in )
{
    sTextureDescription texture_description;
    texture_description.extent        = description_in.extent;
    texture_description.format        = description_in.format;
    texture_description.usage         = eTextureUsage::RenderTarget |
        eTextureUsage::TransferSource;
    texture_description.initial_state = eTextureState::RenderTarget;
    const RendererResult< sTextureHandle > texture_result =
        this->createTexture( texture_description );
    if( !texture_result.succeeded() )
    {
        return RendererResult< sSurfaceHandle >::failure( texture_result.error() );
    }

    const std::uint64_t surface_id = this->m_next_resource_id++;
    sSurfaceRecord surface;
    surface.description = description_in;
    surface.textures.emplace_back( texture_result.value() );
    this->m_surfaces.emplace( surface_id, std::move( surface ) );
    this->m_textures.at( texture_result.value().value ).surface_owner = surface_id;
    return RendererResult< sSurfaceHandle >::success(
        sSurfaceHandle{ surface_id, this->m_handle_generation } );
}

RendererResult< sSurfaceHandle > D3D12RendererBackend::createWindowSurface(
    const sSurfaceDescription& description_in )
{
    const DXGI_FORMAT native_format = toNativeFormat( description_in.format );
    if( native_format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        native_format != DXGI_FORMAT_B8G8R8A8_UNORM )
    {
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "Window surfaces support RGBA8 and BGRA8."
            )
        );
    }
    const RendererError class_error = this->ensureWindowClass();
    if( !class_error.ok() )
    {
        return RendererResult< sSurfaceHandle >::failure( class_error );
    }

    // UTF-8 titleとClient extentから内部所有Windowを生成する.
    if( description_in.title.size() > static_cast< std::size_t >(
            ( std::numeric_limits< int >::max )() ) )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError( eRendererErrorCategory::Validation,
                eRendererErrorCode::InvalidDescription, "Window title is too long." ) );
    }
    const int title_length = MultiByteToWideChar(
          CP_UTF8
        , MB_ERR_INVALID_CHARS
        , description_in.title.data()
        , static_cast< int >( description_in.title.size() )
        , nullptr
        , 0
    );
    if( title_length <= 0 )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError( eRendererErrorCategory::Validation,
                eRendererErrorCode::InvalidDescription, "Window title must be valid UTF-8." ) );
    }
    std::wstring window_title( static_cast< std::size_t >( title_length ), L'\0' );
    MultiByteToWideChar(
          CP_UTF8
        , MB_ERR_INVALID_CHARS
        , description_in.title.data()
        , static_cast< int >( description_in.title.size() )
        , window_title.data()
        , title_length
    );
    const DWORD window_style = WS_OVERLAPPEDWINDOW;
    RECT window_rectangle = {
          0L
        , 0L
        , static_cast< LONG >( description_in.extent.width )
        , static_cast< LONG >( description_in.extent.height )
    };
    if( !AdjustWindowRect( &window_rectangle, window_style, FALSE ) )
    {
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to calculate the renderer window extent.",
                HRESULT_FROM_WIN32( GetLastError() ) ) );
    }
    std::unique_ptr< sWindowEventState > window_events( new sWindowEventState() );
    HWND window = CreateWindowExW(
          0U
        , this->m_window_class_name.c_str()
        , window_title.c_str()
        , window_style
        , CW_USEDEFAULT
        , CW_USEDEFAULT
        , window_rectangle.right - window_rectangle.left
        , window_rectangle.bottom - window_rectangle.top
        , nullptr
        , nullptr
        , this->m_module_instance
        , window_events.get()
    );
    if( window == nullptr )
    {
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to create the renderer window.",
                HRESULT_FROM_WIN32( GetLastError() ) ) );
    }

    // Command queueへFlip-model Swap chainを接続する.
    DXGI_SWAP_CHAIN_DESC1 swap_chain_description = {};
    swap_chain_description.Width       = description_in.extent.width;
    swap_chain_description.Height      = description_in.extent.height;
    swap_chain_description.Format      = native_format;
    swap_chain_description.SampleDesc.Count = 1U;
    swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_description.BufferCount = description_in.buffer_count;
    swap_chain_description.Scaling     = DXGI_SCALING_STRETCH;
    swap_chain_description.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_description.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    swap_chain_description.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    ComPtr< IDXGISwapChain1 > base_swap_chain;
    HRESULT result = this->m_factory->CreateSwapChainForHwnd(
          this->m_queue.Get()
        , window
        , &swap_chain_description
        , nullptr
        , nullptr
        , base_swap_chain.ReleaseAndGetAddressOf()
    );
    if( FAILED( result ) )
    {
        DestroyWindow( window );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to create the window swap chain.", result ) );
    }
    result = this->m_factory->MakeWindowAssociation( window, DXGI_MWA_NO_ALT_ENTER );
    if( FAILED( result ) )
    {
        DestroyWindow( window );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to configure the window association.", result ) );
    }
    sSurfaceRecord surface;
    surface.description = description_in;
    surface.window      = window;
    surface.window_events = std::move( window_events );
    surface.window_events->extent_changed = false;
    surface.window_events->display_topology_changed = false;
    surface.window_mode = eSurfaceWindowMode::Windowed;
    result = base_swap_chain.As( &surface.swap_chain );
    if( FAILED( result ) )
    {
        DestroyWindow( window );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to query the window swap-chain interface.", result ) );
    }

    // Back bufferをSurface所有Textureとして登録する.
    D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
    heap_description.NumDescriptors = description_in.buffer_count;
    heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ComPtr< ID3D12DescriptorHeap > render_target_heap;
    result = this->m_device->CreateDescriptorHeap(
          &heap_description
        , IID_PPV_ARGS( render_target_heap.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        DestroyWindow( window );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to create the swap-chain descriptor heap.", result ) );
    }
    const UINT descriptor_increment = this->m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
    const std::uint64_t surface_id = this->m_next_resource_id++;
    for( std::uint32_t index = 0U; index < description_in.buffer_count; ++index )
    {
        sTextureRecord texture;
        result = surface.swap_chain->GetBuffer(
              index
            , IID_PPV_ARGS( texture.resource.ReleaseAndGetAddressOf() )
        );
        if( FAILED( result ) )
        {
            for( const sTextureHandle handle : surface.textures )
            {
                this->m_textures.erase( handle.value );
            }
            DestroyWindow( window );
            return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to acquire a swap-chain buffer.", result ) );
        }
        texture.render_target_heap   = render_target_heap;
        texture.render_target_handle = render_target_heap->GetCPUDescriptorHandleForHeapStart();
        texture.render_target_handle.ptr += static_cast< SIZE_T >( index ) * descriptor_increment;
        texture.description.extent        = description_in.extent;
        texture.description.format        = description_in.format;
        texture.description.usage         = eTextureUsage::RenderTarget |
            eTextureUsage::TransferSource;
        texture.description.initial_state = eTextureState::Present;
        texture.state                     = eTextureState::Present;
        texture.surface_owner             = surface_id;
        this->m_device->CreateRenderTargetView(
              texture.resource.Get()
            , nullptr
            , texture.render_target_handle
        );
        const std::uint64_t texture_id = this->m_next_resource_id++;
        this->m_textures.emplace( texture_id, std::move( texture ) );
        surface.textures.emplace_back( sTextureHandle{ texture_id, this->m_handle_generation } );
    }
    this->m_surfaces.emplace( surface_id, std::move( surface ) );
    if( description_in.visible )
    {
        ShowWindow( window, SW_SHOW );
        UpdateWindow( window );
    }
    return RendererResult< sSurfaceHandle >::success(
        sSurfaceHandle{ surface_id, this->m_handle_generation } );
}

RendererResult< sSurfaceHandle > D3D12RendererBackend::createDirectDisplaySurface(
    const sSurfaceDescription& description_in )
{
    const auto target_result = findD3D12DisplayTarget(
        this->m_factory.Get(), this->m_adapter.Get(), description_in.display_id );
    if( !target_result.succeeded() )
    {
        return RendererResult< sSurfaceHandle >::failure( target_result.error() );
    }
    const auto mode_iterator = std::find_if(
          target_result.value().description.modes.begin()
        , target_result.value().description.modes.end()
        , [&description_in]( const sDisplayMode& mode_in )
          {
              return sameDisplayMode( mode_in, description_in.display_mode );
          }
    );
    if( mode_iterator == target_result.value().description.modes.end() )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Direct-display mode is not present in the current display snapshot."
            )
        );
    }

    sSurfaceDescription window_description = description_in;
    window_description.type = eSurfaceType::Window;
    window_description.display_id.clear();
    window_description.display_mode = {};
    const auto surface_result = this->createWindowSurface( window_description );
    if( !surface_result.succeeded() )
    {
        return surface_result;
    }
    sSurfaceWindowModeRequest request;
    request.mode = eSurfaceWindowMode::DisplayModeFullscreen;
    request.display_id = description_in.display_id;
    request.display_mode = description_in.display_mode;
    const RendererStatus transition_result = this->setSurfaceWindowMode(
        surface_result.value(), request );
    if( !transition_result.succeeded() )
    {
        this->destroySurface( surface_result.value() );
        return RendererResult< sSurfaceHandle >::failure( transition_result.error() );
    }
    this->m_surfaces.at( surface_result.value().value ).description = description_in;
    return surface_result;
}

RendererError D3D12RendererBackend::waitForGpuIdle()
{
    const std::uint64_t fence_value = ++this->m_last_fence_value;
    const HRESULT signal_result = this->m_queue->Signal( this->m_fence.Get(), fence_value );
    if( FAILED( signal_result ) )
    {
        return backendError( "Failed to signal the surface-transition fence.", signal_result );
    }
    const RendererStatus wait_result = this->waitFenceValue( fence_value, SHUTDOWN_TIMEOUT_MS );
    return wait_result.succeeded() ? RendererError() : wait_result.error();
}

RendererError D3D12RendererBackend::enterDisplayModeFullscreen(
      sSurfaceRecord* const            p_surface_inout
    , const std::uint64_t              surface_id_in
    , const sSurfaceWindowModeRequest& request_in
)
{
    if( p_surface_inout == nullptr || p_surface_inout->swap_chain == nullptr ||
        p_surface_inout->window == nullptr )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidArgument
            , "Display-mode fullscreen requires a window swap chain."
        );
    }
    if( p_surface_inout->window_mode != eSurfaceWindowMode::Windowed )
    {
        return makeError(
              eRendererErrorCategory::Lifecycle
            , eRendererErrorCode::ResourceInUse
            , "Return the surface to windowed mode before selecting a display mode."
        );
    }
    if( request_in.display_mode.format != p_surface_inout->description.format )
    {
        return makeError(
              eRendererErrorCategory::Unsupported
            , eRendererErrorCode::UnsupportedFormat
            , "Display-mode fullscreen must preserve the swap-chain pixel format."
        );
    }
    const auto target_result = findD3D12DisplayTarget(
        this->m_factory.Get(), this->m_adapter.Get(), request_in.display_id );
    if( !target_result.succeeded() )
    {
        return target_result.error();
    }
    const auto mode_iterator = std::find_if(
          target_result.value().description.modes.begin()
        , target_result.value().description.modes.end()
        , [&request_in]( const sDisplayMode& mode_in )
          {
              return sameDisplayMode( mode_in, request_in.display_mode );
          }
    );
    if( mode_iterator == target_result.value().description.modes.end() )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidDescription
            , "Requested display mode is not currently advertised by the target display."
        );
    }

    const auto capture_result = captureWin32WindowedState( p_surface_inout->window );
    if( !capture_result.succeeded() )
    {
        return capture_result.error();
    }
    DEVMODEW restore_native_mode = {};
    restore_native_mode.dmSize = sizeof( restore_native_mode );
    if( EnumDisplaySettingsExW(
            target_result.value().device_name.c_str()
          , ENUM_CURRENT_SETTINGS
          , &restore_native_mode
          , 0U ) == FALSE )
    {
        return backendError(
            "Failed to capture the pre-transition native display mode.",
            HRESULT_FROM_WIN32( GetLastError() ) );
    }
    DEVMODEW requested_native_mode = {};
    bool found_native_mode = false;
    std::uint64_t closest_refresh_difference = ( std::numeric_limits< std::uint64_t >::max )();
    for( DWORD mode_index = 0U; ; ++mode_index )
    {
        DEVMODEW candidate = {};
        candidate.dmSize = sizeof( candidate );
        if( EnumDisplaySettingsExW(
                target_result.value().device_name.c_str()
              , mode_index
              , &candidate
              , 0U ) == FALSE )
        {
            break;
        }
        const bool candidate_interlaced = ( candidate.dmDisplayFlags & DM_INTERLACED ) != 0U;
        if( candidate.dmPelsWidth != request_in.display_mode.extent.width ||
            candidate.dmPelsHeight != request_in.display_mode.extent.height ||
            candidate_interlaced != request_in.display_mode.interlaced )
        {
            continue;
        }
        const std::uint64_t requested_refresh =
            request_in.display_mode.refresh_rate_numerator;
        const std::uint64_t candidate_refresh =
            static_cast< std::uint64_t >( candidate.dmDisplayFrequency ) *
            request_in.display_mode.refresh_rate_denominator;
        const std::uint64_t refresh_difference = requested_refresh == 0U ? 0U :
            ( requested_refresh > candidate_refresh
                ? requested_refresh - candidate_refresh
                : candidate_refresh - requested_refresh );
        if( refresh_difference < closest_refresh_difference )
        {
            requested_native_mode = candidate;
            closest_refresh_difference = refresh_difference;
            found_native_mode = true;
        }
    }
    if( !found_native_mode )
    {
        return makeError(
              eRendererErrorCategory::Resource
            , eRendererErrorCode::ResourceNotFound
            , "No native display mode matches the requested portable mode."
        );
    }
    if( !sameDisplayMode(
            request_in.display_mode, target_result.value().description.current_mode ) )
    {
        const LONG mode_test_result = ChangeDisplaySettingsExW(
              target_result.value().device_name.c_str()
            , &requested_native_mode
            , nullptr
            , CDS_TEST
            , nullptr
        );
        if( mode_test_result != DISP_CHANGE_SUCCESSFUL )
        {
            return displayChangeError(
                "The operating system rejected the requested display mode.", mode_test_result );
        }
    }
    const sRendererExtent2D saved_extent = p_surface_inout->description.extent;
    const RendererError idle_error = this->waitForGpuIdle();
    if( !idle_error.ok() )
    {
        return idle_error;
    }
    const LONG mode_change_result = ChangeDisplaySettingsExW(
          target_result.value().device_name.c_str()
        , &requested_native_mode
        , nullptr
        , CDS_FULLSCREEN
        , nullptr
    );
    if( mode_change_result != DISP_CHANGE_SUCCESSFUL )
    {
        return displayChangeError(
            "Failed to apply the requested temporary display mode.", mode_change_result );
    }
    const RendererError placement_error = applyWin32BorderlessWindow(
          p_surface_inout->window
        , capture_result.value()
        , target_result.value().description.position
        , request_in.display_mode.extent
    );
    if( !placement_error.ok() )
    {
        const LONG rollback_result = ChangeDisplaySettingsExW(
              target_result.value().device_name.c_str()
            , &restore_native_mode
            , nullptr
            , CDS_FULLSCREEN
            , nullptr
        );
        const RendererError rollback_error = rollback_result == DISP_CHANGE_SUCCESSFUL
            ? RendererError() : displayChangeError(
                "Failed to restore the display after window-placement failure.", rollback_result );
        return transitionWithRollbackError( placement_error, rollback_error );
    }
    const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
        p_surface_inout, surface_id_in, request_in.display_mode.extent, true );
    if( !buffer_error.ok() )
    {
        const LONG mode_rollback_result = ChangeDisplaySettingsExW(
              target_result.value().device_name.c_str()
            , &restore_native_mode
            , nullptr
            , CDS_FULLSCREEN
            , nullptr
        );
        RendererError rollback_error = restoreWin32WindowedState(
            p_surface_inout->window, capture_result.value() );
        if( mode_rollback_result != DISP_CHANGE_SUCCESSFUL )
        {
            rollback_error = displayChangeError(
                "Failed to restore the display after buffer failure.", mode_rollback_result );
        }
        return transitionWithRollbackError( buffer_error, rollback_error );
    }

    p_surface_inout->windowed_state = capture_result.value();
    p_surface_inout->windowed_extent = saved_extent;
    p_surface_inout->has_windowed_state = true;
    p_surface_inout->window_mode = eSurfaceWindowMode::DisplayModeFullscreen;
    p_surface_inout->display_id = request_in.display_id;
    p_surface_inout->display_mode = request_in.display_mode;
    p_surface_inout->restore_display_mode = target_result.value().description.current_mode;
    p_surface_inout->display_device_name = target_result.value().device_name;
    p_surface_inout->restore_native_display_mode = restore_native_mode;
    p_surface_inout->has_native_display_mode = true;
    if( p_surface_inout->window_events != nullptr )
    {
        p_surface_inout->window_events->extent_changed = false;
    }
    return RendererError();
}

RendererError D3D12RendererBackend::leaveDisplayModeFullscreen(
      sSurfaceRecord* const p_surface_inout
    , const std::uint64_t   surface_id_in
)
{
    if( p_surface_inout == nullptr || p_surface_inout->swap_chain == nullptr ||
        p_surface_inout->window_mode != eSurfaceWindowMode::DisplayModeFullscreen ||
        !p_surface_inout->has_windowed_state ||
        !p_surface_inout->has_native_display_mode ||
        p_surface_inout->display_device_name.empty() )
    {
        return makeError(
              eRendererErrorCategory::Lifecycle
            , eRendererErrorCode::InvalidDescription
            , "Display-mode fullscreen restoration state is unavailable."
        );
    }
    const RendererError idle_error = this->waitForGpuIdle();
    if( !idle_error.ok() )
    {
        return idle_error;
    }
    const LONG mode_restore_result = ChangeDisplaySettingsExW(
          p_surface_inout->display_device_name.c_str()
        , &p_surface_inout->restore_native_display_mode
        , nullptr
        , CDS_FULLSCREEN
        , nullptr
    );
    if( mode_restore_result != DISP_CHANGE_SUCCESSFUL )
    {
        return displayChangeError(
            "Failed to restore the pre-transition native display mode.", mode_restore_result );
    }
    const RendererError window_error = restoreWin32WindowedState(
        p_surface_inout->window, p_surface_inout->windowed_state );
    if( !window_error.ok() )
    {
        return window_error;
    }
    const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
        p_surface_inout, surface_id_in, p_surface_inout->windowed_extent, true );
    if( !buffer_error.ok() )
    {
        return buffer_error;
    }
    const auto displays_result = enumerateD3D12Displays(
        this->m_factory.Get(), this->m_adapter.Get() );
    if( !displays_result.succeeded() )
    {
        return displays_result.error();
    }
    const auto display_iterator = std::find_if(
          displays_result.value().begin()
        , displays_result.value().end()
        , [p_surface_inout]( const sDisplayDescription& display_in )
          {
              return display_in.id == p_surface_inout->display_id;
          }
    );
    if( display_iterator == displays_result.value().end() ||
        !sameDisplayMode(
            display_iterator->current_mode, p_surface_inout->restore_display_mode ) )
    {
        return makeError(
              eRendererErrorCategory::Backend
            , eRendererErrorCode::BackendFailure
            , "Display mode did not return to the pre-transition snapshot."
        );
    }
    p_surface_inout->window_mode = eSurfaceWindowMode::Windowed;
    p_surface_inout->display_id.clear();
    p_surface_inout->display_mode = {};
    p_surface_inout->restore_display_mode = {};
    p_surface_inout->display_device_name.clear();
    p_surface_inout->restore_native_display_mode = {};
    p_surface_inout->has_native_display_mode = false;
    p_surface_inout->has_windowed_state = false;
    p_surface_inout->windowed_state = {};
    p_surface_inout->windowed_extent = {};
    if( p_surface_inout->window_events != nullptr )
    {
        p_surface_inout->window_events->extent_changed = false;
    }
    return RendererError();
}

RendererError D3D12RendererBackend::resizeWindowSurfaceBuffers(
      sSurfaceRecord* const    p_surface_inout
    , const std::uint64_t      surface_id_in
    , const sRendererExtent2D  extent_in
    , const bool               force_rebuild_in
)
{
    if( p_surface_inout == nullptr || p_surface_inout->swap_chain == nullptr )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidArgument
            , "Window surface resize requires a swap chain."
        );
    }
    if( !force_rebuild_in &&
        p_surface_inout->description.extent.width == extent_in.width &&
        p_surface_inout->description.extent.height == extent_in.height )
    {
        return RendererError();
    }

    D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
    heap_description.NumDescriptors = p_surface_inout->description.buffer_count;
    heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ComPtr< ID3D12DescriptorHeap > replacement_heap;
    HRESULT result = this->m_device->CreateDescriptorHeap(
          &heap_description
        , IID_PPV_ARGS( replacement_heap.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return backendError( "Failed to create the resized swap-chain descriptor heap.", result );
    }

    const RendererError idle_error = this->waitForGpuIdle();
    if( !idle_error.ok() )
    {
        return idle_error;
    }

    const sRendererExtent2D old_extent = p_surface_inout->description.extent;
    const std::vector< sTextureHandle > old_handles = p_surface_inout->textures;
    std::vector< eTextureState > old_states;
    old_states.reserve( old_handles.size() );
    for( const sTextureHandle handle : old_handles )
    {
        const sTextureRecord* const p_old_texture = this->findTexture( handle );
        if( p_old_texture == nullptr )
        {
            return resourceNotFoundError( "Surface texture" );
        }
        old_states.emplace_back( p_old_texture->state );
    }
    for( const sTextureHandle handle : old_handles )
    {
        this->m_textures.erase( handle.value );
    }
    p_surface_inout->textures.clear();

    const auto acquire_buffers = [this, surface_id_in, p_surface_inout](
          const sRendererExtent2D&             buffer_extent_in
        , const ComPtr< ID3D12DescriptorHeap >& heap_in
        , const std::vector< sTextureHandle >* const p_reused_handles_in
        , const std::vector< eTextureState >* const p_reused_states_in
    ) -> RendererError
    {
        const std::uint32_t buffer_count = p_surface_inout->description.buffer_count;
        const UINT descriptor_increment = this->m_device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
        std::vector< sTextureRecord > records( buffer_count );
        for( std::uint32_t index = 0U; index < buffer_count; ++index )
        {
            sTextureRecord& texture = records[ index ];
            const HRESULT buffer_result = p_surface_inout->swap_chain->GetBuffer(
                  index
                , IID_PPV_ARGS( texture.resource.ReleaseAndGetAddressOf() )
            );
            if( FAILED( buffer_result ) )
            {
                return backendError( "Failed to acquire a resized swap-chain buffer.", buffer_result );
            }
            texture.render_target_heap   = heap_in;
            texture.render_target_handle = heap_in->GetCPUDescriptorHandleForHeapStart();
            texture.render_target_handle.ptr += static_cast< SIZE_T >( index ) * descriptor_increment;
            texture.description.extent        = buffer_extent_in;
            texture.description.format        = p_surface_inout->description.format;
            texture.description.usage         = eTextureUsage::RenderTarget |
                eTextureUsage::TransferSource;
            texture.description.initial_state = eTextureState::Present;
            texture.state = p_reused_states_in != nullptr &&
                    p_reused_states_in->size() == buffer_count
                ? ( *p_reused_states_in )[ index ] : eTextureState::Present;
            texture.surface_owner             = surface_id_in;
            this->m_device->CreateRenderTargetView(
                  texture.resource.Get()
                , nullptr
                , texture.render_target_handle
            );
        }

        std::vector< sTextureHandle > handles;
        handles.reserve( buffer_count );
        if( p_reused_handles_in != nullptr && p_reused_handles_in->size() == buffer_count )
        {
            handles = *p_reused_handles_in;
        }
        else
        {
            for( std::uint32_t index = 0U; index < buffer_count; ++index )
            {
                handles.emplace_back( sTextureHandle{
                    this->m_next_resource_id++, this->m_handle_generation } );
            }
        }
        for( std::uint32_t index = 0U; index < buffer_count; ++index )
        {
            this->m_textures.emplace( handles[ index ].value, std::move( records[ index ] ) );
        }
        p_surface_inout->textures = std::move( handles );
        return RendererError();
    };

    const DXGI_FORMAT native_format = toNativeFormat( p_surface_inout->description.format );
    DXGI_SWAP_CHAIN_DESC1 swap_chain_description = {};
    result = p_surface_inout->swap_chain->GetDesc1( &swap_chain_description );
    if( FAILED( result ) )
    {
        return backendError( "Failed to query swap-chain resize flags.", result );
    }
    result = p_surface_inout->swap_chain->ResizeBuffers(
          p_surface_inout->description.buffer_count
        , extent_in.width
        , extent_in.height
        , native_format
        , swap_chain_description.Flags
    );
    if( FAILED( result ) )
    {
        const RendererError recovery_error = acquire_buffers(
            old_extent, replacement_heap, &old_handles, &old_states );
        if( !recovery_error.ok() )
        {
            return makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Swap-chain resize failed and the previous buffers could not be reacquired."
                , result
            );
        }
        return backendError( "Failed to resize the window swap chain.", result );
    }

    const RendererError acquire_error = acquire_buffers(
        extent_in, replacement_heap, nullptr, nullptr );
    if( !acquire_error.ok() )
    {
        for( const sTextureHandle handle : p_surface_inout->textures )
        {
            this->m_textures.erase( handle.value );
        }
        p_surface_inout->textures.clear();
        const HRESULT rollback_result = p_surface_inout->swap_chain->ResizeBuffers(
              p_surface_inout->description.buffer_count
            , old_extent.width
            , old_extent.height
            , native_format
            , swap_chain_description.Flags
        );
        if( SUCCEEDED( rollback_result ) )
        {
            const RendererError recovery_error = acquire_buffers(
                old_extent, replacement_heap, &old_handles, nullptr );
            if( recovery_error.ok() )
            {
                return acquire_error;
            }
        }
        return makeError(
              eRendererErrorCategory::Backend
            , eRendererErrorCode::BackendFailure
            , "Swap-chain buffer acquisition failed and resize rollback was unsuccessful."
            , rollback_result
        );
    }
    p_surface_inout->description.extent = extent_in;
    return RendererError();
}

RendererResult< sTextureHandle > D3D12RendererBackend::getSurfaceTexture(
    const sSurfaceHandle surface_in ) const
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererResult< sTextureHandle >::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererResult< sTextureHandle >::failure( resourceNotFoundError( "Surface" ) );
    }
    const sSurfaceRecord& surface = iterator->second;
    const std::uint32_t texture_index = surface.swap_chain == nullptr ? 0U :
        surface.swap_chain->GetCurrentBackBufferIndex();
    if( texture_index >= surface.textures.size() )
    {
        return RendererResult< sTextureHandle >::failure( makeError( eRendererErrorCategory::Backend,
                eRendererErrorCode::BackendFailure, "Surface buffer index is invalid." ) );
    }
    return RendererResult< sTextureHandle >::success( surface.textures[ texture_index ] );
}

RendererResult< sSurfaceState > D3D12RendererBackend::getSurfaceState(
    const sSurfaceHandle surface_in ) const
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererResult< sSurfaceState >::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererResult< sSurfaceState >::failure( resourceNotFoundError( "Surface" ) );
    }
    const sSurfaceRecord& surface = iterator->second;
    sSurfaceState state;
    state.type        = surface.description.type;
    state.extent      = surface.description.extent;
    state.window_mode = surface.window_mode;
    state.display_id  = surface.display_id;
    state.display_mode = surface.display_mode;
    const RendererError validation_error = validateSurfaceState( state );
    if( !validation_error.ok() )
    {
        return RendererResult< sSurfaceState >::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Native surface state violates the portable contract: " +
                    validation_error.message()
            )
        );
    }
    return RendererResult< sSurfaceState >::success( std::move( state ) );
}

RendererStatus D3D12RendererBackend::resizeSurface(
      const sSurfaceHandle     surface_in
    , const sRendererExtent2D extent_in
)
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    sSurfaceRecord& surface = iterator->second;
    if( surface.description.type != eSurfaceType::Window || surface.swap_chain == nullptr )
    {
        return RendererStatus::failure( unsupportedError( eRendererErrorCode::UnsupportedOperation,
                "Only window surfaces can be resized." ) );
    }
    if( surface.window_mode != eSurfaceWindowMode::Windowed )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::ResourceInUse
                , "A borderless-fullscreen surface cannot be resized directly."
            )
        );
    }
    if( extent_in.width > this->m_capabilities.maximum_texture_extent ||
        extent_in.height > this->m_capabilities.maximum_texture_extent )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Surface resize exceeds the backend extent limit."
            )
        );
    }
    if( surface.description.extent.width == extent_in.width &&
        surface.description.extent.height == extent_in.height )
    {
        return RendererStatus::success();
    }

    const auto old_window_result = captureWin32WindowedState( surface.window );
    if( !old_window_result.succeeded() )
    {
        return RendererStatus::failure( old_window_result.error() );
    }
    const RendererError window_error = resizeWin32WindowClient( surface.window, extent_in );
    if( !window_error.ok() )
    {
        const RendererError rollback_error = restoreWin32WindowedState(
            surface.window, old_window_result.value() );
        return RendererStatus::failure( transitionWithRollbackError( window_error, rollback_error ) );
    }
    const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
        &surface, surface_in.value, extent_in );
    if( !buffer_error.ok() )
    {
        const RendererError rollback_error = restoreWin32WindowedState(
            surface.window, old_window_result.value() );
        return RendererStatus::failure( transitionWithRollbackError( buffer_error, rollback_error ) );
    }
    if( surface.window_events != nullptr )
    {
        surface.window_events->extent_changed = false;
    }
    return RendererStatus::success();
}

RendererStatus D3D12RendererBackend::setSurfaceWindowMode(
      const sSurfaceHandle            surface_in
    , const sSurfaceWindowModeRequest& request_in
)
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    sSurfaceRecord& surface = iterator->second;
    if( surface.description.type == eSurfaceType::DirectDisplay )
    {
        if( request_in.mode == surface.window_mode &&
            request_in.display_id == surface.display_id &&
            sameDisplayMode( request_in.display_mode, surface.display_mode ) )
        {
            return RendererStatus::success();
        }
        return RendererStatus::failure( unsupportedError( eRendererErrorCode::UnsupportedOperation,
                "Direct-display mode is fixed for the surface lifetime." ) );
    }
    if( surface.description.type != eSurfaceType::Window || surface.swap_chain == nullptr )
    {
        return RendererStatus::failure( unsupportedError( eRendererErrorCode::UnsupportedOperation,
                "Only window surfaces have a window mode." ) );
    }
    if( request_in.mode == surface.window_mode && request_in.display_id == surface.display_id &&
        sameDisplayMode( request_in.display_mode, surface.display_mode ) )
    {
        return RendererStatus::success();
    }

    if( request_in.mode == eSurfaceWindowMode::DisplayModeFullscreen )
    {
        const RendererError transition_error = this->enterDisplayModeFullscreen(
            &surface, surface_in.value, request_in );
        return transition_error.ok() ? RendererStatus::success() :
            RendererStatus::failure( transition_error );
    }
    if( surface.window_mode == eSurfaceWindowMode::DisplayModeFullscreen )
    {
        if( request_in.mode != eSurfaceWindowMode::Windowed )
        {
            return RendererStatus::failure( makeError(
                      eRendererErrorCategory::Lifecycle
                    , eRendererErrorCode::ResourceInUse
                    , "Return display-mode fullscreen to windowed mode before another transition."
                )
            );
        }
        const RendererError restoration_error = this->leaveDisplayModeFullscreen(
            &surface, surface_in.value );
        return restoration_error.ok() ? RendererStatus::success() :
            RendererStatus::failure( restoration_error );
    }

    RECT old_window_rectangle = {};
    if( GetWindowRect( surface.window, &old_window_rectangle ) == FALSE )
    {
        return RendererStatus::failure( backendError( "Failed to query the current renderer window bounds.",
                HRESULT_FROM_WIN32( GetLastError() ) ) );
    }

    if( request_in.mode == eSurfaceWindowMode::BorderlessFullscreen )
    {
        const auto displays_result = enumerateD3D12Displays(
            this->m_factory.Get(), this->m_adapter.Get() );
        if( !displays_result.succeeded() )
        {
            return RendererStatus::failure( displays_result.error() );
        }
        const auto display_iterator = std::find_if(
              displays_result.value().begin()
            , displays_result.value().end()
            , [&request_in]( const sDisplayDescription& display_in )
              {
                  return display_in.id == request_in.display_id;
              }
        );
        if( display_iterator == displays_result.value().end() )
        {
            return RendererStatus::failure( makeError(
                      eRendererErrorCategory::Resource
                    , eRendererErrorCode::ResourceNotFound
                    , "Requested display ID is not active."
                )
            );
        }
        if( display_iterator->desktop_extent.width > this->m_capabilities.maximum_texture_extent ||
            display_iterator->desktop_extent.height > this->m_capabilities.maximum_texture_extent )
        {
            return RendererStatus::failure( makeError(
                      eRendererErrorCategory::Unsupported
                    , eRendererErrorCode::UnsupportedOperation
                    , "Display extent exceeds the backend surface limit."
                )
            );
        }

        const bool entering_from_windowed =
            surface.window_mode == eSurfaceWindowMode::Windowed;
        const sRendererExtent2D saved_windowed_extent = surface.description.extent;
        sWin32WindowedState restore_state = surface.windowed_state;
        if( entering_from_windowed )
        {
            const auto capture_result = captureWin32WindowedState( surface.window );
            if( !capture_result.succeeded() )
            {
                return RendererStatus::failure( capture_result.error() );
            }
            restore_state = capture_result.value();
        }
        const RendererError window_error = applyWin32BorderlessWindow(
              surface.window
            , restore_state
            , display_iterator->position
            , display_iterator->desktop_extent
        );
        if( !window_error.ok() )
        {
            if( surface.window_mode == eSurfaceWindowMode::BorderlessFullscreen )
            {
                const RendererError rollback_error = applyWin32BorderlessWindow(
                      surface.window
                    , restore_state
                    , { old_window_rectangle.left, old_window_rectangle.top }
                    , surface.description.extent
                );
                return RendererStatus::failure( transitionWithRollbackError( window_error, rollback_error ) );
            }
            return RendererStatus::failure( window_error );
        }
        const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
            &surface, surface_in.value, display_iterator->desktop_extent );
        if( !buffer_error.ok() )
        {
            RendererError rollback_error;
            if( surface.window_mode == eSurfaceWindowMode::Windowed )
            {
                rollback_error = restoreWin32WindowedState( surface.window, restore_state );
            }
            else
            {
                rollback_error = applyWin32BorderlessWindow(
                      surface.window
                    , restore_state
                    , { old_window_rectangle.left, old_window_rectangle.top }
                    , surface.description.extent
                );
            }
            return RendererStatus::failure( transitionWithRollbackError( buffer_error, rollback_error ) );
        }
        surface.windowed_state = restore_state;
        if( entering_from_windowed )
        {
            surface.windowed_extent = saved_windowed_extent;
        }
        surface.has_windowed_state = true;
        surface.window_mode = eSurfaceWindowMode::BorderlessFullscreen;
        surface.display_id = request_in.display_id;
        surface.display_mode = {};
        if( surface.window_events != nullptr )
        {
            surface.window_events->extent_changed = false;
        }
        return RendererStatus::success();
    }

    if( !surface.has_windowed_state )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::ResourceNotFound
                , "Windowed restoration state is unavailable."
            )
        );
    }
    const RendererError restore_error = restoreWin32WindowedState(
        surface.window, surface.windowed_state );
    if( !restore_error.ok() )
    {
        const RendererError rollback_error = applyWin32BorderlessWindow(
              surface.window
            , surface.windowed_state
            , { old_window_rectangle.left, old_window_rectangle.top }
            , surface.description.extent
        );
        return RendererStatus::failure( transitionWithRollbackError( restore_error, rollback_error ) );
    }
    const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
        &surface, surface_in.value, surface.windowed_extent );
    if( !buffer_error.ok() )
    {
        const RendererError rollback_error = applyWin32BorderlessWindow(
              surface.window
            , surface.windowed_state
            , { old_window_rectangle.left, old_window_rectangle.top }
            , surface.description.extent
        );
        return RendererStatus::failure( transitionWithRollbackError( buffer_error, rollback_error ) );
    }
    surface.window_mode = eSurfaceWindowMode::Windowed;
    surface.display_id.clear();
    surface.display_mode = {};
    surface.has_windowed_state = false;
    surface.windowed_state = {};
    surface.windowed_extent = {};
    if( surface.window_events != nullptr )
    {
        surface.window_events->extent_changed = false;
    }
    return RendererStatus::success();
}

RendererStatus D3D12RendererBackend::destroySurface( const sSurfaceHandle surface_in )
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    }
    if( iterator->second.window_mode == eSurfaceWindowMode::DisplayModeFullscreen )
    {
        const RendererError restoration_error = this->leaveDisplayModeFullscreen(
            &iterator->second, surface_in.value );
        if( !restoration_error.ok() )
        {
            return RendererStatus::failure( restoration_error );
        }
    }
    if( iterator->second.window != nullptr && IsWindow( iterator->second.window ) )
    {
        DestroyWindow( iterator->second.window );
    }
    for( const sTextureHandle texture : iterator->second.textures )
    {
        this->m_textures.erase( texture.value );
    }
    this->m_surfaces.erase( iterator );
    return RendererStatus::success();
}

RendererResult< bool > D3D12RendererBackend::processSurfaceEvents(
    const sSurfaceHandle surface_in )
{
    const auto events_result = this->pollSurfaceEvents( surface_in );
    return events_result.succeeded() ? RendererResult< bool >::success( events_result.value().alive ) :
        RendererResult< bool >::failure( events_result.error() );
}

RendererResult< sSurfaceEvents > D3D12RendererBackend::pollSurfaceEvents(
    const sSurfaceHandle surface_in )
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererResult< sSurfaceEvents >::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererResult< sSurfaceEvents >::failure( resourceNotFoundError( "Surface" ) );
    }
    sSurfaceRecord& surface = iterator->second;
    if( surface.description.type == eSurfaceType::Offscreen || surface.window_events == nullptr )
    {
        return RendererResult< sSurfaceEvents >::failure( unsupportedError( eRendererErrorCode::UnsupportedOperation,
                "Only presentable surfaces process operating-system events." ) );
    }
    MSG message = {};
    while( IsWindow( surface.window ) &&
        PeekMessageW( &message, surface.window, 0U, 0U, PM_REMOVE ) )
    {
        TranslateMessage( &message );
        DispatchMessageW( &message );
    }

    sSurfaceEvents events;
    events.alive = IsWindow( surface.window ) != FALSE && surface.window_events->alive;
    events.extent = surface.description.extent;
    events.display_topology_changed = surface.window_events->display_topology_changed;
    surface.window_events->display_topology_changed = false;

    if( surface.window_events->extent_changed )
    {
        const sRendererExtent2D requested_extent = surface.window_events->extent;
        surface.window_events->extent_changed = false;
        if( surface.description.type == eSurfaceType::Window &&
            surface.window_mode == eSurfaceWindowMode::Windowed &&
            !surface.window_events->minimized &&
            ( requested_extent.width != surface.description.extent.width ||
              requested_extent.height != surface.description.extent.height ) )
        {
            const sRendererExtent2D old_extent = surface.description.extent;
            const RendererError buffer_error = this->resizeWindowSurfaceBuffers(
                &surface, surface_in.value, requested_extent );
            if( !buffer_error.ok() )
            {
                const RendererError rollback_error = resizeWin32WindowClient(
                    surface.window, old_extent );
                return RendererResult< sSurfaceEvents >::failure( transitionWithRollbackError( buffer_error, rollback_error ) );
            }
            events.extent_changed = true;
            events.extent = requested_extent;
        }
    }
    return RendererResult< sSurfaceEvents >::success( events );
}

RendererResult< sFenceHandle > D3D12RendererBackend::presentSurface(
    const sSurfaceHandle surface_in )
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Surface" ) );
    }
    const auto iterator = this->m_surfaces.find( surface_in.value );
    if( iterator == this->m_surfaces.end() )
    {
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Surface" ) );
    }
    sSurfaceRecord& surface = iterator->second;
    if( ( surface.description.type != eSurfaceType::Window &&
          surface.description.type != eSurfaceType::DirectDisplay ) ||
        surface.swap_chain == nullptr )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError( eRendererErrorCode::UnsupportedOperation,
                "Only window surfaces can be presented." ) );
    }
    if( !IsWindow( surface.window ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError( eRendererErrorCategory::Lifecycle,
                eRendererErrorCode::ResourceNotFound, "Renderer window has been closed." ) );
    }
    const sTextureHandle current_texture = surface.textures[
        surface.swap_chain->GetCurrentBackBufferIndex() ];
    const sTextureRecord* const p_texture = this->findTexture( current_texture );
    if( p_texture == nullptr || p_texture->state != eTextureState::Present )
    {
        return RendererResult< sFenceHandle >::failure( makeError( eRendererErrorCategory::Lifecycle,
                eRendererErrorCode::InvalidDescription,
                "The current window buffer must end its render pass in Present state." ) );
    }

    const UINT sync_interval = surface.description.vertical_sync ? 1U : 0U;
    const HRESULT present_result = surface.swap_chain->Present( sync_interval, 0U );
    if( FAILED( present_result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to present the window surface.", present_result ) );
    }
    const std::uint64_t fence_value = ++this->m_last_fence_value;
    const HRESULT signal_result = this->m_queue->Signal( this->m_fence.Get(), fence_value );
    if( FAILED( signal_result ) )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to signal the present fence.", signal_result ) );
    }
    sPendingSubmission pending;
    pending.fence_value = fence_value;
    this->m_pending_submissions.emplace_back( std::move( pending ) );
    this->collectCompletedSubmissions();
    return RendererResult< sFenceHandle >::success(
        sFenceHandle{ fence_value, this->m_handle_generation } );
}

// Render-pass execution and CPU readback.
RendererResult< sFenceHandle > D3D12RendererBackend::executeRenderPass(
    const sRenderPassDescription& description_in )
{
    sTextureRecord* const p_texture = this->findTexture( description_in.color_attachment );
    if( p_texture == nullptr )
    {
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Texture" ) );
    }
    if( !hasTextureUsage( p_texture->description.usage, eTextureUsage::RenderTarget ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Render pass attachment lacks RenderTarget usage."
            )
        );
    }
    if( description_in.load_operation == eAttachmentLoadOperation::Discard ||
        description_in.store_operation == eAttachmentStoreOperation::Discard )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Discard attachment operations are not implemented."
            )
        );
    }
    if( description_in.final_state == eTextureState::CopySource &&
        !hasTextureUsage( p_texture->description.usage, eTextureUsage::TransferSource ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "CopySource final state requires TransferSource usage."
            )
        );
    }
    if( description_in.final_state == eTextureState::Present )
    {
        const auto surface = this->m_surfaces.find( p_texture->surface_owner );
        if( surface == this->m_surfaces.end() ||
            ( surface->second.description.type != eSurfaceType::Window &&
              surface->second.description.type != eSurfaceType::DirectDisplay ) ||
            surface->second.swap_chain == nullptr )
        {
            return RendererResult< sFenceHandle >::failure( unsupportedError(
                      eRendererErrorCode::UnsupportedOperation
                    , "Present state requires a window or direct-display swap-chain attachment."
                )
            );
        }
    }

    struct sResolvedDraw
    {
        sMeshRecord*             p_mesh;
        sTextureRecord*          p_source;
        sTextureRecord*          p_alpha;
        const sMeshDrawCommand*  p_description;

        //! @brief Construct all members with explicit defaults.
        sResolvedDraw(
              sMeshRecord * p_mesh_in = nullptr
            , sTextureRecord * p_source_in = nullptr
            , sTextureRecord * p_alpha_in = nullptr
            , const sMeshDrawCommand * p_description_in = nullptr
        )
            : p_mesh        ( p_mesh_in )
            , p_source      ( p_source_in )
            , p_alpha       ( p_alpha_in )
            , p_description ( p_description_in )
        {
        }
    };
    std::vector< sResolvedDraw > resolved_draws;
    resolved_draws.reserve( description_in.draw_commands.size() );
    for( const sMeshDrawCommand& draw_command : description_in.draw_commands )
    {
        sMeshRecord* const p_mesh = this->findMesh( draw_command.mesh );
        sTextureRecord* const p_source = this->findTexture( draw_command.source_texture );
        sTextureRecord* const p_alpha = draw_command.alpha_texture.valid()
            ? this->findTexture( draw_command.alpha_texture ) : nullptr;
        if( p_mesh == nullptr )
        {
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Mesh" ) );
        }
        if( p_source == nullptr )
        {
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Source texture" ) );
        }
        if( draw_command.alpha_texture.valid() && p_alpha == nullptr )
        {
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Alpha texture" ) );
        }
        if( !hasTextureUsage( p_source->description.usage, eTextureUsage::Sampled ) ||
            p_source->shader_resource_heap == nullptr )
        {
            return RendererResult< sFenceHandle >::failure( makeError(
                      eRendererErrorCategory::Validation
                    , eRendererErrorCode::InvalidDescription
                    , "Mesh draw source texture lacks Sampled usage."
                )
            );
        }
        if( p_alpha != nullptr &&
            ( !hasTextureUsage( p_alpha->description.usage, eTextureUsage::Sampled ) ||
              p_alpha->shader_resource_heap == nullptr ) )
        {
            return RendererResult< sFenceHandle >::failure( makeError(
                      eRendererErrorCategory::Validation
                    , eRendererErrorCode::InvalidDescription
                    , "Mesh draw alpha texture lacks Sampled usage."
                )
            );
        }
        resolved_draws.emplace_back(
            sResolvedDraw{ p_mesh, p_source, p_alpha, &draw_command } );
    }
    if( !resolved_draws.empty() && this->findPipelineState(
            p_texture->description.format, eColorBlendMode::Replace ) == nullptr )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError( eRendererErrorCode::UnsupportedFormat,
                "Mesh drawing does not support the attachment format." ) );
    }

    const std::uint32_t area_width = description_in.render_area.extent.empty()
        ? p_texture->description.extent.width : description_in.render_area.extent.width;
    const std::uint32_t area_height = description_in.render_area.extent.empty()
        ? p_texture->description.extent.height : description_in.render_area.extent.height;
    if( area_width > p_texture->description.extent.width ||
        area_height > p_texture->description.extent.height ||
        description_in.render_area.x > p_texture->description.extent.width - area_width ||
        description_in.render_area.y > p_texture->description.extent.height - area_height )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Render area is outside the attachment extent."
            )
        );
    }

    ComPtr< ID3D12CommandAllocator > allocator;
    ComPtr< ID3D12GraphicsCommandList > command_list;
    const RendererError command_error = this->createCommandList( &allocator, &command_list );
    if( !command_error.ok() )
    {
        return RendererResult< sFenceHandle >::failure( command_error );
    }

    // 全DrawのSource／Alpha SRVを1つのShader-visible Heapへ複製する.
    ComPtr< ID3D12DescriptorHeap > draw_descriptor_heap;
    UINT descriptor_increment = 0U;
    if( !resolved_draws.empty() )
    {
        if( resolved_draws.size() >
            static_cast< std::size_t >(
                ( std::numeric_limits< UINT >::max )() / DRAW_DESCRIPTORS_PER_COMMAND ) )
        {
            return RendererResult< sFenceHandle >::failure( makeError(
                      eRendererErrorCategory::Validation
                    , eRendererErrorCode::InvalidDescription
                    , "Mesh draw descriptor count exceeds D3D12 limits."
                )
            );
        }
        D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
        heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_description.NumDescriptors = static_cast< UINT >(
            resolved_draws.size() * DRAW_DESCRIPTORS_PER_COMMAND );
        heap_description.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        const HRESULT heap_result = this->m_device->CreateDescriptorHeap(
              &heap_description
            , IID_PPV_ARGS( draw_descriptor_heap.ReleaseAndGetAddressOf() )
        );
        if( FAILED( heap_result ) )
        {
            return RendererResult< sFenceHandle >::failure( backendError( "Failed to create the draw descriptor heap.", heap_result ) );
        }
        descriptor_increment = this->m_device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        D3D12_CPU_DESCRIPTOR_HANDLE destination =
            draw_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        for( const sResolvedDraw& draw : resolved_draws )
        {
            this->m_device->CopyDescriptorsSimple(
                  1U
                , destination
                , draw.p_source->shader_resource_heap->GetCPUDescriptorHandleForHeapStart()
                , D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
            destination.ptr += descriptor_increment;
            const sTextureRecord* const p_alpha_source =
                draw.p_alpha == nullptr ? draw.p_source : draw.p_alpha;
            this->m_device->CopyDescriptorsSimple(
                  1U
                , destination
                , p_alpha_source->shader_resource_heap->GetCPUDescriptorHandleForHeapStart()
                , D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
            destination.ptr += descriptor_increment;
        }
    }

    const D3D12_RESOURCE_STATES original_state = toNativeState( p_texture->state );
    if( original_state != D3D12_RESOURCE_STATE_RENDER_TARGET )
    {
        const D3D12_RESOURCE_BARRIER barrier = makeTransition(
              p_texture->resource.Get()
            , original_state
            , D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        command_list->ResourceBarrier( 1U, &barrier );
    }

    if( description_in.load_operation == eAttachmentLoadOperation::Clear )
    {
        const float clear_color[] = {
              description_in.clear_color.red
            , description_in.clear_color.green
            , description_in.clear_color.blue
            , description_in.clear_color.alpha
        };
        const LONG left   = static_cast< LONG >( description_in.render_area.x );
        const LONG top    = static_cast< LONG >( description_in.render_area.y );
        const LONG right  = static_cast< LONG >( description_in.render_area.x + area_width );
        const LONG bottom = static_cast< LONG >( description_in.render_area.y + area_height );
        const D3D12_RECT clear_region = { left, top, right, bottom };
        command_list->ClearRenderTargetView(
              p_texture->render_target_handle
            , clear_color
            , 1U
            , &clear_region
        );
    }

    // Draw commandをPortable Mesh／TextureからNative pipelineへ記録する.
    std::vector< sTextureRecord* > transitioned_sources;
    std::vector< ComPtr< ID3D12Resource > > retained_resources = {
        p_texture->resource };
    if( !resolved_draws.empty() )
    {
        const D3D12_VIEWPORT viewport = {
              static_cast< FLOAT >( description_in.render_area.x )
            , static_cast< FLOAT >( description_in.render_area.y )
            , static_cast< FLOAT >( area_width )
            , static_cast< FLOAT >( area_height )
            , 0.0F
            , 1.0F
        };
        const D3D12_RECT scissor = {
              static_cast< LONG >( description_in.render_area.x )
            , static_cast< LONG >( description_in.render_area.y )
            , static_cast< LONG >( description_in.render_area.x + area_width )
            , static_cast< LONG >( description_in.render_area.y + area_height )
        };
        command_list->SetGraphicsRootSignature( this->m_root_signature.Get() );
        command_list->OMSetRenderTargets(
            1U, &p_texture->render_target_handle, FALSE, nullptr );
        command_list->RSSetViewports( 1U, &viewport );
        command_list->RSSetScissorRects( 1U, &scissor );
        ID3D12DescriptorHeap* descriptor_heaps[] = { draw_descriptor_heap.Get() };
        command_list->SetDescriptorHeaps( 1U, descriptor_heaps );

        std::size_t draw_index = 0U;
        for( const sResolvedDraw& draw : resolved_draws )
        {
            // SourceとOptional Alpha mapをShader Resourceへ遷移する.
            sTextureRecord* sampled_textures[] = { draw.p_source, draw.p_alpha };
            for( sTextureRecord* const p_sampled_texture : sampled_textures )
            {
                if( p_sampled_texture != nullptr &&
                    p_sampled_texture->state != eTextureState::ShaderResource &&
                    std::find(
                          transitioned_sources.begin()
                        , transitioned_sources.end()
                        , p_sampled_texture
                    ) == transitioned_sources.end() )
                {
                    const D3D12_RESOURCE_BARRIER barrier = makeTransition(
                          p_sampled_texture->resource.Get()
                        , toNativeState( p_sampled_texture->state )
                        , D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                    );
                    command_list->ResourceBarrier( 1U, &barrier );
                    transitioned_sources.emplace_back( p_sampled_texture );
                }
            }

            // Draw固有のBlend、Sampling、Alpha mapおよびOpacityを設定する.
            command_list->SetPipelineState( this->findPipelineState(
                  p_texture->description.format
                , draw.p_description->blend_mode
            ) );
            const D3D_PRIMITIVE_TOPOLOGY topology =
                draw.p_mesh->topology == ePrimitiveTopology::TriangleStrip
                    ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                    : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            command_list->IASetPrimitiveTopology( topology );
            command_list->IASetVertexBuffers( 0U, 1U, &draw.p_mesh->vertex_view );
            command_list->IASetIndexBuffer( &draw.p_mesh->index_view );
            D3D12_GPU_DESCRIPTOR_HANDLE descriptor_handle =
                draw_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
            descriptor_handle.ptr += static_cast< UINT64 >(
                draw_index * DRAW_DESCRIPTORS_PER_COMMAND ) * descriptor_increment;
            command_list->SetGraphicsRootDescriptorTable(
                0U, descriptor_handle );
            std::uint32_t draw_flags = 0U;
            if( draw.p_alpha != nullptr )
            {
                draw_flags |= DRAW_FLAG_USE_ALPHA_MAP;
                if( draw.p_alpha->description.format == eRendererPixelFormat::R8Unorm )
                {
                    draw_flags |= DRAW_FLAG_ALPHA_RED_CHANNEL;
                }
            }
            if( draw.p_description->sampling_filter == eTextureSamplingFilter::Linear )
            {
                draw_flags |= DRAW_FLAG_LINEAR_SAMPLING;
            }
            const sEdgeBlendDescription& edge_blend =
                draw.p_description->edge_blend;
            if( edge_blend.left > 0.0F || edge_blend.right > 0.0F ||
                edge_blend.top > 0.0F || edge_blend.bottom > 0.0F )
            {
                draw_flags |= DRAW_FLAG_USE_EDGE_BLEND;
                if( edge_blend.curve == eEdgeBlendCurve::Smoothstep )
                {
                    draw_flags |= DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND;
                }
            }
            std::uint32_t draw_parameters[ DRAW_PARAMETER_COUNT ] = { draw_flags };
            const float parameter_values[] = {
                  draw.p_description->opacity
                , edge_blend.left
                , edge_blend.right
                , edge_blend.top
                , edge_blend.bottom
            };
            static_assert(
                std::size( parameter_values ) + 1U == DRAW_PARAMETER_COUNT,
                "Draw parameter layout must match the root constants." );
            std::memcpy(
                  draw_parameters + 1U
                , parameter_values
                , sizeof( parameter_values )
            );
            command_list->SetGraphicsRoot32BitConstants(
                  1U
                , static_cast< UINT >( std::size( draw_parameters ) )
                , draw_parameters
                , 0U
            );
            command_list->DrawIndexedInstanced(
                draw.p_mesh->index_count, 1U, 0U, 0, 0U );
            retained_resources.emplace_back( draw.p_source->resource );
            if( draw.p_alpha != nullptr )
            {
                retained_resources.emplace_back( draw.p_alpha->resource );
            }
            retained_resources.emplace_back( draw.p_mesh->vertex_buffer );
            retained_resources.emplace_back( draw.p_mesh->index_buffer );
            ++draw_index;
        }
    }

    const D3D12_RESOURCE_STATES final_state = toNativeState( description_in.final_state );
    if( final_state != D3D12_RESOURCE_STATE_RENDER_TARGET )
    {
        const D3D12_RESOURCE_BARRIER barrier = makeTransition(
              p_texture->resource.Get()
            , D3D12_RESOURCE_STATE_RENDER_TARGET
            , final_state
        );
        command_list->ResourceBarrier( 1U, &barrier );
    }

    std::vector< ComPtr< ID3D12DescriptorHeap > > retained_descriptor_heaps;
    if( draw_descriptor_heap != nullptr )
    {
        retained_descriptor_heaps.emplace_back( std::move( draw_descriptor_heap ) );
    }
    RendererResult< sFenceHandle > result = this->submit(
          std::move( allocator )
        , std::move( command_list )
        , std::move( retained_resources )
        , std::move( retained_descriptor_heaps )
    );
    if( result.succeeded() )
    {
        p_texture->state = description_in.final_state;
        for( sTextureRecord* const p_source : transitioned_sources )
        {
            p_source->state = eTextureState::ShaderResource;
        }
    }
    return result;
}

RendererStatus D3D12RendererBackend::waitFence(
      const sFenceHandle  fence_in
    , const std::uint32_t timeout_ms_in
)
{
    if( fence_in.generation != this->m_handle_generation ||
        fence_in.value == 0U || fence_in.value > this->m_last_fence_value )
    {
        return RendererStatus::failure( resourceNotFoundError( "Fence" ) );
    }
    return this->waitFenceValue( fence_in.value, timeout_ms_in );
}

RendererResult< sRendererFrame > D3D12RendererBackend::readTexture(
      const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in
)
{
    sTextureRecord* const p_texture = this->findTexture( texture_in );
    if( p_texture == nullptr )
    {
        return RendererResult< sRendererFrame >::failure( resourceNotFoundError( "Texture" ) );
    }
    if( !hasTextureUsage( p_texture->description.usage, eTextureUsage::TransferSource ) )
    {
        return RendererResult< sRendererFrame >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Texture readback requires TransferSource usage."
            )
        );
    }

    const D3D12_RESOURCE_DESC texture_description = p_texture->resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 readback_size = 0U;
    this->m_device->GetCopyableFootprints(
          &texture_description
        , 0U
        , 1U
        , 0U
        , &footprint
        , nullptr
        , nullptr
        , &readback_size
    );

    D3D12_RESOURCE_DESC readback_description = {};
    readback_description.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    readback_description.Width              = readback_size;
    readback_description.Height             = 1U;
    readback_description.DepthOrArraySize   = 1U;
    readback_description.MipLevels          = 1U;
    readback_description.SampleDesc.Count   = 1U;
    readback_description.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const D3D12_HEAP_PROPERTIES readback_heap = makeHeapProperties( D3D12_HEAP_TYPE_READBACK );

    ComPtr< ID3D12Resource > readback;
    HRESULT result = this->m_device->CreateCommittedResource(
          &readback_heap
        , D3D12_HEAP_FLAG_NONE
        , &readback_description
        , D3D12_RESOURCE_STATE_COPY_DEST
        , nullptr
        , IID_PPV_ARGS( readback.ReleaseAndGetAddressOf() )
    );
    if( FAILED( result ) )
    {
        return RendererResult< sRendererFrame >::failure( backendError( "Failed to create the readback buffer.", result ) );
    }

    ComPtr< ID3D12CommandAllocator > allocator;
    ComPtr< ID3D12GraphicsCommandList > command_list;
    const RendererError command_error = this->createCommandList( &allocator, &command_list );
    if( !command_error.ok() )
    {
        return RendererResult< sRendererFrame >::failure( command_error );
    }
    if( p_texture->state != eTextureState::CopySource )
    {
        const D3D12_RESOURCE_BARRIER barrier = makeTransition(
              p_texture->resource.Get()
            , toNativeState( p_texture->state )
            , D3D12_RESOURCE_STATE_COPY_SOURCE
        );
        command_list->ResourceBarrier( 1U, &barrier );
    }

    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource        = p_texture->resource.Get();
    source.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0U;
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource       = readback.Get();
    destination.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = footprint;
    command_list->CopyTextureRegion( &destination, 0U, 0U, 0U, &source, nullptr );

    std::vector< ComPtr< ID3D12Resource > > retained_resources = {
          p_texture->resource
        , readback
    };
    RendererResult< sFenceHandle > submit_result = this->submit(
          std::move( allocator )
        , std::move( command_list )
        , std::move( retained_resources )
    );
    if( !submit_result.succeeded() )
    {
        return RendererResult< sRendererFrame >::failure( submit_result.error() );
    }
    p_texture->state = eTextureState::CopySource;
    const RendererStatus wait_result = this->waitFenceValue(
        submit_result.value().value, timeout_ms_in );
    if( !wait_result.succeeded() )
    {
        return RendererResult< sRendererFrame >::failure( wait_result.error() );
    }

    sRendererFrame frame;
    frame.description.extent = p_texture->description.extent;
    frame.description.format = p_texture->description.format;
    frame.description.row_pitch = 0U;
    const std::size_t packed_row_pitch = frame.description.minimumRowPitch();
    frame.data.resize( frame.description.memorySize() );

    const D3D12_RANGE read_range = { 0U, static_cast< SIZE_T >( readback_size ) };
    void* p_mapped_data = nullptr;
    result = readback->Map( 0U, &read_range, &p_mapped_data );
    if( FAILED( result ) )
    {
        return RendererResult< sRendererFrame >::failure( backendError( "Failed to map the readback buffer.", result ) );
    }

    const std::uint8_t* const p_source = static_cast< const std::uint8_t* >( p_mapped_data );
    for( std::uint32_t row = 0U; row < frame.description.extent.height; ++row )
    {
        std::copy_n(
              p_source + static_cast< std::size_t >( row ) * footprint.Footprint.RowPitch
            , packed_row_pitch
            , frame.data.data() + static_cast< std::size_t >( row ) * packed_row_pitch
        );
    }
    const D3D12_RANGE written_range = { 0U, 0U };
    readback->Unmap( 0U, &written_range );
    return RendererResult< sRendererFrame >::success( std::move( frame ) );
}

std::unique_ptr< RendererBackend > createPlatformRendererBackend(
          RendererError*                p_error_out
    , const sRendererConfiguration& configuration_in
)
{
    if( p_error_out == nullptr )
    {
        return nullptr;
    }
    if( configuration_in.backend == eRendererBackend::Vulkan12 )
    {
        *p_error_out = unsupportedError(
              eRendererErrorCode::UnsupportedBackend
            , "Vulkan 1.2 is not implemented on the Windows OUI backend."
        );
        return nullptr;
    }
    *p_error_out = RendererError();
    return std::make_unique< D3D12RendererBackend >();
}

} // namespace internal
} // namespace oui
} // namespace wse
