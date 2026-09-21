// Compile the actual internal adapter with per-instance native-call faults.
// Uses synthetic descriptor/buffers; never opens a device or certifies hardware.
#include "../../platform/tmr/linux/camera/V4L2CameraBackend.cpp"
#include <array>
#include <iostream>

namespace wse::tmr::detail
{
namespace
{
struct V4L2CameraBackendTestAccess
{
    static void prepare( V4L2CameraBackend* backend_inout,
        int (*ioctl_in)( int, unsigned long, void* ), int (*close_in)( int ),
        int (*unmap_in)( void*, std::size_t ) )
    {
        auto& backend_inout_ref = *backend_inout;
        backend_inout_ref.m_calls = { ioctl_in, close_in, unmap_in };
        backend_inout_ref.m_descriptor = 42;
        static std::array< unsigned char, 16 > storage;
        backend_inout_ref.m_buffers.assign( 4, { storage.data(), storage.size() } );
    }
};
} // namespace
} // namespace wse::tmr::detail

namespace
{
using namespace wse::tmr;
int failures = 0;
void expect( bool value_in, const char* message_in )
{
    if( !value_in ) { ++failures; std::cerr << message_in << '\n'; }
}
struct Script
{
    int fail_queue = -1;
    bool fail_start = false, fail_stop = false;
    unsigned queued = 0, stream_on = 0, stream_off = 0, closed = 0, unmapped = 0;
    bool duplicate = false, unmap_before_close = false;
    std::array< bool, 4 > owned{};
} script;
int nativeIoctl( int descriptor_in, unsigned long request_in, void* argument_in )
{
    expect( descriptor_in == 42, "Only the synthetic descriptor is used" );
    if( request_in == VIDIOC_QBUF )
    {
        const auto index = static_cast< v4l2_buffer* >( argument_in )->index;
        ++script.queued;
        if( static_cast< int >( index ) == script.fail_queue ) { errno = ENOSPC; return -1; }
        if( script.owned.at( index ) ) script.duplicate = true;
        script.owned.at( index ) = true;
    }
    else if( request_in == VIDIOC_STREAMON )
    {
        ++script.stream_on;
        if( script.fail_start ) { errno = EBUSY; return -1; }
    }
    else if( request_in == VIDIOC_STREAMOFF )
    {
        ++script.stream_off;
        if( script.fail_stop ) { errno = EACCES; return -1; }
        script.owned.fill( false );
    }
    else { expect( false, "Unexpected lifecycle ioctl" ); errno = EINVAL; return -1; }
    return 0;
}
int nativeClose( int ) { ++script.closed; script.owned.fill( false ); return 0; }
int nativeUnmap( void*, std::size_t )
{
    ++script.unmapped;
    if( script.closed == 0 ) script.unmap_before_close = true;
    return 0;
}
void prepare( detail::V4L2CameraBackend* backend_inout )
{
    script = {};
    detail::V4L2CameraBackendTestAccess::prepare( backend_inout, nativeIoctl, nativeClose, nativeUnmap );
}
} // namespace

int main()
{
    using namespace wse::tmr;
    for( int failed_queue : { 0, 1, 2, 3, -1 } )
    for( bool rollback_fails : { false, true } )
    {
        detail::V4L2CameraBackend backend;
        prepare( &backend );
        script.fail_queue = failed_queue;
        script.fail_start = failed_queue < 0;
        script.fail_stop = rollback_fails;
        const auto status = backend.start();
        expect( !status.succeeded() && status.error().nativeCode() == ( failed_queue < 0 ? EBUSY : ENOSPC ),
            "Start retains original errno even when rollback fails" );
        expect( !backend.isStreaming() && backend.isOpen() == !rollback_fails,
            "Partial start restores stopped state or closes on failed reset" );
        expect( script.stream_off >= 1, "Every partial start resets the native queue" );
        if( !rollback_fails )
        {
            script.fail_queue = -1;
            script.fail_start = false;
            expect( backend.start().succeeded() && !script.duplicate,
                "Retry queues each buffer once after successful rollback" );
            expect( backend.stop().succeeded(), "Retry can stop normally" );
        }
        backend.close();
        expect( script.closed == 1 && script.unmapped == 4 && !script.unmap_before_close,
            "Cleanup closes native ownership before releasing each mapping once" );
    }
    {
        detail::V4L2CameraBackend backend;
        prepare( &backend );
        expect( backend.start().succeeded(), "Capture starts" );
        script.fail_stop = true;
        const auto status = backend.stop();
        expect( !status.succeeded() && status.error().nativeCode() == EACCES && !backend.isOpen(),
            "Failed STREAMOFF closes instead of leaving a reusable uncertain queue" );
        expect( backend.stop().succeeded() && script.closed == 1 && script.unmapped == 4,
            "Stop after failed native shutdown is idempotent" );
    }
    {
        detail::V4L2CameraBackend backend;
        prepare( &backend );
        expect( backend.start().succeeded(), "Destructor capture starts" );
        script.fail_stop = true;
    }
    expect( script.closed == 1 && script.unmapped == 4 && !script.unmap_before_close,
        "Destructor cleans native resources even when STREAMOFF fails" );
    return failures == 0 ? 0 : 1;
}
