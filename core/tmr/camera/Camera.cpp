//*****************************************************************************************************************
//! @file    Camera.cpp
//! @brief   \~japanese Portable Tmr Camera sessionの実装.
//! @brief   \~english  Portable Tmr camera session implementation.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <tmr/camera/Camera.h>

#include "CameraBackend.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <utility>

namespace wse
{
namespace tmr
{
namespace detail
{

CameraError makeCameraError(
      const eCameraErrorCategory category_in
    , const eCameraErrorCode     code_in
    , const char* const          message_in
    , const std::int64_t         native_code_in )
{
    return CameraError( category_in, code_in, message_in, native_code_in );
}

} // namespace detail

namespace
{
// Self-stop must not inspect std::thread while its owner is joining that object.
thread_local const void* current_callback_state = nullptr;

std::thread launchCallbackThread( const std::function< void() >& task_in )
{
    return std::thread( task_in );
}
} // namespace

// The session owns at most one backend and at most one callback worker. CallbackState is a
// separate shared object because the worker may outlive the session's interest in it: it keeps
// the backend alive until the worker's current invocation finishes, which is what makes a stop
// requested from inside the callback memory-safe.
class CameraSession::Impl final
{
  public:
    struct CallbackState final
    {
        std::shared_ptr< detail::ICameraBackend > backend;
        std::atomic_bool stop;
        std::atomic_bool active;
        std::mutex backend_mutex;
        std::mutex publication_mutex;
        std::condition_variable publication_condition;
        bool published = false;
        // Construct error storage before capture starts; the worker's allocation-failure
        // path must not allocate while trying to report the failure.
        const CameraResult< sCameraFrame > allocation_error = CameraResult< sCameraFrame >::failure(
            detail::makeCameraError( eCameraErrorCategory::Backend,
                eCameraErrorCode::ResourceExhausted, "Camera frame allocation failed." ) );
        const CameraResult< sCameraFrame > backend_error = CameraResult< sCameraFrame >::failure(
            detail::makeCameraError( eCameraErrorCategory::Backend,
                eCameraErrorCode::BackendFailure, "Camera frame backend threw an exception." ) );

        //! @brief Construct all members with explicit defaults.
        CallbackState()
            : backend ()
            , stop    ( false )
            , active  ( false )
        {
        }
    };

    std::shared_ptr< detail::ICameraBackend > backend;
    std::shared_ptr< CallbackState > callback_state;
    std::thread callback_thread;
    detail::CameraSessionTestAccess::ThreadLauncher thread_launcher = launchCallbackThread;

    bool isCallbackThread() const noexcept
    {
        return this->callback_state && current_callback_state == this->callback_state.get();
    }

    ~Impl() noexcept
    {
        if( !this->callback_thread.joinable() )
            return;
        if( this->isCallbackThread() )
        {
            // Destruction from the callback is discouraged, but remains memory-safe:
            // CallbackState retains the backend until the worker finishes this invocation.
            this->callback_thread.detach();
            return;
        }
        this->callback_thread.join();
    }

    // Joins the finished worker unless the caller IS the worker, in which case the join is
    // deferred to the next stop/start/close from the owner thread; false says "not yet".
    bool reapCallbackThread() noexcept
    {
        if( this->isCallbackThread() )
            return false;
        if( !this->callback_thread.joinable() )
        {
            this->callback_state.reset();
            return true;
        }
        this->callback_thread.join();
        this->callback_state.reset();
        return true;
    }

    CameraStatus prepareStart()
    {
        if( this->callback_state && this->callback_state->active.load() )
            return CameraStatus::failure( detail::makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
                "Camera callback stream is already active." ) );
        if( !this->reapCallbackThread() )
            return CameraStatus::failure( detail::makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
                "A camera callback cannot restart its own worker." ) );
        if( !this->backend || !this->backend->isOpen() )
            return CameraStatus::failure( detail::makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::NotOpen,
                "Camera must be open before starting a stream." ) );
        if( this->backend->isStreaming() )
            return CameraStatus::failure( detail::makeCameraError(
                eCameraErrorCategory::Lifecycle, eCameraErrorCode::AlreadyStreaming,
                "Camera stream is already active." ) );
        return CameraStatus::success();
    }

    CameraStatus startBackend()
    {
        try { return this->backend->start(); }
        catch( ... )
        {
            // A native adapter throwing during start cannot prove its partial state.
            this->backend->close();
            throw;
        }
    }
};

CameraSession::CameraSession()
    : m_impl ( std::make_unique< Impl >() )
{
}

CameraSession::~CameraSession()
{
    this->close();
}

void detail::CameraSessionTestAccess::installBackend(
      CameraSession* const              p_session_inout
    , std::shared_ptr< ICameraBackend > backend_in )
{
    CameraSession& session_inout = *p_session_inout;

    session_inout.close();
    session_inout.m_impl->backend = std::move( backend_in );
}

void detail::CameraSessionTestAccess::setThreadLauncher(
      CameraSession* const p_session_inout
    , ThreadLauncher launcher_in )
{
    CameraSession& session_inout = *p_session_inout;
    session_inout.m_impl->thread_launcher = launcher_in ? launcher_in : launchCallbackThread;
}

CameraResult< std::vector< sCameraDeviceInfo > > CameraSession::enumerate(
    const eCameraBackend backend_in )
{
    return detail::enumeratePlatformCameras( backend_in );
}

CameraResult< sCameraCapability > CameraSession::capabilities( const sCameraDeviceInfo& device_in )
{
    if( !device_in.valid() )
        return CameraResult< sCameraCapability >::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Camera device identity is invalid." ) );
    return detail::queryPlatformCameraCapabilities( device_in );
}

CameraStatus CameraSession::open( const sCameraOpenDescription& description_in )
{
    if( this->isOpen() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::AlreadyOpen
            , "Camera is already open." ) );
    if( !description_in.device.valid() || !description_in.format.valid() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Camera open description is invalid." ) );

    std::unique_ptr< detail::ICameraBackend > backend =
        detail::createPlatformCameraBackend( description_in.device.backend );
    if( !backend )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedBackend
            , "Requested camera backend is unavailable on this platform." ) );

    CameraStatus result = backend->open( description_in );
    if( result.succeeded() )
        this->m_impl->backend = std::shared_ptr< detail::ICameraBackend >( std::move( backend ) );
    return result;
}

CameraStatus CameraSession::open(
      const sCameraDeviceInfo&          device_in
    , const sCameraStreamConfiguration& configuration_in )
{
    if( !configuration_in.valid() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Camera stream configuration is invalid." ) );

    sCameraOpenDescription description;
    description.device = device_in;
    description.format = configuration_in.native_format;
    description.format.pixel_format = configuration_in.output_format;
    description.native_pixel_format = configuration_in.native_format.pixel_format;
    description.allow_format_conversion = configuration_in.allow_conversion;
    return this->open( description );
}

void CameraSession::close() noexcept
{
    if( this->m_impl == nullptr )
        return;
    try { (void)this->stop(); }
    catch( ... ) { /* stop already closed the backend and reaped the worker. */ }
    if( this->m_impl->backend )
        this->m_impl->backend->close();
    this->m_impl->backend.reset();
}

CameraStatus CameraSession::start()
{
    const CameraStatus prepared = this->m_impl->prepareStart();
    if( !prepared.succeeded() ) return prepared;
    return this->m_impl->startBackend();
}

// The callback stream requests 100ms reads; driver calls and callbacks can take longer, so this
// is not a shutdown deadline. A timeout is not delivered to the callback; every other
// error is delivered once and ends the stream. A throwing callback also ends the stream: the
// worker cannot know what the exception left behind, so it stops delivering rather than repeat it.
CameraStatus CameraSession::start( const CameraFrameCallback& callback_in )
{
    if( !callback_in )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Camera frame callback must not be empty." ) );
    const CameraStatus prepared = this->m_impl->prepareStart();
    if( !prepared.succeeded() ) return prepared;
    const auto callback_state = std::make_shared< Impl::CallbackState >();
    callback_state->backend = this->m_impl->backend;
    const std::function< void() > task = [ callback_state, callback_in ]()
    {
        {
            std::unique_lock< std::mutex > lock( callback_state->publication_mutex );
            callback_state->publication_condition.wait( lock,
                [ & ]() { return callback_state->published; } );
        }
        current_callback_state = callback_state.get();
        const auto deliver = [ & ]( const CameraResult< sCameraFrame >& result_in )
        {
            if( callback_state->stop.load() ) return false;
            try { callback_in( result_in ); }
            catch( ... ) { return false; }
            return result_in.succeeded();
        };
        try
        {
            while( !callback_state->stop.load() )
            {
                CameraResult< sCameraFrame > frame_result = [ & ]()
                {
                    std::lock_guard< std::mutex > lock( callback_state->backend_mutex );
                    return callback_state->stop.load() ? callback_state->backend_error
                        : callback_state->backend->readFrame( 100U );
                }();
                if( callback_state->stop.load() ) break;
                if( !frame_result.succeeded()
                    && frame_result.error().code() == eCameraErrorCode::TimedOut ) continue;
                if( !deliver( frame_result ) ) break;
            }
        }
        catch( const std::bad_alloc& ) { (void)deliver( callback_state->allocation_error ); }
        catch( ... ) { (void)deliver( callback_state->backend_error ); }
        callback_state->active.store( false );
        // Keep self identity through destruction of the thread's callable/captures:
        // their last owner can request stop too. This thread exits immediately afterward.
    };
    std::unique_lock< std::mutex > publication_lock( callback_state->publication_mutex );
    const CameraStatus start_result = this->m_impl->startBackend();
    if( !start_result.succeeded() ) return start_result;
    this->m_impl->callback_state = callback_state;
    callback_state->active.store( true );
    try
    {
        this->m_impl->callback_thread = this->m_impl->thread_launcher( task );
    }
    catch( ... )
    {
        try
        {
            if( !this->m_impl->backend->stop().succeeded() ) this->m_impl->backend->close();
        }
        catch( ... ) { this->m_impl->backend->close(); }
        this->m_impl->callback_state.reset();
        throw;
    }
    callback_state->published = true;
    publication_lock.unlock();
    callback_state->publication_condition.notify_one();
    return CameraStatus::success();
}

CameraStatus CameraSession::stop()
{
    if( this->m_impl == nullptr )
        return CameraStatus::success();
    const auto state = this->m_impl->callback_state;
    if( state ) state->stop.store( true );

    CameraStatus backend_result = CameraStatus::success();
    try
    {
        std::unique_lock< std::mutex > lock;
        if( state ) lock = std::unique_lock< std::mutex >( state->backend_mutex );
        try
        {
            if( this->m_impl->backend && this->m_impl->backend->isStreaming() )
                backend_result = this->m_impl->backend->stop();
        }
        catch( ... )
        {
            this->m_impl->backend->close();
            throw;
        }
    }
    catch( ... )
    {
        (void)this->m_impl->reapCallbackThread();
        throw;
    }

    // A callback may request stop. Its owner performs the deferred join on the
    // next stop/start/close instead of detaching a normally owned worker.
    if( !this->m_impl->reapCallbackThread() )
        return backend_result;
    return backend_result;
}

CameraResult< sCameraFrame > CameraSession::readFrame( const std::uint32_t timeout_ms_in )
{
    if( !this->isOpen() )
        return CameraResult< sCameraFrame >::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::NotOpen
            , "Camera is not open." ) );
    if( this->m_impl->callback_state && this->m_impl->callback_state->active.load() )
        return CameraResult< sCameraFrame >::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::ConcurrentRead
            , "Synchronous reads are disabled while a callback stream is active." ) );
    if( timeout_ms_in == 0U )
        return CameraResult< sCameraFrame >::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Camera frame timeout must be greater than zero." ) );
    return this->m_impl->backend->readFrame( timeout_ms_in );
}

CameraResult< sCameraControlValue > CameraSession::getControl( const eCameraControl control_in )
{
    if( !this->isOpen() )
        return CameraResult< sCameraControlValue >::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::NotOpen
            , "Camera is not open." ) );
    return this->m_impl->backend->getControl( control_in );
}

CameraStatus CameraSession::setControl( const sCameraControlValue& value_in )
{
    if( !this->isOpen() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::NotOpen
            , "Camera is not open." ) );
    return this->m_impl->backend->setControl( value_in );
}

CameraResult< sCameraExtensionUnitValue > CameraSession::getExtensionUnit(
    const sCameraExtensionUnitSelector& selector_in )
{
    if( !this->isOpen() )
        return CameraResult< sCameraExtensionUnitValue >::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::NotOpen
            , "Camera is not open." ) );
    if( !selector_in.valid() )
        return CameraResult< sCameraExtensionUnitValue >::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Extension-unit selector is invalid." ) );
    if( !selector_in.readable )
        return CameraResult< sCameraExtensionUnitValue >::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::AccessDenied
            , "Extension-unit selector is not readable." ) );
    return this->m_impl->backend->getExtensionUnit( selector_in );
}

CameraStatus CameraSession::setExtensionUnit( const sCameraExtensionUnitValue& value_in )
{
    if( !this->isOpen() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Lifecycle
            , eCameraErrorCode::NotOpen
            , "Camera is not open." ) );
    if( !value_in.selector.valid() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::InvalidArgument
            , "Extension-unit selector is invalid." ) );
    if( !value_in.selector.writable )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::AccessDenied
            , "Extension-unit selector is not writable." ) );
    if( !value_in.valid() )
        return CameraStatus::failure( detail::makeCameraError(
              eCameraErrorCategory::Validation
            , eCameraErrorCode::PayloadSizeMismatch
            , "Extension-unit payload length is outside the advertised range." ) );
    return this->m_impl->backend->setExtensionUnit( value_in );
}

bool CameraSession::isOpen() const noexcept
{
    std::unique_lock< std::mutex > lock;
    if( this->m_impl && this->m_impl->callback_state )
        lock = std::unique_lock< std::mutex >( this->m_impl->callback_state->backend_mutex );
    return this->m_impl != nullptr && this->m_impl->backend
        && this->m_impl->backend->isOpen();
}

bool CameraSession::isStreaming() const noexcept
{
    std::unique_lock< std::mutex > lock;
    if( this->m_impl && this->m_impl->callback_state )
        lock = std::unique_lock< std::mutex >( this->m_impl->callback_state->backend_mutex );
    return this->m_impl != nullptr && this->m_impl->backend
        && this->m_impl->backend->isStreaming();
}

} // namespace tmr
} // namespace wse
