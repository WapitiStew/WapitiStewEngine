#include "../../lang/cs/native/camera_callback.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace { bool fail_next=false; int live=0; }
void* operator new(std::size_t size_in)
{
    if(fail_next) { fail_next=false; throw std::bad_alloc(); }
    if(auto* memory=std::malloc(size_in ? size_in : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* value_in) noexcept { std::free(value_in); }
void operator delete(void* value_in,std::size_t) noexcept { std::free(value_in); }
#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
struct Frame { int mode; };
struct Handle
{
    explicit Handle(const Frame& frame_in)
    {
        if(frame_in.mode==1) throw std::bad_alloc();
        if(frame_in.mode==2) throw std::runtime_error("synthetic copy failure");
        ++live;
    }
    ~Handle() { --live; }
};
int main()
{
    using namespace wse::capi;
    for(int mode=0;mode<4;++mode)
    {
        int calls=0; bool stopped=false; Handle* retained=nullptr;
        const auto callback=[&](Handle* frame_in,wse_capi_status status_in,wse_capi_user_data token_in)
        {
            ++calls; CHECK(token_in==42);
            if(mode==0) { CHECK(status_in.category==WSE_CAPI_ERROR_NONE); retained=frame_in; }
            else
            {
                CHECK(!frame_in && status_in.category==(mode==2 ? WSE_CAPI_ERROR_INTERNAL : WSE_CAPI_ERROR_RESOURCE_EXHAUSTED));
                CHECK(lastErrorMessage()[0]!='\0');
            }
        };
        if(mode==3) fail_next=true;
        try { deliverCameraFrame<Handle>(Frame{mode},callback,42); }
        catch(const CameraCallbackDeliveryStopped&) { stopped=true; }
        CHECK(!fail_next && calls==1 && stopped==(mode!=0));
        CHECK(live==(mode==0 ? 1 : 0)); delete retained; CHECK(live==0);
    }
    int calls=0; bool propagated=false;
    try
    {
        deliverCameraFrame<Handle>(Frame{0},[&](Handle* frame_in,wse_capi_status,wse_capi_user_data)
        { ++calls; delete frame_in; throw 19; },0);
    }
    catch(int value) { propagated=value==19; }
    CHECK(calls==1 && live==0 && propagated);
    std::cout << "5 native callback preparation/transfer fault cases passed.\n";
}
