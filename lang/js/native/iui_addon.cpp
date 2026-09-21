//*****************************************************************************************************************
//!
//! @file    iui_addon.cpp
//! @brief   \~japanese IUI Keyboard面のNode-API addon. IUI無しのBuildでは登録関数が何もせず成功する.
//! @brief   \~english  Node-API addon for the IUI keyboard surface; in a build without IUI the registration
//!                     function succeeds without registering anything.
//!
//! @date
//!   Sep-01, 2026   Create New.
//*****************************************************************************************************************

#define NAPI_VERSION 8
#include "component_addons.h"

#include <wse/binding/stew.h>
#ifdef WSE_HAS_IUI
#include <iui/stew.h>
#include <wse/binding/IuiErrorAdapter.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>

namespace
{

napi_value stringValue( napi_env env_in, const std::string& value_in )
{
    napi_value value = nullptr;
    napi_create_string_utf8( env_in, value_in.c_str(), value_in.size(), &value );
    return value;
}

napi_value int64Value( napi_env env_in, const std::int64_t value_in )
{
    napi_value value = nullptr;
    napi_create_int64( env_in, value_in, &value );
    return value;
}

napi_value uint32Value( napi_env env_in, const std::uint32_t value_in )
{
    napi_value value = nullptr;
    napi_create_uint32( env_in, value_in, &value );
    return value;
}

napi_value boolValue( napi_env env_in, const bool value_in )
{
    napi_value value = nullptr;
    napi_get_boolean( env_in, value_in, &value );
    return value;
}

bool setProperty( napi_env env_in, napi_value object_in,
    const char* name_in, napi_value value_in )
{
    return value_in != nullptr
        && napi_set_named_property( env_in, object_in, name_in, value_in ) == napi_ok;
}

napi_value enumValue( napi_env env_in,
    const std::initializer_list<std::pair<const char*, std::uint32_t>>& values_in )
{
    napi_value result = nullptr;
    if ( napi_create_object( env_in, &result ) != napi_ok ) return nullptr;
    for ( const auto& value : values_in )
    {
        if ( !setProperty( env_in, result, value.first,
                 uint32Value( env_in, value.second ) ) ) return nullptr;
    }
    return result;
}

#ifdef WSE_HAS_IUI
void throwError( napi_env env_in, const wse::binding::Error& error_in )
{
    napi_value failure = nullptr;
    if ( napi_create_error( env_in, nullptr,
             stringValue( env_in, error_in.message() ), &failure ) != napi_ok ) return;
    setProperty( env_in, failure, "category",
        uint32Value( env_in, static_cast<std::uint32_t>( error_in.category() ) ) );
    setProperty( env_in, failure, "code", int64Value( env_in, error_in.code() ) );
    setProperty( env_in, failure, "nativeCode", int64Value( env_in, error_in.nativeCode() ) );
    napi_throw( env_in, failure );
}

//! Keyboard state shared by the wrapper object and its finalizer.
struct KeyboardState final
{
    std::mutex mutex;
    std::unique_ptr<wse::iui::Keyboard> keyboard;
    bool closed;

    //! @brief Construct all members with explicit defaults.
    KeyboardState()
        : mutex    ()
        , keyboard ()
        , closed   ( false )
    {
    }
};

struct KeyboardHolder final
{
    std::shared_ptr<KeyboardState> state;
};

void closeKeyboardState( const std::shared_ptr<KeyboardState>& state_in )
{
    if ( !state_in ) return;
    std::lock_guard<std::mutex> lock( state_in->mutex );
    if ( state_in->closed ) return;
    state_in->keyboard.reset();
    state_in->closed = true;
}

void finalizeKeyboard( napi_env, void* data_in, void* )
{
    auto* holder = static_cast<KeyboardHolder*>( data_in );
    closeKeyboardState( holder->state );
    delete holder;
}

KeyboardHolder* keyboardHolder( napi_env env_in, napi_callback_info info_in )
{
    napi_value self = nullptr;
    if ( napi_get_cb_info( env_in, info_in, nullptr, nullptr, &self, nullptr ) != napi_ok )
    {
        return nullptr;
    }
    void* data = nullptr;
    if ( napi_unwrap( env_in, self, &data ) != napi_ok || data == nullptr )
    {
        napi_throw_error( env_in, "WSE_KEYBOARD_STATE", "The keyboard is not initialized." );
        return nullptr;
    }
    auto* holder = static_cast<KeyboardHolder*>( data );
    if ( holder->state->closed )
    {
        napi_throw_error( env_in, "WSE_KEYBOARD_CLOSED", "The keyboard is already closed." );
        return nullptr;
    }
    return holder;
}

//! Converts one key group into a JavaScript boolean array.
template <std::size_t Count>
napi_value groupValue( napi_env env_in, const std::array<bool, Count>& source_in )
{
    napi_value array = nullptr;
    if ( napi_create_array_with_length( env_in, Count, &array ) != napi_ok ) return nullptr;
    for ( std::size_t index = 0U; index < Count; ++index )
    {
        if ( napi_set_element( env_in, array, static_cast<std::uint32_t>( index ),
                 boolValue( env_in, source_in[ index ] ) ) != napi_ok ) return nullptr;
    }
    return array;
}

napi_value keyboardAccessState( napi_env env_in, napi_callback_info info_in )
{
    KeyboardHolder* holder = keyboardHolder( env_in, info_in );
    if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    return uint32Value( env_in,
        static_cast<std::uint32_t>( holder->state->keyboard->accessState() ) );
}

napi_value keyboardIsAvailable( napi_env env_in, napi_callback_info info_in )
{
    KeyboardHolder* holder = keyboardHolder( env_in, info_in );
    if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    return boolValue( env_in, holder->state->keyboard->isAvailable() );
}

napi_value keyboardSnapshot( napi_env env_in, napi_callback_info info_in )
{
    KeyboardHolder* holder = keyboardHolder( env_in, info_in );
    if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );

    const wse::binding::Error readiness =
        wse::binding::fromIuiKeyboardState( holder->state->keyboard->accessState() );
    if ( !readiness.ok() ) { throwError( env_in, readiness ); return nullptr; }

    const wse::iui::KeyboardState snapshot = holder->state->keyboard->snapshot();
    napi_value result = nullptr;
    if ( napi_create_object( env_in, &result ) != napi_ok ) return nullptr;
    if ( !setProperty( env_in, result, "ascii", groupValue( env_in, snapshot.ascii ) )
         || !setProperty( env_in, result, "function", groupValue( env_in, snapshot.function ) )
         || !setProperty( env_in, result, "arrow", groupValue( env_in, snapshot.arrow ) )
         || !setProperty( env_in, result, "lock", groupValue( env_in, snapshot.lock ) )
         || !setProperty( env_in, result, "command", groupValue( env_in, snapshot.command ) ) )
    {
        return nullptr;
    }
    return result;
}

napi_value keyboardPressedAscii( napi_env env_in, napi_callback_info info_in )
{
    KeyboardHolder* holder = keyboardHolder( env_in, info_in );
    if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );

    const wse::binding::Error readiness =
        wse::binding::fromIuiKeyboardState( holder->state->keyboard->accessState() );
    if ( !readiness.ok() ) { throwError( env_in, readiness ); return nullptr; }

    return int64Value( env_in,
        static_cast<std::int64_t>( holder->state->keyboard->getASCII() ) );
}

napi_value keyboardClose( napi_env env_in, napi_callback_info info_in )
{
    napi_value self = nullptr;
    if ( napi_get_cb_info( env_in, info_in, nullptr, nullptr, &self, nullptr ) != napi_ok )
    {
        return nullptr;
    }
    void* data = nullptr;
    napi_value result = nullptr;
    napi_get_undefined( env_in, &result );
    if ( napi_unwrap( env_in, self, &data ) != napi_ok || data == nullptr ) return result;
    closeKeyboardState( static_cast<KeyboardHolder*>( data )->state );
    return result;
}

napi_value keyboardIsClosed( napi_env env_in, napi_callback_info info_in )
{
    napi_value self = nullptr;
    if ( napi_get_cb_info( env_in, info_in, nullptr, nullptr, &self, nullptr ) != napi_ok )
    {
        return nullptr;
    }
    void* data = nullptr;
    if ( napi_unwrap( env_in, self, &data ) != napi_ok || data == nullptr )
    {
        return boolValue( env_in, true );
    }
    return boolValue( env_in, static_cast<KeyboardHolder*>( data )->state->closed );
}

napi_value createKeyboard( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) KeyboardHolder();
    if ( holder == nullptr ) return nullptr;
    try
    {
        holder->state = std::make_shared<KeyboardState>();
        holder->state->keyboard = std::make_unique<wse::iui::Keyboard>();
    }
    catch ( ... ) { delete holder; return nullptr; }

    napi_value object = nullptr;
    napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "accessState", nullptr, keyboardAccessState, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "isAvailable", nullptr, keyboardIsAvailable, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "snapshot", nullptr, keyboardSnapshot, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "pressedAscii", nullptr, keyboardPressedAscii, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "isClosed", nullptr, keyboardIsClosed, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "close", nullptr, keyboardClose, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object,
             sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeKeyboard, nullptr, nullptr ) != napi_ok )
    {
        delete holder;
        return nullptr;
    }
    return object;
}
#endif // WSE_HAS_IUI

} // namespace

napi_status registerIuiAddon( napi_env env_in, napi_value exports_in )
{
#ifdef WSE_HAS_IUI
    if ( !setProperty( env_in, exports_in, "KeyboardAccessState", enumValue( env_in, {
            { "Starting", 0U }, { "Ready", 1U }, { "Unavailable", 2U },
            { "PermissionDenied", 3U }, { "Disconnected", 4U },
        } ) )
         || !setProperty( env_in, exports_in, "KeyboardGroupSize", enumValue( env_in, {
            { "ascii", static_cast<std::uint32_t>( wse::iui::ASCII_NUM ) },
            { "function", static_cast<std::uint32_t>( wse::iui::FANCTION_NUM ) },
            { "arrow", static_cast<std::uint32_t>( wse::iui::ARROW_NUM ) },
            { "lock", static_cast<std::uint32_t>( wse::iui::LOCK_NUM ) },
            { "command", static_cast<std::uint32_t>( wse::iui::COMMAND_NUM ) },
        } ) ) ) return napi_generic_failure;
    const napi_property_descriptor properties[] = {
        { "_createKeyboard", nullptr, createKeyboard, nullptr, nullptr, nullptr,
            napi_default, nullptr },
    };
    return napi_define_properties( env_in, exports_in,
        sizeof( properties ) / sizeof( properties[ 0 ] ), properties );
#else
    (void) env_in; (void) exports_in;
    return napi_ok;
#endif
}
