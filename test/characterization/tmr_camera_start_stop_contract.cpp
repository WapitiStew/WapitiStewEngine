// Fault injection for the real portable session. No hardware access or certification.
#include "../../core/tmr/camera/CameraBackend.h"
#include <tmr/camera/Camera.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>

namespace
{
using namespace wse::tmr;
int failures = 0;
void expect( bool value_in, const char* message_in )
{
    if( !value_in ) { ++failures; std::cerr << message_in << '\n'; }
}
class Backend final : public detail::ICameraBackend
{
public:
    bool opened = true, streaming = false;
    unsigned starts = 0, stops = 0, closes = 0;
    bool throw_start = false, fail_stop = false, throw_stop = false;
    std::shared_ptr< bool > fail_copy_on_start;
    int throw_read = 0;
    std::atomic_bool reading{ false }, overlap{ false };
    std::atomic_bool stop_entered{ false }, callback_stop_attempted{ false };
    bool block_read = false, race_stop = false;
    CameraStatus open( const sCameraOpenDescription& ) override { opened = true; return CameraStatus::success(); }
    void close() noexcept override { ++closes; opened = streaming = false; }
    CameraStatus start() override
    {
        ++starts; streaming = true;
        if( fail_copy_on_start ) *fail_copy_on_start = true;
        if( throw_start ) throw std::bad_alloc();
        return CameraStatus::success();
    }
    CameraStatus stop() override
    {
        ++stops;
        if( reading.load() ) overlap = true;
        stop_entered = true;
        if( race_stop )
        {
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 2 );
            while( !callback_stop_attempted.load() && std::chrono::steady_clock::now() < until )
                std::this_thread::yield();
        }
        if( throw_stop ) throw std::runtime_error( "stop fault" );
        if( fail_stop ) return CameraStatus::failure( CameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::BackendFailure, "stop fault" ) );
        streaming = false;
        return CameraStatus::success();
    }
    CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in ) override
    {
        if( throw_read == 1 ) throw std::bad_alloc();
        if( throw_read == 2 ) throw std::runtime_error( "read fault" );
        reading = true;
        if( block_read ) std::this_thread::sleep_for( std::chrono::milliseconds( timeout_ms_in ) );
        sCameraFrame frame;
        frame.description = { 2U, 2U, eCameraPixelFormat::Bgra8, 8U };
        frame.data.resize( 16U );
        reading = false;
        return CameraResult< sCameraFrame >::success( std::move( frame ) );
    }
    CameraResult< sCameraControlValue > getControl( eCameraControl ) override
    { return CameraResult< sCameraControlValue >::success( {} ); }
    CameraStatus setControl( const sCameraControlValue& ) override { return CameraStatus::success(); }
    bool isOpen() const noexcept override { return opened; }
    bool isStreaming() const noexcept override { return streaming; }
};
std::thread failThread( const std::function< void() >& )
{ throw std::system_error( std::make_error_code( std::errc::resource_unavailable_try_again ) ); }
std::thread failAllocation( const std::function< void() >& ) { throw std::bad_alloc(); }
struct ThrowingCopy
{
    std::shared_ptr< bool > fail;
    explicit ThrowingCopy( std::shared_ptr< bool > fail_in ) : fail( std::move( fail_in ) ) {}
    ThrowingCopy( const ThrowingCopy& other_in ) : fail( other_in.fail )
    { if( *fail ) throw std::bad_alloc(); }
    void operator()( const CameraResult< sCameraFrame >& ) const {}
};
struct StopOnCaptureRelease
{
    CameraSession* session;
    std::atomic_bool* stopped;
    ~StopOnCaptureRelease() { stopped->store( session->stop().succeeded() ); }
};
std::atomic_bool launcher_returned{ false };
std::atomic_uint publication_callbacks{ 0U };
std::thread delayedLaunch( const std::function< void() >& task_in )
{
    std::thread worker( task_in );
    std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    expect( publication_callbacks == 0U, "Worker must wait for thread ownership publication" );
    launcher_returned = true;
    return worker;
}
template< typename Predicate > bool waitFor( Predicate predicate_in )
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 2 );
    while( !predicate_in() && std::chrono::steady_clock::now() < until ) std::this_thread::yield();
    return predicate_in();
}
} // namespace

int main()
{
    using namespace wse::tmr;
    // Callable copies fail before native start; the original exception remains observable.
    {
        auto backend = std::make_shared< Backend >();
        auto fail = std::make_shared< bool >( false );
        CameraFrameCallback callback = ThrowingCopy( fail );
        *fail = true;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        bool caught = false;
        try { (void)session.start( callback ); } catch( const std::bad_alloc& ) { caught = true; }
        expect( caught && backend->starts == 0 && session.isOpen() && !session.isStreaming(),
            "Callable-copy failure leaves the open backend untouched" );
        expect( session.start().succeeded(), "Synchronous start succeeds" );
        expect( session.start( callback ).error().code() == eCameraErrorCode::AlreadyStreaming,
            "Lifecycle rejection precedes a throwing callable copy" );
    }
    {
        auto backend = std::make_shared< Backend >();
        auto fail = std::make_shared< bool >( false );
        CameraFrameCallback callback = ThrowingCopy( fail );
        backend->fail_copy_on_start = fail;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        bool caught = false;
        try { (void)session.start( callback ); } catch( const std::bad_alloc& ) { caught = true; }
        expect( caught && backend->starts == 1 && backend->stops == 1 && !session.isStreaming(),
            "Actual std::thread callable-copy failure rolls back the started backend" );
    }
    for( auto launcher : { failThread, failAllocation } )
    for( int stop_fault : { 0, 1, 2 } )
    {
        auto backend = std::make_shared< Backend >();
        backend->fail_stop = stop_fault == 1;
        backend->throw_stop = stop_fault == 2;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        detail::CameraSessionTestAccess::setThreadLauncher( &session, launcher );
        bool caught = false;
        try { (void)session.start( []( const CameraResult< sCameraFrame >& ) {} ); }
        catch( const std::system_error& ) { caught = launcher == failThread; }
        catch( const std::bad_alloc& ) { caught = launcher == failAllocation; }
        expect( caught && backend->starts == 1 && backend->stops == 1 && !session.isStreaming(),
            "Thread failure preserves its exception and rolls back capture" );
        expect( session.isOpen() == ( stop_fault == 0 ), "Failed rollback closes the backend" );
        if( stop_fault == 0 )
        {
            detail::CameraSessionTestAccess::setThreadLauncher( &session, nullptr );
            expect( session.start().succeeded() && session.stop().succeeded(),
                "Successful rollback allows restart without reopening" );
        }
    }
    {
        auto backend = std::make_shared< Backend >();
        backend->throw_start = true;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        bool caught = false;
        try { (void)session.start(); } catch( const std::bad_alloc& ) { caught = true; }
        expect( caught && !session.isOpen() && backend->closes == 1,
            "Throwing native start closes partial state before rethrow" );
    }
    for( int read_fault : { 1, 2 } )
    {
        auto backend = std::make_shared< Backend >();
        backend->throw_read = read_fault;
        std::atomic_uint callbacks{ 0U };
        std::atomic_bool correct{ false };
        auto lifetime = std::make_shared< int >( 0 );
        std::weak_ptr< int > finished = lifetime;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        expect( session.start( [ &, lifetime ]( const CameraResult< sCameraFrame >& result_in )
        {
            ++callbacks;
            correct = !result_in.succeeded() && result_in.error().code() == ( read_fault == 1
                ? eCameraErrorCode::ResourceExhausted : eCameraErrorCode::BackendFailure );
        } ).succeeded(), "Throwing read worker starts" );
        lifetime.reset();
        expect( waitFor( [ & ]() { return finished.expired(); } ), "Throwing read ends the worker" );
        expect( callbacks == 1 && correct && session.isStreaming(),
            "Read exception becomes one terminal result, retaining explicit stop contract" );
        expect( session.stop().succeeded(), "Owner reaps read-exception worker" );
    }
    {
        auto backend = std::make_shared< Backend >();
        std::atomic_bool stopped{ false };
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        detail::CameraSessionTestAccess::setThreadLauncher( &session, delayedLaunch );
        expect( session.start( [ & ]( const CameraResult< sCameraFrame >& )
        {
            ++publication_callbacks;
            stopped = launcher_returned && session.stop().succeeded();
        } ).succeeded(), "Delayed thread launch succeeds" );
        expect( waitFor( [ & ]() { return stopped.load(); } ), "Published worker can self-stop" );
        (void)session.stop();
    }
    {
        auto backend = std::make_shared< Backend >();
        backend->block_read = true;
        std::atomic_uint callbacks{ 0U };
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        (void)session.start( [ & ]( const CameraResult< sCameraFrame >& ) { ++callbacks; } );
        expect( waitFor( [ & ]() { return backend->reading.load(); } ), "Read is in flight" );
        expect( session.stop().succeeded() && !backend->overlap && callbacks == 0,
            "Native stop waits for read completion and suppresses cancelled delivery" );
    }
    for( unsigned iteration = 0; iteration < 30; ++iteration )
    {
        auto backend = std::make_shared< Backend >();
        backend->race_stop = true;
        std::atomic_bool callback_entered{ false }, callback_stopped{ false };
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        (void)session.start( [ & ]( const CameraResult< sCameraFrame >& )
        {
            callback_entered = true;
            if( !waitFor( [ & ]() { return backend->stop_entered.load(); } ) ) return;
            backend->callback_stop_attempted = true;
            callback_stopped = session.stop().succeeded();
        } );
        expect( waitFor( [ & ]() { return callback_entered.load(); } ), "Race callback entered" );
        expect( session.stop().succeeded() && callback_stopped && backend->stops == 1,
            "Owner join and callback stop serialize one native stop without self-join" );
    }
    {
        auto backend = std::make_shared< Backend >();
        backend->throw_stop = true;
        CameraSession session;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        (void)session.start();
        session.close();
        expect( !session.isOpen(), "noexcept close survives a throwing native stop" );
    }
    {
        auto backend = std::make_shared< Backend >();
        std::atomic_bool release{ false }, stopped{ false };
        CameraSession session;
        auto capture = std::make_shared< StopOnCaptureRelease >();
        capture->session = &session;
        capture->stopped = &stopped;
        detail::CameraSessionTestAccess::installBackend( &session, backend );
        (void)session.start( [ &, capture ]( const CameraResult< sCameraFrame >& )
        {
            (void)waitFor( [ & ]() { return release.load(); } );
            throw std::runtime_error( "end delivery before capture release" );
        } );
        capture.reset();
        release = true;
        expect( waitFor( [ & ]() { return stopped.load(); } ),
            "Last callback capture can stop during worker callable destruction without self-join" );
        (void)session.stop();
    }
    return failures == 0 ? 0 : 1;
}
