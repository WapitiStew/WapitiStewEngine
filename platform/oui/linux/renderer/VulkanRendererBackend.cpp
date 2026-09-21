//*****************************************************************************************************************
//!
//! @file    VulkanRendererBackend.cpp
//! @brief   \~japanese Linux向けVulkan 1.2 OUI Renderer Backendを実装する.
//! @brief   \~english  Implements the Vulkan 1.2 OUI renderer backend for Linux.
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

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include "../../../../core/oui/renderer/RendererBackend.h"
#include "../../../../core/oui/renderer/RendererIdentity.h"
#include "WaylandWindow.h"
#include "VulkanSubmission.h"

#include "wse_portable_fragment_shader.h"
#include "wse_portable_vertex_shader.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

constexpr std::uint32_t GPU_TIMEOUT_MS = 30000U;
constexpr std::uint32_t DRAW_FLAG_USE_ALPHA_MAP = 1U << 0U;
constexpr std::uint32_t DRAW_FLAG_LINEAR_SAMPLING = 1U << 1U;
constexpr std::uint32_t DRAW_FLAG_ALPHA_RED_CHANNEL = 1U << 2U;
constexpr std::uint32_t DRAW_FLAG_USE_EDGE_BLEND = 1U << 3U;
constexpr std::uint32_t DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND = 1U << 4U;

wse::oui::RendererError makeError(
      const wse::oui::eRendererErrorCategory category_in
    , const wse::oui::eRendererErrorCode     code_in
    , const std::string&                     message_in
    , const std::int64_t                     native_code_in = 0
)
{
    return wse::oui::RendererError(
        category_in, code_in, message_in, native_code_in );
}

wse::oui::RendererError backendError(
      const std::string& message_in
    , const VkResult     result_in
)
{
    const bool device_lost = result_in == VK_ERROR_DEVICE_LOST;
    return makeError(
          wse::oui::eRendererErrorCategory::Backend
        , device_lost ? wse::oui::eRendererErrorCode::DeviceLost
                      : wse::oui::eRendererErrorCode::BackendFailure
        , message_in
        , static_cast< std::int64_t >( result_in )
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

VkFormat toNativeFormat( const wse::oui::eRendererPixelFormat format_in ) noexcept
{
    switch( format_in )
    {
        case wse::oui::eRendererPixelFormat::R8Unorm:
            return VK_FORMAT_R8_UNORM;
        case wse::oui::eRendererPixelFormat::Rgba8Unorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case wse::oui::eRendererPixelFormat::Bgra8Unorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case wse::oui::eRendererPixelFormat::Rgba16Float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case wse::oui::eRendererPixelFormat::Rgba16Unorm:
            return VK_FORMAT_R16G16B16A16_UNORM;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

wse::oui::eRendererPixelFormat fromNativeFormat( const VkFormat format_in ) noexcept
{
    switch( format_in )
    {
        case VK_FORMAT_R8_UNORM:
            return wse::oui::eRendererPixelFormat::R8Unorm;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return wse::oui::eRendererPixelFormat::Rgba8Unorm;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return wse::oui::eRendererPixelFormat::Bgra8Unorm;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return wse::oui::eRendererPixelFormat::Rgba16Float;
        case VK_FORMAT_R16G16B16A16_UNORM:
            return wse::oui::eRendererPixelFormat::Rgba16Unorm;
        default:
            return wse::oui::eRendererPixelFormat::Unknown;
    }
}

VkImageLayout toNativeLayout( const wse::oui::eTextureState state_in ) noexcept
{
    switch( state_in )
    {
        case wse::oui::eTextureState::RenderTarget:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case wse::oui::eTextureState::ShaderResource:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case wse::oui::eTextureState::CopySource:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case wse::oui::eTextureState::CopyDestination:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case wse::oui::eTextureState::Present:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case wse::oui::eTextureState::Common:
            return VK_IMAGE_LAYOUT_GENERAL;
        case wse::oui::eTextureState::Undefined:
        default:
            return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkAccessFlags accessForLayout( const VkImageLayout layout_in ) noexcept
{
    switch( layout_in )
    {
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        default:
            return 0U;
    }
}

VkPipelineStageFlags stageForLayout( const VkImageLayout layout_in ) noexcept
{
    switch( layout_in )
    {
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

void transitionImage(
      const VkCommandBuffer command_in
    , const VkImage         image_in
    , const VkImageLayout   before_in
    , const VkImageLayout   after_in
)
{
    if( before_in == after_in )
    {
        return;
    }
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = accessForLayout( before_in );
    barrier.dstAccessMask = accessForLayout( after_in );
    barrier.oldLayout = before_in;
    barrier.newLayout = after_in;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_in;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0U;
    barrier.subresourceRange.levelCount = 1U;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = 1U;
    vkCmdPipelineBarrier(
          command_in
        , stageForLayout( before_in )
        , stageForLayout( after_in )
        , 0U
        , 0U
        , nullptr
        , 0U
        , nullptr
        , 1U
        , &barrier
    );
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

std::string drmConnectorName( const drmModeConnector& connector_in )
{
    const char* type = "Unknown";
    switch( connector_in.connector_type )
    {
        case DRM_MODE_CONNECTOR_VGA: type = "VGA"; break;
        case DRM_MODE_CONNECTOR_DVII: type = "DVI-I"; break;
        case DRM_MODE_CONNECTOR_DVID: type = "DVI-D"; break;
        case DRM_MODE_CONNECTOR_DVIA: type = "DVI-A"; break;
        case DRM_MODE_CONNECTOR_Composite: type = "Composite"; break;
        case DRM_MODE_CONNECTOR_SVIDEO: type = "S-Video"; break;
        case DRM_MODE_CONNECTOR_LVDS: type = "LVDS"; break;
        case DRM_MODE_CONNECTOR_Component: type = "Component"; break;
        case DRM_MODE_CONNECTOR_9PinDIN: type = "DIN"; break;
        case DRM_MODE_CONNECTOR_DisplayPort: type = "DP"; break;
        case DRM_MODE_CONNECTOR_HDMIA: type = "HDMI-A"; break;
        case DRM_MODE_CONNECTOR_HDMIB: type = "HDMI-B"; break;
        case DRM_MODE_CONNECTOR_TV: type = "TV"; break;
        case DRM_MODE_CONNECTOR_eDP: type = "eDP"; break;
        case DRM_MODE_CONNECTOR_VIRTUAL: type = "Virtual"; break;
        case DRM_MODE_CONNECTOR_DSI: type = "DSI"; break;
        case DRM_MODE_CONNECTOR_DPI: type = "DPI"; break;
        default: break;
    }
    return std::string( type ) + "-" + std::to_string( connector_in.connector_type_id );
}

wse::oui::sDisplayMode toPortableDrmMode( const drmModeModeInfo& mode_in )
{
    wse::oui::sDisplayMode result;
    result.extent = { mode_in.hdisplay, mode_in.vdisplay };
    result.refresh_rate_numerator = mode_in.vrefresh;
    result.refresh_rate_denominator = 1U;
    result.format = wse::oui::eRendererPixelFormat::Bgra8Unorm;
    result.interlaced = ( mode_in.flags & DRM_MODE_FLAG_INTERLACE ) != 0U;
    return result;
}

} // namespace

namespace wse
{
namespace oui
{
namespace internal
{

class VulkanRendererBackend final : public RendererBackend
{
    friend struct VulkanRendererBackendTestAccess;
  private:
    struct sTextureRecord
    {
        VkImage             image;
        VkDeviceMemory      memory;
        VkImageView         view;
        sTextureDescription description;
        eTextureState       state;
        VkImageLayout       layout;
        std::uint64_t       surface_owner;
        bool                owns_image;

        //! @brief Construct all members with explicit defaults.
        sTextureRecord(
              const VkImage& image_in = VK_NULL_HANDLE
            , const VkDeviceMemory& memory_in = VK_NULL_HANDLE
            , const VkImageView& view_in = VK_NULL_HANDLE
            , const sTextureDescription& description_in = {}
            , eTextureState state_in = eTextureState::Undefined
            , const VkImageLayout& layout_in = VK_IMAGE_LAYOUT_UNDEFINED
            , std::uint64_t surface_owner_in = 0U
            , bool owns_image_in = true
        )
            : image         ( image_in )
            , memory        ( memory_in )
            , view          ( view_in )
            , description   ( description_in )
            , state         ( state_in )
            , layout        ( layout_in )
            , surface_owner ( surface_owner_in )
            , owns_image    ( owns_image_in )
        {
        }
    };

    struct sMeshRecord
    {
        VkBuffer           vertex_buffer;
        VkDeviceMemory     vertex_memory;
        VkBuffer           index_buffer;
        VkDeviceMemory     index_memory;
        ePrimitiveTopology topology;
        std::uint32_t      index_count;

        //! @brief Construct all members with explicit defaults.
        sMeshRecord(
              const VkBuffer& vertex_buffer_in = VK_NULL_HANDLE
            , const VkDeviceMemory& vertex_memory_in = VK_NULL_HANDLE
            , const VkBuffer& index_buffer_in = VK_NULL_HANDLE
            , const VkDeviceMemory& index_memory_in = VK_NULL_HANDLE
            , ePrimitiveTopology topology_in = ePrimitiveTopology::TriangleList
            , std::uint32_t index_count_in = 0U
        )
            : vertex_buffer ( vertex_buffer_in )
            , vertex_memory ( vertex_memory_in )
            , index_buffer  ( index_buffer_in )
            , index_memory  ( index_memory_in )
            , topology      ( topology_in )
            , index_count   ( index_count_in )
        {
        }
    };

    struct sDrmRestoreState
    {
        int             fd;
        std::string     node;
        std::uint32_t   connector_id;
        std::uint32_t   crtc_id;
        std::uint32_t   buffer_id;
        std::uint32_t   x;
        std::uint32_t   y;
        drmModeModeInfo mode;
        bool            mode_valid;
        VkDisplayKHR    display;
        bool            acquired;

        //! @brief Construct all members with explicit defaults.
        sDrmRestoreState(
              int fd_in = -1
            , const std::string& node_in = {}
            , std::uint32_t connector_id_in = 0U
            , std::uint32_t crtc_id_in = 0U
            , std::uint32_t buffer_id_in = 0U
            , std::uint32_t x_in = 0U
            , std::uint32_t y_in = 0U
            , const drmModeModeInfo& mode_in = {}
            , bool mode_valid_in = false
            , const VkDisplayKHR& display_in = VK_NULL_HANDLE
            , bool acquired_in = false
        )
            : fd           ( fd_in )
            , node         ( node_in )
            , connector_id ( connector_id_in )
            , crtc_id      ( crtc_id_in )
            , buffer_id    ( buffer_id_in )
            , x            ( x_in )
            , y            ( y_in )
            , mode         ( mode_in )
            , mode_valid   ( mode_valid_in )
            , display      ( display_in )
            , acquired     ( acquired_in )
        {
        }
    };

    struct sSurfaceRecord
    {
        sSurfaceDescription description;
        std::vector< sTextureHandle > textures;
        std::uint32_t current_image;
        VkSurfaceKHR   native_surface;
        VkSwapchainKHR swap_chain;
        VkSemaphore    acquire_semaphore;
        bool           acquire_pending;
        std::uint32_t  present_family;
        std::unique_ptr< sWaylandWindow > window;
        eSurfaceWindowMode window_mode;
        std::string        display_id;
        sDisplayMode       display_mode;
        sDrmRestoreState   drm;

        //! @brief Construct all members with explicit defaults.
        sSurfaceRecord(
              const sSurfaceDescription& description_in = {}
            , const std::vector< sTextureHandle >& textures_in = {}
            , std::uint32_t current_image_in = 0U
            , const VkSurfaceKHR& native_surface_in = VK_NULL_HANDLE
            , const VkSwapchainKHR& swap_chain_in = VK_NULL_HANDLE
            , const VkSemaphore& acquire_semaphore_in = VK_NULL_HANDLE
            , bool acquire_pending_in = false
            , std::uint32_t present_family_in = 0U
            , std::unique_ptr< sWaylandWindow > window_in = {}
            , eSurfaceWindowMode window_mode_in = eSurfaceWindowMode::NotApplicable
            , const std::string& display_id_in = {}
            , const sDisplayMode& display_mode_in = {}
            , const sDrmRestoreState& drm_in = {}
        )
            : description       ( description_in )
            , textures          ( textures_in )
            , current_image     ( current_image_in )
            , native_surface    ( native_surface_in )
            , swap_chain        ( swap_chain_in )
            , acquire_semaphore ( acquire_semaphore_in )
            , acquire_pending   ( acquire_pending_in )
            , present_family    ( present_family_in )
            , window            ( std::move( window_in ) )
            , window_mode       ( window_mode_in )
            , display_id        ( display_id_in )
            , display_mode      ( display_mode_in )
            , drm               ( drm_in )
        {
        }
    };

    struct sDrawParameters
    {
        std::uint32_t flags;
        float opacity;
        float edge_left;
        float edge_right;
        float edge_top;
        float edge_bottom;

        //! @brief Construct all members with explicit defaults.
        sDrawParameters(
              std::uint32_t flags_in = 0U
            , float opacity_in = 1.0F
            , float edge_left_in = 0.0F
            , float edge_right_in = 0.0F
            , float edge_top_in = 0.0F
            , float edge_bottom_in = 0.0F
        )
            : flags       ( flags_in )
            , opacity     ( opacity_in )
            , edge_left   ( edge_left_in )
            , edge_right  ( edge_right_in )
            , edge_top    ( edge_top_in )
            , edge_bottom ( edge_bottom_in )
        {
        }
    };

    VkInstance       m_instance;
    VkPhysicalDevice m_physical_device;
    VkDevice         m_device;
    VkPhysicalDeviceProperties m_physical_properties;
    std::vector< VkQueueFamilyProperties > m_queue_properties;
    std::vector< VkQueue > m_queues;
    std::uint32_t     m_graphics_family;
    VkQueue           m_graphics_queue;
    VkCommandPool     m_command_pool;
    VkDescriptorSetLayout m_descriptor_layout;
    VkPipelineLayout  m_pipeline_layout;
    VkSampler         m_nearest_sampler;
    VkSampler         m_linear_sampler;
    VkShaderModule    m_vertex_shader;
    VkShaderModule    m_fragment_shader;
    std::unordered_map< std::uint32_t, VkRenderPass > m_render_passes;
    std::unordered_map< std::uint32_t, VkPipeline > m_pipelines;
    std::unique_ptr< WaylandConnection > m_wayland;
    PFN_vkGetDrmDisplayEXT m_get_drm_display;
    PFN_vkAcquireDrmDisplayEXT m_acquire_drm_display;
    PFN_vkReleaseDisplayEXT m_release_display;
    bool m_has_wayland_surface;
    bool m_has_display_surface;
    bool m_has_swapchain;
    bool m_initialized;
    std::uint32_t m_handle_generation = 0U;
    std::uint64_t m_next_resource_id;
    std::uint64_t m_last_fence_id;
    sRendererCapabilities m_capabilities;
    std::unordered_map< std::uint64_t, sTextureRecord > m_textures;
    std::unordered_map< std::uint64_t, sMeshRecord > m_meshes;
    std::unordered_map< std::uint64_t, sSurfaceRecord > m_surfaces;
    std::unordered_set< std::uint64_t > m_completed_fences;

    sVulkanSubmissionCalls m_submission_calls;
    std::unique_ptr< sVulkanSubmission > m_pending_submission;
    VkResult m_terminal_submission_error = VK_SUCCESS;

    RendererError pollPendingSubmission();
    void waitDeviceIdleForRelease() noexcept;
    RendererError createBuffer(
          VkBuffer* p_buffer_out
        , VkDeviceMemory* p_memory_out
        , VkDeviceSize size_in
        , VkBufferUsageFlags usage_in
        , VkMemoryPropertyFlags memory_flags_in
    );
    RendererError writeMemory(
          VkDeviceMemory memory_in
        , const void*    p_data_in
        , VkDeviceSize   size_in
    );
    RendererError findMemoryType(
          std::uint32_t* p_index_out
        , std::uint32_t type_bits_in
        , VkMemoryPropertyFlags flags_in
    ) const;
    RendererResult< VkCommandBuffer > beginCommands();
    VkResult submitAndWait(
          std::unique_ptr< sVulkanSubmission >& work_inout
        , bool* p_submitted_out
        , VkSemaphore wait_semaphore_in = VK_NULL_HANDLE
        , std::uint32_t timeout_ms_in = GPU_TIMEOUT_MS
    ) noexcept;
    RendererResult< sFenceHandle > submissionResult( VkResult result_in );
    RendererResult< sFenceHandle > completedFence();
    RendererError initializePipelineResources();
    VkRenderPass findOrCreateRenderPass( VkFormat format_in, bool clear_in );
    VkPipeline findOrCreatePipeline(
          VkFormat           format_in
        , eColorBlendMode    blend_in
        , ePrimitiveTopology topology_in
    );
    sTextureRecord* findTexture( sTextureHandle texture_in ) noexcept;
    const sTextureRecord* findTexture( sTextureHandle texture_in ) const noexcept;
    sMeshRecord* findMesh( sMeshHandle mesh_in ) noexcept;
    const sMeshRecord* findMesh( sMeshHandle mesh_in ) const noexcept;
    sSurfaceRecord* findSurface( sSurfaceHandle surface_in ) noexcept;
    const sSurfaceRecord* findSurface( sSurfaceHandle surface_in ) const noexcept;
    void destroyTextureRecord( sTextureRecord* p_texture_inout ) noexcept;
    void destroyMeshRecord( sMeshRecord* p_mesh_inout ) noexcept;
    void destroySwapchainResources( sSurfaceRecord* p_surface_inout ) noexcept;
    RendererError restoreDrmDisplay( sSurfaceRecord* p_surface_inout ) noexcept;
    RendererError createSwapchain(
          sSurfaceRecord* p_surface_inout
        , std::uint64_t surface_id_in
        , sRendererExtent2D requested_extent_in
    );
    RendererError recreateSwapchain(
          sSurfaceRecord* p_surface_inout
        , std::uint64_t surface_id_in
        , sRendererExtent2D requested_extent_in
    );
    RendererResult< sSurfaceHandle > createOffscreenSurface(
        const sSurfaceDescription& description_in );
    RendererResult< sSurfaceHandle > createWindowSurface(
        const sSurfaceDescription& description_in );
    RendererResult< sSurfaceHandle > createDirectDisplaySurface(
        const sSurfaceDescription& description_in );
    std::vector< sDisplayDescription > enumerateDrmDisplays() const;

  public:
    VulkanRendererBackend() noexcept;
    ~VulkanRendererBackend() override;

    RendererStatus initialize( const sRendererConfiguration& configuration_in ) override;
    void shutdown() noexcept override;
    bool isInitialized() const noexcept override;
    sRendererCapabilities getCapabilities() const noexcept override;
    RendererResult< std::vector< sDisplayDescription > > enumerateDisplays() const override;
    RendererResult< sTextureHandle > createTexture(
        const sTextureDescription& description_in ) override;
    RendererStatus destroyTexture( sTextureHandle texture_in ) override;
    RendererResult< sFenceHandle > uploadTexture(
          sTextureHandle texture_in
        , const sRendererFrame& frame_in ) override;
    RendererResult< sMeshHandle > createMesh(
        const sMeshDescription& description_in ) override;
    RendererStatus updateMesh(
          sMeshHandle mesh_in
        , const sMeshDescription& description_in ) override;
    RendererStatus destroyMesh( sMeshHandle mesh_in ) override;
    RendererResult< sSurfaceHandle > createSurface(
        const sSurfaceDescription& description_in ) override;
    RendererResult< sTextureHandle > getSurfaceTexture(
        sSurfaceHandle surface_in ) const override;
    RendererResult< sSurfaceState > getSurfaceState(
        sSurfaceHandle surface_in ) const override;
    RendererStatus resizeSurface(
          sSurfaceHandle surface_in
        , sRendererExtent2D extent_in ) override;
    RendererStatus setSurfaceWindowMode(
          sSurfaceHandle surface_in
        , const sSurfaceWindowModeRequest& request_in ) override;
    RendererResult< sSurfaceEvents > pollSurfaceEvents(
        sSurfaceHandle surface_in ) override;
    RendererStatus destroySurface( sSurfaceHandle surface_in ) override;
    RendererResult< bool > processSurfaceEvents(
        sSurfaceHandle surface_in ) override;
    RendererResult< sFenceHandle > presentSurface(
        sSurfaceHandle surface_in ) override;
    RendererResult< sFenceHandle > executeRenderPass(
        const sRenderPassDescription& description_in ) override;
    RendererStatus waitFence(
          sFenceHandle fence_in
        , std::uint32_t timeout_ms_in ) override;
    RendererResult< sRendererFrame > readTexture(
          sTextureHandle texture_in
        , std::uint32_t timeout_ms_in ) override;
};

VulkanRendererBackend::VulkanRendererBackend() noexcept
    : m_instance            ( VK_NULL_HANDLE )
    , m_physical_device     ( VK_NULL_HANDLE )
    , m_device              ( VK_NULL_HANDLE )
    , m_physical_properties ()
    , m_queue_properties    ()
    , m_queues              ()
    , m_graphics_family     ( 0U )
    , m_graphics_queue      ( VK_NULL_HANDLE )
    , m_command_pool        ( VK_NULL_HANDLE )
    , m_descriptor_layout   ( VK_NULL_HANDLE )
    , m_pipeline_layout     ( VK_NULL_HANDLE )
    , m_nearest_sampler     ( VK_NULL_HANDLE )
    , m_linear_sampler      ( VK_NULL_HANDLE )
    , m_vertex_shader       ( VK_NULL_HANDLE )
    , m_fragment_shader     ( VK_NULL_HANDLE )
    , m_render_passes       ()
    , m_pipelines           ()
    , m_wayland             ()
    , m_get_drm_display     ( nullptr )
    , m_acquire_drm_display ( nullptr )
    , m_release_display     ( nullptr )
    , m_has_wayland_surface ( false )
    , m_has_display_surface ( false )
    , m_has_swapchain       ( false )
    , m_initialized         ( false )
    , m_next_resource_id    ( 1U )
    , m_last_fence_id       ( 0U )
    , m_capabilities        ()
    , m_textures            ()
    , m_meshes              ()
    , m_surfaces            ()
    , m_completed_fences    ()
{
}

VulkanRendererBackend::~VulkanRendererBackend()
{
    this->shutdown();
}

RendererStatus VulkanRendererBackend::initialize(
    const sRendererConfiguration& configuration_in )
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

    if( configuration_in.backend != eRendererBackend::Automatic &&
        configuration_in.backend != eRendererBackend::Vulkan12 )
    {
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedBackend
                , "Linux OUI supports Vulkan 1.2 only."
            )
        );
    }

    std::uint32_t extension_count = 0U;
    VkResult native_result = vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_count, nullptr );
    if( native_result != VK_SUCCESS )
    {
        return RendererStatus::failure( backendError( "Failed to enumerate Vulkan instance extensions.", native_result ) );
    }
    std::vector< VkExtensionProperties > extension_properties( extension_count );
    native_result = vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_count, extension_properties.data() );
    if( native_result != VK_SUCCESS )
    {
        return RendererStatus::failure( backendError( "Failed to read Vulkan instance extensions.", native_result ) );
    }
    const auto has_instance_extension = [&extension_properties]( const char* const p_name_in )
    {
        return std::any_of(
              extension_properties.begin()
            , extension_properties.end()
            , [p_name_in]( const VkExtensionProperties& extension_in )
              {
                  return std::strcmp( extension_in.extensionName, p_name_in ) == 0;
              }
        );
    };
    std::vector< const char* > enabled_extensions;
    const bool has_surface = has_instance_extension( VK_KHR_SURFACE_EXTENSION_NAME );
    if( has_surface )
    {
        enabled_extensions.emplace_back( VK_KHR_SURFACE_EXTENSION_NAME );
    }
    this->m_has_wayland_surface = has_surface &&
        has_instance_extension( VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME );
    if( this->m_has_wayland_surface )
    {
        enabled_extensions.emplace_back( VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME );
    }
    this->m_has_display_surface = has_surface &&
        has_instance_extension( VK_KHR_DISPLAY_EXTENSION_NAME );
    if( this->m_has_display_surface )
    {
        enabled_extensions.emplace_back( VK_KHR_DISPLAY_EXTENSION_NAME );
    }
    const bool has_acquire_drm = this->m_has_display_surface &&
        has_instance_extension( VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME );
    const bool has_direct_mode = this->m_has_display_surface &&
        has_instance_extension( VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME );
    if( has_acquire_drm )
    {
        enabled_extensions.emplace_back( VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME );
    }
    if( has_direct_mode )
    {
        enabled_extensions.emplace_back( VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME );
    }

    std::vector< const char* > enabled_layers;
    if( configuration_in.enable_validation )
    {
        std::uint32_t layer_count = 0U;
        vkEnumerateInstanceLayerProperties( &layer_count, nullptr );
        std::vector< VkLayerProperties > layers( layer_count );
        vkEnumerateInstanceLayerProperties( &layer_count, layers.data() );
        const char* const validation_layer = "VK_LAYER_KHRONOS_validation";
        const bool has_validation = std::any_of(
              layers.begin()
            , layers.end()
            , [validation_layer]( const VkLayerProperties& layer_in )
              {
                  return std::strcmp( layer_in.layerName, validation_layer ) == 0;
              }
        );
        if( !has_validation )
        {
            return RendererStatus::failure( unsupportedError(
                      eRendererErrorCode::UnsupportedOperation
                    , "Vulkan validation was requested but VK_LAYER_KHRONOS_validation is unavailable."
                )
            );
        }
        enabled_layers.emplace_back( validation_layer );
    }

    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "WonderStewEngine OUI";
    application.applicationVersion = VK_MAKE_API_VERSION( 0U, 0U, 1U, 0U );
    application.pEngineName = "WonderStewEngine";
    application.engineVersion = VK_MAKE_API_VERSION( 0U, 0U, 1U, 0U );
    application.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instance_description = {};
    instance_description.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_description.pApplicationInfo = &application;
    instance_description.enabledExtensionCount =
        static_cast< std::uint32_t >( enabled_extensions.size() );
    instance_description.ppEnabledExtensionNames = enabled_extensions.data();
    instance_description.enabledLayerCount =
        static_cast< std::uint32_t >( enabled_layers.size() );
    instance_description.ppEnabledLayerNames = enabled_layers.data();
    native_result = vkCreateInstance( &instance_description, nullptr, &this->m_instance );
    if( native_result != VK_SUCCESS )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the Vulkan 1.2 instance.", native_result ) );
    }

    std::uint32_t physical_count = 0U;
    native_result = vkEnumeratePhysicalDevices( this->m_instance, &physical_count, nullptr );
    if( native_result != VK_SUCCESS || physical_count == 0U )
    {
        this->shutdown();
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceExhausted
                , "No Vulkan physical device is available."
                , native_result
            )
        );
    }
    std::vector< VkPhysicalDevice > physical_devices( physical_count );
    vkEnumeratePhysicalDevices( this->m_instance, &physical_count, physical_devices.data() );
    // 指名Adapterは大文字小文字を区別しない部分一致で照合する。
    std::string requested_adapter = configuration_in.adapter_name;
    for( char& character : requested_adapter )
        character = static_cast< char >( std::tolower( static_cast< unsigned char >( character ) ) );

    int best_score = ( std::numeric_limits< int >::min )();
    for( const VkPhysicalDevice candidate : physical_devices )
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties( candidate, &properties );

        if( !requested_adapter.empty() )
        {
            std::string device_name = properties.deviceName;
            for( char& character : device_name )
                character = static_cast< char >(
                    std::tolower( static_cast< unsigned char >( character ) ) );
            if( device_name.find( requested_adapter ) == std::string::npos )
                continue;
        }
        if( VK_VERSION_MAJOR( properties.apiVersion ) < 1U ||
            ( VK_VERSION_MAJOR( properties.apiVersion ) == 1U &&
              VK_VERSION_MINOR( properties.apiVersion ) < 2U ) )
        {
            continue;
        }
        std::uint32_t family_count = 0U;
        vkGetPhysicalDeviceQueueFamilyProperties( candidate, &family_count, nullptr );
        std::vector< VkQueueFamilyProperties > families( family_count );
        vkGetPhysicalDeviceQueueFamilyProperties( candidate, &family_count, families.data() );
        const bool has_graphics = std::any_of(
              families.begin()
            , families.end()
            , []( const VkQueueFamilyProperties& family_in )
              {
                  return family_in.queueCount > 0U &&
                      ( family_in.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0U;
              }
        );
        if( !has_graphics )
        {
            continue;
        }
        int score = 0;
        if( properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) score = 300;
        else if( properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) score = 200;
        else if( properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ) score = 100;
        if( configuration_in.use_software_adapter )
        {
            score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 1000 : score;
        }
        else if( properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU )
        {
            score = 10;
        }
        if( score > best_score )
        {
            best_score = score;
            this->m_physical_device = candidate;
            this->m_physical_properties = properties;
            this->m_queue_properties = std::move( families );
        }
    }
    if( this->m_physical_device == VK_NULL_HANDLE )
    {
        this->shutdown();
        if( !requested_adapter.empty() )
        {
            // 指名したAdapterが無いまま別のAdapterを使うほうが危険であるため、明示的に失敗させる。
            return RendererStatus::failure( unsupportedError(
                      eRendererErrorCode::ResourceNotFound
                    , "No Vulkan device matches the requested adapter name."
                )
            );
        }
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedBackend
                , "No Vulkan 1.2 graphics device is available."
            )
        );
    }

    std::uint32_t device_extension_count = 0U;
    vkEnumerateDeviceExtensionProperties(
        this->m_physical_device, nullptr, &device_extension_count, nullptr );
    std::vector< VkExtensionProperties > device_extensions( device_extension_count );
    vkEnumerateDeviceExtensionProperties(
          this->m_physical_device
        , nullptr
        , &device_extension_count
        , device_extensions.data()
    );
    this->m_has_swapchain = std::any_of(
          device_extensions.begin()
        , device_extensions.end()
        , []( const VkExtensionProperties& extension_in )
          {
              return std::strcmp(
                  extension_in.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0;
          }
    );
    std::vector< const char* > device_extension_names;
    if( this->m_has_swapchain )
    {
        device_extension_names.emplace_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
    }
    const float queue_priority = 1.0F;
    std::vector< VkDeviceQueueCreateInfo > queue_descriptions;
    bool graphics_family_found = false;
    for( std::uint32_t index = 0U;
         index < static_cast< std::uint32_t >( this->m_queue_properties.size() ); ++index )
    {
        if( this->m_queue_properties[ index ].queueCount == 0U )
        {
            continue;
        }
        VkDeviceQueueCreateInfo queue_description = {};
        queue_description.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_description.queueFamilyIndex = index;
        queue_description.queueCount = 1U;
        queue_description.pQueuePriorities = &queue_priority;
        queue_descriptions.emplace_back( queue_description );
        if( !graphics_family_found &&
            ( this->m_queue_properties[ index ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0U )
        {
            this->m_graphics_family = index;
            graphics_family_found = true;
        }
    }
    VkDeviceCreateInfo device_description = {};
    device_description.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_description.queueCreateInfoCount =
        static_cast< std::uint32_t >( queue_descriptions.size() );
    device_description.pQueueCreateInfos = queue_descriptions.data();
    device_description.enabledExtensionCount =
        static_cast< std::uint32_t >( device_extension_names.size() );
    device_description.ppEnabledExtensionNames = device_extension_names.data();
    native_result = vkCreateDevice(
        this->m_physical_device, &device_description, nullptr, &this->m_device );
    if( native_result != VK_SUCCESS )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the Vulkan logical device.", native_result ) );
    }
    this->m_queues.resize( this->m_queue_properties.size(), VK_NULL_HANDLE );
    for( std::uint32_t index = 0U;
         index < static_cast< std::uint32_t >( this->m_queue_properties.size() ); ++index )
    {
        if( this->m_queue_properties[ index ].queueCount > 0U )
        {
            vkGetDeviceQueue( this->m_device, index, 0U, &this->m_queues[ index ] );
        }
    }
    this->m_graphics_queue = this->m_queues[ this->m_graphics_family ];

    VkCommandPoolCreateInfo pool_description = {};
    pool_description.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_description.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_description.queueFamilyIndex = this->m_graphics_family;
    native_result = vkCreateCommandPool(
        this->m_device, &pool_description, nullptr, &this->m_command_pool );
    if( native_result != VK_SUCCESS )
    {
        this->shutdown();
        return RendererStatus::failure( backendError( "Failed to create the Vulkan command pool.", native_result ) );
    }
    const RendererError pipeline_error = this->initializePipelineResources();
    if( !pipeline_error.ok() )
    {
        this->shutdown();
        return RendererStatus::failure( pipeline_error );
    }

    this->m_get_drm_display = reinterpret_cast< PFN_vkGetDrmDisplayEXT >(
        vkGetInstanceProcAddr( this->m_instance, "vkGetDrmDisplayEXT" ) );
    this->m_acquire_drm_display = reinterpret_cast< PFN_vkAcquireDrmDisplayEXT >(
        vkGetInstanceProcAddr( this->m_instance, "vkAcquireDrmDisplayEXT" ) );
    this->m_release_display = reinterpret_cast< PFN_vkReleaseDisplayEXT >(
        vkGetInstanceProcAddr( this->m_instance, "vkReleaseDisplayEXT" ) );
    if( !has_acquire_drm || !has_direct_mode )
    {
        this->m_get_drm_display = nullptr;
        this->m_acquire_drm_display = nullptr;
        this->m_release_display = nullptr;
    }

    this->m_wayland = std::make_unique< WaylandConnection >();
    std::string wayland_error;
    if( !this->m_has_wayland_surface ||
        !this->m_wayland->initialize( &wayland_error ) )
    {
        this->m_wayland.reset();
    }

    this->m_capabilities.backend = eRendererBackend::Vulkan12;
    this->m_capabilities.adapter_name = this->m_physical_properties.deviceName;
    this->m_capabilities.maximum_texture_extent =
        this->m_physical_properties.limits.maxImageDimension2D;
    this->m_capabilities.supports_offscreen = true;
    this->m_capabilities.supports_window = this->m_wayland != nullptr && this->m_has_swapchain;
    this->m_capabilities.supports_direct_display = this->m_has_swapchain &&
        this->m_get_drm_display != nullptr && this->m_acquire_drm_display != nullptr &&
        this->m_release_display != nullptr;
    this->m_capabilities.supports_mesh_rendering = true;
    this->m_capabilities.supports_display_enumeration = true;
    this->m_capabilities.supports_surface_resize = true;
    this->m_capabilities.supports_borderless_fullscreen =
        this->m_capabilities.supports_window;
    this->m_capabilities.supports_display_mode_fullscreen = false;
    this->m_capabilities.supports_interactive_resize =
        this->m_capabilities.supports_window;
    this->m_capabilities.supports_display_hotplug =
        this->m_capabilities.supports_window;
    this->m_initialized = true;
    return RendererStatus::success();
}

void VulkanRendererBackend::shutdown() noexcept
{
    if( this->m_device != VK_NULL_HANDLE )
    {
        this->waitDeviceIdleForRelease();
    }
    this->m_pending_submission.reset();
    this->m_terminal_submission_error = VK_SUCCESS;
    for( auto& surface_pair : this->m_surfaces )
    {
        sSurfaceRecord& surface = surface_pair.second;
        this->destroySwapchainResources( &surface );
        if( surface.native_surface != VK_NULL_HANDLE && this->m_instance != VK_NULL_HANDLE )
        {
            vkDestroySurfaceKHR( this->m_instance, surface.native_surface, nullptr );
            surface.native_surface = VK_NULL_HANDLE;
        }
        this->restoreDrmDisplay( &surface );
        if( this->m_wayland != nullptr && surface.window != nullptr )
        {
            this->m_wayland->destroyWindow( &surface.window );
        }
    }
    this->m_surfaces.clear();
    for( auto& texture_pair : this->m_textures )
    {
        this->destroyTextureRecord( &texture_pair.second );
    }
    this->m_textures.clear();
    for( auto& mesh_pair : this->m_meshes )
    {
        this->destroyMeshRecord( &mesh_pair.second );
    }
    this->m_meshes.clear();
    if( this->m_device != VK_NULL_HANDLE )
    {
        for( auto& pipeline_pair : this->m_pipelines )
        {
            vkDestroyPipeline( this->m_device, pipeline_pair.second, nullptr );
        }
        for( auto& pass_pair : this->m_render_passes )
        {
            vkDestroyRenderPass( this->m_device, pass_pair.second, nullptr );
        }
        if( this->m_vertex_shader != VK_NULL_HANDLE )
            vkDestroyShaderModule( this->m_device, this->m_vertex_shader, nullptr );
        if( this->m_fragment_shader != VK_NULL_HANDLE )
            vkDestroyShaderModule( this->m_device, this->m_fragment_shader, nullptr );
        if( this->m_nearest_sampler != VK_NULL_HANDLE )
            vkDestroySampler( this->m_device, this->m_nearest_sampler, nullptr );
        if( this->m_linear_sampler != VK_NULL_HANDLE )
            vkDestroySampler( this->m_device, this->m_linear_sampler, nullptr );
        if( this->m_pipeline_layout != VK_NULL_HANDLE )
            vkDestroyPipelineLayout( this->m_device, this->m_pipeline_layout, nullptr );
        if( this->m_descriptor_layout != VK_NULL_HANDLE )
            vkDestroyDescriptorSetLayout( this->m_device, this->m_descriptor_layout, nullptr );
        if( this->m_command_pool != VK_NULL_HANDLE )
            vkDestroyCommandPool( this->m_device, this->m_command_pool, nullptr );
        vkDestroyDevice( this->m_device, nullptr );
    }
    this->m_pipelines.clear();
    this->m_render_passes.clear();
    this->m_vertex_shader = VK_NULL_HANDLE;
    this->m_fragment_shader = VK_NULL_HANDLE;
    this->m_nearest_sampler = VK_NULL_HANDLE;
    this->m_linear_sampler = VK_NULL_HANDLE;
    this->m_pipeline_layout = VK_NULL_HANDLE;
    this->m_descriptor_layout = VK_NULL_HANDLE;
    this->m_command_pool = VK_NULL_HANDLE;
    this->m_graphics_queue = VK_NULL_HANDLE;
    this->m_queues.clear();
    this->m_device = VK_NULL_HANDLE;
    if( this->m_wayland != nullptr )
    {
        this->m_wayland->shutdown();
        this->m_wayland.reset();
    }
    if( this->m_instance != VK_NULL_HANDLE )
    {
        vkDestroyInstance( this->m_instance, nullptr );
        this->m_instance = VK_NULL_HANDLE;
    }
    this->m_physical_device = VK_NULL_HANDLE;
    this->m_get_drm_display = nullptr;
    this->m_acquire_drm_display = nullptr;
    this->m_release_display = nullptr;
    this->m_completed_fences.clear();
    this->m_last_fence_id = 0U;
    this->m_handle_generation = 0U;
    this->m_next_resource_id = 1U;
    this->m_capabilities = {};
    this->m_initialized = false;
}

bool VulkanRendererBackend::isInitialized() const noexcept
{
    return this->m_initialized;
}

sRendererCapabilities VulkanRendererBackend::getCapabilities() const noexcept
{
    return this->m_capabilities;
}

RendererError VulkanRendererBackend::findMemoryType(
      std::uint32_t* const        p_index_out
    , const std::uint32_t         type_bits_in
    , const VkMemoryPropertyFlags flags_in
) const
{
    VkPhysicalDeviceMemoryProperties properties = {};
    vkGetPhysicalDeviceMemoryProperties( this->m_physical_device, &properties );
    for( std::uint32_t index = 0U; index < properties.memoryTypeCount; ++index )
    {
        if( ( type_bits_in & ( 1U << index ) ) != 0U &&
            ( properties.memoryTypes[ index ].propertyFlags & flags_in ) == flags_in )
        {
            *p_index_out = index;
            return RendererError();
        }
    }
    return makeError(
          eRendererErrorCategory::Resource
        , eRendererErrorCode::ResourceExhausted
        , "No compatible Vulkan memory type is available."
    );
}

RendererError VulkanRendererBackend::createBuffer(
      VkBuffer* const             p_buffer_out
    , VkDeviceMemory* const       p_memory_out
    , const VkDeviceSize          size_in
    , const VkBufferUsageFlags    usage_in
    , const VkMemoryPropertyFlags memory_flags_in
)
{
    VkBufferCreateInfo description = {};
    description.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    description.size = size_in;
    description.usage = usage_in;
    description.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer( this->m_device, &description, nullptr, p_buffer_out );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create a Vulkan buffer.", result );
    }
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements( this->m_device, *p_buffer_out, &requirements );
    std::uint32_t memory_index = 0U;
    const RendererError memory_error = this->findMemoryType(
        &memory_index, requirements.memoryTypeBits, memory_flags_in );
    if( !memory_error.ok() )
    {
        vkDestroyBuffer( this->m_device, *p_buffer_out, nullptr );
        *p_buffer_out = VK_NULL_HANDLE;
        return memory_error;
    }
    VkMemoryAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_index;
    result = vkAllocateMemory( this->m_device, &allocation, nullptr, p_memory_out );
    if( result == VK_SUCCESS )
    {
        result = vkBindBufferMemory( this->m_device, *p_buffer_out, *p_memory_out, 0U );
    }
    if( result != VK_SUCCESS )
    {
        if( *p_memory_out != VK_NULL_HANDLE )
            vkFreeMemory( this->m_device, *p_memory_out, nullptr );
        vkDestroyBuffer( this->m_device, *p_buffer_out, nullptr );
        *p_memory_out = VK_NULL_HANDLE;
        *p_buffer_out = VK_NULL_HANDLE;
        return backendError( "Failed to allocate a Vulkan buffer.", result );
    }
    return RendererError();
}

RendererError VulkanRendererBackend::writeMemory(
      const VkDeviceMemory memory_in
    , const void* const    p_data_in
    , const VkDeviceSize   size_in
)
{
    void* p_mapped = nullptr;
    const VkResult result = vkMapMemory(
        this->m_device, memory_in, 0U, size_in, 0U, &p_mapped );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to map Vulkan host memory.", result );
    }
    std::memcpy( p_mapped, p_data_in, static_cast< std::size_t >( size_in ) );
    vkUnmapMemory( this->m_device, memory_in );
    return RendererError();
}

RendererResult< VkCommandBuffer > VulkanRendererBackend::beginCommands()
{
    VkCommandBufferAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = this->m_command_pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1U;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers( this->m_device, &allocation, &command );
    if( result != VK_SUCCESS )
    {
        return RendererResult< VkCommandBuffer >::failure(
            backendError( "Failed to allocate a Vulkan command buffer.", result ) );
    }
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer( command, &begin );
    if( result != VK_SUCCESS )
    {
        vkFreeCommandBuffers( this->m_device, this->m_command_pool, 1U, &command );
        return RendererResult< VkCommandBuffer >::failure(
            backendError( "Failed to begin a Vulkan command buffer.", result ) );
    }
    return RendererResult< VkCommandBuffer >::success( command );
}

RendererError VulkanRendererBackend::pollPendingSubmission()
{
    if( this->m_terminal_submission_error != VK_SUCCESS )
        return backendError( "Vulkan submission requires shutdown and reinitialization.",
            this->m_terminal_submission_error );
    if( this->m_pending_submission == nullptr ) return {};
    const VkResult result = this->m_submission_calls.wait_fences(
        this->m_device, 1U, &this->m_pending_submission->fence, VK_TRUE, 0U );
    if( result == VK_SUCCESS )
    {
        this->m_pending_submission.reset();
        return {};
    }
    if( result == VK_TIMEOUT )
        return makeError( eRendererErrorCategory::Lifecycle, eRendererErrorCode::ResourceInUse,
            "A previous Vulkan submission is still pending; retry after completion.", result );
    if( result == VK_ERROR_DEVICE_LOST ) this->m_terminal_submission_error = result;
    return backendError( "Failed to poll the previous Vulkan submission.", result );
}

void VulkanRendererBackend::waitDeviceIdleForRelease() noexcept
{
    // A failed idle call other than DeviceLost does not prove completion. Shutdown has
    // no finite-time guarantee: retain everything and retry instead of freeing in-use work.
    for( ;; )
    {
        const VkResult result = this->m_submission_calls.device_wait_idle( this->m_device );
        if( result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST ) return;
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
}

VkResult VulkanRendererBackend::submitAndWait(
      std::unique_ptr< sVulkanSubmission >& work_inout
    , bool* const p_submitted_out
    , const VkSemaphore wait_semaphore_in
    , const std::uint32_t timeout_ms_in
) noexcept
{
    bool& submitted = *p_submitted_out;
    submitted = false;
    sVulkanSubmission& work = *work_inout;
    VkResult result = this->m_submission_calls.end_command( work.command );
    if( result == VK_SUCCESS )
    {
        VkFenceCreateInfo fence_description = {};
        fence_description.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result = this->m_submission_calls.create_fence(
            this->m_device, &fence_description, nullptr, &work.fence );
    }
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submission = {};
    submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submission.commandBufferCount = 1U;
    submission.pCommandBuffers = &work.command;
    if( wait_semaphore_in != VK_NULL_HANDLE )
    {
        submission.waitSemaphoreCount = 1U;
        submission.pWaitSemaphores = &wait_semaphore_in;
        submission.pWaitDstStageMask = &wait_stage;
    }
    if( result == VK_SUCCESS )
    {
        result = this->m_submission_calls.queue_submit(
            this->m_graphics_queue, 1U, &submission, work.fence );
        submitted = result == VK_SUCCESS;
        // QueueSubmit OOM leaves resources and synchronization unchanged (Vulkan contract).
        // An unknown failure or DeviceLost is terminal; keep its owner until teardown.
        if( result != VK_SUCCESS && result != VK_ERROR_OUT_OF_HOST_MEMORY &&
            result != VK_ERROR_OUT_OF_DEVICE_MEMORY )
        {
            this->m_terminal_submission_error = result;
            this->m_pending_submission = std::move( work_inout );
        }
    }
    if( submitted )
    {
        result = this->m_submission_calls.wait_fences(
            this->m_device, 1U, &work.fence, VK_TRUE,
            static_cast< std::uint64_t >( timeout_ms_in ) * 1000000ULL );
        if( result != VK_SUCCESS )
        {
            // This move cannot allocate, even when the driver reports host OOM.
            this->m_pending_submission = std::move( work_inout );
            if( result == VK_ERROR_DEVICE_LOST ) this->m_terminal_submission_error = result;
        }
    }
    if( result == VK_ERROR_DEVICE_LOST ) this->m_terminal_submission_error = result;
    return result;
}

RendererResult< sFenceHandle > VulkanRendererBackend::submissionResult( const VkResult result_in )
{
    const VkResult result = result_in;
    if( result == VK_TIMEOUT )
        return RendererResult< sFenceHandle >::failure( makeError(
            eRendererErrorCategory::Timeout, eRendererErrorCode::TimedOut,
            "Vulkan submission timed out; resources are retained until completion.", result ) );
    if( result != VK_SUCCESS )
        return RendererResult< sFenceHandle >::failure(
            backendError( "Vulkan command submission failed.", result ) );
    return this->completedFence();
}

RendererResult< sFenceHandle > VulkanRendererBackend::completedFence()
{
    const std::uint64_t id = ++this->m_last_fence_id;
    this->m_completed_fences.emplace( id );
    return RendererResult< sFenceHandle >::success( sFenceHandle{ id, this->m_handle_generation } );
}

RendererError VulkanRendererBackend::initializePipelineResources()
{
    VkDescriptorSetLayoutBinding bindings[ 2U ] = {};
    for( std::uint32_t index = 0U; index < 2U; ++index )
    {
        bindings[ index ].binding = index;
        bindings[ index ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[ index ].descriptorCount = 1U;
        bindings[ index ].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo descriptor_description = {};
    descriptor_description.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_description.bindingCount = 2U;
    descriptor_description.pBindings = bindings;
    VkResult result = vkCreateDescriptorSetLayout(
          this->m_device
        , &descriptor_description
        , nullptr
        , &this->m_descriptor_layout
    );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create the Vulkan draw descriptor layout.", result );
    }
    VkPushConstantRange push_range = {};
    push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0U;
    push_range.size = sizeof( sDrawParameters );
    VkPipelineLayoutCreateInfo pipeline_layout_description = {};
    pipeline_layout_description.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_description.setLayoutCount = 1U;
    pipeline_layout_description.pSetLayouts = &this->m_descriptor_layout;
    pipeline_layout_description.pushConstantRangeCount = 1U;
    pipeline_layout_description.pPushConstantRanges = &push_range;
    result = vkCreatePipelineLayout(
          this->m_device
        , &pipeline_layout_description
        , nullptr
        , &this->m_pipeline_layout
    );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create the Vulkan draw pipeline layout.", result );
    }

    VkSamplerCreateInfo sampler_description = {};
    sampler_description.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_description.magFilter = VK_FILTER_NEAREST;
    sampler_description.minFilter = VK_FILTER_NEAREST;
    sampler_description.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_description.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_description.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_description.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_description.minLod = 0.0F;
    sampler_description.maxLod = 0.0F;
    result = vkCreateSampler(
        this->m_device, &sampler_description, nullptr, &this->m_nearest_sampler );
    if( result == VK_SUCCESS )
    {
        sampler_description.magFilter = VK_FILTER_LINEAR;
        sampler_description.minFilter = VK_FILTER_LINEAR;
        sampler_description.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        result = vkCreateSampler(
            this->m_device, &sampler_description, nullptr, &this->m_linear_sampler );
    }
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create Vulkan texture samplers.", result );
    }

    VkShaderModuleCreateInfo shader_description = {};
    shader_description.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_description.codeSize = sizeof( wse_portable_vertex_shader );
    shader_description.pCode = wse_portable_vertex_shader;
    result = vkCreateShaderModule(
        this->m_device, &shader_description, nullptr, &this->m_vertex_shader );
    if( result == VK_SUCCESS )
    {
        shader_description.codeSize = sizeof( wse_portable_fragment_shader );
        shader_description.pCode = wse_portable_fragment_shader;
        result = vkCreateShaderModule(
            this->m_device, &shader_description, nullptr, &this->m_fragment_shader );
    }
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create embedded Vulkan shader modules.", result );
    }
    return RendererError();
}

VkRenderPass VulkanRendererBackend::findOrCreateRenderPass(
      const VkFormat format_in
    , const bool     clear_in
)
{
    const std::uint32_t key = static_cast< std::uint32_t >( format_in ) * 2U +
        ( clear_in ? 1U : 0U );
    const auto existing = this->m_render_passes.find( key );
    if( existing != this->m_render_passes.end() )
    {
        return existing->second;
    }
    VkAttachmentDescription attachment = {};
    attachment.format = format_in;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = clear_in ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0U;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &color_reference;
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo description = {};
    description.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    description.attachmentCount = 1U;
    description.pAttachments = &attachment;
    description.subpassCount = 1U;
    description.pSubpasses = &subpass;
    description.dependencyCount = 1U;
    description.pDependencies = &dependency;
    VkRenderPass pass = VK_NULL_HANDLE;
    if( vkCreateRenderPass( this->m_device, &description, nullptr, &pass ) != VK_SUCCESS )
    {
        return VK_NULL_HANDLE;
    }
    this->m_render_passes.emplace( key, pass );
    return pass;
}

VkPipeline VulkanRendererBackend::findOrCreatePipeline(
      const VkFormat           format_in
    , const eColorBlendMode    blend_in
    , const ePrimitiveTopology topology_in
)
{
    const std::uint32_t key =
        ( static_cast< std::uint32_t >( format_in ) & 0xFFFFU ) |
        ( static_cast< std::uint32_t >( blend_in ) << 16U ) |
        ( static_cast< std::uint32_t >( topology_in ) << 20U );
    const auto existing = this->m_pipelines.find( key );
    if( existing != this->m_pipelines.end() )
    {
        return existing->second;
    }
    const VkRenderPass render_pass = this->findOrCreateRenderPass( format_in, false );
    if( render_pass == VK_NULL_HANDLE )
    {
        return VK_NULL_HANDLE;
    }
    VkPipelineShaderStageCreateInfo stages[ 2U ] = {};
    stages[ 0U ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[ 0U ].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[ 0U ].module = this->m_vertex_shader;
    stages[ 0U ].pName = "main";
    stages[ 1U ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[ 1U ].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[ 1U ].module = this->m_fragment_shader;
    stages[ 1U ].pName = "main";
    VkVertexInputBindingDescription vertex_binding = {};
    vertex_binding.binding = 0U;
    vertex_binding.stride = sizeof( sRendererVertex2D );
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertex_attributes[ 2U ] = {};
    vertex_attributes[ 0U ].location = 0U;
    vertex_attributes[ 0U ].binding = 0U;
    vertex_attributes[ 0U ].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[ 0U ].offset = 0U;
    vertex_attributes[ 1U ].location = 1U;
    vertex_attributes[ 1U ].binding = 0U;
    vertex_attributes[ 1U ].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[ 1U ].offset = sizeof( float ) * 2U;
    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1U;
    vertex_input.pVertexBindingDescriptions = &vertex_binding;
    vertex_input.vertexAttributeDescriptionCount = 2U;
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly = {};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = topology_in == ePrimitiveTopology::TriangleStrip
        ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1U;
    viewport_state.scissorCount = 1U;
    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if( blend_in == eColorBlendMode::SourceAlpha )
    {
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo blend_state = {};
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = 1U;
    blend_state.pAttachments = &blend_attachment;
    const VkDynamicState dynamic_states[] = {
          VK_DYNAMIC_STATE_VIEWPORT
        , VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2U;
    dynamic_state.pDynamicStates = dynamic_states;
    VkGraphicsPipelineCreateInfo description = {};
    description.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    description.stageCount = 2U;
    description.pStages = stages;
    description.pVertexInputState = &vertex_input;
    description.pInputAssemblyState = &assembly;
    description.pViewportState = &viewport_state;
    description.pRasterizationState = &rasterization;
    description.pMultisampleState = &multisample;
    description.pColorBlendState = &blend_state;
    description.pDynamicState = &dynamic_state;
    description.layout = this->m_pipeline_layout;
    description.renderPass = render_pass;
    description.subpass = 0U;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if( vkCreateGraphicsPipelines(
            this->m_device, VK_NULL_HANDLE, 1U, &description, nullptr, &pipeline ) != VK_SUCCESS )
    {
        return VK_NULL_HANDLE;
    }
    this->m_pipelines.emplace( key, pipeline );
    return pipeline;
}

VulkanRendererBackend::sTextureRecord* VulkanRendererBackend::findTexture(
    const sTextureHandle texture_in ) noexcept
{
    if( texture_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_textures.find( texture_in.value );
    return found == this->m_textures.end() ? nullptr : &found->second;
}

const VulkanRendererBackend::sTextureRecord* VulkanRendererBackend::findTexture(
    const sTextureHandle texture_in ) const noexcept
{
    if( texture_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_textures.find( texture_in.value );
    return found == this->m_textures.end() ? nullptr : &found->second;
}

VulkanRendererBackend::sMeshRecord* VulkanRendererBackend::findMesh(
    const sMeshHandle mesh_in ) noexcept
{
    if( mesh_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_meshes.find( mesh_in.value );
    return found == this->m_meshes.end() ? nullptr : &found->second;
}

const VulkanRendererBackend::sMeshRecord* VulkanRendererBackend::findMesh(
    const sMeshHandle mesh_in ) const noexcept
{
    if( mesh_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_meshes.find( mesh_in.value );
    return found == this->m_meshes.end() ? nullptr : &found->second;
}

VulkanRendererBackend::sSurfaceRecord* VulkanRendererBackend::findSurface(
    const sSurfaceHandle surface_in ) noexcept
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_surfaces.find( surface_in.value );
    return found == this->m_surfaces.end() ? nullptr : &found->second;
}

const VulkanRendererBackend::sSurfaceRecord* VulkanRendererBackend::findSurface(
    const sSurfaceHandle surface_in ) const noexcept
{
    if( surface_in.generation != this->m_handle_generation )
    {
        return nullptr;
    }
    const auto found = this->m_surfaces.find( surface_in.value );
    return found == this->m_surfaces.end() ? nullptr : &found->second;
}

void VulkanRendererBackend::destroyTextureRecord( sTextureRecord* const p_texture_inout ) noexcept
{
    if( p_texture_inout == nullptr || this->m_device == VK_NULL_HANDLE )
    {
        return;
    }
    if( p_texture_inout->view != VK_NULL_HANDLE )
        vkDestroyImageView( this->m_device, p_texture_inout->view, nullptr );
    if( p_texture_inout->owns_image && p_texture_inout->image != VK_NULL_HANDLE )
        vkDestroyImage( this->m_device, p_texture_inout->image, nullptr );
    if( p_texture_inout->owns_image && p_texture_inout->memory != VK_NULL_HANDLE )
        vkFreeMemory( this->m_device, p_texture_inout->memory, nullptr );
    *p_texture_inout = {};
}

void VulkanRendererBackend::destroyMeshRecord( sMeshRecord* const p_mesh_inout ) noexcept
{
    if( p_mesh_inout == nullptr || this->m_device == VK_NULL_HANDLE )
    {
        return;
    }
    if( p_mesh_inout->vertex_buffer != VK_NULL_HANDLE )
        vkDestroyBuffer( this->m_device, p_mesh_inout->vertex_buffer, nullptr );
    if( p_mesh_inout->vertex_memory != VK_NULL_HANDLE )
        vkFreeMemory( this->m_device, p_mesh_inout->vertex_memory, nullptr );
    if( p_mesh_inout->index_buffer != VK_NULL_HANDLE )
        vkDestroyBuffer( this->m_device, p_mesh_inout->index_buffer, nullptr );
    if( p_mesh_inout->index_memory != VK_NULL_HANDLE )
        vkFreeMemory( this->m_device, p_mesh_inout->index_memory, nullptr );
    *p_mesh_inout = {};
}

RendererResult< sTextureHandle > VulkanRendererBackend::createTexture(
    const sTextureDescription& description_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sTextureHandle >::failure( pending_error );
    if( description_in.mip_levels != 1U )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "The Vulkan 1.2 OUI backend currently supports one mip level."
            )
        );
    }
    if( description_in.extent.width > this->m_capabilities.maximum_texture_extent ||
        description_in.extent.height > this->m_capabilities.maximum_texture_extent )
    {
        return RendererResult< sTextureHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceExhausted
                , "Texture extent exceeds the Vulkan device limit."
            )
        );
    }
    if( description_in.initial_state == eTextureState::Present )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Present state is reserved for textures owned by a presentation surface."
            )
        );
    }
    const VkFormat format = toNativeFormat( description_in.format );
    if( format != VK_FORMAT_R8_UNORM && format != VK_FORMAT_R8G8B8A8_UNORM &&
        format != VK_FORMAT_B8G8R8A8_UNORM )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "The Vulkan 1.2 OUI backend supports R8, RGBA8 and BGRA8 textures."
            )
        );
    }
    if( format == VK_FORMAT_R8_UNORM &&
        hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget ) )
    {
        return RendererResult< sTextureHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "R8 Vulkan textures cannot be OUI color attachments."
            )
        );
    }
    VkImageUsageFlags usage = 0U;
    if( hasTextureUsage( description_in.usage, eTextureUsage::Sampled ) )
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if( hasTextureUsage( description_in.usage, eTextureUsage::RenderTarget ) )
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if( hasTextureUsage( description_in.usage, eTextureUsage::TransferSource ) )
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if( hasTextureUsage( description_in.usage, eTextureUsage::TransferDestination ) )
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateInfo image_description = {};
    image_description.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_description.imageType = VK_IMAGE_TYPE_2D;
    image_description.format = format;
    image_description.extent = {
        description_in.extent.width, description_in.extent.height, 1U };
    image_description.mipLevels = 1U;
    image_description.arrayLayers = 1U;
    image_description.samples = VK_SAMPLE_COUNT_1_BIT;
    image_description.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_description.usage = usage;
    image_description.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto work = std::make_unique< sVulkanSubmission >(
        this->m_submission_calls, this->m_device, this->m_command_pool );
    VkResult result = vkCreateImage(
        this->m_device, &image_description, nullptr, &work->image );
    if( result != VK_SUCCESS )
    {
        return RendererResult< sTextureHandle >::failure( backendError( "Failed to create a Vulkan texture.", result ) );
    }
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements( this->m_device, work->image, &requirements );
    std::uint32_t memory_index = 0U;
    RendererError error = this->findMemoryType(
          &memory_index
        , requirements.memoryTypeBits
        , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if( error.ok() )
    {
        VkMemoryAllocateInfo allocation = {};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_index;
        result = vkAllocateMemory( this->m_device, &allocation, nullptr, &work->image_memory );
        if( result == VK_SUCCESS )
            result = vkBindImageMemory( this->m_device, work->image, work->image_memory, 0U );
        if( result != VK_SUCCESS )
            error = backendError( "Failed to allocate Vulkan texture memory.", result );
    }
    if( error.ok() )
    {
        VkImageViewCreateInfo view_description = {};
        view_description.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_description.image = work->image;
        view_description.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_description.format = format;
        view_description.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_description.subresourceRange.levelCount = 1U;
        view_description.subresourceRange.layerCount = 1U;
        result = vkCreateImageView(
            this->m_device, &view_description, nullptr, &work->image_view );
        if( result != VK_SUCCESS )
            error = backendError( "Failed to create a Vulkan texture view.", result );
    }
    const eTextureState initial_state = description_in.initial_state == eTextureState::Undefined
        ? eTextureState::Undefined : description_in.initial_state;
    const VkImageLayout initial_layout = toNativeLayout( initial_state );
    if( error.ok() && initial_layout != VK_IMAGE_LAYOUT_UNDEFINED )
    {
        const auto command_result = this->beginCommands();
        if( !command_result.succeeded() )
        {
            error = command_result.error();
        }
        else
        {
            work->command = command_result.value();
            transitionImage(
                work->command, work->image, VK_IMAGE_LAYOUT_UNDEFINED, initial_layout );
            bool submitted = false;
            const auto submit_result = this->submitAndWait( work, &submitted );
            if( submit_result != VK_SUCCESS )
                error = this->submissionResult( submit_result ).error();
        }
    }
    if( !error.ok() )
    {
        return RendererResult< sTextureHandle >::failure( error );
    }
    sTextureRecord record( work->image, work->image_memory, work->image_view );
    record.description = description_in;
    record.state = initial_state;
    record.layout = initial_layout;
    const std::uint64_t id = this->m_next_resource_id++;
    this->m_textures.emplace( id, std::move( record ) );
    work->image = VK_NULL_HANDLE;
    work->image_view = VK_NULL_HANDLE;
    work->image_memory = VK_NULL_HANDLE;
    return RendererResult< sTextureHandle >::success( sTextureHandle{ id, this->m_handle_generation } );
}

RendererStatus VulkanRendererBackend::destroyTexture( const sTextureHandle texture_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sTextureRecord* const p_texture = this->findTexture( texture_in );
    if( p_texture == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Texture" ) );
    }
    if( p_texture->surface_owner != 0U )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Lifecycle
                , eRendererErrorCode::ResourceInUse
                , "Texture is owned by a surface and must be released through that surface."
            )
        );
    }
    this->destroyTextureRecord( p_texture );
    this->m_textures.erase( texture_in.value );
    return RendererStatus::success();
}

RendererResult< sFenceHandle > VulkanRendererBackend::uploadTexture(
      const sTextureHandle texture_in
    , const sRendererFrame& frame_in
)
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sFenceHandle >::failure( pending_error );
    sTextureRecord* const p_texture = this->findTexture( texture_in );
    if( p_texture == nullptr )
    {
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Texture" ) );
    }
    if( !hasTextureUsage( p_texture->description.usage, eTextureUsage::Sampled ) ||
        !hasTextureUsage( p_texture->description.usage, eTextureUsage::TransferDestination ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Texture upload requires Sampled and TransferDestination usage."
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
                , "Upload frame extent and format must match the Vulkan texture."
            )
        );
    }
    const std::size_t packed_pitch = frame_in.description.minimumRowPitch();
    const std::size_t upload_size = packed_pitch * frame_in.description.extent.height;
    auto work = std::make_unique< sVulkanSubmission >(
        this->m_submission_calls, this->m_device, this->m_command_pool );
    VkBuffer& staging_buffer = work->buffer;
    VkDeviceMemory& staging_memory = work->buffer_memory;
    RendererError error = this->createBuffer(
          &staging_buffer
        , &staging_memory
        , upload_size
        , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if( error.ok() )
    {
        std::vector< std::uint8_t > packed( upload_size );
        const std::size_t source_pitch = frame_in.description.effectiveRowPitch();
        for( std::uint32_t row = 0U; row < frame_in.description.extent.height; ++row )
        {
            std::memcpy(
                  packed.data() + static_cast< std::size_t >( row ) * packed_pitch
                , frame_in.data.data() + static_cast< std::size_t >( row ) * source_pitch
                , packed_pitch
            );
        }
        error = this->writeMemory( staging_memory, packed.data(), upload_size );
    }
    auto result = error.ok()
        ? RendererResult< sFenceHandle >::success( {} )
        : RendererResult< sFenceHandle >::failure( error );
    if( error.ok() )
    {
        const auto command_result = this->beginCommands();
        if( !command_result.succeeded() )
        {
            result = RendererResult< sFenceHandle >::failure( command_result.error() );
        }
        else
        {
            const VkCommandBuffer command = command_result.value();
            work->command = command;
            transitionImage(
                  command
                , p_texture->image
                , p_texture->layout
                , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1U;
            region.imageExtent = {
                  p_texture->description.extent.width
                , p_texture->description.extent.height
                , 1U
            };
            vkCmdCopyBufferToImage(
                  command
                , staging_buffer
                , p_texture->image
                , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                , 1U
                , &region
            );
            transitionImage(
                  command
                , p_texture->image
                , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            bool submitted = false;
            const VkResult native_result = this->submitAndWait( work, &submitted );
            if( submitted )
            {
                p_texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                p_texture->state = eTextureState::ShaderResource;
            }
            result = this->submissionResult( native_result );
        }
    }
    return result;
}

RendererResult< sMeshHandle > VulkanRendererBackend::createMesh(
    const sMeshDescription& description_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sMeshHandle >::failure( pending_error );
    const VkDeviceSize vertex_size = description_in.vertices.size() * sizeof( sRendererVertex2D );
    const VkDeviceSize index_size = description_in.indices.size() * sizeof( std::uint32_t );
    sMeshRecord record;
    RendererError error = this->createBuffer(
          &record.vertex_buffer
        , &record.vertex_memory
        , vertex_size
        , VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if( error.ok() )
    {
        error = this->writeMemory(
            record.vertex_memory, description_in.vertices.data(), vertex_size );
    }
    if( error.ok() )
    {
        error = this->createBuffer(
              &record.index_buffer
            , &record.index_memory
            , index_size
            , VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }
    if( error.ok() )
    {
        error = this->writeMemory(
            record.index_memory, description_in.indices.data(), index_size );
    }
    if( !error.ok() )
    {
        this->destroyMeshRecord( &record );
        return RendererResult< sMeshHandle >::failure( error );
    }
    record.topology = description_in.topology;
    record.index_count = static_cast< std::uint32_t >( description_in.indices.size() );
    const std::uint64_t id = this->m_next_resource_id++;
    this->m_meshes.emplace( id, std::move( record ) );
    return RendererResult< sMeshHandle >::success( sMeshHandle{ id, this->m_handle_generation } );
}

RendererStatus VulkanRendererBackend::updateMesh(
      const sMeshHandle        mesh_in
    , const sMeshDescription& description_in
)
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sMeshRecord* const p_existing = this->findMesh( mesh_in );
    if( p_existing == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Mesh" ) );
    }
    const auto replacement_result = this->createMesh( description_in );
    if( !replacement_result.succeeded() )
    {
        return RendererStatus::failure( replacement_result.error() );
    }
    sMeshRecord* const p_replacement = this->findMesh( replacement_result.value() );
    sMeshRecord old = std::move( *p_existing );
    *p_existing = std::move( *p_replacement );
    this->m_meshes.erase( replacement_result.value().value );
    this->destroyMeshRecord( &old );
    return RendererStatus::success();
}

RendererStatus VulkanRendererBackend::destroyMesh( const sMeshHandle mesh_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sMeshRecord* const p_mesh = this->findMesh( mesh_in );
    if( p_mesh == nullptr )
    {
        return RendererStatus::failure( resourceNotFoundError( "Mesh" ) );
    }
    this->destroyMeshRecord( p_mesh );
    this->m_meshes.erase( mesh_in.value );
    return RendererStatus::success();
}

std::vector< sDisplayDescription > VulkanRendererBackend::enumerateDrmDisplays() const
{
    std::vector< sDisplayDescription > result;
    for( std::uint32_t card_index = 0U; card_index < 16U; ++card_index )
    {
        const std::string card_name = "card" + std::to_string( card_index );
        const std::string node = "/dev/dri/" + card_name;
        const int fd = open( node.c_str(), O_RDONLY | O_CLOEXEC );
        if( fd < 0 )
        {
            continue;
        }
        drmModeRes* const p_resources = drmModeGetResources( fd );
        if( p_resources == nullptr )
        {
            close( fd );
            continue;
        }
        for( int connector_index = 0; connector_index < p_resources->count_connectors;
             ++connector_index )
        {
            drmModeConnector* const p_connector = drmModeGetConnector(
                fd, p_resources->connectors[ connector_index ] );
            if( p_connector == nullptr || p_connector->connection != DRM_MODE_CONNECTED ||
                p_connector->count_modes <= 0 )
            {
                if( p_connector != nullptr ) drmModeFreeConnector( p_connector );
                continue;
            }
            sDisplayDescription description;
            description.id = "drm:" + card_name + ":" +
                std::to_string( p_connector->connector_id );
            description.adapter_id = "vulkan:" +
                std::to_string( this->m_physical_properties.vendorID ) + ":" +
                std::to_string( this->m_physical_properties.deviceID );
            description.adapter_name = this->m_physical_properties.deviceName;
            description.display_name = drmConnectorName( *p_connector );
            description.rotation = eDisplayRotation::Identity;
            drmModeCrtc* p_crtc = nullptr;
            if( p_connector->encoder_id != 0U )
            {
                drmModeEncoder* const p_encoder = drmModeGetEncoder(
                    fd, p_connector->encoder_id );
                if( p_encoder != nullptr )
                {
                    if( p_encoder->crtc_id != 0U )
                        p_crtc = drmModeGetCrtc( fd, p_encoder->crtc_id );
                    drmModeFreeEncoder( p_encoder );
                }
            }
            for( int mode_index = 0; mode_index < p_connector->count_modes; ++mode_index )
            {
                description.modes.emplace_back(
                    toPortableDrmMode( p_connector->modes[ mode_index ] ) );
            }
            std::size_t current_index = 0U;
            if( p_crtc != nullptr && p_crtc->mode_valid != 0 )
            {
                description.position = {
                      static_cast< std::int32_t >( p_crtc->x )
                    , static_cast< std::int32_t >( p_crtc->y )
                };
                const sDisplayMode current = toPortableDrmMode( p_crtc->mode );
                const auto found = std::find_if(
                      description.modes.begin()
                    , description.modes.end()
                    , [&current]( const sDisplayMode& mode_in )
                      {
                          return sameDisplayMode( mode_in, current );
                      }
                );
                if( found != description.modes.end() )
                    current_index = static_cast< std::size_t >(
                        std::distance( description.modes.begin(), found ) );
            }
            description.current_mode = description.modes[ current_index ];
            description.desktop_extent = description.current_mode.extent;
            description.primary = result.empty();
            description.renderer_compatible =
                this->m_get_drm_display != nullptr && this->m_acquire_drm_display != nullptr &&
                this->m_release_display != nullptr;
            description.modes_complete = true;
            result.emplace_back( std::move( description ) );
            if( p_crtc != nullptr ) drmModeFreeCrtc( p_crtc );
            drmModeFreeConnector( p_connector );
        }
        drmModeFreeResources( p_resources );
        close( fd );
    }
    return result;
}

RendererResult< std::vector< sDisplayDescription > >
VulkanRendererBackend::enumerateDisplays() const
{
    const std::string adapter_id = "vulkan:" +
        std::to_string( this->m_physical_properties.vendorID ) + ":" +
        std::to_string( this->m_physical_properties.deviceID );
    std::vector< sDisplayDescription > result;
    if( this->m_wayland != nullptr )
    {
        result = this->m_wayland->enumerateDisplays(
            adapter_id, this->m_physical_properties.deviceName );
    }
    std::vector< sDisplayDescription > drm_displays = this->enumerateDrmDisplays();
    result.insert(
          result.end()
        , std::make_move_iterator( drm_displays.begin() )
        , std::make_move_iterator( drm_displays.end() )
    );
    return RendererResult< std::vector< sDisplayDescription > >::success( std::move( result ) );
}

void VulkanRendererBackend::destroySwapchainResources(
    sSurfaceRecord* const p_surface_inout ) noexcept
{
    if( p_surface_inout == nullptr || this->m_device == VK_NULL_HANDLE )
    {
        return;
    }
    this->waitDeviceIdleForRelease();
    for( const sTextureHandle texture : p_surface_inout->textures )
    {
        const auto found = this->m_textures.find( texture.value );
        if( found != this->m_textures.end() )
        {
            this->destroyTextureRecord( &found->second );
            this->m_textures.erase( found );
        }
    }
    p_surface_inout->textures.clear();
    if( p_surface_inout->acquire_semaphore != VK_NULL_HANDLE )
    {
        vkDestroySemaphore( this->m_device, p_surface_inout->acquire_semaphore, nullptr );
        p_surface_inout->acquire_semaphore = VK_NULL_HANDLE;
    }
    if( p_surface_inout->swap_chain != VK_NULL_HANDLE )
    {
        vkDestroySwapchainKHR( this->m_device, p_surface_inout->swap_chain, nullptr );
        p_surface_inout->swap_chain = VK_NULL_HANDLE;
    }
    p_surface_inout->current_image = 0U;
    p_surface_inout->acquire_pending = false;
}

RendererError VulkanRendererBackend::createSwapchain(
      sSurfaceRecord* const   p_surface_inout
    , const std::uint64_t     surface_id_in
    , const sRendererExtent2D requested_extent_in
)
{
    if( p_surface_inout == nullptr || p_surface_inout->native_surface == VK_NULL_HANDLE )
    {
        return makeError(
              eRendererErrorCategory::Validation
            , eRendererErrorCode::InvalidArgument
            , "Vulkan swapchain surface is invalid."
        );
    }
    std::uint32_t present_family = ( std::numeric_limits< std::uint32_t >::max )();
    for( std::uint32_t index = 0U;
         index < static_cast< std::uint32_t >( this->m_queue_properties.size() ); ++index )
    {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            this->m_physical_device, index, p_surface_inout->native_surface, &supported );
        if( supported == VK_TRUE && this->m_queues[ index ] != VK_NULL_HANDLE )
        {
            present_family = index;
            if( index == this->m_graphics_family ) break;
        }
    }
    if( present_family == ( std::numeric_limits< std::uint32_t >::max )() )
    {
        return unsupportedError(
              eRendererErrorCode::UnsupportedSurface
            , "No Vulkan queue can present to the requested surface."
        );
    }
    VkSurfaceCapabilitiesKHR capabilities = {};
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        this->m_physical_device, p_surface_inout->native_surface, &capabilities );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to query Vulkan surface capabilities.", result );
    }
    std::uint32_t format_count = 0U;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        this->m_physical_device, p_surface_inout->native_surface, &format_count, nullptr );
    std::vector< VkSurfaceFormatKHR > formats( format_count );
    if( format_count > 0U )
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
              this->m_physical_device
            , p_surface_inout->native_surface
            , &format_count
            , formats.data()
        );
    }
    const VkFormat requested_format = toNativeFormat( p_surface_inout->description.format );
    const auto format_found = std::find_if(
          formats.begin()
        , formats.end()
        , [requested_format]( const VkSurfaceFormatKHR& format_in )
          {
              return format_in.format == requested_format &&
                  format_in.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
          }
    );
    if( format_found == formats.end() )
    {
        return unsupportedError(
              eRendererErrorCode::UnsupportedFormat
            , "Requested pixel format is unavailable on the Vulkan presentation surface."
        );
    }
    VkExtent2D extent = {};
    if( capabilities.currentExtent.width != ( std::numeric_limits< std::uint32_t >::max )() )
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = std::clamp(
            requested_extent_in.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width );
        extent.height = std::clamp(
            requested_extent_in.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height );
    }
    std::uint32_t image_count = std::max(
        p_surface_inout->description.buffer_count, capabilities.minImageCount );
    if( capabilities.maxImageCount > 0U )
        image_count = std::min( image_count, capabilities.maxImageCount );
    std::uint32_t present_mode_count = 0U;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        this->m_physical_device, p_surface_inout->native_surface,
        &present_mode_count, nullptr );
    std::vector< VkPresentModeKHR > present_modes( present_mode_count );
    if( present_mode_count > 0U )
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            this->m_physical_device, p_surface_inout->native_surface,
            &present_mode_count, present_modes.data() );
    }
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if( !p_surface_inout->description.vertical_sync )
    {
        if( std::find( present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR ) !=
            present_modes.end() )
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if( std::find( present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR ) !=
                 present_modes.end() )
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR composite_candidates[] = {
          VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        , VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        , VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
        , VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    for( const VkCompositeAlphaFlagBitsKHR candidate : composite_candidates )
    {
        if( ( capabilities.supportedCompositeAlpha & candidate ) != 0U )
        {
            composite_alpha = candidate;
            break;
        }
    }
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if( ( capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) != 0U )
        image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const std::uint32_t queue_indices[] = { this->m_graphics_family, present_family };
    VkSwapchainCreateInfoKHR description = {};
    description.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    description.surface = p_surface_inout->native_surface;
    description.minImageCount = image_count;
    description.imageFormat = format_found->format;
    description.imageColorSpace = format_found->colorSpace;
    description.imageExtent = extent;
    description.imageArrayLayers = 1U;
    description.imageUsage = image_usage;
    if( present_family != this->m_graphics_family )
    {
        description.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        description.queueFamilyIndexCount = 2U;
        description.pQueueFamilyIndices = queue_indices;
    }
    else
    {
        description.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    description.preTransform = ( capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR )
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
    description.compositeAlpha = composite_alpha;
    description.presentMode = present_mode;
    description.clipped = VK_TRUE;
    result = vkCreateSwapchainKHR(
        this->m_device, &description, nullptr, &p_surface_inout->swap_chain );
    if( result != VK_SUCCESS )
    {
        return backendError( "Failed to create the Vulkan swapchain.", result );
    }
    std::uint32_t swap_image_count = 0U;
    vkGetSwapchainImagesKHR(
        this->m_device, p_surface_inout->swap_chain, &swap_image_count, nullptr );
    std::vector< VkImage > images( swap_image_count );
    result = vkGetSwapchainImagesKHR(
        this->m_device, p_surface_inout->swap_chain, &swap_image_count, images.data() );
    if( result != VK_SUCCESS || images.empty() )
    {
        this->destroySwapchainResources( p_surface_inout );
        return backendError( "Failed to enumerate Vulkan swapchain images.", result );
    }
    p_surface_inout->textures.reserve( images.size() );
    for( const VkImage image : images )
    {
        sTextureRecord texture;
        texture.image = image;
        texture.owns_image = false;
        texture.surface_owner = surface_id_in;
        texture.description.extent = { extent.width, extent.height };
        texture.description.format = fromNativeFormat( format_found->format );
        texture.description.usage = eTextureUsage::RenderTarget;
        if( ( image_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) != 0U )
            texture.description.usage = texture.description.usage | eTextureUsage::TransferSource;
        texture.description.initial_state = eTextureState::Present;
        texture.state = eTextureState::Present;
        texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageViewCreateInfo view_description = {};
        view_description.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_description.image = image;
        view_description.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_description.format = format_found->format;
        view_description.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_description.subresourceRange.levelCount = 1U;
        view_description.subresourceRange.layerCount = 1U;
        result = vkCreateImageView(
            this->m_device, &view_description, nullptr, &texture.view );
        if( result != VK_SUCCESS )
        {
            this->destroyTextureRecord( &texture );
            this->destroySwapchainResources( p_surface_inout );
            return backendError( "Failed to create a Vulkan swapchain image view.", result );
        }
        const std::uint64_t texture_id = this->m_next_resource_id++;
        this->m_textures.emplace( texture_id, std::move( texture ) );
        p_surface_inout->textures.emplace_back(
            sTextureHandle{ texture_id, this->m_handle_generation } );
    }
    VkSemaphoreCreateInfo semaphore_description = {};
    semaphore_description.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(
        this->m_device, &semaphore_description, nullptr,
        &p_surface_inout->acquire_semaphore );
    if( result == VK_SUCCESS )
    {
        result = vkAcquireNextImageKHR(
              this->m_device
            , p_surface_inout->swap_chain
            , ( std::numeric_limits< std::uint64_t >::max )()
            , p_surface_inout->acquire_semaphore
            , VK_NULL_HANDLE
            , &p_surface_inout->current_image
        );
    }
    if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
    {
        this->destroySwapchainResources( p_surface_inout );
        return backendError( "Failed to acquire the first Vulkan swapchain image.", result );
    }
    p_surface_inout->description.extent = { extent.width, extent.height };
    p_surface_inout->present_family = present_family;
    p_surface_inout->acquire_pending = true;
    return RendererError();
}

RendererError VulkanRendererBackend::recreateSwapchain(
      sSurfaceRecord* const    p_surface_inout
    , const std::uint64_t      surface_id_in
    , const sRendererExtent2D  requested_extent_in
)
{
    this->destroySwapchainResources( p_surface_inout );
    return this->createSwapchain( p_surface_inout, surface_id_in, requested_extent_in );
}

RendererError VulkanRendererBackend::restoreDrmDisplay(
    sSurfaceRecord* const p_surface_inout ) noexcept
{
    if( p_surface_inout == nullptr || p_surface_inout->drm.fd < 0 )
    {
        return RendererError();
    }
    RendererError first_error;
    sDrmRestoreState& drm = p_surface_inout->drm;
    if( drm.acquired && drm.display != VK_NULL_HANDLE && this->m_release_display != nullptr )
    {
        const VkResult release_result = this->m_release_display(
            this->m_physical_device, drm.display );
        if( release_result != VK_SUCCESS )
            first_error = backendError( "Failed to release the Vulkan DRM display.", release_result );
    }
    int restore_result = 0;
    if( drm.crtc_id != 0U )
    {
        if( drm.mode_valid )
        {
            std::uint32_t connector = drm.connector_id;
            restore_result = drmModeSetCrtc(
                  drm.fd
                , drm.crtc_id
                , drm.buffer_id
                , drm.x
                , drm.y
                , &connector
                , 1
                , &drm.mode
            );
        }
        else
        {
            restore_result = drmModeSetCrtc(
                drm.fd, drm.crtc_id, 0U, 0U, 0U, nullptr, 0, nullptr );
        }
    }
    if( restore_result != 0 && first_error.ok() )
    {
        first_error = makeError(
              eRendererErrorCategory::Backend
            , eRendererErrorCode::BackendFailure
            , "Failed to restore the original DRM/KMS CRTC state."
            , restore_result
        );
    }
    close( drm.fd );
    drm = {};
    drm.fd = -1;
    return first_error;
}

RendererResult< sSurfaceHandle > VulkanRendererBackend::createOffscreenSurface(
    const sSurfaceDescription& description_in )
{
    sTextureDescription texture_description;
    texture_description.extent = description_in.extent;
    texture_description.format = description_in.format;
    texture_description.usage =
        eTextureUsage::RenderTarget | eTextureUsage::TransferSource;
    texture_description.initial_state = eTextureState::RenderTarget;
    const auto texture_result = this->createTexture( texture_description );
    if( !texture_result.succeeded() )
    {
        return RendererResult< sSurfaceHandle >::failure( texture_result.error() );
    }
    const std::uint64_t surface_id = this->m_next_resource_id++;
    sSurfaceRecord surface;
    surface.description = description_in;
    surface.textures.emplace_back( texture_result.value() );
    surface.window_mode = eSurfaceWindowMode::NotApplicable;
    this->m_textures[ texture_result.value().value ].surface_owner = surface_id;
    this->m_surfaces.emplace( surface_id, std::move( surface ) );
    return RendererResult< sSurfaceHandle >::success(
        sSurfaceHandle{ surface_id, this->m_handle_generation } );
}

RendererResult< sSurfaceHandle > VulkanRendererBackend::createWindowSurface(
    const sSurfaceDescription& description_in )
{
    if( this->m_wayland == nullptr || !this->m_has_swapchain )
    {
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedSurface
                , "Wayland presentation is unavailable in this Linux session."
            )
        );
    }
    std::string wayland_error;
    sSurfaceRecord surface;
    surface.description = description_in;
    surface.window = this->m_wayland->createWindow(
        &wayland_error, description_in.title, description_in.extent, description_in.visible );
    if( surface.window == nullptr )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Failed to create the Wayland window: " + wayland_error
            )
        );
    }
    VkWaylandSurfaceCreateInfoKHR native_description = {};
    native_description.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    native_description.display = this->m_wayland->nativeDisplay();
    native_description.surface = surface.window->surface;
    VkResult result = vkCreateWaylandSurfaceKHR(
        this->m_instance, &native_description, nullptr, &surface.native_surface );
    if( result != VK_SUCCESS )
    {
        this->m_wayland->destroyWindow( &surface.window );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to create the Vulkan Wayland surface.", result ) );
    }
    const std::uint64_t surface_id = this->m_next_resource_id++;
    const RendererError swapchain_error = this->createSwapchain(
        &surface, surface_id, surface.window->extent );
    if( !swapchain_error.ok() )
    {
        this->destroySwapchainResources( &surface );
        vkDestroySurfaceKHR( this->m_instance, surface.native_surface, nullptr );
        this->m_wayland->destroyWindow( &surface.window );
        return RendererResult< sSurfaceHandle >::failure( swapchain_error );
    }
    surface.window_mode = eSurfaceWindowMode::Windowed;
    this->m_surfaces.emplace( surface_id, std::move( surface ) );
    return RendererResult< sSurfaceHandle >::success(
        sSurfaceHandle{ surface_id, this->m_handle_generation } );
}

RendererResult< sSurfaceHandle > VulkanRendererBackend::createDirectDisplaySurface(
    const sSurfaceDescription& description_in )
{
    if( !this->m_capabilities.supports_direct_display )
    {
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedSurface
                , "Vulkan DRM/KMS direct display extensions are unavailable."
            )
        );
    }
    const std::string prefix = "drm:";
    const std::size_t separator = description_in.display_id.rfind( ':' );
    if( description_in.display_id.compare( 0U, prefix.size(), prefix ) != 0 ||
        separator == std::string::npos || separator <= prefix.size() )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "DirectDisplay requires a DRM display ID returned by enumerateDisplays()."
            )
        );
    }
    const std::string card_name = description_in.display_id.substr(
        prefix.size(), separator - prefix.size() );
    std::uint32_t connector_id = 0U;
    try
    {
        connector_id = static_cast< std::uint32_t >(
            std::stoul( description_in.display_id.substr( separator + 1U ) ) );
    }
    catch( const std::exception& )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "DirectDisplay DRM connector ID is invalid."
            )
        );
    }
    sSurfaceRecord surface;
    surface.description = description_in;
    surface.window_mode = eSurfaceWindowMode::DisplayModeFullscreen;
    surface.display_id = description_in.display_id;
    surface.display_mode = description_in.display_mode;
    surface.drm.node = "/dev/dri/" + card_name;
    surface.drm.connector_id = connector_id;
    surface.drm.fd = open( surface.drm.node.c_str(), O_RDWR | O_CLOEXEC );
    if( surface.drm.fd < 0 )
    {
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Failed to open the DRM/KMS card for direct display."
            )
        );
    }
    drmModeRes* const p_resources = drmModeGetResources( surface.drm.fd );
    drmModeConnector* p_connector = p_resources == nullptr
        ? nullptr : drmModeGetConnector( surface.drm.fd, connector_id );
    if( p_resources == nullptr || p_connector == nullptr ||
        p_connector->connection != DRM_MODE_CONNECTED )
    {
        if( p_connector != nullptr ) drmModeFreeConnector( p_connector );
        if( p_resources != nullptr ) drmModeFreeResources( p_resources );
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceNotFound
                , "Requested DRM/KMS connector is not connected."
            )
        );
    }
    drmModeEncoder* p_encoder = p_connector->encoder_id == 0U
        ? nullptr : drmModeGetEncoder( surface.drm.fd, p_connector->encoder_id );
    if( p_encoder != nullptr )
    {
        surface.drm.crtc_id = p_encoder->crtc_id;
        drmModeFreeEncoder( p_encoder );
    }
    if( surface.drm.crtc_id == 0U && p_resources->count_crtcs > 0 )
        surface.drm.crtc_id = p_resources->crtcs[ 0U ];
    drmModeCrtc* const p_crtc = surface.drm.crtc_id == 0U
        ? nullptr : drmModeGetCrtc( surface.drm.fd, surface.drm.crtc_id );
    if( p_crtc != nullptr )
    {
        surface.drm.buffer_id = p_crtc->buffer_id;
        surface.drm.x = p_crtc->x;
        surface.drm.y = p_crtc->y;
        surface.drm.mode_valid = p_crtc->mode_valid != 0;
        if( surface.drm.mode_valid ) surface.drm.mode = p_crtc->mode;
        drmModeFreeCrtc( p_crtc );
    }
    bool requested_mode_found = false;
    for( int index = 0; index < p_connector->count_modes; ++index )
    {
        requested_mode_found = requested_mode_found || sameDisplayMode(
            toPortableDrmMode( p_connector->modes[ index ] ), description_in.display_mode );
    }
    drmModeFreeConnector( p_connector );
    drmModeFreeResources( p_resources );
    if( !requested_mode_found )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Requested DRM/KMS display mode is no longer available."
            )
        );
    }

    VkResult native_result = this->m_get_drm_display(
          this->m_physical_device
        , surface.drm.fd
        , surface.drm.connector_id
        , &surface.drm.display
    );
    if( native_result == VK_SUCCESS )
    {
        native_result = this->m_acquire_drm_display(
            this->m_physical_device, surface.drm.fd, surface.drm.display );
        surface.drm.acquired = native_result == VK_SUCCESS;
    }
    if( native_result != VK_SUCCESS )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to acquire the Vulkan DRM display.", native_result ) );
    }

    std::uint32_t mode_count = 0U;
    vkGetDisplayModePropertiesKHR(
        this->m_physical_device, surface.drm.display, &mode_count, nullptr );
    std::vector< VkDisplayModePropertiesKHR > modes( mode_count );
    native_result = vkGetDisplayModePropertiesKHR(
        this->m_physical_device, surface.drm.display, &mode_count, modes.data() );
    const std::uint64_t requested_refresh =
        static_cast< std::uint64_t >( description_in.display_mode.refresh_rate_numerator ) * 1000ULL /
        description_in.display_mode.refresh_rate_denominator;
    const auto mode_found = std::min_element(
          modes.begin()
        , modes.end()
        , [&description_in, requested_refresh](
              const VkDisplayModePropertiesKHR& left_in,
              const VkDisplayModePropertiesKHR& right_in )
          {
              const auto score = [&description_in, requested_refresh](
                  const VkDisplayModePropertiesKHR& mode_in )
              {
                  if( mode_in.parameters.visibleRegion.width != description_in.display_mode.extent.width ||
                      mode_in.parameters.visibleRegion.height != description_in.display_mode.extent.height )
                      return ( std::numeric_limits< std::uint64_t >::max )();
                  return mode_in.parameters.refreshRate > requested_refresh
                      ? mode_in.parameters.refreshRate - requested_refresh
                      : requested_refresh - mode_in.parameters.refreshRate;
              };
              return score( left_in ) < score( right_in );
          }
    );
    if( native_result != VK_SUCCESS || mode_found == modes.end() ||
        mode_found->parameters.visibleRegion.width != description_in.display_mode.extent.width ||
        mode_found->parameters.visibleRegion.height != description_in.display_mode.extent.height )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedSurface
                , "Vulkan driver does not expose the requested DRM display mode."
            )
        );
    }

    std::uint32_t plane_count = 0U;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
        this->m_physical_device, &plane_count, nullptr );
    std::vector< VkDisplayPlanePropertiesKHR > planes( plane_count );
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
        this->m_physical_device, &plane_count, planes.data() );
    std::uint32_t plane_index = ( std::numeric_limits< std::uint32_t >::max )();
    for( std::uint32_t index = 0U; index < plane_count; ++index )
    {
        std::uint32_t supported_count = 0U;
        vkGetDisplayPlaneSupportedDisplaysKHR(
            this->m_physical_device, index, &supported_count, nullptr );
        std::vector< VkDisplayKHR > supported( supported_count );
        vkGetDisplayPlaneSupportedDisplaysKHR(
            this->m_physical_device, index, &supported_count, supported.data() );
        if( std::find( supported.begin(), supported.end(), surface.drm.display ) != supported.end() &&
            ( planes[ index ].currentDisplay == VK_NULL_HANDLE ||
              planes[ index ].currentDisplay == surface.drm.display ) )
        {
            plane_index = index;
            break;
        }
    }
    if( plane_index == ( std::numeric_limits< std::uint32_t >::max )() )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedSurface
                , "No Vulkan display plane can present to the DRM connector."
            )
        );
    }
    VkDisplayPlaneCapabilitiesKHR plane_capabilities = {};
    native_result = vkGetDisplayPlaneCapabilitiesKHR(
          this->m_physical_device
        , mode_found->displayMode
        , plane_index
        , &plane_capabilities
    );
    if( native_result != VK_SUCCESS )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to query Vulkan display-plane capabilities.", native_result ) );
    }
    if( plane_capabilities.supportedAlpha == 0U )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedSurface
                , "The Vulkan display plane exposes no supported alpha mode."
            )
        );
    }
    VkDisplayPlaneAlphaFlagBitsKHR alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    if( ( plane_capabilities.supportedAlpha & alpha_mode ) == 0U )
        alpha_mode = static_cast< VkDisplayPlaneAlphaFlagBitsKHR >(
            plane_capabilities.supportedAlpha & ( 0U - plane_capabilities.supportedAlpha ) );
    VkDisplaySurfaceCreateInfoKHR surface_description = {};
    surface_description.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surface_description.displayMode = mode_found->displayMode;
    surface_description.planeIndex = plane_index;
    surface_description.planeStackIndex = planes[ plane_index ].currentStackIndex;
    surface_description.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surface_description.globalAlpha = 1.0F;
    surface_description.alphaMode = alpha_mode;
    surface_description.imageExtent = mode_found->parameters.visibleRegion;
    native_result = vkCreateDisplayPlaneSurfaceKHR(
        this->m_instance, &surface_description, nullptr, &surface.native_surface );
    if( native_result != VK_SUCCESS )
    {
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( backendError( "Failed to create the Vulkan DRM display surface.", native_result ) );
    }
    const std::uint64_t surface_id = this->m_next_resource_id++;
    const RendererError swapchain_error = this->createSwapchain(
        &surface, surface_id, description_in.extent );
    if( !swapchain_error.ok() )
    {
        this->destroySwapchainResources( &surface );
        vkDestroySurfaceKHR( this->m_instance, surface.native_surface, nullptr );
        surface.native_surface = VK_NULL_HANDLE;
        this->restoreDrmDisplay( &surface );
        return RendererResult< sSurfaceHandle >::failure( swapchain_error );
    }
    this->m_surfaces.emplace( surface_id, std::move( surface ) );
    return RendererResult< sSurfaceHandle >::success(
        sSurfaceHandle{ surface_id, this->m_handle_generation } );
}

RendererResult< sSurfaceHandle > VulkanRendererBackend::createSurface(
    const sSurfaceDescription& description_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sSurfaceHandle >::failure( pending_error );
    if( description_in.type == eSurfaceType::Offscreen )
        return this->createOffscreenSurface( description_in );
    if( description_in.type == eSurfaceType::Window )
        return this->createWindowSurface( description_in );
    return this->createDirectDisplaySurface( description_in );
}

RendererResult< sTextureHandle > VulkanRendererBackend::getSurfaceTexture(
    const sSurfaceHandle surface_in ) const
{
    const sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererResult< sTextureHandle >::failure( resourceNotFoundError( "Surface" ) );
    if( p_surface->textures.empty() || p_surface->current_image >= p_surface->textures.size() )
    {
        return RendererResult< sTextureHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceNotFound
                , "Surface has no current Vulkan texture."
            )
        );
    }
    return RendererResult< sTextureHandle >::success(
        p_surface->textures[ p_surface->current_image ] );
}

RendererResult< sSurfaceState > VulkanRendererBackend::getSurfaceState(
    const sSurfaceHandle surface_in ) const
{
    const sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererResult< sSurfaceState >::failure( resourceNotFoundError( "Surface" ) );
    sSurfaceState state;
    state.type = p_surface->description.type;
    state.extent = p_surface->description.extent;
    state.window_mode = p_surface->window_mode;
    state.display_id = p_surface->display_id;
    state.display_mode = p_surface->display_mode;
    return RendererResult< sSurfaceState >::success( std::move( state ) );
}

RendererStatus VulkanRendererBackend::resizeSurface(
      const sSurfaceHandle     surface_in
    , const sRendererExtent2D extent_in
)
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    if( p_surface->description.type == eSurfaceType::DirectDisplay )
    {
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "A DRM/KMS DirectDisplay surface has a fixed display-mode extent."
            )
        );
    }
    if( p_surface->description.type == eSurfaceType::Offscreen )
    {
        sTextureDescription texture_description;
        texture_description.extent = extent_in;
        texture_description.format = p_surface->description.format;
        texture_description.usage =
            eTextureUsage::RenderTarget | eTextureUsage::TransferSource;
        texture_description.initial_state = eTextureState::RenderTarget;
        const auto replacement_result = this->createTexture( texture_description );
        if( !replacement_result.succeeded() )
            return RendererStatus::failure( replacement_result.error() );
        const sTextureHandle old = p_surface->textures.front();
        this->m_textures[ replacement_result.value().value ].surface_owner = surface_in.value;
        p_surface->textures.front() = replacement_result.value();
        p_surface->description.extent = extent_in;
        sTextureRecord* const p_old = this->findTexture( old );
        this->destroyTextureRecord( p_old );
        this->m_textures.erase( old.value );
        return RendererStatus::success();
    }
    if( p_surface->window_mode != eSurfaceWindowMode::Windowed )
    {
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "A fullscreen Wayland surface follows the compositor extent."
            )
        );
    }
    std::string wayland_error;
    if( !this->m_wayland->requestExtent( p_surface->window.get(), &wayland_error, extent_in ) )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Wayland resize failed: " + wayland_error
            )
        );
    }
    const RendererError recreate_error = this->recreateSwapchain(
        p_surface, surface_in.value, p_surface->window->extent );
    return recreate_error.ok()
        ? RendererStatus::success() : RendererStatus::failure( recreate_error );
}

RendererStatus VulkanRendererBackend::setSurfaceWindowMode(
      const sSurfaceHandle            surface_in
    , const sSurfaceWindowModeRequest& request_in
)
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    if( p_surface->description.type == eSurfaceType::Offscreen )
    {
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Offscreen surfaces do not have a Wayland window mode."
            )
        );
    }
    if( p_surface->description.type == eSurfaceType::DirectDisplay )
    {
        const bool same = request_in.mode == eSurfaceWindowMode::DisplayModeFullscreen &&
            request_in.display_id == p_surface->display_id &&
            sameDisplayMode( request_in.display_mode, p_surface->display_mode );
        return same ? RendererStatus::success() : RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "A DRM/KMS DirectDisplay mode is fixed for the surface lifetime."
            )
        );
    }
    if( request_in.mode == eSurfaceWindowMode::DisplayModeFullscreen )
    {
        return RendererStatus::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Wayland windows use borderless compositor fullscreen; use DirectDisplay for DRM/KMS modes."
            )
        );
    }
    if( request_in.mode == p_surface->window_mode &&
        request_in.display_id == p_surface->display_id )
    {
        return RendererStatus::success();
    }
    const bool fullscreen = request_in.mode == eSurfaceWindowMode::BorderlessFullscreen;
    std::string wayland_error;
    if( !this->m_wayland->setFullscreen(
            p_surface->window.get(), &wayland_error, request_in.display_id, fullscreen ) )
    {
        return RendererStatus::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Wayland fullscreen transition failed: " + wayland_error
            )
        );
    }
    const RendererError recreate_error = this->recreateSwapchain(
        p_surface, surface_in.value, p_surface->window->extent );
    if( !recreate_error.ok() )
    {
        std::string ignored;
        this->m_wayland->setFullscreen(
            p_surface->window.get(), &ignored, p_surface->display_id,
            p_surface->window_mode == eSurfaceWindowMode::BorderlessFullscreen );
        return RendererStatus::failure( recreate_error );
    }
    p_surface->window_mode = request_in.mode;
    p_surface->display_id = fullscreen ? request_in.display_id : std::string();
    p_surface->display_mode = {};
    return RendererStatus::success();
}

RendererResult< sSurfaceEvents > VulkanRendererBackend::pollSurfaceEvents(
    const sSurfaceHandle surface_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sSurfaceEvents >::failure( pending_error );
    sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererResult< sSurfaceEvents >::failure( resourceNotFoundError( "Surface" ) );
    sSurfaceEvents events;
    events.extent = p_surface->description.extent;
    if( p_surface->description.type != eSurfaceType::Window )
    {
        return RendererResult< sSurfaceEvents >::success( events );
    }
    std::string wayland_error;
    if( !this->m_wayland->poll( p_surface->window.get(), &events, &wayland_error ) )
    {
        return RendererResult< sSurfaceEvents >::failure( makeError(
                  eRendererErrorCategory::Backend
                , eRendererErrorCode::BackendFailure
                , "Wayland event processing failed: " + wayland_error
            )
        );
    }
    if( events.extent_changed && !events.extent.empty() &&
        ( events.extent.width != p_surface->description.extent.width ||
          events.extent.height != p_surface->description.extent.height ) )
    {
        const RendererError recreate_error = this->recreateSwapchain(
            p_surface, surface_in.value, events.extent );
        if( !recreate_error.ok() )
            return RendererResult< sSurfaceEvents >::failure( recreate_error );
        events.extent = p_surface->description.extent;
    }
    return RendererResult< sSurfaceEvents >::success( events );
}

RendererStatus VulkanRendererBackend::destroySurface( const sSurfaceHandle surface_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererStatus::failure( pending_error );
    sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererStatus::failure( resourceNotFoundError( "Surface" ) );
    this->waitDeviceIdleForRelease();
    RendererError restore_error;
    if( p_surface->description.type == eSurfaceType::Offscreen )
    {
        for( const sTextureHandle texture : p_surface->textures )
        {
            sTextureRecord* const p_texture = this->findTexture( texture );
            if( p_texture != nullptr )
            {
                this->destroyTextureRecord( p_texture );
                this->m_textures.erase( texture.value );
            }
        }
        p_surface->textures.clear();
    }
    else
    {
        this->destroySwapchainResources( p_surface );
        if( p_surface->native_surface != VK_NULL_HANDLE )
        {
            vkDestroySurfaceKHR( this->m_instance, p_surface->native_surface, nullptr );
            p_surface->native_surface = VK_NULL_HANDLE;
        }
        restore_error = this->restoreDrmDisplay( p_surface );
        if( this->m_wayland != nullptr && p_surface->window != nullptr )
            this->m_wayland->destroyWindow( &p_surface->window );
    }
    this->m_surfaces.erase( surface_in.value );
    return restore_error.ok()
        ? RendererStatus::success() : RendererStatus::failure( restore_error );
}

RendererResult< bool > VulkanRendererBackend::processSurfaceEvents(
    const sSurfaceHandle surface_in )
{
    const auto events_result = this->pollSurfaceEvents( surface_in );
    if( !events_result.succeeded() )
        return RendererResult< bool >::failure( events_result.error() );
    return RendererResult< bool >::success( events_result.value().alive );
}

RendererResult< sFenceHandle > VulkanRendererBackend::presentSurface(
    const sSurfaceHandle surface_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sFenceHandle >::failure( pending_error );
    sSurfaceRecord* const p_surface = this->findSurface( surface_in );
    if( p_surface == nullptr )
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Surface" ) );
    if( p_surface->description.type == eSurfaceType::Offscreen )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Offscreen surfaces cannot be presented."
            )
        );
    }
    if( p_surface->current_image >= p_surface->textures.size() )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceNotFound
                , "Vulkan swapchain image index is invalid."
            )
        );
    }
    sTextureRecord* const p_texture = this->findTexture(
        p_surface->textures[ p_surface->current_image ] );
    if( p_texture == nullptr )
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Surface texture" ) );
    if( p_surface->acquire_pending )
    {
        auto work = std::make_unique< sVulkanSubmission >(
            this->m_submission_calls, this->m_device, this->m_command_pool );
        const auto command_result = this->beginCommands();
        if( !command_result.succeeded() )
            return RendererResult< sFenceHandle >::failure( command_result.error() );
        work->command = command_result.value();
        transitionImage(
              work->command
            , p_texture->image
            , p_texture->layout
            , VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        bool submitted = false;
        const auto consume_result = this->submitAndWait(
            work, &submitted, p_surface->acquire_semaphore );
        if( submitted )
        {
            p_surface->acquire_pending = false;
            p_texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            p_texture->state = eTextureState::Present;
        }
        if( consume_result != VK_SUCCESS ) return this->submissionResult( consume_result );
    }
    if( p_texture->layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Surface texture must finish rendering in Present state."
            )
        );
    }
    VkPresentInfoKHR presentation = {};
    presentation.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentation.swapchainCount = 1U;
    presentation.pSwapchains = &p_surface->swap_chain;
    presentation.pImageIndices = &p_surface->current_image;
    VkResult result = vkQueuePresentKHR(
        this->m_queues[ p_surface->present_family ], &presentation );
    if( result == VK_ERROR_OUT_OF_DATE_KHR &&
        p_surface->description.type == eSurfaceType::Window )
    {
        const RendererError recreate_error = this->recreateSwapchain(
            p_surface, surface_in.value, p_surface->window->extent );
        if( !recreate_error.ok() )
            return RendererResult< sFenceHandle >::failure( recreate_error );
        return this->completedFence();
    }
    if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Vulkan surface presentation failed.", result ) );
    }
    result = vkQueueWaitIdle( this->m_queues[ p_surface->present_family ] );
    if( result == VK_SUCCESS )
    {
        result = vkAcquireNextImageKHR(
              this->m_device
            , p_surface->swap_chain
            , ( std::numeric_limits< std::uint64_t >::max )()
            , p_surface->acquire_semaphore
            , VK_NULL_HANDLE
            , &p_surface->current_image
        );
    }
    if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to acquire the next Vulkan surface image.", result ) );
    }
    p_surface->acquire_pending = true;
    return this->completedFence();
}

RendererResult< sFenceHandle > VulkanRendererBackend::executeRenderPass(
    const sRenderPassDescription& description_in )
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sFenceHandle >::failure( pending_error );
    sTextureRecord* const p_target = this->findTexture( description_in.color_attachment );
    if( p_target == nullptr )
        return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Texture" ) );
    if( !hasTextureUsage( p_target->description.usage, eTextureUsage::RenderTarget ) )
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
                , "Vulkan attachment discard operations are not implemented."
            )
        );
    }
    if( description_in.final_state == eTextureState::CopySource &&
        !hasTextureUsage( p_target->description.usage, eTextureUsage::TransferSource ) )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "CopySource final state requires TransferSource usage."
            )
        );
    }
    if( description_in.final_state == eTextureState::Present &&
        p_target->surface_owner == 0U )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedOperation
                , "Present final state requires a presentation-surface texture."
            )
        );
    }
    const std::uint32_t area_width = description_in.render_area.extent.empty()
        ? p_target->description.extent.width : description_in.render_area.extent.width;
    const std::uint32_t area_height = description_in.render_area.extent.empty()
        ? p_target->description.extent.height : description_in.render_area.extent.height;
    if( area_width > p_target->description.extent.width ||
        area_height > p_target->description.extent.height ||
        description_in.render_area.x > p_target->description.extent.width - area_width ||
        description_in.render_area.y > p_target->description.extent.height - area_height )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Render area is outside the Vulkan attachment extent."
            )
        );
    }
    struct sResolvedDraw
    {
        sMeshRecord* p_mesh;
        sTextureRecord* p_source;
        sTextureRecord* p_alpha;
        const sMeshDrawCommand* p_description;

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
    std::vector< sResolvedDraw > draws;
    draws.reserve( description_in.draw_commands.size() );
    for( const sMeshDrawCommand& draw : description_in.draw_commands )
    {
        sMeshRecord* const p_mesh = this->findMesh( draw.mesh );
        sTextureRecord* const p_source = this->findTexture( draw.source_texture );
        sTextureRecord* const p_alpha = draw.alpha_texture.valid()
            ? this->findTexture( draw.alpha_texture ) : nullptr;
        if( p_mesh == nullptr )
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Mesh" ) );
        if( p_source == nullptr )
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Source texture" ) );
        if( draw.alpha_texture.valid() && p_alpha == nullptr )
            return RendererResult< sFenceHandle >::failure( resourceNotFoundError( "Alpha texture" ) );
        if( !hasTextureUsage( p_source->description.usage, eTextureUsage::Sampled ) ||
            ( p_alpha != nullptr &&
              !hasTextureUsage( p_alpha->description.usage, eTextureUsage::Sampled ) ) )
        {
            return RendererResult< sFenceHandle >::failure( makeError(
                      eRendererErrorCategory::Validation
                    , eRendererErrorCode::InvalidDescription
                    , "Vulkan draw textures require Sampled usage."
                )
            );
        }
        draws.emplace_back( sResolvedDraw{ p_mesh, p_source, p_alpha, &draw } );
    }
    const VkFormat target_format = toNativeFormat( p_target->description.format );
    if( target_format != VK_FORMAT_R8G8B8A8_UNORM &&
        target_format != VK_FORMAT_B8G8R8A8_UNORM )
    {
        return RendererResult< sFenceHandle >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "Vulkan mesh drawing supports RGBA8 and BGRA8 attachments."
            )
        );
    }
    const bool clear = description_in.load_operation == eAttachmentLoadOperation::Clear;
    const VkRenderPass render_pass = this->findOrCreateRenderPass( target_format, clear );
    if( render_pass == VK_NULL_HANDLE )
    {
        return RendererResult< sFenceHandle >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::BackendFailure
                , "Failed to create the Vulkan render pass."
            )
        );
    }
    auto work = std::make_unique< sVulkanSubmission >(
        this->m_submission_calls, this->m_device, this->m_command_pool );
    VkFramebuffer& framebuffer = work->framebuffer;
    VkFramebufferCreateInfo framebuffer_description = {};
    framebuffer_description.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_description.renderPass = render_pass;
    framebuffer_description.attachmentCount = 1U;
    framebuffer_description.pAttachments = &p_target->view;
    framebuffer_description.width = p_target->description.extent.width;
    framebuffer_description.height = p_target->description.extent.height;
    framebuffer_description.layers = 1U;
    VkResult native_result = vkCreateFramebuffer(
        this->m_device, &framebuffer_description, nullptr, &framebuffer );
    if( native_result != VK_SUCCESS )
    {
        return RendererResult< sFenceHandle >::failure( backendError( "Failed to create the Vulkan framebuffer.", native_result ) );
    }

    VkDescriptorPool& descriptor_pool = work->descriptor_pool;
    std::vector< VkDescriptorSet > descriptor_sets( draws.size(), VK_NULL_HANDLE );
    if( !draws.empty() )
    {
        VkDescriptorPoolSize pool_size = {};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = static_cast< std::uint32_t >( draws.size() * 2U );
        VkDescriptorPoolCreateInfo pool_description = {};
        pool_description.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_description.maxSets = static_cast< std::uint32_t >( draws.size() );
        pool_description.poolSizeCount = 1U;
        pool_description.pPoolSizes = &pool_size;
        native_result = vkCreateDescriptorPool(
            this->m_device, &pool_description, nullptr, &descriptor_pool );
        std::vector< VkDescriptorSetLayout > layouts(
            draws.size(), this->m_descriptor_layout );
        VkDescriptorSetAllocateInfo allocation = {};
        allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation.descriptorPool = descriptor_pool;
        allocation.descriptorSetCount = static_cast< std::uint32_t >( layouts.size() );
        allocation.pSetLayouts = layouts.data();
        if( native_result == VK_SUCCESS )
        {
            native_result = vkAllocateDescriptorSets(
                this->m_device, &allocation, descriptor_sets.data() );
        }
        if( native_result != VK_SUCCESS )
        {
            return RendererResult< sFenceHandle >::failure( backendError( "Failed to allocate Vulkan draw descriptors.", native_result ) );
        }
        for( std::size_t index = 0U; index < draws.size(); ++index )
        {
            const sResolvedDraw& draw = draws[ index ];
            const sTextureRecord* const p_alpha = draw.p_alpha == nullptr
                ? draw.p_source : draw.p_alpha;
            const VkSampler sampler =
                draw.p_description->sampling_filter == eTextureSamplingFilter::Linear
                    ? this->m_linear_sampler : this->m_nearest_sampler;
            VkDescriptorImageInfo images[ 2U ] = {};
            images[ 0U ].sampler = sampler;
            images[ 0U ].imageView = draw.p_source->view;
            images[ 0U ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            images[ 1U ].sampler = sampler;
            images[ 1U ].imageView = p_alpha->view;
            images[ 1U ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet writes[ 2U ] = {};
            for( std::uint32_t binding = 0U; binding < 2U; ++binding )
            {
                writes[ binding ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[ binding ].dstSet = descriptor_sets[ index ];
                writes[ binding ].dstBinding = binding;
                writes[ binding ].descriptorCount = 1U;
                writes[ binding ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[ binding ].pImageInfo = &images[ binding ];
            }
            vkUpdateDescriptorSets( this->m_device, 2U, writes, 0U, nullptr );
        }
    }

    const auto command_result = this->beginCommands();
    if( !command_result.succeeded() )
    {
        return RendererResult< sFenceHandle >::failure( command_result.error() );
    }
    const VkCommandBuffer command = command_result.value();
    work->command = command;
    transitionImage(
        command, p_target->image, p_target->layout,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
    std::vector< sTextureRecord* > transitioned_sources;
    for( const sResolvedDraw& draw : draws )
    {
        sTextureRecord* sampled[] = { draw.p_source, draw.p_alpha };
        for( sTextureRecord* const p_sampled : sampled )
        {
            if( p_sampled != nullptr && p_sampled->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                std::find( transitioned_sources.begin(), transitioned_sources.end(), p_sampled ) ==
                    transitioned_sources.end() )
            {
                transitionImage(
                    command, p_sampled->image, p_sampled->layout,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
                transitioned_sources.emplace_back( p_sampled );
            }
        }
    }
    VkClearValue clear_value = {};
    clear_value.color.float32[ 0U ] = description_in.clear_color.red;
    clear_value.color.float32[ 1U ] = description_in.clear_color.green;
    clear_value.color.float32[ 2U ] = description_in.clear_color.blue;
    clear_value.color.float32[ 3U ] = description_in.clear_color.alpha;
    VkRenderPassBeginInfo render_begin = {};
    render_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_begin.renderPass = render_pass;
    render_begin.framebuffer = framebuffer;
    render_begin.renderArea.offset = {
        static_cast< std::int32_t >( description_in.render_area.x ),
        static_cast< std::int32_t >( description_in.render_area.y ) };
    render_begin.renderArea.extent = { area_width, area_height };
    render_begin.clearValueCount = clear ? 1U : 0U;
    render_begin.pClearValues = clear ? &clear_value : nullptr;
    vkCmdBeginRenderPass( command, &render_begin, VK_SUBPASS_CONTENTS_INLINE );
    VkViewport viewport = {};
    viewport.x = static_cast< float >( description_in.render_area.x );
    viewport.y = static_cast< float >( description_in.render_area.y + area_height );
    viewport.width = static_cast< float >( area_width );
    viewport.height = -static_cast< float >( area_height );
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    VkRect2D scissor = render_begin.renderArea;
    vkCmdSetViewport( command, 0U, 1U, &viewport );
    vkCmdSetScissor( command, 0U, 1U, &scissor );
    for( std::size_t index = 0U; index < draws.size(); ++index )
    {
        const sResolvedDraw& draw = draws[ index ];
        const VkPipeline pipeline = this->findOrCreatePipeline(
            target_format, draw.p_description->blend_mode, draw.p_mesh->topology );
        if( pipeline == VK_NULL_HANDLE )
        {
            vkCmdEndRenderPass( command );
            vkEndCommandBuffer( command );
            return RendererResult< sFenceHandle >::failure( makeError(
                      eRendererErrorCategory::Resource
                    , eRendererErrorCode::BackendFailure
                    , "Failed to create the Vulkan graphics pipeline."
                )
            );
        }
        vkCmdBindPipeline( command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
        const VkDeviceSize offset = 0U;
        vkCmdBindVertexBuffers( command, 0U, 1U, &draw.p_mesh->vertex_buffer, &offset );
        vkCmdBindIndexBuffer(
            command, draw.p_mesh->index_buffer, 0U, VK_INDEX_TYPE_UINT32 );
        vkCmdBindDescriptorSets(
              command
            , VK_PIPELINE_BIND_POINT_GRAPHICS
            , this->m_pipeline_layout
            , 0U
            , 1U
            , &descriptor_sets[ index ]
            , 0U
            , nullptr
        );
        sDrawParameters parameters;
        if( draw.p_alpha != nullptr )
        {
            parameters.flags |= DRAW_FLAG_USE_ALPHA_MAP;
            if( draw.p_alpha->description.format == eRendererPixelFormat::R8Unorm )
                parameters.flags |= DRAW_FLAG_ALPHA_RED_CHANNEL;
        }
        if( draw.p_description->sampling_filter == eTextureSamplingFilter::Linear )
            parameters.flags |= DRAW_FLAG_LINEAR_SAMPLING;
        parameters.opacity = draw.p_description->opacity;
        parameters.edge_left = draw.p_description->edge_blend.left;
        parameters.edge_right = draw.p_description->edge_blend.right;
        parameters.edge_top = draw.p_description->edge_blend.top;
        parameters.edge_bottom = draw.p_description->edge_blend.bottom;
        if( parameters.edge_left > 0.0F || parameters.edge_right > 0.0F ||
            parameters.edge_top > 0.0F || parameters.edge_bottom > 0.0F )
        {
            parameters.flags |= DRAW_FLAG_USE_EDGE_BLEND;
            if( draw.p_description->edge_blend.curve == eEdgeBlendCurve::Smoothstep )
                parameters.flags |= DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND;
        }
        vkCmdPushConstants(
              command
            , this->m_pipeline_layout
            , VK_SHADER_STAGE_FRAGMENT_BIT
            , 0U
            , sizeof( parameters )
            , &parameters
        );
        vkCmdDrawIndexed( command, draw.p_mesh->index_count, 1U, 0U, 0, 0U );
    }
    vkCmdEndRenderPass( command );
    const VkImageLayout final_layout = toNativeLayout( description_in.final_state );
    transitionImage(
        command, p_target->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, final_layout );
    VkSemaphore wait_semaphore = VK_NULL_HANDLE;
    sSurfaceRecord* p_owner = nullptr;
    if( p_target->surface_owner != 0U )
    {
        const auto owner = this->m_surfaces.find( p_target->surface_owner );
        if( owner != this->m_surfaces.end() )
        {
            p_owner = &owner->second;
            if( p_owner->acquire_pending &&
                p_owner->current_image < p_owner->textures.size() &&
                p_owner->textures[ p_owner->current_image ].value ==
                    description_in.color_attachment.value )
            {
                wait_semaphore = p_owner->acquire_semaphore;
            }
        }
    }
    bool submitted = false;
    const VkResult result = this->submitAndWait( work, &submitted, wait_semaphore );
    if( submitted )
    {
        p_target->layout = final_layout;
        p_target->state = description_in.final_state;
        for( sTextureRecord* const p_source : transitioned_sources )
        {
            p_source->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            p_source->state = eTextureState::ShaderResource;
        }
        if( p_owner != nullptr && wait_semaphore != VK_NULL_HANDLE )
            p_owner->acquire_pending = false;
    }
    return this->submissionResult( result );
}

RendererStatus VulkanRendererBackend::waitFence(
      const sFenceHandle  fence_in
    , const std::uint32_t timeout_ms_in
)
{
    (void)timeout_ms_in;
    if( fence_in.generation != this->m_handle_generation ||
        this->m_completed_fences.find( fence_in.value ) == this->m_completed_fences.end() )
    {
        return RendererStatus::failure( resourceNotFoundError( "Fence" ) );
    }
    return RendererStatus::success();
}

RendererResult< sRendererFrame > VulkanRendererBackend::readTexture(
      const sTextureHandle texture_in
    , const std::uint32_t  timeout_ms_in
)
{
    const RendererError pending_error = this->pollPendingSubmission();
    if( !pending_error.ok() ) return RendererResult< sRendererFrame >::failure( pending_error );
    sTextureRecord* const p_texture = this->findTexture( texture_in );
    if( p_texture == nullptr )
        return RendererResult< sRendererFrame >::failure( resourceNotFoundError( "Texture" ) );
    if( !hasTextureUsage( p_texture->description.usage, eTextureUsage::TransferSource ) )
    {
        return RendererResult< sRendererFrame >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidDescription
                , "Texture readback requires TransferSource usage."
            )
        );
    }
    sRendererFrame frame;
    frame.description.extent = p_texture->description.extent;
    frame.description.format = p_texture->description.format;
    const std::size_t data_size = frame.description.memorySize();
    if( data_size == 0U )
    {
        return RendererResult< sRendererFrame >::failure( unsupportedError(
                  eRendererErrorCode::UnsupportedFormat
                , "Vulkan texture format cannot be read back."
            )
        );
    }
    // Allocate host output before mapping or submitting, so allocation failure cannot
    // strand a mapping or require allocation while retaining an unresolved submission.
    frame.data.resize( data_size );
    auto work = std::make_unique< sVulkanSubmission >(
        this->m_submission_calls, this->m_device, this->m_command_pool );
    VkBuffer& readback_buffer = work->buffer;
    VkDeviceMemory& readback_memory = work->buffer_memory;
    RendererError error = this->createBuffer(
          &readback_buffer
        , &readback_memory
        , data_size
        , VK_BUFFER_USAGE_TRANSFER_DST_BIT
        , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if( !error.ok() )
        return RendererResult< sRendererFrame >::failure( error );
    const auto command_result = this->beginCommands();
    auto result = RendererResult< sRendererFrame >::success( {} );
    if( !command_result.succeeded() )
    {
        result = RendererResult< sRendererFrame >::failure( command_result.error() );
    }
    else
    {
        const VkCommandBuffer command = command_result.value();
        work->command = command;
        const VkImageLayout original_layout = p_texture->layout;
        transitionImage(
            command, p_texture->image, original_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1U;
        region.imageExtent = {
              p_texture->description.extent.width
            , p_texture->description.extent.height
            , 1U
        };
        vkCmdCopyImageToBuffer(
              command
            , p_texture->image
            , VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            , readback_buffer
            , 1U
            , &region
        );
        transitionImage(
            command, p_texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, original_layout );
        bool submitted = false;
        const auto submit_result = this->submissionResult(
            this->submitAndWait( work, &submitted, VK_NULL_HANDLE, timeout_ms_in ) );
        if( !submit_result.succeeded() )
        {
            result = RendererResult< sRendererFrame >::failure( submit_result.error() );
        }
        else
        {
            void* p_mapped = nullptr;
            const VkResult map_result = vkMapMemory(
                this->m_device, readback_memory, 0U, data_size, 0U, &p_mapped );
            if( map_result != VK_SUCCESS )
            {
                result = RendererResult< sRendererFrame >::failure( backendError( "Failed to map the Vulkan readback buffer.", map_result ) );
            }
            else
            {
                std::memcpy( frame.data.data(), p_mapped, data_size );
                vkUnmapMemory( this->m_device, readback_memory );
                result = RendererResult< sRendererFrame >::success( std::move( frame ) );
            }
        }
    }
    return result;
}

std::unique_ptr< RendererBackend > createPlatformRendererBackend(
          RendererError* const          p_error_out
    , const sRendererConfiguration& configuration_in
)
{
    if( p_error_out == nullptr )
        return nullptr;
    if( configuration_in.backend == eRendererBackend::Direct3D12 )
    {
        *p_error_out = unsupportedError(
              eRendererErrorCode::UnsupportedBackend
            , "Direct3D 12 is unavailable on Linux."
        );
        return nullptr;
    }
    *p_error_out = RendererError();
    return std::make_unique< VulkanRendererBackend >();
}

} // namespace internal
} // namespace oui
} // namespace wse
