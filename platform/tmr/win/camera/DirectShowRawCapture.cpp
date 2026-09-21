// Native packed-video capture for profiles the MF source cannot stream.
// Only direct pin connections are allowed: a converter must never rewrite native bytes.
#include "DirectShowRawCapture.h"
#include "DirectShowCaptureFormat.h"
#include <wrl/client.h>
#include <setupapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace wse { namespace tmr { namespace detail {
namespace {
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

// The Sample Grabber COM ABI is no longer declared by the Windows SDK.
struct __declspec(uuid("0579154A-2B53-4994-B0D0-E773148EFF85")) GrabberCallback : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SampleCB( double time_in, IMediaSample* sample_in ) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB( double time_in, BYTE* buffer_in, long size_in ) = 0;
};
struct __declspec(uuid("6B652FFF-11FE-4FCE-92AD-0266B5D7C78F")) SampleGrabber : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetOneShot( BOOL value_in ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType( const AM_MEDIA_TYPE* type_in ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( AM_MEDIA_TYPE* type_out ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples( BOOL value_in ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer( long* size_inout, long* buffer_out ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample( IMediaSample** sample_out ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback( GrabberCallback* callback_in, long method_in ) = 0;
};
const CLSID sample_grabber_class = { 0xc1f400a0, 0x3f08, 0x11d3, { 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 } };
const CLSID null_renderer_class = { 0xc1f400a4, 0x3f08, 0x11d3, { 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 } };

void freeMediaType( AM_MEDIA_TYPE* const type_inout ) noexcept
{
    AM_MEDIA_TYPE& type = *type_inout;
    CoTaskMemFree( type.pbFormat );
    ComPtr< IUnknown > unknown;
    unknown.Attach( type.pUnk );
    type = {};
}
struct MediaTypeOwner
{
    AM_MEDIA_TYPE* value = nullptr;
    ~MediaTypeOwner() { if( value ) { freeMediaType( value ); CoTaskMemFree( value ); } }
};

CameraError error( const eCameraErrorCode code_in, const char* message_in, const HRESULT native_in )
{
    return CameraError( code_in == eCameraErrorCode::DeviceDisconnected
        ? eCameraErrorCategory::Device : eCameraErrorCategory::InputOutput,
        code_in, message_in, native_in );
}

std::wstring wideIdentity( const std::string& text_in )
{
    const int size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text_in.data(),
        static_cast< int >( text_in.size() ), nullptr, 0 );
    if( size <= 0 ) return {};
    std::wstring result( static_cast< std::size_t >( size ), L'\0' );
    MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text_in.data(),
        static_cast< int >( text_in.size() ), result.data(), size );
    return result;
}

std::wstring deviceInstance( const wchar_t* path_in )
{
    struct DeviceSet
    {
        HDEVINFO value = SetupDiCreateDeviceInfoList( nullptr, nullptr );
        ~DeviceSet() { if( value != INVALID_HANDLE_VALUE ) SetupDiDestroyDeviceInfoList( value ); }
    } devices;
    if( devices.value == INVALID_HANDLE_VALUE ) return {};
    SP_DEVICE_INTERFACE_DATA interface_data{};
    interface_data.cbSize = sizeof( interface_data );
    if( !SetupDiOpenDeviceInterfaceW( devices.value, path_in, 0, &interface_data ) ) return {};
    SP_DEVINFO_DATA device_data{};
    device_data.cbSize = sizeof( device_data );
    DWORD required = 0;
    // The size query also fills the devnode, despite ERROR_INSUFFICIENT_BUFFER.
    (void)SetupDiGetDeviceInterfaceDetailW( devices.value, &interface_data, nullptr, 0, &required, &device_data );
    if( GetLastError() != ERROR_INSUFFICIENT_BUFFER ) return {};
    required = 0;
    (void)SetupDiGetDeviceInstanceIdW( devices.value, &device_data, nullptr, 0, &required );
    if( required == 0U ) return {};
    std::wstring identity( required, L'\0' );
    if( !SetupDiGetDeviceInstanceIdW( devices.value, &device_data, identity.data(), required, nullptr ) ) return {};
    identity.resize( required - 1U );
    return identity;
}

HRESULT findSource( IBaseFilter** source_out, const std::string& identity_in )
{
    IBaseFilter*& source = *source_out;
    source = nullptr;
    const auto wanted = wideIdentity( identity_in );
    if( wanted.empty() ) return E_INVALIDARG;
    const auto wanted_instance = deviceInstance( wanted.c_str() );
    ComPtr< IMoniker > instance_match;
    unsigned matches = 0U;
    ComPtr< ICreateDevEnum > devices;
    HRESULT result = CoCreateInstance( CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS( devices.GetAddressOf() ) );
    if( FAILED( result ) ) return result;
    ComPtr< IEnumMoniker > entries;
    result = devices->CreateClassEnumerator( CLSID_VideoInputDeviceCategory, entries.GetAddressOf(), 0 );
    if( result != S_OK ) return HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
    ComPtr< IMoniker > entry;
    while( entries->Next( 1U, entry.ReleaseAndGetAddressOf(), nullptr ) == S_OK )
    {
        ComPtr< IPropertyBag > properties;
        if( FAILED( entry->BindToStorage( nullptr, nullptr, IID_PPV_ARGS( properties.GetAddressOf() ) ) ) ) continue;
        VARIANT path;
        VariantInit( &path );
        result = properties->Read( L"DevicePath", &path, nullptr );
        const bool match = SUCCEEDED( result ) && path.vt == VT_BSTR
            && CompareStringOrdinal( path.bstrVal, -1, wanted.c_str(), -1, TRUE ) == CSTR_EQUAL;
        // MF and DirectShow may expose different interface-class paths for the same devnode.
        // Resolve through SetupAPI; never select by friendly name or enumeration order.
        if( !match && SUCCEEDED( result ) && path.vt == VT_BSTR && !wanted_instance.empty() )
        {
            const auto instance = deviceInstance( path.bstrVal );
            if( !instance.empty() && CompareStringOrdinal( instance.c_str(), -1,
                wanted_instance.c_str(), -1, TRUE ) == CSTR_EQUAL )
            { instance_match = entry; ++matches; }
        }
        VariantClear( &path );
        if( match ) return entry->BindToObject( nullptr, nullptr, IID_IBaseFilter,
            reinterpret_cast< void** >( &source ) );
    }
    if( matches == 1U ) return instance_match->BindToObject( nullptr, nullptr, IID_IBaseFilter,
        reinterpret_cast< void** >( &source ) );
    if( matches > 1U ) return HRESULT_FROM_WIN32( ERROR_DUP_NAME );
    return HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
}

ComPtr< IPin > pin( IBaseFilter* filter_in, const PIN_DIRECTION direction_in )
{
    ComPtr< IEnumPins > entries;
    if( FAILED( filter_in->EnumPins( entries.GetAddressOf() ) ) ) return {};
    ComPtr< IPin > candidate;
    while( entries->Next( 1U, candidate.ReleaseAndGetAddressOf(), nullptr ) == S_OK )
    {
        PIN_DIRECTION direction;
        if( SUCCEEDED( candidate->QueryDirection( &direction ) ) && direction == direction_in ) return candidate;
    }
    return {};
}

struct SampleState
{
    std::mutex mutex;
    std::condition_variable ready;
    sCameraFrameDescription description;
    sCameraFrame latest;
    bool active = false;
    HRESULT failure = S_OK;
    std::uint64_t sequence = 0U;
};

class Callback final : public GrabberCallback
{
    std::atomic< ULONG > m_refs{ 1U };
    std::shared_ptr< SampleState > m_state;
  public:
    explicit Callback( std::shared_ptr< SampleState > state_in ) : m_state( std::move( state_in ) ) {}
    STDMETHODIMP QueryInterface( REFIID iid_in, void** object_out ) override
    {
        if( object_out == nullptr ) return E_POINTER;
        void*& object = *object_out;
        object = nullptr;
        if( iid_in != IID_IUnknown && iid_in != __uuidof( GrabberCallback ) ) return E_NOINTERFACE;
        object = static_cast< GrabberCallback* >( this );
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++m_refs; }
    STDMETHODIMP_(ULONG) Release() override
    {
        const ULONG remaining = --m_refs;
        if( remaining == 0U ) delete this;
        return remaining;
    }
    STDMETHODIMP SampleCB( double, IMediaSample* ) override { return E_NOTIMPL; }
    STDMETHODIMP BufferCB( double, BYTE* buffer_in, const long size_in ) override
    {
        std::lock_guard< std::mutex > lock( m_state->mutex );
        if( !m_state->active ) return S_OK;
        try
        {
            const auto& description = m_state->description;
            const auto required = description.row_stride * description.height;
            if( buffer_in == nullptr || size_in < 0 || static_cast< std::size_t >( size_in ) < required )
                m_state->failure = E_INVALIDARG;
            else
            {
                sCameraFrame frame;
                frame.description = description;
                frame.data.assign( buffer_in, buffer_in + required );
                frame.sequence = ++m_state->sequence;
                frame.monotonic_timestamp_ns = std::chrono::duration_cast< std::chrono::nanoseconds >(
                    Clock::now().time_since_epoch() ).count();
                m_state->latest = std::move( frame );
            }
        }
        catch( ... ) { m_state->failure = E_OUTOFMEMORY; }
        m_state->ready.notify_all();
        return S_OK;
    }
};
} // namespace

class DirectShowRawCapture::Impl
{
  public:
    const char* phase = "DirectShow native source lookup failed.";
    ComPtr< IGraphBuilder > graph;
    ComPtr< IBaseFilter > source;
    ComPtr< IBaseFilter > grabber_filter;
    ComPtr< IBaseFilter > sink;
    ComPtr< SampleGrabber > grabber;
    ComPtr< IMediaControl > control;
    ComPtr< IMediaEvent > events;
    ComPtr< Callback > callback;
    std::shared_ptr< SampleState > state = std::make_shared< SampleState >();

    ~Impl()
    {
        { std::lock_guard< std::mutex > lock( state->mutex ); state->active = false; state->ready.notify_all(); }
        if( control ) (void)control->Stop();
        if( grabber ) (void)grabber->SetCallback( nullptr, 1 );
        // The graph and filters release their callback references before our state can disappear.
    }

    HRESULT configure( const sCameraOpenDescription& description_in )
    {
        HRESULT result = findSource( source.GetAddressOf(), description_in.device.id );
        if( FAILED( result ) ) return result;
        phase = "DirectShow native graph creation failed.";
        result = CoCreateInstance( CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( graph.GetAddressOf() ) );
        if( FAILED( result ) ) return result;
        ComPtr< ICaptureGraphBuilder2 > builder;
        result = CoCreateInstance( CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( builder.GetAddressOf() ) );
        if( FAILED( result ) ) return result;
        if( FAILED( result = builder->SetFiltergraph( graph.Get() ) ) ) return result;
        if( FAILED( result = graph->AddFilter( source.Get(), L"Native camera" ) ) ) return result;
        ComPtr< IPin > capture;
        phase = "DirectShow native capture pin lookup failed.";
        if( FAILED( result = builder->FindPin( source.Get(), PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE,
            &MEDIATYPE_Video, FALSE, 0, capture.GetAddressOf() ) ) ) return result;
        ComPtr< IAMStreamConfig > config;
        phase = "DirectShow native format selection failed.";
        if( FAILED( result = capture.As( &config ) ) ) return result;
        int count = 0, capability_size = 0;
        if( FAILED( result = config->GetNumberOfCapabilities( &count, &capability_size ) ) ) return result;
        if( capability_size <= 0 ) return E_INVALIDARG;
        std::vector< BYTE > capabilities( static_cast< std::size_t >( capability_size ) );
        const auto& format = description_in.format;
        const GUID subtype = format.pixel_format == eCameraPixelFormat::Uyvy422 ? MEDIASUBTYPE_UYVY : MEDIASUBTYPE_YUY2;
        MediaTypeOwner selected;
        for( int index = 0; index < count; ++index )
        {
            MediaTypeOwner candidate;
            if( FAILED( config->GetStreamCaps( index, &candidate.value, capabilities.data() ) ) ) continue;
            if( !candidate.value || !matchesDirectShowFormat( *candidate.value, subtype, format, false ) ) continue;
            VIDEO_STREAM_CONFIG_CAPS caps{};
            if( capabilities.size() >= sizeof( caps ) ) std::memcpy( &caps, capabilities.data(), sizeof( caps ) );
            auto& header = *reinterpret_cast< VIDEOINFOHEADER* >( candidate.value->pbFormat );
            if( !selectDirectShowInterval( &header, caps, format ) ) continue;
            selected.value = candidate.value;
            candidate.value = nullptr;
            break;
        }
        if( !selected.value ) return VFW_E_TYPE_NOT_ACCEPTED;
        phase = "DirectShow native format setting failed.";
        if( FAILED( result = config->SetFormat( selected.value ) ) ) return result;
        // SetFormat may choose a nearby rate. Never publish it as the requested rate.
        phase = "DirectShow selected format readback failed.";
        MediaTypeOwner actual;
        if( FAILED( result = config->GetFormat( &actual.value ) ) ) return result;
        if( !actual.value || !matchesDirectShowFormat( *actual.value, subtype, format ) ) return VFW_E_TYPE_NOT_ACCEPTED;
        phase = "DirectShow sample grabber creation failed.";
        if( FAILED( result = CoCreateInstance( sample_grabber_class, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( grabber_filter.GetAddressOf() ) ) ) ) return result;
        if( FAILED( result = grabber_filter.As( &grabber ) ) ) return result;
        if( FAILED( result = grabber->SetMediaType( selected.value ) ) ) return result;
        if( FAILED( result = grabber->SetOneShot( FALSE ) ) ) return result;
        if( FAILED( result = grabber->SetBufferSamples( FALSE ) ) ) return result;
        if( FAILED( result = graph->AddFilter( grabber_filter.Get(), L"Native sample copy" ) ) ) return result;
        if( FAILED( result = CoCreateInstance( null_renderer_class, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( sink.GetAddressOf() ) ) ) ) return result;
        if( FAILED( result = graph->AddFilter( sink.Get(), L"Native sink" ) ) ) return result;
        const auto input = pin( grabber_filter.Get(), PINDIR_INPUT );
        phase = "DirectShow native pin connection failed.";
        const auto output = pin( grabber_filter.Get(), PINDIR_OUTPUT );
        const auto sink_input = pin( sink.Get(), PINDIR_INPUT );
        if( !input || !output || !sink_input ) return E_NOINTERFACE;
        if( FAILED( result = graph->ConnectDirect( capture.Get(), input.Get(), selected.value ) ) ) return result;
        if( FAILED( result = graph->ConnectDirect( output.Get(), sink_input.Get(), nullptr ) ) ) return result;
        AM_MEDIA_TYPE connected{};
        phase = "DirectShow connected format verification failed.";
        result = grabber->GetConnectedMediaType( &connected );
        if( FAILED( result ) ) return result;
        if( !matchesDirectShowFormat( connected, subtype, format ) ) result = VFW_E_TYPE_NOT_ACCEPTED;
        else
        {
            const auto& header = *reinterpret_cast< const VIDEOINFOHEADER* >( connected.pbFormat );
            const auto& bitmap = header.bmiHeader;
            {
                state->description.width = format.width;
                state->description.height = format.height;
                state->description.pixel_format = format.pixel_format;
                state->description.row_stride = static_cast< std::size_t >( format.width ) * 2U;
                if( bitmap.biSizeImage != 0U )
                {
                    if( bitmap.biSizeImage % format.height != 0U
                        || bitmap.biSizeImage / format.height < state->description.row_stride ) result = VFW_E_TYPE_NOT_ACCEPTED;
                    else state->description.row_stride = bitmap.biSizeImage / format.height;
                }
            }
        }
        freeMediaType( &connected );
        if( FAILED( result ) ) return result;
        callback.Attach( new Callback( state ) );
        if( FAILED( result = grabber->SetCallback( callback.Get(), 1 ) ) ) return result;
        if( FAILED( result = graph.As( &control ) ) ) return result;
        return graph.As( &events );
    }
};

DirectShowRawCapture::DirectShowRawCapture() : m_impl( std::make_unique< Impl >() ) {}
DirectShowRawCapture::~DirectShowRawCapture() = default;
IBaseFilter* DirectShowRawCapture::source() const noexcept { return m_impl->source.Get(); }
CameraStatus DirectShowRawCapture::open( const sCameraOpenDescription& description_in )
{
    const HRESULT result = m_impl->configure( description_in );
    if( FAILED( result ) ) return CameraStatus::failure( error( eCameraErrorCode::ConfigurationFailed,
        m_impl->phase, result ) );
    return CameraStatus::success();
}
CameraStatus DirectShowRawCapture::start()
{
    {
        std::lock_guard< std::mutex > lock( m_impl->state->mutex );
        m_impl->state->latest = {};
        m_impl->state->failure = S_OK;
        m_impl->state->active = true;
    }
    const HRESULT result = m_impl->control->Run();
    if( FAILED( result ) )
    {
        (void)stop();
        return CameraStatus::failure( error( eCameraErrorCode::ConfigurationFailed,
            "DirectShow native camera could not start.", result ) );
    }
    return CameraStatus::success();
}
CameraStatus DirectShowRawCapture::stop()
{
    {
        std::lock_guard< std::mutex > lock( m_impl->state->mutex );
        m_impl->state->active = false;
        m_impl->state->latest = {};
        m_impl->state->ready.notify_all();
    }
    const HRESULT result = m_impl->control ? m_impl->control->Stop() : S_OK;
    if( FAILED( result ) ) return CameraStatus::failure( error( eCameraErrorCode::DeviceDisconnected,
        "DirectShow native camera could not stop.", result ) );
    return CameraStatus::success();
}
CameraResult< sCameraFrame > DirectShowRawCapture::readFrame( const std::uint32_t timeout_ms_in )
{
    auto& state = *m_impl->state;
    const auto deadline = Clock::now() + std::chrono::milliseconds( timeout_ms_in );
    std::unique_lock< std::mutex > lock( state.mutex );
    while( state.active && SUCCEEDED( state.failure ) && state.latest.data.empty() )
    {
        long event = 0;
        LONG_PTR first = 0, second = 0;
        while( m_impl->events->GetEvent( &event, &first, &second, 0 ) == S_OK )
        {
            (void)m_impl->events->FreeEventParams( event, first, second );
            if( event == EC_DEVICE_LOST || event == EC_ERRORABORT || event == EC_COMPLETE )
                state.failure = event == EC_ERRORABORT ? static_cast< HRESULT >( first ) : HRESULT_FROM_WIN32( ERROR_DEVICE_NOT_CONNECTED );
        }
        if( FAILED( state.failure ) || Clock::now() >= deadline ) break;
        state.ready.wait_until( lock, ( std::min )( deadline, Clock::now() + std::chrono::milliseconds( 50 ) ) );
    }
    if( !state.active ) return CameraResult< sCameraFrame >::failure( CameraError(
        eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotStreaming, "Native camera capture stopped." ) );
    if( FAILED( state.failure ) ) return CameraResult< sCameraFrame >::failure( error(
        eCameraErrorCode::DeviceDisconnected, "DirectShow native camera stopped producing frames.", state.failure ) );
    if( state.latest.data.empty() ) return CameraResult< sCameraFrame >::failure( CameraError(
        eCameraErrorCategory::Timeout, eCameraErrorCode::TimedOut, "Native camera frame deadline expired." ) );
    auto frame = std::move( state.latest );
    state.latest = {};
    return CameraResult< sCameraFrame >::success( std::move( frame ) );
}

} } }
