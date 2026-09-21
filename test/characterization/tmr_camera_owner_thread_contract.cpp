#include "../../core/tmr/camera/CameraOwnerThread.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { std::atomic<bool> reject_allocations{false}; }
void* operator new(std::size_t size_in)
{
    if(reject_allocations) throw std::bad_alloc();
    if(auto* result = std::malloc(size_in ? size_in : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* value_in) noexcept { std::free(value_in); }
void operator delete(void* value_in, std::size_t) noexcept { std::free(value_in); }
#define CHECK(x) do { if(!(x)) { reject_allocations=false; std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
using wse::tmr::detail::CameraOwnerThread;
namespace
{
std::thread::id created, destroyed;
struct Native
{
    unsigned calls = 0;
    Native() { created = std::this_thread::get_id(); }
    ~Native() { destroyed = std::this_thread::get_id(); }
};
std::unique_ptr<Native> create() { return std::make_unique<Native>(); }
std::unique_ptr<Native> fail() { throw std::bad_alloc(); }
std::unique_ptr<Native> empty() { return {}; }
}
int main()
{
    const auto caller = std::this_thread::get_id();
    for(auto factory : {&fail, &empty})
    {
        bool caught = false;
        try { CameraOwnerThread<Native> owner(factory); }
        catch(const std::exception&) { caught = true; }
        CHECK(caught);
    }
    {
        auto owner = std::make_unique<CameraOwnerThread<Native>>(&create);
        CHECK(created != caller);
        CHECK(owner->invoke([](auto&) { return std::this_thread::get_id(); }) == created);
        auto result = owner->invoke([](auto&) { return std::make_unique<int>(42); });
        CHECK(*result == 42);
        bool caught = false;
        try { owner->invoke([](auto&) -> int { throw 73; }); }
        catch(int value) { caught = value == 73; }
        CHECK(caught);
        std::array<std::thread, 4> callers;
        for(auto& thread : callers) thread = std::thread([&]() {
            for(unsigned i = 0; i < 250; ++i) owner->invoke([](auto& native) {
                CHECK(std::this_thread::get_id() == created); ++native.calls;
            });
        });
        for(auto& thread : callers) thread.join();
        CHECK(owner->invoke([](auto& native) { return native.calls; }) == 1000);
        reject_allocations = true;
        owner->invoke([](auto& native) { ++native.calls; });
        CHECK(owner->invoke([](auto& native) { return native.calls; }) == 1001);
        reject_allocations = false;
        std::thread finalizer([&]() {
            reject_allocations = true;
            owner.reset();
            reject_allocations = false;
        });
        finalizer.join();
        CHECK(destroyed == created && destroyed != caller);
    }
    std::cout << "owner thread: factory failures, move-only/void results, exception recovery, "
                 "1000 concurrent calls, allocation-free dispatch and cross-thread destruction passed\n";
}
