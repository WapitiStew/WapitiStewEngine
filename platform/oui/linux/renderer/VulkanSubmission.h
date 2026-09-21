// Internal Vulkan submission ownership; never installed as public API.
#ifndef WSE_PLATFORM_OUI_VULKAN_SUBMISSION_H
#define WSE_PLATFORM_OUI_VULKAN_SUBMISSION_H

#include <vulkan/vulkan.h>

namespace wse::oui::internal
{

// Injectable only in the internal backend test. Production uses the loader entry points.
struct sVulkanSubmissionCalls
{
    PFN_vkEndCommandBuffer end_command = vkEndCommandBuffer;
    PFN_vkCreateFence create_fence = vkCreateFence;
    PFN_vkQueueSubmit queue_submit = vkQueueSubmit;
    PFN_vkWaitForFences wait_fences = vkWaitForFences;
    PFN_vkDeviceWaitIdle device_wait_idle = vkDeviceWaitIdle;
    PFN_vkFreeCommandBuffers free_commands = vkFreeCommandBuffers;
    PFN_vkDestroyFence destroy_fence = vkDestroyFence;
    PFN_vkDestroyDescriptorPool destroy_descriptor_pool = vkDestroyDescriptorPool;
    PFN_vkDestroyFramebuffer destroy_framebuffer = vkDestroyFramebuffer;
    PFN_vkDestroyBuffer destroy_buffer = vkDestroyBuffer;
    PFN_vkDestroyImageView destroy_image_view = vkDestroyImageView;
    PFN_vkDestroyImage destroy_image = vkDestroyImage;
    PFN_vkFreeMemory free_memory = vkFreeMemory;
};

// One owner is allocated before recording/submission. On an unresolved wait it moves
// without allocation into the backend's single pending slot. Its destructor is called
// only before submission, after fence completion, or after device-idle/lost teardown.
// Referenced textures, meshes, pipelines and acquire semaphores stay in the backend;
// every mutating operation polls that slot before touching those resources.
struct sVulkanSubmission
{
    const sVulkanSubmissionCalls& calls;
    VkDevice device;
    VkCommandPool command_pool;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;

    sVulkanSubmission(
          const sVulkanSubmissionCalls& calls_in
        , VkDevice device_in
        , VkCommandPool command_pool_in
    ) noexcept
        : calls( calls_in ), device( device_in ), command_pool( command_pool_in )
    {
    }

    sVulkanSubmission( const sVulkanSubmission& ) = delete;
    sVulkanSubmission& operator=( const sVulkanSubmission& ) = delete;

    ~sVulkanSubmission()
    {
        if( command != VK_NULL_HANDLE )
            calls.free_commands( device, command_pool, 1U, &command );
        if( fence != VK_NULL_HANDLE ) calls.destroy_fence( device, fence, nullptr );
        if( descriptor_pool != VK_NULL_HANDLE )
            calls.destroy_descriptor_pool( device, descriptor_pool, nullptr );
        if( framebuffer != VK_NULL_HANDLE )
            calls.destroy_framebuffer( device, framebuffer, nullptr );
        if( buffer != VK_NULL_HANDLE ) calls.destroy_buffer( device, buffer, nullptr );
        if( buffer_memory != VK_NULL_HANDLE ) calls.free_memory( device, buffer_memory, nullptr );
        if( image_view != VK_NULL_HANDLE ) calls.destroy_image_view( device, image_view, nullptr );
        if( image != VK_NULL_HANDLE ) calls.destroy_image( device, image, nullptr );
        if( image_memory != VK_NULL_HANDLE ) calls.free_memory( device, image_memory, nullptr );
    }
};

} // namespace wse::oui::internal
#endif
