// The actual native adapter uses only synthetic operations and a virtual clock.
#include "../../platform/tmr/linux/camera/V4L2CameraBackend.cpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { bool fail_next=false; }
void* operator new(std::size_t size_in)
{
    if(fail_next) { fail_next=false; throw std::bad_alloc(); }
    if(auto* memory=std::malloc(size_in ? size_in : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory_in) noexcept { std::free(memory_in); }
void operator delete(void* memory_in,std::size_t) noexcept { std::free(memory_in); }
#define CHECK(x) do { if(!(x)) { fail_next=false; std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)

namespace
{
using Clock=std::chrono::steady_clock;
struct Script
{
    std::array<unsigned char,16> data{1,2,3,4};
    unsigned opens=0, closes=0, maps=0, unmaps=0, polls=0, dequeues=0, returns=0;
    bool copy_oom=false, open_oom=false, error_oom=false, return_fail=false, stop_fail=false;
    bool invalid_index=false, corrupt=false, event_error=false;
    int map_failure=-1, query_failure=-1, dequeue_error=0, poll_error=0;
    unsigned length=16, interrupts=0, again=0;
    bool wait_to_timeout=false;
    std::array<int,8> waits{};
    Clock::time_point now{};
} script;
int openNative(const char*,int flags_in)
{ CHECK((flags_in&O_NONBLOCK)!=0); ++script.opens; return 42; }
int closeNative(int fd_in) { CHECK(fd_in==42); ++script.closes; errno=EBADF; return 0; }
int unmapNative(void*,std::size_t) { CHECK(script.closes==1); ++script.unmaps; return 0; }
void* mapNative(void*,std::size_t,int,int,int,off_t)
{
    if(static_cast<int>(script.maps)==script.map_failure)
    { errno=ENOMEM; if(script.error_oom) fail_next=true; return MAP_FAILED; }
    ++script.maps; return script.data.data();
}
int ioctlNative(int fd_in,unsigned long op_in,void* argument_in)
{
    CHECK(fd_in==42);
    if(op_in==VIDIOC_QUERYCAP)
        static_cast<v4l2_capability*>(argument_in)->capabilities=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING;
    else if(op_in==VIDIOC_S_FMT) static_cast<v4l2_format*>(argument_in)->fmt.pix.bytesperline=8;
    else if(op_in==VIDIOC_S_PARM) {}
    else if(op_in==VIDIOC_REQBUFS) { if(script.open_oom) fail_next=true; }
    else if(op_in==VIDIOC_QUERYBUF)
    {
        auto& buffer=*static_cast<v4l2_buffer*>(argument_in);
        if(static_cast<int>(buffer.index)==script.query_failure) { errno=ENOSPC; return -1; }
        buffer.length=16;
    }
    else if(op_in==VIDIOC_STREAMON) {}
    else if(op_in==VIDIOC_STREAMOFF) { if(script.stop_fail) { errno=EBUSY; return -1; } }
    else if(op_in==VIDIOC_QBUF)
    {
        const auto& buffer=*static_cast<v4l2_buffer*>(argument_in);
        CHECK(buffer.index<4 && buffer.flags==0 && buffer.bytesused==0);
        if(script.dequeues)
        { ++script.returns; if(script.return_fail) { errno=ENOSPC; return -1; } }
    }
    else CHECK(false);
    return 0;
}
int pollNative(pollfd* descriptor_inout,nfds_t count_in,int timeout_in)
{
    auto& descriptor=*descriptor_inout;
    CHECK(count_in==1 && descriptor.fd==42 && timeout_in>0 && script.polls<script.waits.size());
    script.waits[script.polls++]=timeout_in;
    if(script.wait_to_timeout)
    { script.now+=std::chrono::milliseconds(timeout_in); return 0; }
    script.now+=std::chrono::milliseconds(3);
    if(script.interrupts) { --script.interrupts; errno=EINTR; return -1; }
    if(script.poll_error) { errno=script.poll_error; return -1; }
    descriptor.revents=script.event_error ? POLLHUP : POLLIN;
    return 1;
}
int dequeueNative(int,unsigned long op_in,void* argument_in)
{
    CHECK(op_in==VIDIOC_DQBUF);
    if(script.again) { --script.again; errno=EAGAIN; return -1; }
    if(script.dequeue_error) { errno=script.dequeue_error; return -1; }
    auto& buffer=*static_cast<v4l2_buffer*>(argument_in);
    buffer.index=script.invalid_index ? 4 : 0;
    buffer.bytesused=script.length;
    buffer.sequence=8;
    buffer.flags=V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC|(script.corrupt ? V4L2_BUF_FLAG_ERROR : 0);
    buffer.timestamp.tv_sec=2; buffer.timestamp.tv_usec=3;
    ++script.dequeues;
    if(script.copy_oom) fail_next=true;
    return 0;
}
Clock::time_point nowNative() { return script.now; }
} // namespace

namespace wse::tmr::detail
{
namespace
{
struct V4L2CameraBackendTestAccess
{
    static void install(V4L2CameraBackend& backend_inout)
    {
        backend_inout.m_calls={ioctlNative,closeNative,unmapNative,openNative,mapNative,
            pollNative,dequeueNative,nowNative};
    }
};
}
}

int main()
{
    using namespace wse::tmr;
    sCameraOpenDescription description;
    description.device.id="synthetic";
    description.format={2,2,30,1,eCameraPixelFormat::Bgra8};
    // V4L2 accepts BGR24; use the same 16-byte padded payload for two rows.
    description.format.pixel_format=eCameraPixelFormat::Bgr8;
    for(unsigned scenario=0;scenario<11;++scenario)
    {
        script={};
        detail::V4L2CameraBackend backend;
        detail::V4L2CameraBackendTestAccess::install(backend);
        if(scenario==0) script.open_oom=true;
        if(scenario>=1 && scenario<=4) script.query_failure=static_cast<int>(scenario-1);
        if(scenario>=5 && scenario<=8) script.map_failure=static_cast<int>(scenario-5);
        if(scenario>=9) { script.map_failure=scenario==9 ? 0 : 2; script.error_oom=true; }
        bool failed=false;
        try { failed=!backend.open(description).succeeded(); }
        catch(const std::bad_alloc&) { failed=true; }
        CHECK(failed && !fail_next && !backend.isOpen() && script.closes==1 && script.maps==script.unmaps);
        backend.close(); CHECK(script.closes==1);
        script={};
        CHECK(backend.open(description).succeeded()); // Allocation failure is recoverable by reopening.
        backend.close(); CHECK(script.closes==1 && script.unmaps==4);
    }
    for(unsigned scenario=0;scenario<16;++scenario)
    {
        script={};
        detail::V4L2CameraBackend backend;
        detail::V4L2CameraBackendTestAccess::install(backend);
        CHECK(backend.open(description).succeeded() && backend.start().succeeded());
        if(scenario==1 || scenario==2) script.copy_oom=true;
        if(scenario==2 || scenario==3 || scenario==6) script.return_fail=true;
        if(scenario==2) script.stop_fail=true;
        if(scenario==4) script.invalid_index=true;
        if(scenario==5 || scenario==6) script.length=17;
        if(scenario==7) script.length=15;
        if(scenario==8) script.corrupt=true;
        if(scenario==9) script.interrupts=4;
        if(scenario==10) script.again=1;
        if(scenario==11) script.wait_to_timeout=true;
        if(scenario==12) script.dequeue_error=ENOSPC;
        if(scenario==13) script.dequeue_error=EINTR;
        if(scenario==14) script.event_error=true;
        if(scenario==15) script.poll_error=EINVAL;
        bool threw=false;
        try
        {
            auto frame=backend.readFrame(scenario==11 ? (std::numeric_limits<std::uint32_t>::max)() : 10);
            CHECK(frame.succeeded()==(scenario==0 || scenario==10));
            if(frame.succeeded())
            {
                CHECK(frame.value().sequence==8 && frame.value().monotonic_timestamp_ns==2000003000LL);
                script.data.fill(99); backend.close(); CHECK(frame.value().data[0]==1);
            }
            if(scenario==3) CHECK(frame.error().nativeCode()==ENOSPC);
            if(scenario==6) CHECK(frame.error().category()==eCameraErrorCategory::InputOutput && frame.error().nativeCode()==0);
            if(scenario==9 || scenario==11 || scenario==13) CHECK(frame.error().code()==eCameraErrorCode::TimedOut);
        }
        catch(const std::bad_alloc&) { threw=true; }
        CHECK(threw==(scenario==1 || scenario==2) && !fail_next);
        CHECK(script.returns==((scenario<=8 && scenario!=4) || scenario==10 ? 1U : 0U));
        if(scenario==1 || scenario==5 || scenario==7 || scenario==8 || scenario==9 || scenario==11 || scenario==13 || scenario==15)
            CHECK(backend.isStreaming());
        if(scenario==2 || scenario==3 || scenario==4 || scenario==6 || scenario==12 || scenario==14)
            CHECK(!backend.isOpen());
        if(scenario==9 || scenario==13) CHECK(script.polls==4 && script.waits[0]==10 && script.waits[1]==7 && script.waits[3]==1);
        if(scenario==10) CHECK(script.polls==2 && script.waits[1]==7);
        if(scenario==11) CHECK(script.polls==3 && script.waits[0]==2147483647 && script.waits[2]==1);
        backend.close(); CHECK(script.closes==1 && script.unmaps==4);
    }
    std::cout << "27 synthetic V4L2 open/read/clock cases passed.\n";
}
