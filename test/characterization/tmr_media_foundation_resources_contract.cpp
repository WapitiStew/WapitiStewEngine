// Production ownership helpers with synthetic COM objects and runtime failures.
// The real MF attribute store below does not enumerate or activate any camera.
#include "../../platform/tmr/win/camera/MediaFoundationResources.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { bool fail_next = false; std::size_t fail_size = 0; unsigned cases = 0; }
void* operator new( std::size_t size_in )
{
    // Skip MSVC debug-iterator bookkeeping, whose constructor is noexcept.
    if( fail_next && size_in >= fail_size ) { fail_next = false; throw std::bad_alloc(); }
    if( auto* value = std::malloc( size_in ? size_in : 1 ) ) return value;
    throw std::bad_alloc();
}
void operator delete( void* value_in ) noexcept { std::free( value_in ); }
void operator delete( void* value_in, std::size_t ) noexcept { std::free( value_in ); }
#define CHECK(x) do { if(!(x)) { fail_next=false; std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
using namespace wse::tmr::detail;
using Microsoft::WRL::ComPtr;

struct Events
{
    std::array< int, 32 > values{};
    unsigned count = 0;
    void add( int value_in ) noexcept { CHECK(count < values.size()); values[count++] = value_in; }
};
class Object : public IUnknown
{
public:
    unsigned references = 1, releases = 0, shutdowns = 0;
    Events* events = nullptr;
    HRESULT shutdown_result = S_OK;
    STDMETHODIMP QueryInterface( REFIID, void** value_out ) override
    { auto& value = *value_out; value = nullptr; return E_NOINTERFACE; }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references; }
    STDMETHODIMP_(ULONG) Release() override
    { CHECK(references != 0); ++releases; if(events) events->add(4); return --references; }
    HRESULT Shutdown() noexcept
    { ++shutdowns; if(events) events->add(3); return shutdown_result; }
};
struct CountFree
{
    unsigned* count;
    void operator()( void* value_in ) const noexcept { ++*count; CoTaskMemFree(value_in); }
};

void arrays()
{
    for( int scenario = 0; scenario < 5; ++scenario )
    {
        Object objects[3]; unsigned frees = 0;
        auto** raw = static_cast< Object** >(CoTaskMemAlloc(3 * sizeof(Object*)));
        CHECK(raw != nullptr);
        for(unsigned i = 0; i < 3; ++i) raw[i] = &objects[i];
        std::vector< ComPtr<Object> > adopted;
        try
        {
            MediaFoundationActivationArray<Object, CountFree> array(raw, 3, CountFree{&frees});
            if(scenario == 0) throw std::bad_alloc(); // Failure before adoption.
            if(scenario == 1) { fail_size = 3*sizeof(ComPtr<Object>); fail_next = true; }
            if(scenario == 2)
            {
                auto first = array.take(0);
                CHECK(raw[0] == nullptr && objects[0].references == 1);
                throw std::bad_alloc();
            }
            if(scenario == 3) raw[1] = nullptr; // A null slot is harmless to cleanup.
            adopted = adoptMediaFoundationActivations(array);
            CHECK(adopted.size() == 3 && objects[0].references == 1);
        }
        catch(const std::bad_alloc&) { CHECK(scenario <= 2); }
        CHECK(frees == 1 && !fail_next);
        adopted.clear();
        for(unsigned i = 0; i < 3; ++i)
            CHECK(objects[i].releases == ((scenario == 3 && i == 1) ? 0U : 1U));
        ++cases;
    }
    for(bool allocated : {false, true})
    {
        unsigned frees = 0;
        {
            auto** raw = allocated ? static_cast<Object**>(CoTaskMemAlloc(sizeof(Object*))) : nullptr;
            MediaFoundationActivationArray<Object, CountFree> array(raw, 0, CountFree{&frees});
            CHECK(adoptMediaFoundationActivations(array).empty());
        }
        CHECK(frees == (allocated ? 1U : 0U)); ++cases;
    }
    for(bool fail : {false, true})
    {
        unsigned frees = 0;
        try
        {
            MediaFoundationTaskMemory<wchar_t, CountFree> text(
                static_cast<wchar_t*>(CoTaskMemAlloc(512*sizeof(wchar_t))), CountFree{&frees});
            CHECK(text != nullptr); text.get()[0] = L'A';
            fail_size = 512; fail_next = fail;
            std::string converted(512, static_cast<char>(text.get()[0]));
            CHECK(converted.front() == 'A');
        }
        catch(const std::bad_alloc&) { CHECK(fail); }
        CHECK(frees == 1 && !fail_next); ++cases;
    }
}

struct RuntimeState
{
    HRESULT com = S_OK, startup = S_OK;
    unsigned initialize_calls = 0, startup_calls = 0, shutdown_calls = 0, uninitialize_calls = 0;
    Events events;
};
struct RuntimeCalls
{
    RuntimeState* state;
    HRESULT initializeCom() const noexcept { ++state->initialize_calls; return state->com; }
    HRESULT startup() const noexcept { ++state->startup_calls; return state->startup; }
    void shutdown() const noexcept { ++state->shutdown_calls; state->events.add(5); }
    void uninitializeCom() const noexcept { ++state->uninitialize_calls; state->events.add(6); }
};
void runtimes()
{
    for(HRESULT com : {S_OK, S_FALSE, RPC_E_CHANGED_MODE, E_OUTOFMEMORY})
        for(HRESULT mf : {S_OK, E_FAIL})
        {
            RuntimeState state; state.com = com; state.startup = mf;
            const bool com_owned = SUCCEEDED(com), usable = com_owned || com == RPC_E_CHANGED_MODE;
            {
                MediaFoundationRuntime<RuntimeCalls> runtime(RuntimeCalls{&state});
                CHECK(runtime.initialize() == (usable ? mf : com));
                CHECK(state.startup_calls == (usable ? 1U : 0U));
                if(usable && SUCCEEDED(mf))
                { CHECK(runtime.initialize() == S_OK); CHECK(state.initialize_calls == 1); }
                else CHECK(state.uninitialize_calls == (com_owned ? 1U : 0U));
                runtime.close(); runtime.close();
            }
            CHECK(state.shutdown_calls == (usable && SUCCEEDED(mf) ? 1U : 0U));
            CHECK(state.uninitialize_calls == (com_owned ? 1U : 0U));
            if(state.shutdown_calls && com_owned) CHECK(state.events.values[0] == 5 && state.events.values[1] == 6);
            ++cases;
        }
    RuntimeState state; state.startup = E_OUTOFMEMORY;
    {
        MediaFoundationRuntime<RuntimeCalls> runtime(RuntimeCalls{&state});
        CHECK(runtime.initialize() == E_OUTOFMEMORY && state.uninitialize_calls == 1);
        state.startup = S_OK;
        CHECK(runtime.initialize() == S_OK);
        runtime.close();
        CHECK(runtime.initialize() == S_OK); // Same owner may be reopened after close.
    }
    CHECK(state.initialize_calls == 3 && state.uninitialize_calls == 3 && state.shutdown_calls == 2);
    ++cases;
}

void sources()
{
    for(int scenario = 0; scenario < 4; ++scenario)
    {
        Object object; Events events; object.events = &events;
        object.shutdown_result = scenario == 3 ? E_FAIL : S_OK;
        try
        {
            MediaFoundationSource<Object> source;
            *source.GetAddressOf() = &object;
            if(scenario == 0) source.Reset();
            if(scenario == 1) throw std::bad_alloc();
            if(scenario >= 2)
            {
                // Simulate a reader reference. Its release must precede source Shutdown.
                ComPtr<Object> reader_reference(source.Get());
                CHECK(object.references == 2);
                if(scenario == 3) throw std::bad_alloc();
            }
        }
        catch(const std::bad_alloc&) { CHECK(scenario == 1 || scenario == 3); }
        CHECK(object.references == 0 && object.shutdowns == 1);
        CHECK(events.values[events.count-2] == 3 && events.values[events.count-1] == 4);
        if(scenario >= 2) CHECK(events.values[0] == 4);
        ++cases;
    }
    { MediaFoundationSource<Object> empty; empty.Reset(); ++cases; }
}

struct Backend
{
    RuntimeState state;
    MediaFoundationRuntime<RuntimeCalls> runtime{RuntimeCalls{&state}};
    Object object;
    MediaFoundationSource<Object> source;
    unsigned closes = 0;
    void close() noexcept { ++closes; source.Reset(); runtime.close(); }
};
void transactions()
{
    for(int scenario = 0; scenario < 5; ++scenario)
    {
        Backend backend;
        try
        {
            MediaFoundationOpenGuard<Backend> guard(backend);
            if(scenario == 0) throw std::bad_alloc();
            CHECK(backend.runtime.initialize() == S_OK);
            if(scenario == 1) throw std::bad_alloc();
            *backend.source.GetAddressOf() = &backend.object;
            if(scenario == 2) throw std::bad_alloc(); // Callback/diagnostic allocation failure.
            if(scenario == 4) guard.commit(); // Scenario 3 is an ordinary failure return.
        }
        catch(const std::bad_alloc&) { CHECK(scenario <= 2); }
        CHECK(backend.closes == (scenario == 4 ? 0U : 1U));
        if(scenario == 4) backend.close();
        CHECK(backend.object.shutdowns == (scenario >= 2 ? 1U : 0U));
        CHECK(backend.state.shutdown_calls == (scenario >= 1 ? 1U : 0U));
        ++cases;
    }
}

int main()
{
    arrays(); runtimes(); sources(); transactions();
    {
        MediaFoundationRuntime<> runtime;
        CHECK(SUCCEEDED(runtime.initialize()));
        ComPtr<IMFAttributes> attributes;
        CHECK(SUCCEEDED(createMediaFoundationReaderAttributes(attributes.GetAddressOf())));
        UINT32 disconnect = FALSE;
        CHECK(SUCCEEDED(attributes->GetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, &disconnect)));
        CHECK(disconnect == TRUE); ++cases;
    }
    std::cout << cases << " MF ownership cases passed (synthetic faults and real attribute store; no camera opened).\n";
}
