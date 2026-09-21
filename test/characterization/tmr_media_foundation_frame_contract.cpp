// Synthetic IMFMediaBuffer only: no COM runtime, camera, or device is opened.
#include "../../platform/tmr/win/camera/MediaFoundationFrame.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { bool fail_next = false; }
void* operator new( std::size_t size_in )
{
    if( fail_next && size_in == 512 ) { fail_next = false; throw std::bad_alloc(); }
    if( auto* memory = std::malloc( size_in ? size_in : 1 ) ) return memory;
    throw std::bad_alloc();
}
void operator delete( void* memory_in ) noexcept { std::free( memory_in ); }
void operator delete( void* memory_in, std::size_t ) noexcept { std::free( memory_in ); }
#define CHECK(x) do { if(!(x)) { fail_next=false; std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)

class Buffer final : public IMFMediaBuffer
{
public:
    std::array< BYTE, 512 > data{ 1, 2, 3, 4 };
    DWORD length = 512;
    HRESULT lock_result = S_OK, unlock_result = S_OK;
    bool null_bytes = false, allocation_fails = false;
    unsigned locks = 0, unlocks = 0;
    STDMETHODIMP QueryInterface( REFIID, void** value_out ) override
    { auto& value = *value_out; value = nullptr; return E_NOINTERFACE; }
    STDMETHODIMP_(ULONG) AddRef() override { return 1; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP Lock( BYTE** bytes_out, DWORD* capacity_out, DWORD* length_out ) override
    {
        auto& bytes = *bytes_out;
        ++locks;
        if( FAILED( lock_result ) ) return lock_result;
        bytes = null_bytes ? nullptr : data.data();
        *capacity_out = static_cast< DWORD >( data.size() ); *length_out = length;
        if( allocation_fails ) fail_next = true;
        return S_OK;
    }
    STDMETHODIMP Unlock() override { ++unlocks; return unlock_result; }
    STDMETHODIMP GetCurrentLength( DWORD* value_out ) override { *value_out=length; return S_OK; }
    STDMETHODIMP SetCurrentLength( DWORD value_in ) override { length=value_in; return S_OK; }
    STDMETHODIMP GetMaxLength( DWORD* value_out ) override { *value_out=512; return S_OK; }
};

int main()
{
    using namespace wse::tmr;
    const sCameraFrameDescription description{ 16, 8, eCameraPixelFormat::Bgra8, 64 };
    {
        Buffer buffer;
        auto frame = detail::copyMediaFoundationFrame( buffer, description, 7, 123 );
        CHECK(frame.succeeded() && frame.value().data[0]==1);
        CHECK(frame.value().sequence==7 && frame.value().monotonic_timestamp_ns==12300);
        CHECK(buffer.locks==1 && buffer.unlocks==1);
        buffer.data.fill(99);
        CHECK(frame.value().data[0]==1);
    }
    for( HRESULT failure : { E_OUTOFMEMORY, E_FAIL } )
    {
        Buffer buffer; buffer.lock_result=failure;
        auto result=detail::copyMediaFoundationFrame(buffer,description,0,0);
        CHECK(!result.succeeded() && result.error().nativeCode()==failure && buffer.unlocks==0);
    }
    for( bool fail_unlock : { false, true } )
    {
        Buffer buffer; buffer.allocation_fails=true;
        buffer.unlock_result=fail_unlock ? E_FAIL : S_OK;
        bool threw=false;
        try { (void)detail::copyMediaFoundationFrame(buffer,description,0,0); }
        catch(const std::bad_alloc&) { threw=true; }
        CHECK(threw && !fail_next && buffer.unlocks==1);
    }
    for( unsigned invalid=0; invalid<5; ++invalid )
    for( bool fail_unlock : { false, true } )
    {
        Buffer buffer; buffer.unlock_result=fail_unlock ? E_FAIL : S_OK;
        if(invalid==0) buffer.length=513;
        if(invalid==1) buffer.null_bytes=true;
        if(invalid==2) buffer.length=511;
        const auto timestamp=invalid==3 ? (std::numeric_limits<LONGLONG>::max)()
            : invalid==4 ? (std::numeric_limits<LONGLONG>::min)() : 0;
        auto result=detail::copyMediaFoundationFrame(buffer,description,0,timestamp);
        CHECK(!result.succeeded() && result.error().category()==eCameraErrorCategory::InputOutput);
        CHECK(buffer.unlocks==1); // Primary validation error wins over failed cleanup.
    }
    {
        Buffer buffer; buffer.unlock_result=E_FAIL;
        auto result=detail::copyMediaFoundationFrame(buffer,description,0,0);
        CHECK(!result.succeeded() && result.error().nativeCode()==E_FAIL && buffer.unlocks==1);
    }
    std::cout << "16 synthetic MF lock/copy/unlock cases passed.\n";
}
