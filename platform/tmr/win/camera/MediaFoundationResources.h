#ifndef WSE_INTERNAL_MEDIA_FOUNDATION_RESOURCES_H
#define WSE_INTERNAL_MEDIA_FOUNDATION_RESOURCES_H

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <memory>
#include <utility>
#include <vector>

namespace wse::tmr::detail
{
struct MediaFoundationTaskFree
{
    void operator()( void* memory_in ) const noexcept { CoTaskMemFree( memory_in ); }
};

template< class T, class Free = MediaFoundationTaskFree >
using MediaFoundationTaskMemory = std::unique_ptr< T, Free >;

// The array and every reference have different owners. Null an adopted slot before
// any subsequent operation can throw; ComPtr then owns that reference exclusively.
template< class T, class Free = MediaFoundationTaskFree >
class MediaFoundationActivationArray final
{
    MediaFoundationTaskMemory< T*, Free > m_items;
    UINT32 m_count;
public:
    MediaFoundationActivationArray( T** items_in, const UINT32 count_in,
        const Free& free_in = Free{} ) noexcept : m_items( items_in, free_in ), m_count( count_in ) {}
    MediaFoundationActivationArray( const MediaFoundationActivationArray& ) = delete;
    MediaFoundationActivationArray& operator=( const MediaFoundationActivationArray& ) = delete;
    ~MediaFoundationActivationArray() noexcept
    {
        if( this->m_items )
            for( UINT32 index = 0; index < this->m_count; ++index )
                (void)this->take( index );
    }
    UINT32 size() const noexcept { return this->m_count; }
    Microsoft::WRL::ComPtr< T > take( const UINT32 index_in ) noexcept
    {
        Microsoft::WRL::ComPtr< T > item;
        item.Attach( std::exchange( this->m_items.get()[ index_in ], nullptr ) );
        return item;
    }
};

template< class T, class Free >
std::vector< Microsoft::WRL::ComPtr< T > > adoptMediaFoundationActivations(
    MediaFoundationActivationArray< T, Free >& array_inout )
{
    std::vector< Microsoft::WRL::ComPtr< T > > values;
    values.reserve( array_inout.size() );
    for( UINT32 index = 0; index < array_inout.size(); ++index )
        values.push_back( array_inout.take( index ) );
    return values;
}

// Source readers must use DISCONNECT_MEDIASOURCE_ON_SHUTDOWN=TRUE. This owner
// then has the only WSE Shutdown obligation, including before a reader exists.
template< class Source >
class MediaFoundationSource final
{
    Microsoft::WRL::ComPtr< Source > m_source;
public:
    MediaFoundationSource() = default;
    MediaFoundationSource( const MediaFoundationSource& ) = delete;
    MediaFoundationSource& operator=( const MediaFoundationSource& ) = delete;
    ~MediaFoundationSource() noexcept { this->Reset(); }
    Source* Get() const noexcept { return this->m_source.Get(); }
    Source* operator->() const noexcept { return this->Get(); }
    // Only receive into an empty owner, just as with ComPtr::GetAddressOf().
    Source** GetAddressOf() noexcept { return this->m_source.GetAddressOf(); }
    void Reset() noexcept
    {
        if( this->m_source ) (void)this->m_source->Shutdown();
        this->m_source.Reset();
    }
};

inline HRESULT createMediaFoundationReaderAttributes( IMFAttributes** attributes_out ) noexcept
{
    auto& attributes = *attributes_out;
    attributes = nullptr;
    const HRESULT result = MFCreateAttributes( &attributes, 3U );
    return FAILED( result ) ? result : attributes->SetUINT32(
        MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, TRUE );
}

struct MediaFoundationRuntimeCalls
{
    HRESULT initializeCom() const noexcept { return CoInitializeEx( nullptr, COINIT_MULTITHREADED ); }
    HRESULT startup() const noexcept { return MFStartup( MF_VERSION, MFSTARTUP_FULL ); }
    void shutdown() const noexcept { (void)MFShutdown(); }
    void uninitializeCom() const noexcept { CoUninitialize(); }
};

// initialize and close run on the same COM apartment thread. S_FALSE adds a COM
// obligation too; RPC_E_CHANGED_MODE borrows the caller's apartment instead.
template< class Calls = MediaFoundationRuntimeCalls >
class MediaFoundationRuntime final
{
    Calls m_calls;
    bool m_com = false, m_mf = false;
    HRESULT m_status = E_FAIL;
public:
    explicit MediaFoundationRuntime( const Calls& calls_in = Calls{} ) noexcept : m_calls( calls_in ) {}
    MediaFoundationRuntime( const MediaFoundationRuntime& ) = delete;
    MediaFoundationRuntime& operator=( const MediaFoundationRuntime& ) = delete;
    ~MediaFoundationRuntime() noexcept { this->close(); }
    HRESULT initialize() noexcept
    {
        if( this->m_mf ) return S_OK;
        this->m_status = this->m_calls.initializeCom();
        this->m_com = SUCCEEDED( this->m_status );
        if( !this->m_com && this->m_status != RPC_E_CHANGED_MODE ) return this->m_status;
        this->m_status = this->m_calls.startup();
        this->m_mf = SUCCEEDED( this->m_status );
        if( !this->m_mf ) this->close();
        return this->m_status;
    }
    void close() noexcept
    {
        if( std::exchange( this->m_mf, false ) ) this->m_calls.shutdown();
        if( std::exchange( this->m_com, false ) ) this->m_calls.uninitializeCom();
    }
};

template< class Backend >
class MediaFoundationOpenGuard final
{
    Backend* m_backend;
public:
    explicit MediaFoundationOpenGuard( Backend& backend_inout ) noexcept : m_backend( &backend_inout ) {}
    MediaFoundationOpenGuard( const MediaFoundationOpenGuard& ) = delete;
    MediaFoundationOpenGuard& operator=( const MediaFoundationOpenGuard& ) = delete;
    ~MediaFoundationOpenGuard() noexcept { if( this->m_backend ) this->m_backend->close(); }
    void commit() noexcept { this->m_backend = nullptr; }
};
} // namespace wse::tmr::detail
#endif
