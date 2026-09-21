// Production resource/transaction helpers, synthetic calls only. No camera is opened.
#include "../../platform/tmr/linux/camera/LibcameraResources.h"
#include <tmr/camera/CameraError.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { int allocations_until_failure = -1; }
void* operator new(std::size_t size_in)
{
    if(allocations_until_failure == 0)
    { allocations_until_failure = -1; throw std::bad_alloc(); }
    if(allocations_until_failure > 0) --allocations_until_failure;
    if(auto* memory = std::malloc(size_in ? size_in : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory_in) noexcept { std::free(memory_in); }
void operator delete(void* memory_in, std::size_t) noexcept { std::free(memory_in); }
#define CHECK(x) do { if(!(x)) { allocations_until_failure=-1; std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)

namespace
{
using namespace wse::tmr;
using namespace wse::tmr::detail;
unsigned cases = 0, live_requests = 0;
struct Request
{
    Request() { ++live_requests; }
    ~Request() { --live_requests; }
};
using Pool = LibcameraRequests<Request>;
auto requests(unsigned count_in)
{
    std::vector<std::unique_ptr<Request>> result;
    for(unsigned i = 0; i < count_in; ++i) result.push_back(std::make_unique<Request>());
    return result;
}
CameraStatus failure(int native_in)
{
    return CameraStatus::failure(CameraError(eCameraErrorCategory::Backend,
        eCameraErrorCode::ReadFailed, "synthetic failure", native_in));
}
struct Mapping
{
    std::array<std::uint8_t,16> bytes{9,8,0,127,255,4,5,6};
    unsigned maps=0, unmaps=0;
    bool map_failure=false, unmap_failure=false, copy_failure=false;
    std::size_t length=0;
} mapping;
void* mapNative(void*,std::size_t length_in,int,int,int,off_t) noexcept
{
    ++mapping.maps; mapping.length=length_in;
    if(mapping.map_failure) { errno=ENOMEM; return MAP_FAILED; }
    if(mapping.copy_failure) allocations_until_failure=0;
    return mapping.bytes.data();
}
int unmapNative(void* data_in,std::size_t length_in) noexcept
{
    CHECK(data_in==mapping.bytes.data() && length_in==mapping.length);
    ++mapping.unmaps;
    errno=EIO;
    return mapping.unmap_failure ? -1 : 0;
}
const LibcameraMappingCalls calls{mapNative,unmapNative};

void planes()
{
    for(unsigned mode=0;mode<9;++mode)
    {
        mapping={}; std::vector<std::uint8_t> data;
        std::size_t offset=2,length=6,used=3;
        if(mode==1) used=7;
        if(mode==2) offset=std::numeric_limits<std::size_t>::max()-2;
        if(mode==3) { used=0; length=0; }
        if(mode==4) used=0;
        mapping.map_failure=mode==5; mapping.copy_failure=mode==6;
        mapping.unmap_failure=mode==7;
        int error=0; bool threw=false;
        try { error=appendLibcameraPlane(&data,calls,42,offset,length,used); }
        catch(const std::bad_alloc&) { threw=true; }
        if(mode==0 || mode==8)
        {
            CHECK(error==0 && !threw && data==std::vector<std::uint8_t>({0,127,255}));
            if(mode==8)
            {
                mapping.map_failure=true;
                CHECK(appendLibcameraPlane(&data,calls,42,2,6,3)==ENOMEM);
                CHECK(data.size()==3 && mapping.maps==2 && mapping.unmaps==1);
            }
            else CHECK(mapping.maps==1 && mapping.unmaps==1);
            mapping.bytes.fill(42); CHECK(data[1]==127); // detached bytes
        }
        else if(mode<=3) CHECK(error==EOVERFLOW && mapping.maps==0 && mapping.unmaps==0);
        else if(mode==4) CHECK(error==0 && data.empty() && mapping.maps==0);
        else if(mode==5) CHECK(error==ENOMEM && mapping.maps==1 && mapping.unmaps==0);
        else if(mode==6) CHECK(threw && mapping.maps==1 && mapping.unmaps==1 && data.empty());
        else CHECK(error==EIO && mapping.maps==1 && mapping.unmaps==1);
        ++cases;
    }
}

void ledger()
{
    for(int allocation=0;allocation<2;++allocation)
    {
        Pool pool; pool.prepare(requests(1)); auto* original=pool.owners()[0].get();
        auto candidate=requests(3); allocations_until_failure=allocation;
        bool threw=false;
        try { pool.prepare(std::move(candidate)); } catch(const std::bad_alloc&) { threw=true; }
        CHECK(threw && live_requests==1 && pool.owners()[0].get()==original);
        CHECK(pool.clear()); ++cases;
    }
    {
        Pool pool; pool.prepare(requests(3));
        // Ring wrap/reuse remains valid with the next allocation forced to fail.
        allocations_until_failure=0;
        for(unsigned cycle=0;cycle<1000;++cycle)
        {
            for(const auto& request:pool.owners()) CHECK(pool.queued(request.get()));
            CHECK(!pool.clear() && live_requests==3);
            for(const auto& request:pool.owners()) pool.complete(request.get(),true);
            for(const auto& request:pool.owners()) CHECK(pool.pop()==request.get());
            CHECK(pool.empty() && pool.outstanding()==0 && !pool.fault());
        }
        CHECK(allocations_until_failure==0); allocations_until_failure=-1;
        CHECK(pool.clear()); ++cases;
    }
    {
        Pool pool; pool.prepare(requests(2)); auto* first=pool.owners()[0].get();
        auto* second=pool.owners()[1].get(); CHECK(pool.queued(first)); CHECK(pool.queued(second));
        pool.rejected(second); CHECK(pool.outstanding()==1 && !pool.clear());
        pool.complete(first,false); CHECK(pool.empty() && pool.outstanding()==0 && pool.clear()); ++cases;
    }
    for(unsigned mode=0;mode<3;++mode)
    {
        Pool pool; pool.prepare(requests(1)); auto* request=pool.owners()[0].get();
        if(mode==0) pool.complete(nullptr,true);
        else if(mode==1) pool.complete(request,true);
        else { CHECK(pool.queued(request)); CHECK(!pool.queued(request)); }
        CHECK(pool.fault()); pool.quiesced(); CHECK(pool.clear()); ++cases;
    }
    CHECK(live_requests==0);
}

void starts()
{
    // Creation/addBuffer preparation, initial controls, native start and three
    // queue positions: both status failures and exceptions preserve rollback.
    for(int fail_at=-1;fail_at<8;++fail_at) for(bool throws:{false,true})
    {
        Pool pool; unsigned cleanup=0,native_stops=0; bool active=false;
        int step=0;
        auto checkpoint=[&]()
        {
            if(step++!=fail_at) return CameraStatus::success();
            if(throws) throw std::bad_alloc();
            return failure(-77);
        };
        auto clear=[&]() noexcept
        {
            ++cleanup;
            if(active) { ++native_stops; active=false; }
            pool.quiesced(); CHECK(pool.clear()); errno=EIO;
        };
        bool caught=false;
        try
        {
            const auto result=startLibcameraTransaction(
                [&]()
                {
                    auto candidate=requests(3);
                    for(int i=0;i<3;++i) { auto status=checkpoint(); if(!status.succeeded()) return status; }
                    pool.prepare(std::move(candidate)); return CameraStatus::success();
                },
                [&]()
                {
                    auto status=checkpoint(); if(!status.succeeded()) return status;
                    active=true; return checkpoint();
                },
                [&]()
                {
                    for(const auto& request:pool.owners())
                    {
                        auto status=checkpoint(); if(!status.succeeded()) return status;
                        CHECK(pool.queued(request.get()));
                    }
                    return CameraStatus::success();
                },clear);
            CHECK(result.succeeded()==(fail_at==-1));
            if(fail_at!=-1) CHECK(result.error().nativeCode()==-77);
        }
        catch(const std::bad_alloc&) { caught=true; }
        CHECK(caught==(throws && fail_at!=-1));
        if(fail_at==-1) { CHECK(cleanup==0 && live_requests==3 && active); clear(); }
        else CHECK(cleanup==1 && live_requests==0 && !active);
        CHECK(native_stops==static_cast<unsigned>(fail_at==-1 || fail_at>=4));
        ++cases;
    }
    // Failed stop retains the complete ownership group until actual returns/barrier.
    Pool pool; pool.prepare(requests(2));
    for(const auto& request:pool.owners()) CHECK(pool.queued(request.get()));
    CHECK(!pool.clear() && live_requests==2);
    pool.complete(pool.owners()[0].get(),false); CHECK(!pool.clear());
    pool.complete(pool.owners()[1].get(),false); CHECK(pool.clear() && live_requests==0); ++cases;
}

void reads()
{
    for(unsigned mode=0;mode<6;++mode)
    {
        unsigned cleanup=0,requeues=0; bool threw=false;
        try
        {
            auto result=readLibcameraTransaction(
                [&]()
                {
                    if(mode==1) throw std::bad_alloc();
                    return mode==2 || mode==4 ? CameraResult<int>::failure(failure(-11).error())
                        : CameraResult<int>::success(37);
                },
                [&]()
                {
                    ++requeues;
                    if(mode==5) throw std::bad_alloc();
                    return mode==3 || mode==4 ? failure(-22) : CameraStatus::success();
                },[&]() noexcept { ++cleanup; });
            if(mode==0) CHECK(result.succeeded() && result.value()==37);
            else CHECK(!result.succeeded() && result.error().nativeCode()==(mode==3 ? -22 : -11));
        }
        catch(const std::bad_alloc&) { threw=true; }
        CHECK(threw==(mode==1 || mode==5));
        CHECK(requeues==(mode==1 ? 0U : 1U));
        CHECK(cleanup==(mode==0 || mode==2 ? 0U : 1U)); ++cases;
    }
}
}
int main()
{
    planes(); ledger(); starts(); reads(); CHECK(live_requests==0);
    std::cout << "libcamera production helpers: " << cases
              << " synthetic cases; 1000 allocation-free completion cycles; no hardware\n";
}
