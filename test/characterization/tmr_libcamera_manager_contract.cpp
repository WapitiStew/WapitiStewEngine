#include "../../platform/tmr/linux/camera/LibcameraResources.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#if defined(WSE_TEST_NATIVE_LIBCAMERA)
#include <libcamera/camera_manager.h>
#endif
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
using wse::tmr::detail::LibcameraManagerLease;
namespace
{
std::atomic<int> live{0}, starts{0}, destroyed{0};
int start_result = 0;
bool fail_construction = false;
struct Manager
{
    Manager() { if(fail_construction) throw std::bad_alloc(); CHECK(++live == 1); }
    int start() { ++starts; return start_result; }
    ~Manager() { CHECK(--live == 0); ++destroyed; }
};
}
int main()
{
    {
        LibcameraManagerLease<Manager> first, second;
        start_result = -5; CHECK(first.acquire() == -5 && live == 0);
        fail_construction = true;
        try { first.acquire(); CHECK(false); } catch(const std::bad_alloc&) {}
        CHECK(live == 0);
        fail_construction = false; start_result = 0;
        CHECK(first.acquire() == 0 && second.acquire() == 0 && live == 1 && starts == 2);
        CHECK(first.operator->() == second.operator->());
        first.reset(); CHECK(live == 1);
        second.reset(); CHECK(live == 0);
    }
    std::array<std::thread, 4> callers;
    for(auto& caller : callers) caller = std::thread([]() {
        for(unsigned n = 0; n < 250; ++n)
        {
            LibcameraManagerLease<Manager> lease;
            CHECK(lease.acquire() == 0);
            std::this_thread::yield();
        }
    });
    for(auto& caller : callers) caller.join();
    CHECK(live == 0 && destroyed == starts);
#if defined(WSE_TEST_NATIVE_LIBCAMERA)
    // Starts/enumerates the manager without acquiring or opening a physical camera.
    {
        LibcameraManagerLease<libcamera::CameraManager> first, second;
        CHECK(first.acquire() == 0 && second.acquire() == 0);
        CHECK(first.operator->() == second.operator->());
        (void)first->cameras(); first.reset(); (void)second->cameras();
    }
#endif
    std::cout << "libcamera shared manager: failures, nested leases, 1000 concurrent leases and native lifetime passed\n";
}
