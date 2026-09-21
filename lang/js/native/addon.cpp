//*****************************************************************************************************************
//!
//! @file    addon.cpp
//! @brief   \~japanese WSE Binding facadeのNode-API Adapter. Core面の登録と各Component addonの合流点.
//! @brief   \~english  Node-API adapter for the WSE binding native facade; registers the Core surface and
//!                     joins the per-component addons.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#define NAPI_VERSION 8
#include <utility>
#include <node_api.h>

#include "component_addons.h"

#include <wse/binding/stew.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{

// ------------------------------------------------------------------------------------------------
// Addon state and operation registries. Every operation that owns a worker thread or a pending
// promise registers itself here, so environment teardown can cancel and reap all of them instead
// of leaving a thread calling into a dying Node instance.
// ------------------------------------------------------------------------------------------------

struct CallbackOperation;
struct AsyncOperation;
struct PromiseOperation;

struct AddonState final
{
    std::atomic_bool shutting_down;
    std::mutex callback_mutex;
    std::unordered_set<CallbackOperation*> callbacks;
    std::mutex async_mutex;
    std::unordered_set<AsyncOperation*> async_operations;
    std::mutex promise_mutex;
    std::unordered_set<PromiseOperation*> promise_operations;

    //! @brief Construct all members with explicit defaults.
    AddonState()
        : shutting_down      ( false )
        , callback_mutex     ()
        , callbacks          ()
        , async_mutex        ()
        , async_operations   ()
        , promise_mutex      ()
        , promise_operations ()
    {
    }
};

struct AddonStateHolder final
{
    std::shared_ptr<AddonState> state;
};

struct TimerHolder final
{
    CallbackOperation* operation;

    //! @brief Construct all members with explicit defaults.
    TimerHolder(
          CallbackOperation * operation_in = nullptr
    )
        : operation ( operation_in )
    {
    }
};

struct CallbackOperation final
{
    std::shared_ptr<AddonState> state;
    TimerHolder* holder;
    napi_threadsafe_function threadsafe_function;
    wse::binding::CancellationSource cancellation;
    std::thread worker;
    std::int64_t duration_ms;

    //! @brief Construct all members with explicit defaults.
    CallbackOperation()
        : state               ()
        , holder              ( nullptr )
        , threadsafe_function ( nullptr )
        , cancellation        ()
        , worker              ()
        , duration_ms         ( 0 )
    {
    }
};

struct AsyncOperation final
{
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    std::shared_ptr<AddonState> state;
    std::vector<std::uint8_t> input;
    wse::binding::FrameBuffer output;
    wse::binding::CancellationSource cancellation;
    wse::binding::Error error;

    //! @brief Construct all members with explicit defaults.
    AsyncOperation()
        : env          ( nullptr )
        , work         ( nullptr )
        , deferred     ( nullptr )
        , state        ()
        , input        ()
        , output       ()
        , cancellation ()
        , error        ()
    {
    }
};

struct PromiseOperation final
{
    std::shared_ptr<AddonState> state;
    napi_threadsafe_function threadsafe_function;
    napi_deferred deferred;
    wse::binding::CancellationSource cancellation;
    std::thread worker;
    std::int64_t duration_ms;
    wse::binding::Error error;
    std::atomic_bool runtime_reference_released;

    //! @brief Construct all members with explicit defaults.
    PromiseOperation()
        : state                      ()
        , threadsafe_function        ( nullptr )
        , deferred                   ( nullptr )
        , cancellation               ()
        , worker                     ()
        , duration_ms                ( 0 )
        , error                      ()
        , runtime_reference_released ( false )
    {
    }
};

std::shared_ptr<AddonState> addonState( napi_env env_in )
{
    AddonStateHolder* holder = nullptr;
    if ( napi_get_instance_data(
             env_in, reinterpret_cast<void**>( &holder ) ) != napi_ok
         || holder == nullptr )
    {
        return {};
    }
    return holder->state;
}

// Both the worker thread and the cleanup path may try to release the threadsafe function; the
// atomic exchange makes sure exactly one of them does, whichever arrives first.
void releasePromiseRuntimeReference(
      PromiseOperation* operation_in
    , napi_threadsafe_function_release_mode mode_in
)
{
    if ( !operation_in->runtime_reference_released.exchange(
             true, std::memory_order_acq_rel ) )
    {
        napi_release_threadsafe_function(
            operation_in->threadsafe_function, mode_in );
    }
}

// The order matters: cancel wakes the worker, the join guarantees it can no longer touch the
// operation, and only then is the operation unregistered and freed.
void closeCallbackOperation( CallbackOperation* operation_in )
{
    if ( operation_in == nullptr )
    {
        return;
    }
    operation_in->cancellation.cancel();
    if ( operation_in->worker.joinable() )
    {
        operation_in->worker.join();
    }
    {
        std::lock_guard<std::mutex> lock( operation_in->state->callback_mutex );
        operation_in->state->callbacks.erase( operation_in );
    }
    if ( operation_in->holder != nullptr )
    {
        operation_in->holder->operation = nullptr;
    }
    delete operation_in;
}

// Runs as Node's async cleanup hook when the environment shuts down (worker exit, process end).
// Cancels every registered operation and joins every worker before the hook is removed, so no
// native thread survives into a Node instance that no longer exists. Callback operations are
// drained one at a time without holding the lock, because closing one re-enters the registry.
void cleanupAddon(
      napi_async_cleanup_hook_handle cleanup_handle_in
    , void* data_in
)
{
    auto* holder = static_cast<AddonStateHolder*>( data_in );
    const std::shared_ptr<AddonState> state = holder->state;
    state->shutting_down.store( true, std::memory_order_release );
    {
        std::lock_guard<std::mutex> lock( state->async_mutex );
        for ( AsyncOperation* operation : state->async_operations )
        {
            operation->cancellation.cancel();
            if ( operation->work != nullptr )
            {
                napi_cancel_async_work( operation->env, operation->work );
            }
        }
    }
    std::vector<PromiseOperation*> promise_operations;
    {
        std::lock_guard<std::mutex> lock( state->promise_mutex );
        promise_operations.assign(
            state->promise_operations.begin(), state->promise_operations.end() );
    }
    for ( PromiseOperation* operation : promise_operations )
    {
        operation->cancellation.cancel();
        releasePromiseRuntimeReference( operation, napi_tsfn_abort );
        if ( operation->worker.joinable() )
        {
            operation->worker.join();
        }
    }
    for ( ;; )
    {
        CallbackOperation* operation = nullptr;
        {
            std::lock_guard<std::mutex> lock( state->callback_mutex );
            if ( state->callbacks.empty() )
            {
                break;
            }
            operation = *state->callbacks.begin();
        }
        closeCallbackOperation( operation );
    }
    napi_remove_async_cleanup_hook( cleanup_handle_in );
}

void finalizeAddon( napi_env, void* data_in, void* )
{
    delete static_cast<AddonStateHolder*>( data_in );
}

void finalizeTimer( napi_env, void* data_in, void* )
{
    auto* holder = static_cast<TimerHolder*>( data_in );
    closeCallbackOperation( holder->operation );
    delete holder;
}

bool setNamedValue(
      napi_env env_in
    , napi_value object_in
    , const char* name_in
    , napi_value value_in
)
{
    return napi_set_named_property( env_in, object_in, name_in, value_in ) == napi_ok;
}

napi_value makeString( napi_env env_in, const std::string& value_in )
{
    napi_value result = nullptr;
    if ( napi_create_string_utf8(
             env_in, value_in.c_str(), value_in.size(), &result ) != napi_ok )
    {
        return nullptr;
    }
    return result;
}

napi_value makeError( napi_env env_in, const wse::binding::Error& error_in )
{
    napi_value message = makeString( env_in, error_in.message() );
    napi_value result = nullptr;
    if ( message == nullptr
         || napi_create_error( env_in, nullptr, message, &result ) != napi_ok )
    {
        return nullptr;
    }

    napi_value category = nullptr;
    napi_value code = nullptr;
    napi_value native_code = nullptr;
    napi_create_uint32(
        env_in, static_cast<std::uint32_t>( error_in.category() ), &category );
    napi_create_int32( env_in, error_in.code(), &code );
    napi_create_int64( env_in, error_in.nativeCode(), &native_code );
    if ( category == nullptr || code == nullptr || native_code == nullptr
         || !setNamedValue( env_in, result, "category", category )
         || !setNamedValue( env_in, result, "code", code )
         || !setNamedValue( env_in, result, "nativeCode", native_code ) )
    {
        return nullptr;
    }
    return result;
}

bool readBytes(
      std::vector<std::uint8_t>* const p_bytes_out
    , napi_env env_in
    , napi_value value_in
)
{
    std::vector<std::uint8_t>& bytes_out = *p_bytes_out;

    bool is_buffer = false;
    if ( napi_is_buffer( env_in, value_in, &is_buffer ) != napi_ok )
    {
        return false;
    }
    if ( is_buffer )
    {
        void* data = nullptr;
        std::size_t size = 0U;
        if ( napi_get_buffer_info( env_in, value_in, &data, &size ) != napi_ok )
        {
            return false;
        }
        if ( size == 0U )
        {
            bytes_out.clear();
            return true;
        }
        const auto* begin = static_cast<const std::uint8_t*>( data );
        bytes_out.assign( begin, begin + size );
        return true;
    }

    bool is_typed_array = false;
    if ( napi_is_typedarray( env_in, value_in, &is_typed_array ) != napi_ok )
    {
        return false;
    }
    if ( is_typed_array )
    {
        napi_typedarray_type type = napi_uint8_array;
        std::size_t length = 0U;
        void* data = nullptr;
        napi_value array_buffer = nullptr;
        std::size_t byte_offset = 0U;
        if ( napi_get_typedarray_info(
                 env_in, value_in, &type, &length, &data, &array_buffer, &byte_offset ) != napi_ok )
        {
            return false;
        }
        std::size_t element_size = 0U;
        switch ( type )
        {
            case napi_int8_array:
            case napi_uint8_array:
            case napi_uint8_clamped_array:
                element_size = 1U;
                break;
            case napi_int16_array:
            case napi_uint16_array:
            case napi_float16_array:
                element_size = 2U;
                break;
            case napi_int32_array:
            case napi_uint32_array:
            case napi_float32_array:
                element_size = 4U;
                break;
            case napi_float64_array:
            case napi_bigint64_array:
            case napi_biguint64_array:
                element_size = 8U;
                break;
        }
        if ( element_size == 0U
             || length > std::numeric_limits<std::size_t>::max() / element_size )
        {
            return false;
        }
        const std::size_t byte_length = length * element_size;
        if ( byte_length == 0U )
        {
            bytes_out.clear();
            return true;
        }
        const auto* begin = static_cast<const std::uint8_t*>( data );
        bytes_out.assign( begin, begin + byte_length );
        return true;
    }

    bool is_data_view = false;
    if ( napi_is_dataview( env_in, value_in, &is_data_view ) != napi_ok || !is_data_view )
    {
        return false;
    }
    std::size_t byte_length = 0U;
    void* data = nullptr;
    napi_value array_buffer = nullptr;
    std::size_t byte_offset = 0U;
    if ( napi_get_dataview_info(
             env_in, value_in, &byte_length, &data, &array_buffer, &byte_offset ) != napi_ok )
    {
        return false;
    }
    if ( byte_length == 0U )
    {
        bytes_out.clear();
        return true;
    }
    const auto* begin = static_cast<const std::uint8_t*>( data );
    bytes_out.assign( begin, begin + byte_length );
    return true;
}

// Runs on the libuv thread pool, so it must not touch any napi_value. Every failure — including
// an exception out of the facade — lands in operation.error rather than crossing the C boundary,
// and completeAsync translates it on the JavaScript thread.
void executeAsync( napi_env, void* data_in )
{
    AsyncOperation& operation = *static_cast<AsyncOperation*>( data_in );
    try
    {
        if ( operation.state->shutting_down.load( std::memory_order_acquire ) )
        {
            operation.error = wse::binding::Error(
                  wse::binding::eErrorCategory::Cancellation
                , 1
                , "The Node.js environment is shutting down."
            );
            return;
        }

        const auto result = wse::binding::Runtime().copyFrame( operation.input );
        if ( result.succeeded() )
        {
            operation.output = result.value();
        }
        else
        {
            operation.error = result.error();
        }
    }
    catch ( const std::bad_alloc& )
    {
        operation.error = wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "The Node-API operation could not allocate memory."
        );
    }
    catch ( ... )
    {
        operation.error = wse::binding::Error(
              wse::binding::eErrorCategory::Internal
            , 1
            , "The Node-API operation failed unexpectedly."
        );
    }
}

void completeAsync( napi_env env_in, napi_status status_in, void* data_in )
{
    AsyncOperation* operation = static_cast<AsyncOperation*>( data_in );
    {
        std::lock_guard<std::mutex> lock( operation->state->async_mutex );
        operation->state->async_operations.erase( operation );
    }
    if ( status_in != napi_ok && operation->error.ok() )
    {
        operation->error = wse::binding::Error(
              wse::binding::eErrorCategory::Cancellation
            , 1
            , "The Node-API asynchronous work was cancelled."
        );
    }

    if ( operation->error.ok() )
    {
        napi_value value = nullptr;
        const std::vector<std::uint8_t>& bytes = operation->output.bytes();
        napi_create_buffer_copy(
              env_in
            , bytes.size()
            , bytes.empty() ? nullptr : bytes.data()
            , nullptr
            , &value
        );
        if ( value != nullptr )
        {
            napi_resolve_deferred( env_in, operation->deferred, value );
        }
    }
    else
    {
        napi_value error = makeError( env_in, operation->error );
        if ( error != nullptr )
        {
            napi_reject_deferred( env_in, operation->deferred, error );
        }
    }

    napi_delete_async_work( env_in, operation->work );
    delete operation;
}

napi_value queueOperation( napi_env env_in, AsyncOperation* operation_in )
{
    napi_value promise = nullptr;
    napi_value resource_name = makeString( env_in, "wse.binding.operation" );
    if ( resource_name == nullptr
         || napi_create_promise( env_in, &operation_in->deferred, &promise ) != napi_ok
         || napi_create_async_work(
                env_in
              , nullptr
              , resource_name
              , executeAsync
              , completeAsync
              , operation_in
              , &operation_in->work ) != napi_ok )
    {
        if ( operation_in->work != nullptr )
        {
            napi_delete_async_work( env_in, operation_in->work );
        }
        delete operation_in;
        napi_throw_error( env_in, "WSE_BINDING_QUEUE_FAILED", "Unable to queue WSE work." );
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock( operation_in->state->async_mutex );
        operation_in->state->async_operations.insert( operation_in );
    }
    if ( napi_queue_async_work( env_in, operation_in->work ) != napi_ok )
    {
        {
            std::lock_guard<std::mutex> lock( operation_in->state->async_mutex );
            operation_in->state->async_operations.erase( operation_in );
        }
        napi_delete_async_work( env_in, operation_in->work );
        delete operation_in;
        napi_throw_error( env_in, "WSE_BINDING_QUEUE_FAILED", "Unable to queue WSE work." );
        return nullptr;
    }
    return promise;
}

napi_value runtimeInfo( napi_env env_in, napi_callback_info )
{
    const wse::binding::sRuntimeInfo info = wse::binding::Runtime().info();
    napi_value result = nullptr;
    napi_value version = makeString( env_in, info.version );
    napi_value abi = nullptr;
    napi_value components = nullptr;
    napi_value has_xpt = nullptr;
    napi_value has_tmr = nullptr;
    napi_value has_oui = nullptr;
    napi_value has_gef = nullptr;
    napi_value has_iui = nullptr;
    napi_value has_vpj = nullptr;
    napi_create_object( env_in, &result );
    napi_create_uint32( env_in, info.binding_abi_version, &abi );
    napi_create_object( env_in, &components );
    napi_get_boolean( env_in, info.has_xpt, &has_xpt );
    napi_get_boolean( env_in, info.has_tmr, &has_tmr );
    napi_get_boolean( env_in, info.has_oui, &has_oui );
    napi_get_boolean( env_in, info.has_gef, &has_gef );
    napi_get_boolean( env_in, info.has_iui, &has_iui );
    napi_get_boolean( env_in, info.has_vpj, &has_vpj );
    if ( result == nullptr || version == nullptr || abi == nullptr || components == nullptr
         || has_xpt == nullptr || has_tmr == nullptr || has_oui == nullptr
         || has_gef == nullptr || has_iui == nullptr || has_vpj == nullptr
         || !setNamedValue( env_in, result, "version", version )
         || !setNamedValue( env_in, result, "bindingAbiVersion", abi )
         || !setNamedValue( env_in, components, "xpt", has_xpt )
         || !setNamedValue( env_in, components, "tmr", has_tmr )
         || !setNamedValue( env_in, components, "oui", has_oui )
         || !setNamedValue( env_in, components, "gef", has_gef )
         || !setNamedValue( env_in, components, "iui", has_iui )
         || !setNamedValue( env_in, components, "vpj", has_vpj )
         || !setNamedValue( env_in, result, "components", components ) )
    {
        napi_throw_error( env_in, "WSE_BINDING_INFO_FAILED", "Unable to create runtime info." );
        return nullptr;
    }
    return result;
}

napi_value copyFrame( napi_env env_in, napi_callback_info info_in )
{
    std::size_t argument_count = 1U;
    napi_value arguments[ 1 ] = {};
    if ( napi_get_cb_info(
             env_in, info_in, &argument_count, arguments, nullptr, nullptr ) != napi_ok
         || argument_count != 1U )
    {
        napi_throw_type_error(
            env_in, "WSE_BINDING_ARGUMENT", "copyFrame requires one Buffer or ArrayBufferView." );
        return nullptr;
    }

    auto* operation = new ( std::nothrow ) AsyncOperation();
    if ( operation == nullptr || !readBytes( &operation->input, env_in, arguments[ 0 ] ) )
    {
        delete operation;
        napi_throw_type_error(
            env_in, "WSE_BINDING_ARGUMENT", "copyFrame requires a Buffer or ArrayBufferView." );
        return nullptr;
    }
    operation->env = env_in;
    operation->state = addonState( env_in );
    if ( !operation->state )
    {
        delete operation;
        napi_throw_error( env_in, "WSE_BINDING_STATE", "WSE binding state is unavailable." );
        return nullptr;
    }
    return queueOperation( env_in, operation );
}

void finalizePromiseOperation( napi_env, void* data_in, void* )
{
    auto* operation = static_cast<PromiseOperation*>( data_in );
    if ( operation->worker.joinable() )
    {
        operation->worker.join();
    }
    {
        std::lock_guard<std::mutex> lock( operation->state->promise_mutex );
        operation->state->promise_operations.erase( operation );
    }
    delete operation;
}

void completePromiseOperation(
      napi_env env_in
    , napi_value
    , void* context_in
    , void*
)
{
    auto* operation = static_cast<PromiseOperation*>( context_in );
    if ( env_in != nullptr )
    {
        if ( operation->error.ok() )
        {
            napi_value value = nullptr;
            if ( napi_get_undefined( env_in, &value ) == napi_ok )
            {
                napi_resolve_deferred( env_in, operation->deferred, value );
            }
        }
        else
        {
            napi_value error = makeError( env_in, operation->error );
            if ( error != nullptr )
            {
                napi_reject_deferred( env_in, operation->deferred, error );
            }
        }
        releasePromiseRuntimeReference( operation, napi_tsfn_release );
    }
}

napi_value wait( napi_env env_in, napi_callback_info info_in )
{
    std::size_t argument_count = 1U;
    napi_value arguments[ 1 ] = {};
    napi_valuetype type = napi_undefined;
    std::int64_t duration_ms = 0;
    if ( napi_get_cb_info(
             env_in, info_in, &argument_count, arguments, nullptr, nullptr ) != napi_ok
         || argument_count != 1U
         || napi_typeof( env_in, arguments[ 0 ], &type ) != napi_ok
         || type != napi_number
         || napi_get_value_int64( env_in, arguments[ 0 ], &duration_ms ) != napi_ok )
    {
        napi_throw_type_error(
            env_in, "WSE_BINDING_ARGUMENT", "wait requires one integer millisecond duration." );
        return nullptr;
    }
    if ( duration_ms < 0 || duration_ms > 86400000 )
    {
        napi_throw_range_error(
            env_in, "WSE_BINDING_RANGE", "wait duration must be between 0 and 86400000 ms." );
        return nullptr;
    }

    auto* operation = new ( std::nothrow ) PromiseOperation();
    if ( operation == nullptr )
    {
        napi_throw_error( env_in, "WSE_BINDING_MEMORY", "Unable to allocate WSE work." );
        return nullptr;
    }
    operation->duration_ms = duration_ms;
    operation->state = addonState( env_in );
    if ( !operation->state )
    {
        delete operation;
        napi_throw_error( env_in, "WSE_BINDING_STATE", "WSE binding state is unavailable." );
        return nullptr;
    }

    napi_value promise = nullptr;
    napi_value resource_name = makeString( env_in, "wse.binding.wait" );
    if ( resource_name == nullptr
         || napi_create_promise( env_in, &operation->deferred, &promise ) != napi_ok
         || napi_create_threadsafe_function(
                env_in
              , nullptr
              , nullptr
              , resource_name
              , 1U
              , 2U
              , operation
              , finalizePromiseOperation
              , operation
              , completePromiseOperation
              , &operation->threadsafe_function ) != napi_ok )
    {
        if ( operation->threadsafe_function != nullptr )
        {
            napi_release_threadsafe_function(
                operation->threadsafe_function, napi_tsfn_abort );
            napi_release_threadsafe_function(
                operation->threadsafe_function, napi_tsfn_release );
        }
        else
        {
            delete operation;
        }
        napi_throw_error( env_in, "WSE_BINDING_WAIT", "Unable to create WSE wait work." );
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock( operation->state->promise_mutex );
        operation->state->promise_operations.insert( operation );
    }
    try
    {
        operation->worker = std::thread( [operation]()
        {
            const auto status = wse::binding::Runtime().wait(
                  std::chrono::milliseconds( operation->duration_ms )
                , operation->cancellation.token()
            );
            if ( !status.succeeded() )
            {
                operation->error = status.error();
            }
            if ( !operation->state->shutting_down.load( std::memory_order_acquire ) )
            {
                if ( napi_call_threadsafe_function(
                         operation->threadsafe_function, nullptr, napi_tsfn_blocking ) != napi_ok )
                {
                    releasePromiseRuntimeReference( operation, napi_tsfn_abort );
                }
            }
            napi_release_threadsafe_function(
                operation->threadsafe_function, napi_tsfn_release );
        } );
    }
    catch ( ... )
    {
        releasePromiseRuntimeReference( operation, napi_tsfn_abort );
        napi_release_threadsafe_function(
            operation->threadsafe_function, napi_tsfn_release );
        napi_throw_error( env_in, "WSE_BINDING_WAIT", "Unable to start the WSE wait thread." );
        return nullptr;
    }
    return promise;
}

void callTimerCallback(
      napi_env env_in
    , napi_value callback_in
    , void*
    , void*
)
{
    if ( env_in == nullptr || callback_in == nullptr )
    {
        return;
    }
    napi_value receiver = nullptr;
    napi_value ignored = nullptr;
    if ( napi_get_undefined( env_in, &receiver ) == napi_ok )
    {
        napi_call_function( env_in, receiver, callback_in, 0U, nullptr, &ignored );
    }
}

napi_value cancelTimer( napi_env env_in, napi_callback_info info_in )
{
    void* data = nullptr;
    std::size_t argument_count = 0U;
    napi_get_cb_info( env_in, info_in, &argument_count, nullptr, nullptr, &data );
    auto* holder = static_cast<TimerHolder*>( data );
    if ( holder != nullptr && holder->operation != nullptr )
    {
        holder->operation->cancellation.cancel();
    }
    napi_value result = nullptr;
    napi_get_undefined( env_in, &result );
    return result;
}

napi_value closeTimer( napi_env env_in, napi_callback_info info_in )
{
    void* data = nullptr;
    std::size_t argument_count = 0U;
    napi_get_cb_info( env_in, info_in, &argument_count, nullptr, nullptr, &data );
    auto* holder = static_cast<TimerHolder*>( data );
    if ( holder != nullptr )
    {
        closeCallbackOperation( holder->operation );
    }
    napi_value result = nullptr;
    napi_get_undefined( env_in, &result );
    return result;
}

napi_value runAfter( napi_env env_in, napi_callback_info info_in )
{
    std::size_t argument_count = 2U;
    napi_value arguments[ 2 ] = {};
    napi_valuetype duration_type = napi_undefined;
    napi_valuetype callback_type = napi_undefined;
    std::int64_t duration_ms = 0;
    if ( napi_get_cb_info(
             env_in, info_in, &argument_count, arguments, nullptr, nullptr ) != napi_ok
         || argument_count != 2U
         || napi_typeof( env_in, arguments[ 0 ], &duration_type ) != napi_ok
         || duration_type != napi_number
         || napi_get_value_int64( env_in, arguments[ 0 ], &duration_ms ) != napi_ok
         || napi_typeof( env_in, arguments[ 1 ], &callback_type ) != napi_ok
         || callback_type != napi_function )
    {
        napi_throw_type_error(
            env_in, "WSE_BINDING_ARGUMENT", "runAfter requires milliseconds and a callback." );
        return nullptr;
    }
    if ( duration_ms < 0 || duration_ms > 86400000 )
    {
        napi_throw_range_error(
            env_in, "WSE_BINDING_RANGE", "runAfter duration must be between 0 and 86400000 ms." );
        return nullptr;
    }

    auto* holder = new ( std::nothrow ) TimerHolder();
    auto* operation = new ( std::nothrow ) CallbackOperation();
    if ( holder == nullptr || operation == nullptr )
    {
        delete holder;
        delete operation;
        napi_throw_error( env_in, "WSE_BINDING_MEMORY", "Unable to allocate WSE timer." );
        return nullptr;
    }
    operation->state = addonState( env_in );
    if ( !operation->state )
    {
        holder->operation = nullptr;
        delete operation;
        delete holder;
        napi_throw_error( env_in, "WSE_BINDING_STATE", "WSE binding state is unavailable." );
        return nullptr;
    }
    operation->holder = holder;
    operation->duration_ms = duration_ms;
    holder->operation = operation;

    napi_value resource_name = makeString( env_in, "wse.binding.callback" );
    napi_value result = nullptr;
    if ( resource_name == nullptr
         || napi_create_threadsafe_function(
                env_in
              , arguments[ 1 ]
              , nullptr
              , resource_name
              , 1U
              , 1U
              , nullptr
              , nullptr
              , nullptr
              , callTimerCallback
              , &operation->threadsafe_function ) != napi_ok
         || napi_create_object( env_in, &result ) != napi_ok )
    {
        if ( operation->threadsafe_function != nullptr )
        {
            napi_release_threadsafe_function(
                operation->threadsafe_function, napi_tsfn_abort );
        }
        holder->operation = nullptr;
        delete operation;
        delete holder;
        napi_throw_error( env_in, "WSE_BINDING_TIMER", "Unable to create WSE timer." );
        return nullptr;
    }

    const napi_property_descriptor methods[] = {
        { "cancel", nullptr, cancelTimer, nullptr, nullptr, nullptr, napi_default, holder },
        { "close", nullptr, closeTimer, nullptr, nullptr, nullptr, napi_default, holder },
    };
    if ( napi_define_properties(
             env_in, result, sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, result, holder, finalizeTimer, nullptr, nullptr ) != napi_ok )
    {
        napi_release_threadsafe_function(
            operation->threadsafe_function, napi_tsfn_abort );
        holder->operation = nullptr;
        delete operation;
        delete holder;
        napi_throw_error( env_in, "WSE_BINDING_TIMER", "Unable to export WSE timer." );
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock( operation->state->callback_mutex );
        operation->state->callbacks.insert( operation );
    }
    try
    {
        operation->worker = std::thread( [operation]()
        {
            const auto status = wse::binding::Runtime().wait(
                  std::chrono::milliseconds( operation->duration_ms )
                , operation->cancellation.token()
            );
            if ( status.succeeded()
                 && !operation->state->shutting_down.load( std::memory_order_acquire ) )
            {
                napi_call_threadsafe_function(
                    operation->threadsafe_function, nullptr, napi_tsfn_blocking );
            }
            napi_release_threadsafe_function(
                operation->threadsafe_function, napi_tsfn_release );
        } );
    }
    catch ( ... )
    {
        {
            std::lock_guard<std::mutex> lock( operation->state->callback_mutex );
            operation->state->callbacks.erase( operation );
        }
        napi_release_threadsafe_function(
            operation->threadsafe_function, napi_tsfn_abort );
        holder->operation = nullptr;
        delete operation;
        napi_throw_error( env_in, "WSE_BINDING_TIMER", "Unable to start the WSE timer thread." );
        return nullptr;
    }
    return result;
}

napi_value initialize( napi_env env_in, napi_value exports_in )
{
    auto* holder = new ( std::nothrow ) AddonStateHolder();
    if ( holder != nullptr )
    {
        try
        {
            holder->state = std::make_shared<AddonState>();
        }
        catch ( ... )
        {
            delete holder;
            holder = nullptr;
        }
    }
    if ( holder == nullptr )
    {
        napi_throw_error( env_in, "WSE_BINDING_INIT", "Unable to initialize WSE binding." );
        return nullptr;
    }
    if ( napi_set_instance_data( env_in, holder, finalizeAddon, nullptr ) != napi_ok )
    {
        delete holder;
        napi_throw_error( env_in, "WSE_BINDING_INIT", "Unable to initialize WSE binding." );
        return nullptr;
    }
    napi_async_cleanup_hook_handle cleanup_handle = nullptr;
    if ( napi_add_async_cleanup_hook(
             env_in, cleanupAddon, holder, &cleanup_handle ) != napi_ok )
    {
        napi_throw_error( env_in, "WSE_BINDING_INIT", "Unable to register WSE cleanup." );
        return nullptr;
    }

    const napi_property_descriptor properties[] = {
        { "runtimeInfo", nullptr, runtimeInfo, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "copyFrame", nullptr, copyFrame, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "wait", nullptr, wait, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "runAfter", nullptr, runAfter, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    if ( napi_define_properties(
             env_in
           , exports_in
           , sizeof( properties ) / sizeof( properties[ 0 ] )
           , properties ) != napi_ok )
    {
        napi_throw_error( env_in, "WSE_BINDING_INIT", "Unable to export WSE binding." );
        return nullptr;
    }
    if ( registerTmrAddon( env_in, exports_in ) != napi_ok
         || registerOuiAddon( env_in, exports_in ) != napi_ok
         || registerIuiAddon( env_in, exports_in ) != napi_ok
#ifdef WSE_HAS_VPJ
         || registerVpjAddon( env_in, exports_in ) != napi_ok
#endif
         || registerXptAddon( env_in, exports_in ) != napi_ok )
    {
        napi_throw_error(
            env_in, "WSE_BINDING_INIT", "Unable to export WSE component bindings." );
        return nullptr;
    }
    return exports_in;
}

} // namespace

NAPI_MODULE( NODE_GYP_MODULE_NAME, initialize )
