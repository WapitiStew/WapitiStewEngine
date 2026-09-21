// Exercises the actual internal backend with real offscreen Vulkan resources and
// deterministic native failures. This is contract evidence, not hardware certification.
// The implementation is compiled here so no injectable seam enters the installed ABI.
#include "../../platform/oui/linux/renderer/VulkanRendererBackend.cpp"
#include <oui/renderer/Renderer.h>

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace
{
bool fail_allocations = false;
bool fail_after_wait = false;
bool force_wait = false;
VkResult wait_result = VK_SUCCESS;
VkResult end_result = VK_SUCCESS;
VkResult fence_result = VK_SUCCESS;
VkResult queue_result = VK_SUCCESS;
unsigned idle_failures = 0;
unsigned idle_calls = 0;
unsigned queue_calls = 0;
unsigned wait_calls = 0;
std::uint64_t last_timeout = 0;
struct Counts
{
    unsigned command = 0, fence = 0, buffer = 0, memory = 0;
    unsigned descriptor = 0, framebuffer = 0, image = 0, view = 0;
    unsigned total() const
    { return command + fence + buffer + memory + descriptor + framebuffer + image + view; }
} released;

void require( bool value_in, const char* message_in )
{
    if( !value_in ) throw std::runtime_error( message_in );
}
VKAPI_ATTR VkResult VKAPI_CALL endCommand( VkCommandBuffer command_in )
{ return end_result == VK_SUCCESS ? vkEndCommandBuffer( command_in ) : end_result; }
VKAPI_ATTR VkResult VKAPI_CALL createFence( VkDevice device_in, const VkFenceCreateInfo* info_in,
    const VkAllocationCallbacks* allocator_in, VkFence* fence_out )
{ return fence_result == VK_SUCCESS ? vkCreateFence( device_in, info_in, allocator_in, fence_out ) : fence_result; }
VKAPI_ATTR VkResult VKAPI_CALL queueSubmit( VkQueue queue_in, std::uint32_t count_in,
    const VkSubmitInfo* info_in, VkFence fence_in )
{
    ++queue_calls;
    return queue_result == VK_SUCCESS ? vkQueueSubmit( queue_in, count_in, info_in, fence_in ) : queue_result;
}
VKAPI_ATTR VkResult VKAPI_CALL waitFences( VkDevice device_in, std::uint32_t count_in,
    const VkFence* fences_in, VkBool32 all_in, std::uint64_t timeout_in )
{
    ++wait_calls;
    last_timeout = timeout_in;
    if( fail_after_wait ) fail_allocations = true;
    return force_wait ? wait_result : vkWaitForFences( device_in, count_in, fences_in, all_in, timeout_in );
}
VKAPI_ATTR VkResult VKAPI_CALL deviceIdle( VkDevice device_in )
{
    ++idle_calls;
    if( idle_failures != 0 )
    {
        --idle_failures;
        require( released.total() == 0, "idle error freed unresolved work" );
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return vkDeviceWaitIdle( device_in );
}
VKAPI_ATTR void VKAPI_CALL freeCommands( VkDevice device_in, VkCommandPool pool_in,
    std::uint32_t count_in, const VkCommandBuffer* commands_in )
{ released.command += count_in; vkFreeCommandBuffers( device_in, pool_in, count_in, commands_in ); }
#define WSE_COUNT_RELEASE(function_name, native_name, native_type, counter) \
    VKAPI_ATTR void VKAPI_CALL function_name( VkDevice device_in, native_type handle_in, \
        const VkAllocationCallbacks* allocator_in ) \
    { ++released.counter; native_name( device_in, handle_in, allocator_in ); }
WSE_COUNT_RELEASE(destroyFence, vkDestroyFence, VkFence, fence)
WSE_COUNT_RELEASE(destroyBuffer, vkDestroyBuffer, VkBuffer, buffer)
WSE_COUNT_RELEASE(freeMemory, vkFreeMemory, VkDeviceMemory, memory)
WSE_COUNT_RELEASE(destroyDescriptor, vkDestroyDescriptorPool, VkDescriptorPool, descriptor)
WSE_COUNT_RELEASE(destroyFramebuffer, vkDestroyFramebuffer, VkFramebuffer, framebuffer)
WSE_COUNT_RELEASE(destroyImage, vkDestroyImage, VkImage, image)
WSE_COUNT_RELEASE(destroyView, vkDestroyImageView, VkImageView, view)
#undef WSE_COUNT_RELEASE

void armWait( VkResult result_in )
{
    force_wait = true;
    wait_result = result_in;
    released = {};
    wait_calls = 0;
}
}

void* operator new( std::size_t size_in )
{
    if( fail_allocations ) throw std::bad_alloc();
    if( void* allocation = std::malloc( size_in == 0 ? 1 : size_in ) ) return allocation;
    throw std::bad_alloc();
}
void operator delete( void* allocation_in ) noexcept { std::free( allocation_in ); }
void operator delete( void* allocation_in, std::size_t ) noexcept { std::free( allocation_in ); }

namespace wse::oui::internal
{
struct VulkanRendererBackendTestAccess
{
    static void instrument( VulkanRendererBackend& backend_inout )
    {
        auto& calls = backend_inout.m_submission_calls;
        calls.end_command = endCommand;
        calls.create_fence = createFence;
        calls.queue_submit = queueSubmit;
        calls.wait_fences = waitFences;
        calls.device_wait_idle = deviceIdle;
        calls.free_commands = freeCommands;
        calls.destroy_fence = destroyFence;
        calls.destroy_buffer = destroyBuffer;
        calls.free_memory = freeMemory;
        calls.destroy_descriptor_pool = destroyDescriptor;
        calls.destroy_framebuffer = destroyFramebuffer;
        calls.destroy_image = destroyImage;
        calls.destroy_image_view = destroyView;
    }
    static bool pending( const VulkanRendererBackend& backend_in )
    { return backend_in.m_pending_submission != nullptr; }
    static void drain( VulkanRendererBackend& backend_inout )
    { require( vkDeviceWaitIdle( backend_inout.m_device ) == VK_SUCCESS, "real device drain" ); }
    static void reap( VulkanRendererBackend& backend_inout )
    {
        drain( backend_inout );
        force_wait = false;
        require( backend_inout.pollPendingSubmission().ok(), "pending work must recover" );
        require( !pending( backend_inout ), "pending owner must be released" );
    }
    static VkImageLayout layout( VulkanRendererBackend& backend_inout, sTextureHandle texture_in )
    { return backend_inout.findTexture( texture_in )->layout; }
    // Simulate an acquired presentation image using an ordinary image and a real signaled
    // semaphore. No window or physical display is opened; draw consumes the acquisition.
    static void acquired( VulkanRendererBackend& backend_inout, sSurfaceHandle surface_in )
    {
        auto& surface = *backend_inout.findSurface( surface_in );
        VkSemaphoreCreateInfo description = {};
        description.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        require( vkCreateSemaphore( backend_inout.m_device, &description, nullptr,
            &surface.acquire_semaphore ) == VK_SUCCESS, "acquire semaphore" );
        VkSubmitInfo signal = {};
        signal.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        signal.signalSemaphoreCount = 1;
        signal.pSignalSemaphores = &surface.acquire_semaphore;
        require( vkQueueSubmit( backend_inout.m_graphics_queue, 1, &signal, VK_NULL_HANDLE ) == VK_SUCCESS,
            "signal acquire semaphore" );
        surface.description.type = eSurfaceType::Window;
        surface.acquire_pending = true;
    }
    static bool acquirePending( VulkanRendererBackend& backend_inout, sSurfaceHandle surface_in )
    { return backend_inout.findSurface( surface_in )->acquire_pending; }
    static void releaseAcquired( VulkanRendererBackend& backend_inout, sSurfaceHandle surface_in )
    {
        auto& surface = *backend_inout.findSurface( surface_in );
        vkDestroySemaphore( backend_inout.m_device, surface.acquire_semaphore, nullptr );
        surface.acquire_semaphore = VK_NULL_HANDLE;
        surface.description.type = eSurfaceType::Offscreen;
    }
};
}

int main()
{
    using namespace wse::oui;
    using namespace wse::oui::internal;
    using Probe = VulkanRendererBackendTestAccess;
    try
    {
        Renderer facade;
        const auto infinite = facade.readTexture( { 1U, 1U }, 0xffffffffU );
        require( !infinite.succeeded() && infinite.error().code() == eRendererErrorCode::InvalidArgument,
            "facade must reject UINT32_MAX before native work" );
        VulkanRendererBackend backend;
        sRendererConfiguration configuration;
        configuration.backend = eRendererBackend::Vulkan12;
        require( backend.initialize( configuration ).succeeded(), "initialize Vulkan" );
        Probe::instrument( backend );
        sTextureDescription texture;
        texture.extent = { 2, 2 };
        texture.format = eRendererPixelFormat::Rgba8Unorm;
        texture.usage = eTextureUsage::Sampled | eTextureUsage::TransferDestination | eTextureUsage::TransferSource;
        texture.initial_state = eTextureState::CopyDestination;
        const auto source_result = backend.createTexture( texture );
        require( source_result.succeeded(), "create source" );
        const auto source = source_result.value();
        sRendererFrame frame;
        frame.description.extent = texture.extent;
        frame.description.format = texture.format;
        frame.data.assign( 16, 127 );
        sMeshDescription mesh_description;
        mesh_description.vertices = { { -1, 1, 0, 0 }, { 1, 1, 1, 0 }, { 1, -1, 1, 1 }, { -1, -1, 0, 1 } };
        mesh_description.indices = { 0, 1, 2, 0, 2, 3 };
        const auto mesh = backend.createMesh( mesh_description ).value();
        sSurfaceDescription surface_description;
        surface_description.extent = { 2, 2 };
        const auto surface = backend.createSurface( surface_description ).value();
        const auto target = backend.getSurfaceTexture( surface ).value();

        // Timeout retains the upload staging allocation and freezes every mutating path.
        armWait( VK_TIMEOUT );
        require( backend.uploadTexture( source, frame ).error().code() == eRendererErrorCode::TimedOut, "upload Timeout" );
        require( released.total() == 0 && Probe::pending( backend ), "upload premature free" );
        require( last_timeout == 30000000000ULL, "upload internal timeout" );
        require( Probe::layout( backend, source ) == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "enqueue layout commit" );
        const unsigned queued = queue_calls;
        auto busy = []( const auto& result_in ) {
            require( !result_in.succeeded() && result_in.error().code() == eRendererErrorCode::ResourceInUse,
                "pending work must reject mutation" );
        };
        sRenderPassDescription pass;
        pass.color_attachment = target;
        pass.final_state = eTextureState::CopySource;
        pass.draw_commands.emplace_back( sMeshDrawCommand{ mesh, source } );
        busy( backend.destroyTexture( source ) );
        busy( backend.uploadTexture( source, frame ) );
        busy( backend.createTexture( texture ) );
        busy( backend.createMesh( mesh_description ) );
        busy( backend.updateMesh( mesh, mesh_description ) );
        busy( backend.destroyMesh( mesh ) );
        busy( backend.createSurface( surface_description ) );
        busy( backend.resizeSurface( surface, { 3, 3 } ) );
        busy( backend.setSurfaceWindowMode( surface, {} ) );
        busy( backend.pollSurfaceEvents( surface ) );
        busy( backend.processSurfaceEvents( surface ) );
        busy( backend.destroySurface( surface ) );
        busy( backend.presentSurface( surface ) );
        busy( backend.executeRenderPass( pass ) );
        busy( backend.readTexture( source, 1 ) );
        require( released.total() == 0 && queue_calls == queued && last_timeout == 0, "guard changed native resources" );
        Probe::reap( backend );
        require( released.command == 1 && released.fence == 1 && released.buffer == 1 && released.memory == 1,
            "upload exactly-once release" );
        const auto release_count = released.total();
        Probe::reap( backend );
        require( released.total() == release_count, "double release" );
        require( backend.readTexture( source, 30000 ).value().data == frame.data, "upload/readback content after recovery" );

        // Caller timeout, including zero and maximum finite value, reaches the native wait.
        for( const std::uint32_t timeout : { 0U, 1U, 27U, 0xfffffffeU } )
        {
            armWait( VK_TIMEOUT );
            const auto read = backend.readTexture( source, timeout );
            require( !read.succeeded() && read.error().code() == eRendererErrorCode::TimedOut, "read Timeout result" );
            require( wait_calls == 1 && last_timeout == static_cast< std::uint64_t >( timeout ) * 1000000ULL,
                "read timeout conversion/hidden wait" );
            require( released.total() == 0, "readback premature free" );
            Probe::reap( backend );
            require( released.buffer == 1 && released.memory == 1 && released.command == 1 && released.fence == 1,
                "readback exactly-once release" );
        }
        // Failed initialization has no public handle, but its image/view/memory remain owned.
        armWait( VK_TIMEOUT );
        require( !backend.createTexture( texture ).succeeded(), "initial transition timeout" );
        require( released.total() == 0, "orphan texture premature free" );
        Probe::reap( backend );
        require( released.image == 1 && released.view == 1 && released.memory == 1, "orphan texture release" );

        for( VkResult error : { VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_UNKNOWN } )
        {
            armWait( error );
            const auto draw = backend.executeRenderPass( pass );
            require( !draw.succeeded() && draw.error().nativeCode() == error, "draw native wait error" );
            require( released.total() == 0, "draw premature free" );
            require( Probe::layout( backend, target ) == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "draw layout after failed wait" );
            Probe::reap( backend );
            require( released.descriptor == 1 && released.framebuffer == 1, "draw transient release" );
        }
        require( backend.readTexture( target, 30000 ).value().data == frame.data, "draw pixels after recovery" );

        // Failures before enqueue release immediately and do not commit layout transitions.
        for( int stage = 0; stage != 4; ++stage )
        {
            released = {};
            const unsigned before = queue_calls;
            if( stage == 0 ) end_result = VK_ERROR_OUT_OF_HOST_MEMORY;
            if( stage == 1 ) fence_result = VK_ERROR_OUT_OF_HOST_MEMORY;
            if( stage == 2 ) queue_result = VK_ERROR_OUT_OF_HOST_MEMORY;
            if( stage == 3 ) queue_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            require( !backend.uploadTexture( source, frame ).succeeded(), "pre-enqueue failure" );
            require( !Probe::pending( backend ) && released.command == 1 && released.buffer == 1 && released.memory == 1,
                "pre-enqueue cleanup" );
            require( released.fence == ( stage < 2 ? 0U : 1U ), "failed fence creation cleanup" );
            require( queue_calls == before + ( stage < 2 ? 0U : 1U ), "unexpected enqueue" );
            pass.final_state = eTextureState::RenderTarget;
            require( !backend.executeRenderPass( pass ).succeeded(), "failed draw enqueue" );
            require( Probe::layout( backend, target ) == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                "failed enqueue must not commit layout" );
            pass.final_state = eTextureState::CopySource;
            end_result = fence_result = queue_result = VK_SUCCESS;
        }

        // Actual C++ allocation failure after enqueue cannot lose the pending owner or layout.
        const auto oom_source = backend.createTexture( texture ).value();
        armWait( VK_TIMEOUT );
        fail_after_wait = true;
        bool allocation_failed = false;
        try { (void)backend.uploadTexture( oom_source, frame ); }
        catch( const std::bad_alloc& ) { allocation_failed = true; }
        fail_allocations = fail_after_wait = false;
        require( allocation_failed && Probe::pending( backend ) && released.total() == 0, "host allocation lost owner" );
        require( Probe::layout( backend, oom_source ) == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "OOM lost layout commit" );
        Probe::reap( backend );
        require( backend.destroyTexture( oom_source ).succeeded(), "destroy recovered OOM source" );

        // Acquire semaphore consumption must be recorded even if its wait times out.
        Probe::acquired( backend, surface );
        armWait( VK_TIMEOUT );
        require( !backend.executeRenderPass( pass ).succeeded(), "acquire consume timeout" );
        require( !Probe::acquirePending( backend, surface ), "acquire semaphore could be waited twice" );
        busy( backend.destroySurface( surface ) );
        require( released.total() == 0, "acquire submission premature free" );
        Probe::reap( backend );
        Probe::releaseAcquired( backend, surface );

        // DeviceLost is terminal until shutdown; shutdown retries unproven idle failures.
        armWait( VK_ERROR_DEVICE_LOST );
        require( backend.uploadTexture( source, frame ).error().code() == eRendererErrorCode::DeviceLost, "DeviceLost mapping" );
        require( backend.destroyMesh( mesh ).error().code() == eRendererErrorCode::DeviceLost, "DeviceLost reuse" );
        idle_failures = 2;
        idle_calls = 0;
        backend.shutdown();
        require( idle_calls >= 3 && released.buffer == 1 && released.command == 1 && released.fence == 1, "shutdown retention/release" );
        const auto shutdown_count = released.total();
        backend.shutdown();
        require( released.total() == shutdown_count, "shutdown twice" );
        force_wait = false;
        require( backend.initialize( configuration ).succeeded(), "reinitialize after terminal failure" );
        Probe::instrument( backend );
        queue_result = VK_ERROR_DEVICE_LOST;
        released = {};
        require( backend.createTexture( texture ).error().code() == eRendererErrorCode::DeviceLost, "submit DeviceLost" );
        require( Probe::pending( backend ) && released.total() == 0, "submit DeviceLost retention" );
        backend.shutdown();
        require( released.image == 1 && released.view == 1, "submit DeviceLost orphan release" );
        queue_result = VK_SUCCESS;
        std::cout << "CONTRACT_VERIFIED: Vulkan timeout, pending ownership, recovery, failure and shutdown\n";
        return 0;
    }
    catch( const std::exception& error )
    {
        fail_allocations = fail_after_wait = false;
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
