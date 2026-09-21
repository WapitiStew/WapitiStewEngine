//*****************************************************************************************************************
//!
//! @file    module.cpp
//! @brief   \~japanese WSE Binding facadeのpybind11 Adapter.
//! @brief   \~english  pybind11 adapter for the WSE binding native facade.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#include <utility>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <wse/binding/stew.h>
#ifdef WSE_HAS_TMR
#include <tmr/camera/CameraFrameOps.h>
#include <tmr/stew.h>
#endif
#ifdef WSE_HAS_OUI
#include <oui/stew.h>
#endif
#ifdef WSE_HAS_IUI
#include <iui/stew.h>
#include <wse/binding/IuiErrorAdapter.h>
#endif
#ifdef WSE_HAS_XPT
#include <xpt/stew.h>
#include <wse/binding/XptErrorAdapter.h>
#include "../../common/transfer_failure.h"
#endif
#ifdef WSE_EXTENSION_PYTHON_BINDING
#include "wse_extension_python_includes.inc"
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_includes.inc"
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace
{

PyObject* g_wse_error_type = nullptr;
#ifdef WSE_HAS_XPT
PyObject* g_wse_http_status_error_type = nullptr;
PyObject* g_wse_transfer_error_type = nullptr;
#endif

class WseException final : public std::runtime_error
{
  private:
    wse::binding::Error m_error;

  public:
    explicit WseException( const wse::binding::Error& error_in )
        : std::runtime_error ( error_in.message() )
        , m_error            ( error_in )
    {
    }

    const wse::binding::Error& error() const noexcept
    {
        return this->m_error;
    }
};

[[noreturn]] void throwError( const wse::binding::Error& error_in )
{
    throw WseException( error_in );
}

wse::binding::Error invalidStateError()
{
    return wse::binding::Error(
          wse::binding::eErrorCategory::InvalidState
        , 1
        , "The WSE Python runtime is closed."
    );
}

wse::binding::Error invalidBufferError()
{
    return wse::binding::Error(
          wse::binding::eErrorCategory::InvalidArgument
        , 2
        , "copy_frame requires a contiguous Python buffer."
    );
}

#ifdef WSE_HAS_OUI
wse::binding::Error rendererError( const wse::oui::RendererError& error_in )
{
    return wse::binding::fromOuiError( error_in );
}
#endif

std::vector<std::uint8_t> copyPythonBuffer( const py::buffer& buffer_in )
{
    Py_buffer view = {};
    if ( PyObject_GetBuffer( buffer_in.ptr(), &view, PyBUF_CONTIG_RO ) != 0 )
    {
        PyErr_Clear();
        throwError( invalidBufferError() );
    }

    try
    {
        if ( view.len < 0 )
        {
            PyBuffer_Release( &view );
            throwError( invalidBufferError() );
        }
        const auto size = static_cast<std::size_t>( view.len );
        std::vector<std::uint8_t> result( size );
        if ( size != 0U )
        {
            std::memcpy( result.data(), view.buf, size );
        }
        PyBuffer_Release( &view );
        return result;
    }
    catch ( const WseException& )
    {
        throw;
    }
    catch ( const std::bad_alloc& )
    {
        PyBuffer_Release( &view );
        throwError( wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "Unable to allocate the Python binding input buffer."
        ) );
    }
    catch ( ... )
    {
        PyBuffer_Release( &view );
        throw;
    }
}

py::dict runtimeInfo()
{
    const auto info = wse::binding::Runtime().info();
    py::dict components;
    components[ "xpt" ] = info.has_xpt;
    components[ "tmr" ] = info.has_tmr;
    components[ "oui" ] = info.has_oui;
    components[ "gef" ] = info.has_gef;
    components[ "iui" ] = info.has_iui;
    components[ "vpj" ] = info.has_vpj;

    py::dict result;
    result[ "version" ] = info.version;
    result[ "binding_abi_version" ] = info.binding_abi_version;
    result[ "components" ] = std::move( components );
    return result;
}

wse::binding::FrameBuffer copyFrame(
      const py::buffer& buffer_in
    , const wse::binding::CancellationToken& cancellation_in
)
{
    if ( cancellation_in.isCancellationRequested() )
    {
        throwError( invalidStateError() );
    }
    const std::vector<std::uint8_t> bytes = copyPythonBuffer( buffer_in );
    auto result = [&]()
    {
        py::gil_scoped_release release;
        return wse::binding::Runtime().copyFrame( bytes );
    }();
    if ( !result.succeeded() )
    {
        throwError( result.error() );
    }
    return std::move( result.value() );
}

void wait(
      const std::int64_t duration_ms_in
    , const wse::binding::CancellationToken& cancellation_in
)
{
    if ( duration_ms_in < 0 || duration_ms_in > 86400000 )
    {
        throwError( wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument
            , 1
            , "wait duration must be between 0 and 86400000 milliseconds."
        ) );
    }

    const auto status = [&]()
    {
        py::gil_scoped_release release;
        return wse::binding::Runtime().wait(
            std::chrono::milliseconds( duration_ms_in ), cancellation_in );
    }();
    if ( !status.succeeded() )
    {
        throwError( status.error() );
    }
}

// ----------------------------------------------------------------------------------------------
// Python-facing wrapper classes
//
// Each wrapper owns its native object outright and exposes only picklable/buffer-friendly Python
// values. Blocking facade calls release the GIL for their duration; a callback re-acquires it
// before touching any py::object. Errors surface as WseError exceptions, never as C++ throws.
// ----------------------------------------------------------------------------------------------

struct PythonRuntimeState final
{
    std::mutex mutex;
    bool closed;
    wse::binding::CancellationSource cancellation;

    //! @brief Construct all members with explicit defaults.
    PythonRuntimeState()
        : mutex        ()
        , closed       ( false )
        , cancellation ()
    {
    }
};

class PythonRuntime final
{
  private:
    std::shared_ptr<PythonRuntimeState> m_state;

    wse::binding::CancellationToken operationToken() const
    {
        std::lock_guard<std::mutex> lock( this->m_state->mutex );
        if ( this->m_state->closed )
        {
            throwError( invalidStateError() );
        }
        return this->m_state->cancellation.token();
    }

  public:
    PythonRuntime()
        : m_state ( std::make_shared<PythonRuntimeState>() )
    {
    }

    ~PythonRuntime()
    {
        this->close();
    }

    PythonRuntime( const PythonRuntime& ) = delete;
    PythonRuntime& operator=( const PythonRuntime& ) = delete;

    py::dict info() const
    {
        this->operationToken();
        return runtimeInfo();
    }

    wse::binding::FrameBuffer copyFrame( const py::buffer& buffer_in ) const
    {
        return ::copyFrame( buffer_in, this->operationToken() );
    }

    void wait( const std::int64_t duration_ms_in ) const
    {
        ::wait( duration_ms_in, this->operationToken() );
    }

    void close() noexcept
    {
        std::lock_guard<std::mutex> lock( this->m_state->mutex );
        if ( !this->m_state->closed )
        {
            this->m_state->closed = true;
            this->m_state->cancellation.cancel();
        }
    }

    bool closed() const noexcept
    {
        std::lock_guard<std::mutex> lock( this->m_state->mutex );
        return this->m_state->closed;
    }
};

py::bytes frameBytes( const wse::binding::FrameBuffer& frame_in )
{
    const auto& bytes = frame_in.bytes();
    return py::bytes(
          bytes.empty() ? "" : reinterpret_cast<const char*>( bytes.data() )
        , bytes.size()
    );
}

#ifdef WSE_HAS_TMR

wse::binding::Error cameraError( const wse::tmr::CameraError& error_in )
{
    return wse::binding::fromTmrError( error_in );
}

[[noreturn]] void throwCameraError( const wse::tmr::CameraError& error_in )
{
    throwError( cameraError( error_in ) );
}

py::object makePythonError( const wse::binding::Error& error_in )
{
    py::object instance = py::reinterpret_steal<py::object>(
        PyObject_CallFunction( g_wse_error_type, "s", error_in.message().c_str() ) );
    if ( !instance )
    {
        throw py::error_already_set();
    }
    instance.attr( "category" ) = static_cast<std::uint8_t>( error_in.category() );
    instance.attr( "code" ) = error_in.code();
    instance.attr( "native_code" ) = error_in.nativeCode();
    return instance;
}

struct PythonCameraFrame final
{
    std::uint32_t width;
    std::uint32_t height;
    wse::tmr::eCameraPixelFormat pixel_format;
    std::size_t row_stride;
    std::uint64_t sequence;
    std::int64_t monotonic_timestamp_ns;
    wse::binding::FrameBuffer data;

    //! @brief Construct all members with explicit defaults.
    PythonCameraFrame(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
        , const wse::tmr::eCameraPixelFormat& pixel_format_in = wse::tmr::eCameraPixelFormat::Unknown
        , std::size_t row_stride_in = 0U
        , std::uint64_t sequence_in = 0U
        , std::int64_t monotonic_timestamp_ns_in = 0
        , const wse::binding::FrameBuffer& data_in = {}
    )
        : width                  ( width_in )
        , height                 ( height_in )
        , pixel_format           ( pixel_format_in )
        , row_stride             ( row_stride_in )
        , sequence               ( sequence_in )
        , monotonic_timestamp_ns ( monotonic_timestamp_ns_in )
        , data                   ( data_in )
    {
    }
};

//! \~japanese Python側のFrameをCameraのFrameへ戻す. 演算に渡すために必要である.
//! \~english  Turns a Python frame back into a camera frame, which is what an operation takes.
wse::tmr::sCameraFrame cameraFrame( const PythonCameraFrame& frame_in )
{
    wse::tmr::sCameraFrame frame;
    frame.description.width        = frame_in.width;
    frame.description.height       = frame_in.height;
    frame.description.pixel_format = frame_in.pixel_format;
    frame.description.row_stride   = frame_in.row_stride;
    frame.data                     = frame_in.data.bytes();
    frame.sequence                 = frame_in.sequence;
    frame.monotonic_timestamp_ns   = frame_in.monotonic_timestamp_ns;
    return frame;
}

PythonCameraFrame pythonCameraFrame( wse::tmr::sCameraFrame frame_in )
{
    PythonCameraFrame result;
    result.width = frame_in.description.width;
    result.height = frame_in.description.height;
    result.pixel_format = frame_in.description.pixel_format;
    result.row_stride = frame_in.description.row_stride;
    result.sequence = frame_in.sequence;
    result.monotonic_timestamp_ns = frame_in.monotonic_timestamp_ns;
    result.data = wse::binding::FrameBuffer( std::move( frame_in.data ) );
    return result;
}

//! \~japanese 同一形状のFrameを加算し平均を取り出す累積器.
//! \~english  Accumulates frames of one shape and produces their average.
class PythonFrameAccumulator final
{
  private:
    wse::tmr::CameraFrameAccumulator m_accumulator;

  public:
    void add( const PythonCameraFrame& frame_in )
    {
        const auto status = this->m_accumulator.add( cameraFrame( frame_in ) );
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    std::size_t count() const noexcept { return this->m_accumulator.count(); }

    PythonCameraFrame average() const
    {
        auto result = this->m_accumulator.average();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return pythonCameraFrame( std::move( result.value() ) );
    }

    void reset() noexcept { this->m_accumulator.reset(); }
};

PythonCameraFrame applyOrientation(
      const PythonCameraFrame&     frame_in
    , const wse::eImageOrientation orientation_in )
{
    auto result = wse::tmr::applyOrientation( cameraFrame( frame_in ), orientation_in );
    if ( !result.succeeded() ) throwCameraError( result.error() );
    return pythonCameraFrame( std::move( result.value() ) );
}

PythonCameraFrame demosaicFrame(
      const PythonCameraFrame&                frame_in
    , const wse::tmr::eCameraPixelFormat   output_format_in
    , const wse::eDemosaicMethod              method_in )
{
    auto result = wse::tmr::demosaicFrame( cameraFrame( frame_in ), output_format_in, method_in );
    if ( !result.succeeded() ) throwCameraError( result.error() );
    return pythonCameraFrame( std::move( result.value() ) );
}

wse::eBayerPattern bayerPatternOf( const wse::tmr::eCameraPixelFormat format_in )
{
    wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
    if ( !wse::tmr::bayerPatternOf( &pattern, format_in ) )
    {
        throwError( wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument
            , 1
            , "This pixel format carries no Bayer layout." ) );
    }
    return pattern;
}

class PythonWebCamera final
{
  private:
    std::unique_ptr<wse::tmr::WebCamera> m_camera;
    py::object m_callback;

    //! \~japanese Native Objectの有無だけを見る. Sessionの開閉はNative側が保持する.
    //! \~english  Asks only whether the native object is still here; the session state lives in it.
    void ensureActive() const
    {
        if ( !this->m_camera ) throwError( wse::binding::Error(
            wse::binding::eErrorCategory::InvalidState, 1,
            "WebCamera has been released." ) );
    }

    //! \~japanese GILを保持しているかにかかわらずNative Sessionを終える.
    //! \~english  Ends the native session whether or not the GIL is held.
    void closeSession() noexcept
    {
        if ( !this->m_camera ) return;
        if ( PyGILState_Check() )
        {
            py::gil_scoped_release release;
            this->m_camera->stop();
            this->m_camera->close();
        }
        else
        {
            this->m_camera->stop();
            this->m_camera->close();
        }
    }

  public:
    PythonWebCamera()
        : m_camera   ( std::make_unique<wse::tmr::WebCamera>() )
        , m_callback ( py::none() )
    {
    }

    ~PythonWebCamera()
    {
        this->release();
    }

    PythonWebCamera( const PythonWebCamera& ) = delete;
    PythonWebCamera& operator=( const PythonWebCamera& ) = delete;

    static std::vector<wse::tmr::sCameraDeviceInfo> enumerate(
        const wse::tmr::eCameraBackend backend_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return wse::tmr::WebCamera::enumerate( backend_in );
        }();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    static wse::tmr::sCameraCapability capabilities(
        const wse::tmr::sCameraDeviceInfo& device_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return wse::tmr::WebCamera::capabilities( device_in );
        }();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    void open( const wse::tmr::sCameraDeviceInfo& device_in )
    {
        this->ensureActive();
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->open( device_in );
        }();
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    void open( const wse::tmr::sCameraDeviceInfo& device_in,
        const wse::tmr::sCameraStreamConfiguration& configuration_in )
    {
        this->ensureActive();
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->open( device_in, configuration_in );
        }();
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    void start()
    {
        this->ensureActive();
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->start();
        }();
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    void start( py::object callback_in )
    {
        this->ensureActive();
        if ( callback_in.is_none() ) { this->start(); return; }
        if ( !PyCallable_Check( callback_in.ptr() ) )
        {
            throwError( wse::binding::Error(
                wse::binding::eErrorCategory::InvalidArgument, 3,
                "WebCamera callback must be callable." ) );
        }
        this->m_callback = std::move( callback_in );
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->start(
                [this]( const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& result_in )
            {
                py::gil_scoped_acquire acquire;
                try
                {
                    if ( result_in.succeeded() )
                        this->m_callback( py::cast( pythonCameraFrame( result_in.value() ) ) );
                    else
                        this->m_callback( makePythonError( cameraError( result_in.error() ) ) );
                }
                catch ( py::error_already_set& error )
                {
                    error.discard_as_unraisable( "WSE WebCamera callback" );
                }
            } );
        }();
        if ( !status.succeeded() )
        {
            this->m_callback = py::none();
            throwCameraError( status.error() );
        }
    }

    void stop()
    {
        this->ensureActive();
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->stop();
        }();
        this->m_callback = py::none();
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    PythonCameraFrame readFrame( const std::uint32_t timeout_ms_in )
    {
        this->ensureActive();
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_camera->readFrame( timeout_ms_in );
        }();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return pythonCameraFrame( std::move( result.value() ) );
    }

    //! \~japanese 指定枚数を読み出し、その平均Frameを返す.
    //! \~english  Reads a requested number of frames and returns their average.
    PythonCameraFrame readAveragedFrame(
          const std::size_t   count_in
        , const std::uint32_t timeout_ms_in )
    {
        this->ensureActive();
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return wse::tmr::readAveragedFrame( this->m_camera.get(), count_in, timeout_ms_in );
        }();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return pythonCameraFrame( std::move( result.value() ) );
    }

    wse::tmr::sCameraCapability currentCapabilities() const
    {
        this->ensureActive();
        const auto result = this->m_camera->currentCapabilities();
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    wse::tmr::sCameraControlCapability controlCapability(
        const wse::tmr::eCameraControl control_in ) const
    {
        this->ensureActive();
        const auto result = this->m_camera->controlCapability( control_in );
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    wse::tmr::sCameraControlValue getControl(
        const wse::tmr::eCameraControl control_in )
    {
        this->ensureActive();
        const auto result = this->m_camera->getControl( control_in );
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    void setControl( const wse::tmr::sCameraControlValue& value_in )
    {
        this->ensureActive();
        const auto status = this->m_camera->setControl( value_in );
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    wse::tmr::sCameraExtensionUnitValue getExtensionUnit(
        const wse::tmr::sCameraExtensionUnitSelector& selector_in )
    {
        this->ensureActive();
        const auto result = this->m_camera->getExtensionUnit( selector_in );
        if ( !result.succeeded() ) throwCameraError( result.error() );
        return result.value();
    }

    void setExtensionUnit( const wse::tmr::sCameraExtensionUnitValue& value_in )
    {
        this->ensureActive();
        const auto status = this->m_camera->setExtensionUnit( value_in );
        if ( !status.succeeded() ) throwCameraError( status.error() );
    }

    //! \~japanese Sessionを閉じるだけである. 同じObjectをもう一度`open()`できる.
    //! \~english  Ends the session only; the same object can be opened again afterwards.
    void close() noexcept
    {
        this->closeSession();
        if ( Py_IsInitialized() && !this->m_callback.is_none() ) m_callback = py::none();
    }

    //! \~japanese Sessionを閉じNative Objectを手放す. 以降の操作はInvalidStateとなる.
    //! \~english  Ends the session and drops the native object; every later call is InvalidState.
    void release() noexcept
    {
        this->close();
        this->m_camera.reset();
    }

    bool isOpen() const noexcept
    {
        return static_cast<bool>( this->m_camera ) && this->m_camera->isOpen();
    }

    bool isStreaming() const noexcept
    {
        return static_cast<bool>( this->m_camera ) && this->m_camera->isStreaming();
    }

    bool isReleased() const noexcept { return !this->m_camera; }
};

#endif // WSE_HAS_TMR

#ifdef WSE_HAS_IUI

//! \~japanese Python所有のKeyboard. 監視Threadの寿命を`close()`で終端させる.
//! \~english  Python-owned keyboard whose monitoring thread ends deterministically at `close()`.
class PythonKeyboard final
{
  private:
    std::unique_ptr<wse::iui::Keyboard> m_keyboard;
    bool m_closed;

    void ensureActive() const
    {
        if ( this->m_closed ) throwError( wse::binding::Error(
            wse::binding::eErrorCategory::InvalidState, 1,
            "Keyboard is closed." ) );
    }

    void ensureReadable() const
    {
        this->ensureActive();
        const wse::binding::Error readiness =
            wse::binding::fromIuiKeyboardState( this->m_keyboard->accessState() );
        if ( !readiness.ok() ) throwError( readiness );
    }

    template <std::size_t Count>
    static py::list toList( const std::array<bool, Count>& source_in )
    {
        py::list result;
        for ( std::size_t index = 0U; index < Count; ++index )
        {
            result.append( py::bool_( source_in[ index ] ) );
        }
        return result;
    }

  public:
    PythonKeyboard()
        : m_keyboard ( std::make_unique<wse::iui::Keyboard>() )
        , m_closed   ( false )
    {
    }

    PythonKeyboard( const PythonKeyboard& ) = delete;
    PythonKeyboard& operator=( const PythonKeyboard& ) = delete;

    ~PythonKeyboard() { this->close(); }

    wse::iui::KeyboardAccessState accessState() const
    {
        this->ensureActive();
        return this->m_keyboard->accessState();
    }

    bool isAvailable() const noexcept
    {
        return !this->m_closed && this->m_keyboard->isAvailable();
    }

    py::dict snapshot() const
    {
        this->ensureReadable();
        const wse::iui::KeyboardState state = this->m_keyboard->snapshot();
        py::dict result;
        result[ "ascii" ] = toList( state.ascii );
        result[ "function" ] = toList( state.function );
        result[ "arrow" ] = toList( state.arrow );
        result[ "lock" ] = toList( state.lock );
        result[ "command" ] = toList( state.command );
        return result;
    }

    int pressedAscii() const
    {
        this->ensureReadable();
        return static_cast<int>( this->m_keyboard->getASCII() );
    }

    void close() noexcept
    {
        if ( this->m_closed ) return;
        this->m_closed = true;
        this->m_keyboard.reset();
    }

    bool isClosed() const noexcept { return this->m_closed; }
};

#endif // WSE_HAS_IUI

#ifdef WSE_HAS_XPT

[[noreturn]] void throwTransportError( const wse::xpt::TransportError& error_in )
{
    throwError( wse::binding::fromXptError( error_in ) );
}

// Called after the operation has reacquired the GIL. Python owns every published value.
[[noreturn]] void throwTransferError( const wse::binding::detail::TransferFailureView& failure_in )
{
    const auto error = wse::binding::fromXptError( failure_in.error );
    py::object instance = py::reinterpret_steal<py::object>(
        PyObject_CallFunction( g_wse_transfer_error_type, "s", error.message().c_str() ) );
    if( !instance ) throw py::error_already_set();
    instance.attr( "category" ) = static_cast<std::uint8_t>( error.category() );
    instance.attr( "code" ) = error.code();
    instance.attr( "native_code" ) = error.nativeCode();
    instance.attr( "bytes_transferred" ) = failure_in.bytes_transferred;
    instance.attr( "received_data" ) = py::none();
    instance.attr( "source_endpoint" ) = py::none();
    if( failure_in.datagram )
    {
        const auto& packet = *failure_in.datagram;
        instance.attr( "received_data" ) = py::bytes(
            reinterpret_cast<const char*>( packet.payload().data() ), packet.payload().size() );
        instance.attr( "source_endpoint" ) = py::make_tuple( packet.source().host(), packet.source().port() );
    }
    PyErr_SetObject( g_wse_transfer_error_type, instance.ptr() );
    throw py::error_already_set();
}

//! \~japanese Statusだけが失敗であるHTTP応答. Responseを伴って送出される.
//! \~english  An HTTP answer whose status alone is the failure; it is raised together with the response.
class WseHttpStatusException final : public std::runtime_error
{
  private:
    wse::binding::Error    m_error;
    wse::xpt::HttpResponse m_response;

  public:
    WseHttpStatusException(
          const wse::binding::Error& error_in
        , wse::xpt::HttpResponse     response_in )
        : std::runtime_error ( error_in.message() )
        , m_error            ( error_in )
        , m_response         ( std::move( response_in ) )
    {
    }

    const wse::binding::Error& error() const noexcept
    {
        return this->m_error;
    }

    const wse::xpt::HttpResponse& response() const noexcept
    {
        return this->m_response;
    }
};

//! \~japanese XPT Operationが観測する協調的Cancellationの所有者.
//! \~english  Owner of the cooperative cancellation observed by XPT operations.
class PythonCancellationSource final
{
  private:
    wse::xpt::CancellationSource m_source;

  public:
    PythonCancellationSource() = default;
    PythonCancellationSource( const PythonCancellationSource& ) = delete;
    PythonCancellationSource& operator=( const PythonCancellationSource& ) = delete;

    void cancel() noexcept { this->m_source.cancel(); }
    bool isCancellationRequested() const noexcept
    {
        return this->m_source.isCancellationRequested();
    }
    wse::xpt::CancellationToken token() const noexcept { return this->m_source.token(); }
};

//! \~japanese 全XPT Operationが要求する明示制御. Default Timeoutは持たない.
//! \~english  Explicit controls required by every XPT operation; there is no default timeout.
class PythonOperationContext final
{
  private:
    std::int64_t m_timeout_ms;
    PythonCancellationSource* m_cancellation;

  public:
    PythonOperationContext( const std::int64_t timeout_ms_in, PythonCancellationSource* cancellation_in )
        : m_timeout_ms   ( timeout_ms_in )
        , m_cancellation ( cancellation_in )
    {
        if ( timeout_ms_in < 0 )
        {
            throwError( wse::binding::Error(
                wse::binding::eErrorCategory::InvalidArgument, 1,
                "The operation timeout must not be negative." ) );
        }
    }

    std::int64_t timeoutMilliseconds() const noexcept { return this->m_timeout_ms; }

    wse::xpt::OperationContext native() const
    {
        const wse::xpt::Timeout timeout = wse::xpt::Timeout::milliseconds( this->m_timeout_ms );
        if ( this->m_cancellation == nullptr )
        {
            return wse::xpt::OperationContext( timeout );
        }
        return wse::xpt::OperationContext( timeout, this->m_cancellation->token() );
    }
};

py::bytes toPythonBytes( const std::vector<std::uint8_t>& bytes_in )
{
    return py::bytes(
          bytes_in.empty() ? "" : reinterpret_cast<const char*>( bytes_in.data() )
        , bytes_in.size() );
}

std::vector<std::uint8_t> fromPythonBytes( const py::bytes& bytes_in )
{
    const std::string text = static_cast<std::string>( bytes_in );
    return std::vector<std::uint8_t>( text.begin(), text.end() );
}

//! \~japanese Python所有のTCP接続. 呼出元Thread前提である.
//! \~english  Python-owned TCP connection; caller-confined.
class PythonTcpClient final
{
  private:
    wse::xpt::TcpClient m_client;

  public:
    PythonTcpClient() = default;
    PythonTcpClient( const PythonTcpClient& ) = delete;
    PythonTcpClient& operator=( const PythonTcpClient& ) = delete;

    void connect( const std::string& host_in, const std::uint16_t port_in,
        const PythonOperationContext& context_in )
    {
        const auto status = [&]()
        {
            // The call blocks until its deadline, so every other Python thread has to keep running.
            py::gil_scoped_release release;
            return this->m_client.connect(
                wse::xpt::Endpoint( host_in, port_in ), context_in.native() );
        }();
        if ( !status.succeeded() ) throwTransportError( status.error() );
    }

    void disconnect() noexcept { this->m_client.disconnect(); }
    bool isConnected() const noexcept { return this->m_client.isConnected(); }

    void checkPeerConnection()
    {
        const auto status = [&]()
        {
            py::gil_scoped_release release;
            return this->m_client.checkPeerConnection();
        }();
        if ( !status.succeeded() ) throwTransportError( status.error() );
    }

    py::tuple remoteEndpoint() const
    {
        const wse::xpt::Endpoint endpoint = this->m_client.getRemoteEndpoint();
        return py::make_tuple( endpoint.host(), endpoint.port() );
    }

    py::tuple localEndpoint() const
    {
        const wse::xpt::Endpoint endpoint = this->m_client.getLocalEndpoint();
        return py::make_tuple( endpoint.host(), endpoint.port() );
    }

    std::size_t send( const py::bytes& data_in, const PythonOperationContext& context_in )
    {
        const std::vector<std::uint8_t> bytes = fromPythonBytes( data_in );
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_client.send( bytes, context_in.native() );
        }();
        if ( !result.succeeded() ) throwTransferError( wse::binding::detail::transferFailure( result ) );
        return result.value();
    }

    py::bytes receive( const std::size_t maximum_size_in, const PythonOperationContext& context_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_client.receive( maximum_size_in, context_in.native() );
        }();
        if ( !result.succeeded() ) throwTransportError( result.error() );
        return toPythonBytes( result.value() );
    }
};

//! \~japanese Python所有のUDP Socket.
//! \~english  Python-owned UDP socket.
class PythonUdpClient final
{
  private:
    wse::xpt::UdpClient m_client;

  public:
    PythonUdpClient() = default;
    PythonUdpClient( const PythonUdpClient& ) = delete;
    PythonUdpClient& operator=( const PythonUdpClient& ) = delete;

    static std::size_t maximumDatagramSize() noexcept
    {
        return wse::xpt::UdpClient::maximumDatagramSize();
    }

    void bind( const std::string& host_in, const std::uint16_t port_in,
        const PythonOperationContext& context_in )
    {
        const auto status = [&]()
        {
            // The call blocks until its deadline, so every other Python thread has to keep running.
            py::gil_scoped_release release;
            return this->m_client.bind(
                wse::xpt::Endpoint( host_in, port_in ), context_in.native() );
        }();
        if ( !status.succeeded() ) throwTransportError( status.error() );
    }

    void close() noexcept { this->m_client.close(); }
    bool isOpen() const noexcept { return this->m_client.isOpen(); }

    py::tuple localEndpoint() const
    {
        const wse::xpt::Endpoint endpoint = this->m_client.getLocalEndpoint();
        return py::make_tuple( endpoint.host(), endpoint.port() );
    }

    std::size_t sendTo( const std::string& host_in, const std::uint16_t port_in,
        const py::bytes& data_in, const PythonOperationContext& context_in )
    {
        const std::vector<std::uint8_t> bytes = fromPythonBytes( data_in );
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_client.sendTo(
                wse::xpt::Endpoint( host_in, port_in ), bytes, context_in.native() );
        }();
        if ( !result.succeeded() ) throwTransferError( wse::binding::detail::transferFailure( result ) );
        return result.value();
    }

    //! @return \~japanese `(host, port, payload)`の3要素Tuple. \~english A `(host, port, payload)` tuple.
    py::tuple receiveFrom( const std::size_t maximum_size_in,
        const PythonOperationContext& context_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_client.receiveFrom( maximum_size_in, context_in.native() );
        }();
        if ( !result.succeeded() )
        {
            const auto failure = wse::binding::detail::transferFailure( result );
            if( failure.datagram ) throwTransferError( failure );
            throwTransportError( result.error() );
        }
        const wse::xpt::UdpDatagram& datagram = result.value();
        return py::make_tuple(
              datagram.source().host()
            , datagram.source().port()
            , toPythonBytes( datagram.payload() ) );
    }
};

//! \~japanese Python所有のSerial Port. Portable な新APIのみを公開する.
//! \~english  Python-owned serial port exposing only the portable new API.
class PythonSerialPort final
{
  private:
    wse::xpt::SerialPort m_port;

  public:
    PythonSerialPort() = default;
    PythonSerialPort( const PythonSerialPort& ) = delete;
    PythonSerialPort& operator=( const PythonSerialPort& ) = delete;

    void open( const std::string& device_name_in, const std::int32_t baud_rate_in,
        const PythonOperationContext& context_in )
    {
        const auto status = [&]()
        {
            // The call blocks until its deadline, so every other Python thread has to keep running.
            py::gil_scoped_release release;
            return this->m_port.open( device_name_in, baud_rate_in, context_in.native() );
        }();
        if ( !status.succeeded() ) throwTransportError( status.error() );
    }

    void close() noexcept { this->m_port.close(); }
    bool isOpen() const noexcept { return this->m_port.isOpen(); }
    std::string deviceName() const { return this->m_port.getDeviceName(); }
    std::int32_t baudRate() const noexcept { return this->m_port.getBaudRate(); }

    std::size_t send( const py::bytes& data_in, const PythonOperationContext& context_in )
    {
        const std::vector<std::uint8_t> bytes = fromPythonBytes( data_in );
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_port.send( bytes, context_in.native() );
        }();
        if ( !result.succeeded() ) throwTransferError( wse::binding::detail::transferFailure( result ) );
        return result.value();
    }

    py::bytes receive( const std::size_t maximum_size_in, const PythonOperationContext& context_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return this->m_port.receive( maximum_size_in, context_in.native() );
        }();
        if ( !result.succeeded() ) throwTransportError( result.error() );
        return toPythonBytes( result.value() );
    }
};

//! \~japanese 組立中のHTTP Request.
//! \~english  An HTTP request under construction.
class PythonHttpRequest final
{
  private:
    wse::xpt::HttpRequest m_request;

  public:
    PythonHttpRequest( const wse::xpt::eHttpMethod method_in, const std::string& url_in )
        : m_request ( method_in, url_in )
    {
    }

    void addHeader( const std::string& name_in, const std::string& value_in )
    {
        this->m_request.addHeader( name_in, value_in );
    }

    void setBody( const py::bytes& body_in )
    {
        this->m_request.setBody( fromPythonBytes( body_in ) );
    }

    const wse::xpt::HttpRequest& native() const noexcept { return this->m_request; }
};

//! \~japanese 受信済みHTTP Response.
//! \~english  A received HTTP response.
class PythonHttpResponse final
{
  private:
    wse::xpt::HttpResponse m_response;

  public:
    explicit PythonHttpResponse( wse::xpt::HttpResponse response_in )
        : m_response ( std::move( response_in ) )
    {
    }

    std::uint16_t statusCode() const noexcept { return this->m_response.status_code(); }
    std::uint32_t attemptCount() const noexcept { return this->m_response.attempt_count(); }

    py::list headers() const
    {
        py::list result;
        for ( const auto& header : this->m_response.headers() )
        {
            result.append( py::make_tuple( header.name, header.value ) );
        }
        return result;
    }

    py::bytes body() const { return toPythonBytes( this->m_response.body() ); }
};

//! \~japanese HTTP Requestを実行する. Bindingは呼出元に代わってRetryしない.
//! \~english  Executes an HTTP request; the binding never retries on the caller's behalf.
PythonHttpResponse executeHttpRequest(
      const PythonHttpRequest& request_in
    , const std::size_t maximum_response_body_size_in
    , const PythonOperationContext& context_in
    , const std::string* username_in
    , const std::string* secret_in )
{
    const wse::xpt::HttpExecutionOptions options(
          maximum_response_body_size_in
        , wse::xpt::RetryPolicy()
        , wse::xpt::eRetryOperationSafety::NonIdempotent
        , false );
    const wse::xpt::HttpClient client;

    auto result = [&]()
    {
        // The exchange blocks until its deadline, so every other Python thread has to keep running.
        py::gil_scoped_release release;
        return ( username_in == nullptr )
            ? client.execute( request_in.native(), options, context_in.native() )
            : client.executeAuthenticated(
                  request_in.native()
                , options
                , wse::xpt::HttpAuthentication(
                      wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated
                    , *username_in
                    , ( secret_in == nullptr ) ? std::string() : *secret_in )
                , context_in.native() );
    }();
    if ( !result.succeeded() )
    {
        // A 4xx or 5xx is a complete answer whose status is the failure, so the answer travels with
        // the raised error instead of being discarded. Every other failure keeps the plain error.
        if ( result.error().code() == wse::xpt::eTransportErrorCode::HttpStatusError )
        {
            throw WseHttpStatusException(
                wse::binding::fromXptError( result.error() ), std::move( result.value() ) );
        }
        throwTransportError( result.error() );
    }
    return PythonHttpResponse( std::move( result.value() ) );
}

#endif // WSE_HAS_XPT

#ifdef WSE_EXTENSION_PYTHON_BINDING
#include "wse_extension_python_types.inc"
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_python_types.inc"
#endif

} // namespace

PYBIND11_MODULE( _wse, module )
{
    module.doc() = "WonderStewEngine Python binding";

    py::enum_<wse::binding::eErrorCategory>( module, "ErrorCategory" )
        .value( "NONE", wse::binding::eErrorCategory::None )
        .value( "INVALID_ARGUMENT", wse::binding::eErrorCategory::InvalidArgument )
        .value( "NOT_FOUND", wse::binding::eErrorCategory::NotFound )
        .value( "INVALID_STATE", wse::binding::eErrorCategory::InvalidState )
        .value( "INPUT_OUTPUT", wse::binding::eErrorCategory::InputOutput )
        .value( "TIMEOUT", wse::binding::eErrorCategory::Timeout )
        .value( "CANCELLATION", wse::binding::eErrorCategory::Cancellation )
        .value( "PROTOCOL", wse::binding::eErrorCategory::Protocol )
        .value( "SECURITY", wse::binding::eErrorCategory::Security )
        .value( "UNSUPPORTED", wse::binding::eErrorCategory::Unsupported )
        .value( "RESOURCE_EXHAUSTED", wse::binding::eErrorCategory::ResourceExhausted )
        .value( "INTERNAL", wse::binding::eErrorCategory::Internal )
        .export_values();

    py::exception<WseException> wse_error( module, "WseError", PyExc_RuntimeError );
    g_wse_error_type = wse_error.ptr();
#ifdef WSE_HAS_XPT
    // WseError stays the base, so an existing `except WseError:` keeps catching a status failure.
    py::exception<WseHttpStatusException> wse_http_status_error(
        module, "WseHttpStatusError", wse_error );
    g_wse_http_status_error_type = wse_http_status_error.ptr();
    const std::string transfer_error_name =
        py::str( module.attr( "__name__" ) ).cast<std::string>() + ".WseTransferError";
    py::object transfer_error = py::reinterpret_steal<py::object>(
        PyErr_NewException( transfer_error_name.c_str(), wse_error.ptr(), nullptr ) );
    if( !transfer_error ) throw py::error_already_set();
    g_wse_transfer_error_type = transfer_error.ptr();
    module.add_object( "WseTransferError", std::move( transfer_error ) );
#endif
    py::register_local_exception_translator( []( std::exception_ptr error_in )
    {
        try
        {
            if ( error_in )
            {
                std::rethrow_exception( error_in );
            }
        }
#ifdef WSE_HAS_XPT
        catch ( const WseHttpStatusException& exception )
        {
            const auto& error = exception.error();
            py::object instance = py::reinterpret_steal<py::object>(
                PyObject_CallFunction( g_wse_http_status_error_type, "s", exception.what() ) );
            if ( !instance )
            {
                return;
            }
            instance.attr( "category" ) = static_cast<std::uint8_t>( error.category() );
            instance.attr( "code" ) = error.code();
            instance.attr( "native_code" ) = error.nativeCode();
            instance.attr( "response" ) = py::cast( PythonHttpResponse( exception.response() ) );
            PyErr_SetObject( g_wse_http_status_error_type, instance.ptr() );
        }
#endif
        catch ( const WseException& exception )
        {
            const auto& error = exception.error();
            py::object instance = py::reinterpret_steal<py::object>(
                PyObject_CallFunction( g_wse_error_type, "s", exception.what() ) );
            if ( !instance )
            {
                return;
            }
            instance.attr( "category" ) = static_cast<std::uint8_t>( error.category() );
            instance.attr( "code" ) = error.code();
            instance.attr( "native_code" ) = error.nativeCode();
            PyErr_SetObject( g_wse_error_type, instance.ptr() );
        }
    } );

    py::class_<wse::binding::FrameBuffer>( module, "FrameBuffer", py::buffer_protocol() )
        .def_property_readonly( "size", &wse::binding::FrameBuffer::size )
        .def( "tobytes", &frameBytes )
        .def( "__bytes__", &frameBytes )
        .def( "__len__", &wse::binding::FrameBuffer::size )
        .def_buffer( []( wse::binding::FrameBuffer& frame_in )
        {
            const auto& bytes = frame_in.bytes();
            return py::buffer_info(
                  bytes.empty() ? nullptr : const_cast<std::uint8_t*>( bytes.data() )
                , sizeof( std::uint8_t )
                , py::format_descriptor<std::uint8_t>::format()
                , 1
                , { static_cast<py::ssize_t>( bytes.size() ) }
                , { static_cast<py::ssize_t>( sizeof( std::uint8_t ) ) }
                , true
            );
        } );

    py::class_<PythonRuntime>( module, "Runtime" )
        .def( py::init<>() )
        .def( "info", &PythonRuntime::info )
        .def( "copy_frame", &PythonRuntime::copyFrame, py::arg( "buffer" ) )
        .def( "wait", &PythonRuntime::wait, py::arg( "milliseconds" ) )
        .def( "close", &PythonRuntime::close )
        .def_property_readonly( "closed", &PythonRuntime::closed )
        .def( "__enter__", []( PythonRuntime& runtime_in ) -> PythonRuntime&
        {
            runtime_in.info();
            return runtime_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", [](
              PythonRuntime& runtime_in
            , const py::object&
            , const py::object&
            , const py::object& )
        {
            runtime_in.close();
            return false;
        } );

    module.def( "runtime_info", &runtimeInfo );
    module.def( "copy_frame", []( const py::buffer& buffer_in )
    {
        return copyFrame( buffer_in, wse::binding::CancellationToken() );
    }, py::arg( "buffer" ) );
    module.def( "wait", []( const std::int64_t duration_ms_in )
    {
        wait( duration_ms_in, wse::binding::CancellationToken() );
    }, py::arg( "milliseconds" ) );

#ifdef WSE_HAS_TMR
    py::enum_<wse::tmr::eCameraBackend>( module, "CameraBackend" )
        .value( "AUTOMATIC", wse::tmr::eCameraBackend::Automatic )
        .value( "MEDIA_FOUNDATION", wse::tmr::eCameraBackend::MediaFoundation )
        .value( "VIDEO4LINUX2", wse::tmr::eCameraBackend::Video4Linux2 )
        .value( "LIBCAMERA", wse::tmr::eCameraBackend::Libcamera );
    py::enum_<wse::tmr::eCameraPixelFormat>( module, "CameraPixelFormat" )
        .value( "UNKNOWN", wse::tmr::eCameraPixelFormat::Unknown )
        .value( "GRAY8", wse::tmr::eCameraPixelFormat::Gray8 )
        .value( "RGB8", wse::tmr::eCameraPixelFormat::Rgb8 )
        .value( "BGR8", wse::tmr::eCameraPixelFormat::Bgr8 )
        .value( "BGRA8", wse::tmr::eCameraPixelFormat::Bgra8 )
        .value( "YUYV422", wse::tmr::eCameraPixelFormat::Yuyv422 )
        .value( "NV12", wse::tmr::eCameraPixelFormat::Nv12 )
        .value( "MJPEG", wse::tmr::eCameraPixelFormat::Mjpeg )
        .value( "GRAY16", wse::tmr::eCameraPixelFormat::Gray16 )
        .value( "RGB16", wse::tmr::eCameraPixelFormat::Rgb16 )
        .value( "BGR16", wse::tmr::eCameraPixelFormat::Bgr16 )
        .value( "BAYER16_RGGB", wse::tmr::eCameraPixelFormat::Bayer16Rggb )
        .value( "BAYER16_BGGR", wse::tmr::eCameraPixelFormat::Bayer16Bggr )
        .value( "BAYER16_GRBG", wse::tmr::eCameraPixelFormat::Bayer16Grbg )
        .value( "BAYER16_GBRG", wse::tmr::eCameraPixelFormat::Bayer16Gbrg )
        .value( "UYVY422", wse::tmr::eCameraPixelFormat::Uyvy422 );
    py::enum_<wse::eImageOrientation>( module, "ImageOrientation" )
        .value( "NONE", wse::eImageOrientation::None )
        .value( "ROTATE_90_CW", wse::eImageOrientation::Rotate90CW )
        .value( "ROTATE_180", wse::eImageOrientation::Rotate180 )
        .value( "ROTATE_90_CCW", wse::eImageOrientation::Rotate90CCW )
        .value( "FLIP_HORIZONTAL", wse::eImageOrientation::FlipHorizontal )
        .value( "FLIP_VERTICAL", wse::eImageOrientation::FlipVertical );
    py::enum_<wse::eBayerPattern>( module, "BayerPattern" )
        .value( "RGGB", wse::eBayerPattern::Rggb )
        .value( "BGGR", wse::eBayerPattern::Bggr )
        .value( "GRBG", wse::eBayerPattern::Grbg )
        .value( "GBRG", wse::eBayerPattern::Gbrg );
    py::enum_<wse::eDemosaicMethod>( module, "DemosaicMethod" )
        .value( "BLOCK_2X2", wse::eDemosaicMethod::Block2x2 )
        .value( "BILINEAR", wse::eDemosaicMethod::Bilinear );
    py::enum_<wse::tmr::eCameraTransport>( module, "CameraTransport" )
        .value( "UNKNOWN", wse::tmr::eCameraTransport::Unknown )
        .value( "USB_UVC", wse::tmr::eCameraTransport::UsbUvc )
        .value( "CSI", wse::tmr::eCameraTransport::Csi )
        .value( "VIRTUAL", wse::tmr::eCameraTransport::Virtual )
        .value( "NETWORK", wse::tmr::eCameraTransport::Network );
    py::enum_<wse::tmr::eCameraControl>( module, "CameraControl" )
        .value( "EXPOSURE", wse::tmr::eCameraControl::Exposure )
        .value( "GAIN", wse::tmr::eCameraControl::Gain )
        .value( "FOCUS", wse::tmr::eCameraControl::Focus )
        .value( "BRIGHTNESS", wse::tmr::eCameraControl::Brightness )
        .value( "CONTRAST", wse::tmr::eCameraControl::Contrast )
        .value( "SATURATION", wse::tmr::eCameraControl::Saturation )
        .value( "WHITE_BALANCE", wse::tmr::eCameraControl::WhiteBalance )
        .value( "ZOOM", wse::tmr::eCameraControl::Zoom )
        .value( "IRIS", wse::tmr::eCameraControl::Iris )
        .value( "HUE", wse::tmr::eCameraControl::Hue )
        .value( "SHARPNESS", wse::tmr::eCameraControl::Sharpness )
        .value( "GAMMA", wse::tmr::eCameraControl::Gamma )
        .value( "COLOR_ENABLE", wse::tmr::eCameraControl::ColorEnable )
        .value( "BACKLIGHT_COMPENSATION", wse::tmr::eCameraControl::BacklightCompensation )
        .value( "PAN", wse::tmr::eCameraControl::Pan )
        .value( "TILT", wse::tmr::eCameraControl::Tilt )
        .value( "ROLL", wse::tmr::eCameraControl::Roll )
        .value( "POWER_LINE_FREQUENCY", wse::tmr::eCameraControl::PowerLineFrequency )
        .value( "FRAME_RATE", wse::tmr::eCameraControl::FrameRate );
    py::enum_<wse::tmr::eCameraControlMode>( module, "CameraControlMode" )
        .value( "MANUAL", wse::tmr::eCameraControlMode::Manual )
        .value( "AUTOMATIC", wse::tmr::eCameraControlMode::Automatic );
    py::enum_<wse::tmr::eCameraControlUnit>( module, "CameraControlUnit" )
        .value( "DEVICE_NATIVE", wse::tmr::eCameraControlUnit::DeviceNative )
        .value( "MICROSECONDS", wse::tmr::eCameraControlUnit::Microseconds )
        .value( "KELVIN", wse::tmr::eCameraControlUnit::Kelvin )
        .value( "DIOPTERS", wse::tmr::eCameraControlUnit::Diopters )
        .value( "GAIN_MULTIPLIER", wse::tmr::eCameraControlUnit::GainMultiplier )
        .value( "RELATIVE", wse::tmr::eCameraControlUnit::Relative )
        .value( "DEGREES", wse::tmr::eCameraControlUnit::Degrees )
        .value( "HERTZ", wse::tmr::eCameraControlUnit::Hertz )
        .value( "BOOLEAN", wse::tmr::eCameraControlUnit::Boolean );

    py::class_<wse::tmr::sCameraUsbIdentity>( module, "CameraUsbIdentity" )
        .def( py::init<>() )
        .def_readwrite( "vendor_id", &wse::tmr::sCameraUsbIdentity::vendor_id )
        .def_readwrite( "product_id", &wse::tmr::sCameraUsbIdentity::product_id )
        .def_readwrite( "serial_number", &wse::tmr::sCameraUsbIdentity::serial_number )
        .def_readwrite( "uvc_version_bcd", &wse::tmr::sCameraUsbIdentity::uvc_version_bcd )
        .def_property_readonly( "available", &wse::tmr::sCameraUsbIdentity::available );

    py::class_<wse::tmr::sCameraDeviceInfo>( module, "CameraDevice" )
        .def( py::init<>() )
        .def_readwrite( "backend", &wse::tmr::sCameraDeviceInfo::backend )
        .def_readwrite( "id", &wse::tmr::sCameraDeviceInfo::id )
        .def_readwrite( "display_name", &wse::tmr::sCameraDeviceInfo::display_name )
        .def_readwrite( "transport", &wse::tmr::sCameraDeviceInfo::transport )
        .def_readwrite( "transport_type", &wse::tmr::sCameraDeviceInfo::transport_type )
        .def_readwrite( "usb", &wse::tmr::sCameraDeviceInfo::usb )
        .def_property_readonly( "valid", &wse::tmr::sCameraDeviceInfo::valid );
    py::class_<wse::tmr::sCameraFormat>( module, "CameraFormat" )
        .def( py::init<>() )
        .def_readwrite( "width", &wse::tmr::sCameraFormat::width )
        .def_readwrite( "height", &wse::tmr::sCameraFormat::height )
        .def_readwrite( "frame_rate_numerator", &wse::tmr::sCameraFormat::frame_rate_numerator )
        .def_readwrite( "frame_rate_denominator", &wse::tmr::sCameraFormat::frame_rate_denominator )
        .def_readwrite( "pixel_format", &wse::tmr::sCameraFormat::pixel_format )
        .def_property_readonly( "frames_per_second", &wse::tmr::sCameraFormat::framesPerSecond )
        .def_property_readonly( "valid", &wse::tmr::sCameraFormat::valid );
    py::class_<wse::tmr::sCameraControlCapability>( module, "CameraControlCapability" )
        .def( py::init<>() )
        .def_readwrite( "control", &wse::tmr::sCameraControlCapability::control )
        .def_readwrite( "minimum", &wse::tmr::sCameraControlCapability::minimum )
        .def_readwrite( "maximum", &wse::tmr::sCameraControlCapability::maximum )
        .def_readwrite( "step", &wse::tmr::sCameraControlCapability::step )
        .def_readwrite( "default_value", &wse::tmr::sCameraControlCapability::default_value )
        .def_readwrite( "supports_manual", &wse::tmr::sCameraControlCapability::supports_manual )
        .def_readwrite( "supports_automatic", &wse::tmr::sCameraControlCapability::supports_automatic )
        .def_readwrite( "unit", &wse::tmr::sCameraControlCapability::unit )
        .def_readwrite( "physical_scale", &wse::tmr::sCameraControlCapability::physical_scale )
        .def_readwrite( "readable", &wse::tmr::sCameraControlCapability::readable )
        .def_readwrite( "writable", &wse::tmr::sCameraControlCapability::writable )
        .def_readwrite( "display_name", &wse::tmr::sCameraControlCapability::display_name )
        .def( "value_from_normalized", &wse::tmr::sCameraControlCapability::valueFromNormalized )
        .def( "normalized_from_value", &wse::tmr::sCameraControlCapability::normalizedFromValue )
        .def( "physical_from_value", &wse::tmr::sCameraControlCapability::physicalFromValue )
        .def( "value_from_physical", &wse::tmr::sCameraControlCapability::valueFromPhysical );
    py::class_<wse::tmr::sCameraControlValue>( module, "CameraControlValue" )
        .def( py::init<>() )
        .def_readwrite( "control", &wse::tmr::sCameraControlValue::control )
        .def_readwrite( "mode", &wse::tmr::sCameraControlValue::mode )
        .def_readwrite( "value", &wse::tmr::sCameraControlValue::value );
    py::class_<wse::tmr::sCameraStreamProfile>( module, "CameraStreamProfile" )
        .def( py::init<>() )
        .def_readwrite( "native_format", &wse::tmr::sCameraStreamProfile::native_format )
        .def_readwrite( "output_formats", &wse::tmr::sCameraStreamProfile::output_formats )
        .def_property_readonly( "valid", &wse::tmr::sCameraStreamProfile::valid )
        .def( "supports_output", &wse::tmr::sCameraStreamProfile::supportsOutput );
    py::class_<wse::tmr::sCameraStreamConfiguration>( module, "CameraStreamConfiguration" )
        .def( py::init<>() )
        .def_readwrite( "native_format", &wse::tmr::sCameraStreamConfiguration::native_format )
        .def_readwrite( "output_format", &wse::tmr::sCameraStreamConfiguration::output_format )
        .def_readwrite( "allow_conversion", &wse::tmr::sCameraStreamConfiguration::allow_conversion )
        .def_property_readonly( "valid", &wse::tmr::sCameraStreamConfiguration::valid );
    py::class_<wse::tmr::sCameraExtensionUnitSelector>( module, "CameraExtensionUnitSelector" )
        .def( py::init<>() )
        .def_property( "unit_guid", []( const wse::tmr::sCameraExtensionUnitSelector& value_in )
        {
            return py::bytes( reinterpret_cast<const char*>( value_in.unit_guid.data() ),
                value_in.unit_guid.size() );
        }, []( wse::tmr::sCameraExtensionUnitSelector& value_in, const py::bytes& bytes_in )
        {
            const std::string bytes = bytes_in;
            if ( bytes.size() != value_in.unit_guid.size() )
                throw py::value_error( "unit_guid must contain exactly 16 bytes." );
            std::memcpy( value_in.unit_guid.data(), bytes.data(), bytes.size() );
        } )
        .def_readwrite( "unit_id", &wse::tmr::sCameraExtensionUnitSelector::unit_id )
        .def_readwrite( "selector", &wse::tmr::sCameraExtensionUnitSelector::selector )
        .def_readwrite( "minimum_size", &wse::tmr::sCameraExtensionUnitSelector::minimum_size )
        .def_readwrite( "maximum_size", &wse::tmr::sCameraExtensionUnitSelector::maximum_size )
        .def_readwrite( "readable", &wse::tmr::sCameraExtensionUnitSelector::readable )
        .def_readwrite( "writable", &wse::tmr::sCameraExtensionUnitSelector::writable )
        .def_readwrite( "display_name", &wse::tmr::sCameraExtensionUnitSelector::display_name )
        .def_property_readonly( "valid", &wse::tmr::sCameraExtensionUnitSelector::valid );
    py::class_<wse::tmr::sCameraExtensionUnitValue>( module, "CameraExtensionUnitValue" )
        .def( py::init<>() )
        .def_readwrite( "selector", &wse::tmr::sCameraExtensionUnitValue::selector )
        .def_property( "payload", []( const wse::tmr::sCameraExtensionUnitValue& value_in )
        {
            return py::bytes( value_in.payload.empty() ? "" :
                reinterpret_cast<const char*>( value_in.payload.data() ), value_in.payload.size() );
        }, []( wse::tmr::sCameraExtensionUnitValue& value_in, const py::buffer& buffer_in )
        {
            value_in.payload = copyPythonBuffer( buffer_in );
        } )
        .def_property_readonly( "valid", &wse::tmr::sCameraExtensionUnitValue::valid );
    py::class_<wse::tmr::sCameraCapability>( module, "CameraCapability" )
        .def_readonly( "device", &wse::tmr::sCameraCapability::device )
        .def_readonly( "formats", &wse::tmr::sCameraCapability::formats )
        .def_readonly( "controls", &wse::tmr::sCameraCapability::controls )
        .def_readonly( "stream_profiles", &wse::tmr::sCameraCapability::stream_profiles )
        .def_readonly( "extension_units", &wse::tmr::sCameraCapability::extension_units );
    py::class_<PythonCameraFrame>( module, "CameraFrame" )
        .def_readonly( "width", &PythonCameraFrame::width )
        .def_readonly( "height", &PythonCameraFrame::height )
        .def_readonly( "pixel_format", &PythonCameraFrame::pixel_format )
        .def_readonly( "row_stride", &PythonCameraFrame::row_stride )
        .def_readonly( "sequence", &PythonCameraFrame::sequence )
        .def_readonly( "monotonic_timestamp_ns", &PythonCameraFrame::monotonic_timestamp_ns )
        .def_readonly( "data", &PythonCameraFrame::data )
        // A frame can be built as well as read, because the operations below take a frame and a
        // caller may well have one that did not come from a camera of ours.
        .def( py::init( []( const std::uint32_t width_in
                          , const std::uint32_t height_in
                          , const wse::tmr::eCameraPixelFormat format_in
                          , const py::buffer& data_in
                          , const std::size_t row_stride_in
                          , const std::uint64_t sequence_in
                          , const std::int64_t timestamp_in )
        {
            PythonCameraFrame frame;
            frame.width                  = width_in;
            frame.height                 = height_in;
            frame.pixel_format           = format_in;
            frame.data                   = wse::binding::FrameBuffer( copyPythonBuffer( data_in ) );
            frame.sequence               = sequence_in;
            frame.monotonic_timestamp_ns = timestamp_in;

            wse::tmr::sCameraFrameDescription description;
            description.width        = width_in;
            description.height       = height_in;
            description.pixel_format = format_in;
            frame.row_stride = row_stride_in != 0U
                ? row_stride_in : description.minimumRowStride();
            return frame;
        } )
        , py::arg( "width" )
        , py::arg( "height" )
        , py::arg( "pixel_format" )
        , py::arg( "data" )
        , py::arg( "row_stride" ) = 0U
        , py::arg( "sequence" ) = 0U
        , py::arg( "monotonic_timestamp_ns" ) = 0 );

    // The frame operations are free functions rather than methods on the camera, because moving or
    // averaging pixels is image work and has nothing to do with driving a device.
    module.def( "is_frame_operation_supported", &wse::tmr::isFrameOperationSupported,
        py::arg( "pixel_format" ) );
    module.def( "is_frame_averaging_supported", &wse::tmr::isFrameAveragingSupported,
        py::arg( "pixel_format" ) );
    module.def( "is_bayer_format", &wse::tmr::isBayerFormat, py::arg( "pixel_format" ) );
    module.def( "bayer_pattern_of", &bayerPatternOf, py::arg( "pixel_format" ) );
    module.def( "apply_orientation", &applyOrientation,
        py::arg( "frame" ), py::arg( "orientation" ) );
    module.def( "demosaic_frame", &demosaicFrame,
          py::arg( "frame" )
        , py::arg( "output_format" )
        , py::arg( "method" ) = wse::eDemosaicMethod::Bilinear );

    py::class_<PythonFrameAccumulator>( module, "CameraFrameAccumulator" )
        .def( py::init<>() )
        .def( "add", &PythonFrameAccumulator::add, py::arg( "frame" ) )
        .def( "average", &PythonFrameAccumulator::average )
        .def( "reset", &PythonFrameAccumulator::reset )
        .def_property_readonly( "count", &PythonFrameAccumulator::count );

    py::class_<PythonWebCamera>( module, "WebCamera" )
        .def( py::init<>() )
        .def_static( "enumerate", &PythonWebCamera::enumerate,
            py::arg( "backend" ) = wse::tmr::eCameraBackend::Automatic )
        .def_static( "capabilities", &PythonWebCamera::capabilities )
        .def( "open", py::overload_cast<const wse::tmr::sCameraDeviceInfo&>(
            &PythonWebCamera::open ) )
        .def( "open", py::overload_cast<const wse::tmr::sCameraDeviceInfo&,
            const wse::tmr::sCameraStreamConfiguration&>( &PythonWebCamera::open ) )
        .def( "start", py::overload_cast<>( &PythonWebCamera::start ) )
        .def( "start", py::overload_cast<py::object>( &PythonWebCamera::start ),
            py::arg( "callback" ) )
        .def( "stop", &PythonWebCamera::stop )
        .def( "read_frame", &PythonWebCamera::readFrame, py::arg( "timeout_ms" ) )
        .def( "read_averaged_frame", &PythonWebCamera::readAveragedFrame,
            py::arg( "count" ), py::arg( "timeout_ms" ) )
        .def( "current_capabilities", &PythonWebCamera::currentCapabilities )
        .def( "control_capability", &PythonWebCamera::controlCapability )
        .def( "get_control", &PythonWebCamera::getControl )
        .def( "set_control", &PythonWebCamera::setControl )
        .def( "get_extension_unit", &PythonWebCamera::getExtensionUnit )
        .def( "set_extension_unit", &PythonWebCamera::setExtensionUnit )
        // close() ends the session the way the C++ WebCamera does, so open() works again after it.
        // release() is the one-way step that gives the native camera back, and __exit__ takes it
        // because a `with` block owns the object for its whole lifetime.
        .def( "close", &PythonWebCamera::close )
        .def( "release", &PythonWebCamera::release )
        .def_property_readonly( "is_open", &PythonWebCamera::isOpen )
        .def_property_readonly( "is_streaming", &PythonWebCamera::isStreaming )
        .def_property_readonly( "released", &PythonWebCamera::isReleased )
        .def( "__enter__", []( PythonWebCamera& camera_in ) -> PythonWebCamera&
        {
            return camera_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", []( PythonWebCamera& camera_in,
              const py::object&, const py::object&, const py::object& )
        {
            camera_in.release();
            return false;
        } );
#endif // WSE_HAS_TMR

#ifdef WSE_HAS_IUI
    py::enum_<wse::iui::KeyboardAccessState>( module, "KeyboardAccessState" )
        .value( "STARTING", wse::iui::KeyboardAccessState::Starting )
        .value( "READY", wse::iui::KeyboardAccessState::Ready )
        .value( "UNAVAILABLE", wse::iui::KeyboardAccessState::Unavailable )
        .value( "PERMISSION_DENIED", wse::iui::KeyboardAccessState::PermissionDenied )
        .value( "DISCONNECTED", wse::iui::KeyboardAccessState::Disconnected );

    module.attr( "KEYBOARD_ASCII_COUNT" ) = static_cast<int>( wse::iui::ASCII_NUM );
    module.attr( "KEYBOARD_FUNCTION_COUNT" ) = static_cast<int>( wse::iui::FANCTION_NUM );
    module.attr( "KEYBOARD_ARROW_COUNT" ) = static_cast<int>( wse::iui::ARROW_NUM );
    module.attr( "KEYBOARD_LOCK_COUNT" ) = static_cast<int>( wse::iui::LOCK_NUM );
    module.attr( "KEYBOARD_COMMAND_COUNT" ) = static_cast<int>( wse::iui::COMMAND_NUM );

    py::class_<PythonKeyboard>( module, "Keyboard" )
        .def( py::init<>() )
        .def( "snapshot", &PythonKeyboard::snapshot )
        .def( "pressed_ascii", &PythonKeyboard::pressedAscii )
        .def( "close", &PythonKeyboard::close )
        .def_property_readonly( "access_state", &PythonKeyboard::accessState )
        .def_property_readonly( "is_available", &PythonKeyboard::isAvailable )
        .def_property_readonly( "closed", &PythonKeyboard::isClosed )
        .def( "__enter__", []( PythonKeyboard& keyboard_in ) -> PythonKeyboard&
        {
            return keyboard_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", []( PythonKeyboard& keyboard_in,
              const py::object&, const py::object&, const py::object& )
        {
            keyboard_in.close();
            return false;
        } );
#endif // WSE_HAS_IUI

#ifdef WSE_HAS_XPT
    py::enum_<wse::xpt::eTransportErrorCode>( module, "TransportErrorCode" )
        .value( "NONE", wse::xpt::eTransportErrorCode::None )
        .value( "INVALID_ARGUMENT", wse::xpt::eTransportErrorCode::InvalidArgument )
        .value( "HOST_NOT_FOUND", wse::xpt::eTransportErrorCode::HostNotFound )
        .value( "ADDRESS_UNAVAILABLE", wse::xpt::eTransportErrorCode::AddressUnavailable )
        .value( "CONNECTION_REFUSED", wse::xpt::eTransportErrorCode::ConnectionRefused )
        .value( "CONNECTION_RESET", wse::xpt::eTransportErrorCode::ConnectionReset )
        .value( "NETWORK_UNREACHABLE", wse::xpt::eTransportErrorCode::NetworkUnreachable )
        .value( "NOT_CONNECTED", wse::xpt::eTransportErrorCode::NotConnected )
        .value( "REMOTE_CLOSED", wse::xpt::eTransportErrorCode::RemoteClosed )
        .value( "TIMED_OUT", wse::xpt::eTransportErrorCode::TimedOut )
        .value( "CANCELLED", wse::xpt::eTransportErrorCode::Cancelled )
        .value( "BIND_FAILED", wse::xpt::eTransportErrorCode::BindFailed )
        .value( "SEND_FAILED", wse::xpt::eTransportErrorCode::SendFailed )
        .value( "RECEIVE_FAILED", wse::xpt::eTransportErrorCode::ReceiveFailed )
        .value( "MESSAGE_TOO_LARGE", wse::xpt::eTransportErrorCode::MessageTooLarge )
        .value( "DATAGRAM_TRUNCATED", wse::xpt::eTransportErrorCode::DatagramTruncated )
        .value( "RESOURCE_EXHAUSTED", wse::xpt::eTransportErrorCode::ResourceExhausted )
        .value( "UNSUPPORTED", wse::xpt::eTransportErrorCode::Unsupported )
        .value( "UNKNOWN", wse::xpt::eTransportErrorCode::Unknown )
        .value( "OPEN_FAILED", wse::xpt::eTransportErrorCode::OpenFailed )
        .value( "CONFIGURATION_FAILED", wse::xpt::eTransportErrorCode::ConfigurationFailed )
        .value( "HTTP_STATUS_ERROR", wse::xpt::eTransportErrorCode::HttpStatusError )
        .value( "RESPONSE_TOO_LARGE", wse::xpt::eTransportErrorCode::ResponseTooLarge )
        .value( "SECURITY_FAILED", wse::xpt::eTransportErrorCode::SecurityFailed );

    py::enum_<wse::xpt::eHttpMethod>( module, "HttpMethod" )
        .value( "GET", wse::xpt::eHttpMethod::Get )
        .value( "HEAD", wse::xpt::eHttpMethod::Head )
        .value( "POST", wse::xpt::eHttpMethod::Post )
        .value( "PUT", wse::xpt::eHttpMethod::Put )
        .value( "PATCH", wse::xpt::eHttpMethod::Patch )
        .value( "DELETE", wse::xpt::eHttpMethod::Delete );

    py::class_<PythonCancellationSource>( module, "CancellationSource" )
        .def( py::init<>() )
        .def( "cancel", &PythonCancellationSource::cancel )
        .def_property_readonly(
            "is_cancellation_requested", &PythonCancellationSource::isCancellationRequested );

    py::class_<PythonOperationContext>( module, "OperationContext" )
        .def( py::init( []( const std::int64_t timeout_ms_in, PythonCancellationSource* cancellation_in )
              {
                  return PythonOperationContext( timeout_ms_in, cancellation_in );
              } )
            , py::arg( "timeout_ms" )
            , py::arg( "cancellation" ) = nullptr
            , py::keep_alive<1, 3>() )
        .def_property_readonly( "timeout_ms", &PythonOperationContext::timeoutMilliseconds );

    py::class_<PythonTcpClient>( module, "TcpClient" )
        .def( py::init<>() )
        .def( "connect", &PythonTcpClient::connect,
            py::arg( "host" ), py::arg( "port" ), py::arg( "context" ) )
        .def( "disconnect", &PythonTcpClient::disconnect )
        .def( "check_peer_connection", &PythonTcpClient::checkPeerConnection )
        .def( "remote_endpoint", &PythonTcpClient::remoteEndpoint )
        .def( "local_endpoint", &PythonTcpClient::localEndpoint )
        .def( "send", &PythonTcpClient::send, py::arg( "data" ), py::arg( "context" ) )
        .def( "receive", &PythonTcpClient::receive,
            py::arg( "maximum_size" ), py::arg( "context" ) )
        .def_property_readonly( "is_connected", &PythonTcpClient::isConnected )
        .def( "__enter__", []( PythonTcpClient& client_in ) -> PythonTcpClient&
        {
            return client_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", []( PythonTcpClient& client_in,
              const py::object&, const py::object&, const py::object& )
        {
            client_in.disconnect();
            return false;
        } );

    py::class_<PythonUdpClient>( module, "UdpClient" )
        .def( py::init<>() )
        .def_static( "maximum_datagram_size", &PythonUdpClient::maximumDatagramSize )
        .def( "bind", &PythonUdpClient::bind,
            py::arg( "host" ), py::arg( "port" ), py::arg( "context" ) )
        .def( "close", &PythonUdpClient::close )
        .def( "local_endpoint", &PythonUdpClient::localEndpoint )
        .def( "send_to", &PythonUdpClient::sendTo,
            py::arg( "host" ), py::arg( "port" ), py::arg( "data" ), py::arg( "context" ) )
        .def( "receive_from", &PythonUdpClient::receiveFrom,
            py::arg( "maximum_size" ), py::arg( "context" ) )
        .def_property_readonly( "is_open", &PythonUdpClient::isOpen )
        .def( "__enter__", []( PythonUdpClient& client_in ) -> PythonUdpClient&
        {
            return client_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", []( PythonUdpClient& client_in,
              const py::object&, const py::object&, const py::object& )
        {
            client_in.close();
            return false;
        } );

    py::class_<PythonSerialPort>( module, "SerialPort" )
        .def( py::init<>() )
        .def( "open", &PythonSerialPort::open,
            py::arg( "device_name" ), py::arg( "baud_rate" ), py::arg( "context" ) )
        .def( "close", &PythonSerialPort::close )
        .def( "send", &PythonSerialPort::send, py::arg( "data" ), py::arg( "context" ) )
        .def( "receive", &PythonSerialPort::receive,
            py::arg( "maximum_size" ), py::arg( "context" ) )
        .def_property_readonly( "is_open", &PythonSerialPort::isOpen )
        .def_property_readonly( "device_name", &PythonSerialPort::deviceName )
        .def_property_readonly( "baud_rate", &PythonSerialPort::baudRate )
        .def( "__enter__", []( PythonSerialPort& port_in ) -> PythonSerialPort&
        {
            return port_in;
        }, py::return_value_policy::reference_internal )
        .def( "__exit__", []( PythonSerialPort& port_in,
              const py::object&, const py::object&, const py::object& )
        {
            port_in.close();
            return false;
        } );

    py::class_<PythonHttpRequest>( module, "HttpRequest" )
        .def( py::init<wse::xpt::eHttpMethod, const std::string&>(),
            py::arg( "method" ), py::arg( "url" ) )
        .def( "add_header", &PythonHttpRequest::addHeader,
            py::arg( "name" ), py::arg( "value" ) )
        .def( "set_body", &PythonHttpRequest::setBody, py::arg( "body" ) );

    py::class_<PythonHttpResponse>( module, "HttpResponse" )
        .def_property_readonly( "status_code", &PythonHttpResponse::statusCode )
        .def_property_readonly( "attempt_count", &PythonHttpResponse::attemptCount )
        .def( "headers", &PythonHttpResponse::headers )
        .def( "body", &PythonHttpResponse::body );

    module.def( "http_execute",
        []( const PythonHttpRequest& request_in, const std::size_t maximum_body_in,
            const PythonOperationContext& context_in )
        {
            return executeHttpRequest( request_in, maximum_body_in, context_in, nullptr, nullptr );
        },
        py::arg( "request" ), py::arg( "maximum_response_body_size" ), py::arg( "context" ) );

    module.def( "http_execute_authenticated",
        []( const PythonHttpRequest& request_in, const std::size_t maximum_body_in,
            const std::string& username_in, const std::string& secret_in,
            const PythonOperationContext& context_in )
        {
            return executeHttpRequest(
                request_in, maximum_body_in, context_in, &username_in, &secret_in );
        },
        py::arg( "request" ), py::arg( "maximum_response_body_size" ),
        py::arg( "username" ), py::arg( "secret" ), py::arg( "context" ) );
#endif // WSE_HAS_XPT

#ifdef WSE_EXTENSION_PYTHON_BINDING
#include "wse_extension_python_registrations.inc"
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_python_registrations.inc"
#endif

#ifdef WSE_HAS_OUI
    py::enum_<wse::oui::eRendererBackend>( module, "RendererBackend" )
        .value( "AUTOMATIC", wse::oui::eRendererBackend::Automatic )
        .value( "DIRECT3D12", wse::oui::eRendererBackend::Direct3D12 )
        .value( "VULKAN12", wse::oui::eRendererBackend::Vulkan12 );
    py::enum_<wse::oui::eTextureSamplingFilter>( module, "TextureSamplingFilter" )
        .value( "NEAREST", wse::oui::eTextureSamplingFilter::Nearest )
        .value( "LINEAR", wse::oui::eTextureSamplingFilter::Linear );
    py::enum_<wse::oui::eEdgeBlendCurve>( module, "EdgeBlendCurve" )
        .value( "LINEAR", wse::oui::eEdgeBlendCurve::Linear )
        .value( "SMOOTHSTEP", wse::oui::eEdgeBlendCurve::Smoothstep );
    py::class_<wse::oui::sRendererVertex2D>( module, "ProjectionVertex" )
        .def( py::init( []( const float x_in, const float y_in,
              const float u_in, const float v_in )
        {
            return wse::oui::sRendererVertex2D{ x_in, y_in, u_in, v_in };
        } ), py::arg( "x" ), py::arg( "y" ), py::arg( "u" ), py::arg( "v" ) )
        .def_readwrite( "x", &wse::oui::sRendererVertex2D::position_x )
        .def_readwrite( "y", &wse::oui::sRendererVertex2D::position_y )
        .def_readwrite( "u", &wse::oui::sRendererVertex2D::texture_u )
        .def_readwrite( "v", &wse::oui::sRendererVertex2D::texture_v );
    py::class_<wse::oui::sRendererColor>( module, "RendererColor" )
        .def( py::init<>() )
        .def( py::init( []( const float red_in, const float green_in,
              const float blue_in, const float alpha_in )
        {
            return wse::oui::sRendererColor{ red_in, green_in, blue_in, alpha_in };
        } ), py::arg( "red" ), py::arg( "green" ),
            py::arg( "blue" ), py::arg( "alpha" ) = 1.0F )
        .def_readwrite( "red", &wse::oui::sRendererColor::red )
        .def_readwrite( "green", &wse::oui::sRendererColor::green )
        .def_readwrite( "blue", &wse::oui::sRendererColor::blue )
        .def_readwrite( "alpha", &wse::oui::sRendererColor::alpha );
    py::class_<wse::oui::sEdgeBlendDescription>( module, "EdgeBlend" )
        .def( py::init<>() )
        .def_readwrite( "left", &wse::oui::sEdgeBlendDescription::left )
        .def_readwrite( "right", &wse::oui::sEdgeBlendDescription::right )
        .def_readwrite( "top", &wse::oui::sEdgeBlendDescription::top )
        .def_readwrite( "bottom", &wse::oui::sEdgeBlendDescription::bottom )
        .def_readwrite( "curve", &wse::oui::sEdgeBlendDescription::curve );
    py::class_<wse::oui::sProjectionImageLayer>( module, "ProjectionLayer" )
        .def( py::init( []( const std::uint32_t width_in, const std::uint32_t height_in,
              const py::buffer& rgba_in, std::vector<wse::oui::sRendererVertex2D> vertices_in,
              std::vector<std::uint32_t> indices_in, const py::object& alpha_in )
        {
            wse::oui::sProjectionImageLayer layer;
            layer.width = width_in;
            layer.height = height_in;
            layer.rgba = copyPythonBuffer( rgba_in );
            layer.vertices = std::move( vertices_in );
            layer.indices = std::move( indices_in );
            if ( !alpha_in.is_none() ) layer.alpha = copyPythonBuffer( py::reinterpret_borrow<py::buffer>( alpha_in ) );
            return layer;
        } ), py::arg( "width" ), py::arg( "height" ), py::arg( "rgba" ),
            py::arg( "vertices" ), py::arg( "indices" ), py::arg( "alpha" ) = py::none() )
        .def_readonly( "width", &wse::oui::sProjectionImageLayer::width )
        .def_readonly( "height", &wse::oui::sProjectionImageLayer::height )
        .def_readwrite( "sampling_filter", &wse::oui::sProjectionImageLayer::sampling_filter )
        .def_readwrite( "opacity", &wse::oui::sProjectionImageLayer::opacity )
        .def_readwrite( "edge_blend", &wse::oui::sProjectionImageLayer::edge_blend );
    py::class_<wse::oui::sProjectionRenderRequest>( module, "ProjectionRequest" )
        .def( py::init<>() )
        .def_readwrite( "output_width", &wse::oui::sProjectionRenderRequest::output_width )
        .def_readwrite( "output_height", &wse::oui::sProjectionRenderRequest::output_height )
        .def_readwrite( "clear_color", &wse::oui::sProjectionRenderRequest::clear_color )
        .def_readwrite( "supersample_scale", &wse::oui::sProjectionRenderRequest::supersample_scale )
        .def_readwrite( "backend", &wse::oui::sProjectionRenderRequest::backend )
        .def_readwrite( "use_software_adapter", &wse::oui::sProjectionRenderRequest::use_software_adapter )
        .def_readwrite( "enable_validation", &wse::oui::sProjectionRenderRequest::enable_validation )
        .def_readwrite( "adapter_name", &wse::oui::sProjectionRenderRequest::adapter_name )
        .def_readwrite( "timeout_ms", &wse::oui::sProjectionRenderRequest::timeout_ms )
        .def_readwrite( "layers", &wse::oui::sProjectionRenderRequest::layers );
    py::class_<wse::oui::sProjectionRenderFrame>( module, "ProjectionFrame" )
        .def_readonly( "width", &wse::oui::sProjectionRenderFrame::width )
        .def_readonly( "height", &wse::oui::sProjectionRenderFrame::height )
        .def_readonly( "row_pitch", &wse::oui::sProjectionRenderFrame::row_pitch )
        .def_readonly( "adapter_name", &wse::oui::sProjectionRenderFrame::adapter_name )
        .def_readonly( "data", &wse::oui::sProjectionRenderFrame::data );
    module.def( "render_projection", []( const wse::oui::sProjectionRenderRequest& request_in )
    {
        auto result = [&]()
        {
            py::gil_scoped_release release;
            return wse::oui::renderProjection( request_in );
        }();
        if ( !result.succeeded() ) throwError( rendererError( result.error() ) );
        return std::move( result.value() );
    }, py::arg( "request" ) );
#endif // WSE_HAS_OUI
}
