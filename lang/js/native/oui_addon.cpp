//*****************************************************************************************************************
//!
//! @file    oui_addon.cpp
//! @brief   \~japanese OUI Projection面のNode-API addon. OUI無しのBuildでは登録関数が何もせず成功する.
//! @brief   \~english  Node-API addon for the OUI projection surface; in a build without OUI the registration
//!                     function succeeds without registering anything.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#define NAPI_VERSION 8
#include "component_addons.h"

#include <wse/binding/stew.h>
#ifdef WSE_HAS_OUI
#include <oui/stew.h>
#endif

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

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

napi_value errorValue( napi_env env_in, const wse::binding::Error& error_in )
{
    napi_value result = nullptr;
    if ( napi_create_error( env_in, nullptr,
             stringValue( env_in, error_in.message() ), &result ) != napi_ok ) return nullptr;
    setProperty( env_in, result, "category",
        uint32Value( env_in, static_cast<std::uint32_t>( error_in.category() ) ) );
    setProperty( env_in, result, "code", int64Value( env_in, error_in.code() ) );
    setProperty( env_in, result, "nativeCode", int64Value( env_in, error_in.nativeCode() ) );
    return result;
}

#ifdef WSE_HAS_OUI
wse::binding::Error rendererError( const wse::oui::RendererError& error_in )
{
    return wse::binding::fromOuiError( error_in );
}

bool namedValue(
      napi_value* const p_value_out
    , napi_env env_in, napi_value object_in
    , const char* name_in, const bool required_in = true )
{
    napi_value& value_out = *p_value_out;

    bool present = false;
    if ( napi_has_named_property( env_in, object_in, name_in, &present ) != napi_ok ) return false;
    if ( !present ) return !required_in;
    return napi_get_named_property( env_in, object_in, name_in, &value_out ) == napi_ok;
}

bool undefinedValue( napi_env env_in, napi_value value_in )
{
    if ( value_in == nullptr ) return true;
    napi_valuetype type = napi_undefined;
    return napi_typeof( env_in, value_in, &type ) == napi_ok && type == napi_undefined;
}

bool namedUint32(
      std::uint32_t* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in
    , const bool required_in = true )
{
    std::uint32_t& value_out = *p_value_out;

    napi_value value = nullptr;
    if ( !namedValue( &value, env_in, object_in, name_in, required_in ) ) return false;
    return undefinedValue( env_in, value )
        || napi_get_value_uint32( env_in, value, &value_out ) == napi_ok;
}

bool namedDouble(
      double* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in
    , const bool required_in = true )
{
    double& value_out = *p_value_out;

    napi_value value = nullptr;
    if ( !namedValue( &value, env_in, object_in, name_in, required_in ) ) return false;
    return undefinedValue( env_in, value )
        || napi_get_value_double( env_in, value, &value_out ) == napi_ok;
}

bool namedBool(
      bool* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in
    , const bool required_in = true )
{
    bool& value_out = *p_value_out;

    napi_value value = nullptr;
    if ( !namedValue( &value, env_in, object_in, name_in, required_in ) ) return false;
    return undefinedValue( env_in, value )
        || napi_get_value_bool( env_in, value, &value_out ) == napi_ok;
}

bool namedString(
      std::string* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in
    , const bool required_in = true )
{
    std::string& value_out = *p_value_out;

    napi_value value = nullptr;
    if ( !namedValue( &value, env_in, object_in, name_in, required_in ) ) return false;
    if ( undefinedValue( env_in, value ) ) return true;
    std::size_t size = 0U;
    if ( napi_get_value_string_utf8( env_in, value, nullptr, 0U, &size ) != napi_ok ) return false;
    std::vector<char> buffer( size + 1U, '\0' );
    if ( napi_get_value_string_utf8(
             env_in, value, buffer.data(), buffer.size(), &size ) != napi_ok ) return false;
    value_out.assign( buffer.data(), size );
    return true;
}

bool copyBuffer(
      std::vector<std::uint8_t>* const p_output_inout
    , napi_env env_in, napi_value value_in )
{
    std::vector<std::uint8_t>& output_inout = *p_output_inout;

    bool is_buffer = false;
    if ( value_in == nullptr || napi_is_buffer( env_in, value_in, &is_buffer ) != napi_ok
         || !is_buffer ) return false;
    void* bytes = nullptr;
    std::size_t size = 0U;
    if ( napi_get_buffer_info( env_in, value_in, &bytes, &size ) != napi_ok ) return false;
    const auto* begin = static_cast<const std::uint8_t*>( bytes );
    output_inout.assign( begin, begin + size );
    return true;
}

bool readVertex(
      wse::oui::sRendererVertex2D* const p_vertex_out
    , napi_env env_in, napi_value value_in )
{
    wse::oui::sRendererVertex2D& vertex_out = *p_vertex_out;

    double x = 0.0, y = 0.0, u = 0.0, v = 0.0;
    if ( !namedDouble( &x, env_in, value_in, "x" )
         || !namedDouble( &y, env_in, value_in, "y" )
         || !namedDouble( &u, env_in, value_in, "u" )
         || !namedDouble( &v, env_in, value_in, "v" ) ) return false;
    vertex_out = { static_cast<float>( x ), static_cast<float>( y ),
        static_cast<float>( u ), static_cast<float>( v ) };
    return true;
}

bool readLayer(
      wse::oui::sProjectionImageLayer* const p_layer_out
    , napi_env env_in, napi_value value_in )
{
    wse::oui::sProjectionImageLayer& layer_out = *p_layer_out;

    napi_value rgba = nullptr, alpha = nullptr, vertices = nullptr, indices = nullptr;
    if ( !namedUint32( &layer_out.width, env_in, value_in, "width" )
         || !namedUint32( &layer_out.height, env_in, value_in, "height" )
         || !namedValue( &rgba, env_in, value_in, "rgba" )
         || !copyBuffer( &layer_out.rgba, env_in, rgba )
         || !namedValue( &vertices, env_in, value_in, "vertices" )
         || !namedValue( &indices, env_in, value_in, "indices" ) ) return false;
    if ( !namedValue( &alpha, env_in, value_in, "alpha", false ) ) return false;
    if ( alpha != nullptr )
    {
        napi_valuetype type = napi_undefined;
        if ( napi_typeof( env_in, alpha, &type ) != napi_ok ) return false;
        if ( type != napi_undefined && !copyBuffer( &layer_out.alpha, env_in, alpha ) ) return false;
    }
    bool is_array = false;
    std::uint32_t length = 0U;
    if ( napi_is_array( env_in, vertices, &is_array ) != napi_ok || !is_array
         || napi_get_array_length( env_in, vertices, &length ) != napi_ok ) return false;
    layer_out.vertices.reserve( length );
    for ( std::uint32_t index = 0U; index < length; ++index )
    {
        napi_value element = nullptr;
        wse::oui::sRendererVertex2D vertex;
        if ( napi_get_element( env_in, vertices, index, &element ) != napi_ok
             || !readVertex( &vertex, env_in, element ) ) return false;
        layer_out.vertices.emplace_back( vertex );
    }
    if ( napi_is_array( env_in, indices, &is_array ) != napi_ok || !is_array
         || napi_get_array_length( env_in, indices, &length ) != napi_ok ) return false;
    layer_out.indices.reserve( length );
    for ( std::uint32_t index = 0U; index < length; ++index )
    {
        napi_value element = nullptr;
        std::uint32_t value = 0U;
        if ( napi_get_element( env_in, indices, index, &element ) != napi_ok
             || napi_get_value_uint32( env_in, element, &value ) != napi_ok ) return false;
        layer_out.indices.emplace_back( value );
    }
    std::uint32_t filter = static_cast<std::uint32_t>( layer_out.sampling_filter );
    double opacity = layer_out.opacity;
    if ( !namedUint32( &filter, env_in, value_in, "samplingFilter", false )
         || !namedDouble( &opacity, env_in, value_in, "opacity", false ) ) return false;
    layer_out.sampling_filter = static_cast<wse::oui::eTextureSamplingFilter>( filter );
    layer_out.opacity = static_cast<float>( opacity );
    napi_value edge = nullptr;
    if ( !namedValue( &edge, env_in, value_in, "edgeBlend", false ) ) return false;
    if ( !undefinedValue( env_in, edge ) )
    {
        double left = 0.0, right = 0.0, top = 0.0, bottom = 0.0;
        std::uint32_t curve = 0U;
        if ( !namedDouble( &left, env_in, edge, "left", false )
             || !namedDouble( &right, env_in, edge, "right", false )
             || !namedDouble( &top, env_in, edge, "top", false )
             || !namedDouble( &bottom, env_in, edge, "bottom", false )
             || !namedUint32( &curve, env_in, edge, "curve", false ) ) return false;
        layer_out.edge_blend = { static_cast<float>( left ), static_cast<float>( right ),
            static_cast<float>( top ), static_cast<float>( bottom ),
            static_cast<wse::oui::eEdgeBlendCurve>( curve ) };
    }
    return true;
}

bool readRequest(
      wse::oui::sProjectionRenderRequest* const p_request_out
    , napi_env env_in, napi_value value_in )
{
    wse::oui::sProjectionRenderRequest& request_out = *p_request_out;

    if ( !namedUint32( &request_out.output_width, env_in, value_in, "outputWidth" )
         || !namedUint32( &request_out.output_height, env_in, value_in, "outputHeight" )
         || !namedUint32( &request_out.supersample_scale, env_in, value_in, "supersampleScale", false )
         || !namedUint32( &request_out.timeout_ms, env_in, value_in, "timeoutMilliseconds", false )
         || !namedBool( &request_out.use_software_adapter, env_in, value_in, "useSoftwareAdapter", false )
         || !namedBool( &request_out.enable_validation, env_in, value_in, "enableValidation", false )
         || !namedString( &request_out.adapter_name, env_in, value_in, "adapterName", false ) ) return false;
    std::uint32_t backend = static_cast<std::uint32_t>( request_out.backend );
    if ( !namedUint32( &backend, env_in, value_in, "backend", false ) ) return false;
    request_out.backend = static_cast<wse::oui::eRendererBackend>( backend );
    napi_value clear = nullptr;
    if ( !namedValue( &clear, env_in, value_in, "clearColor", false ) ) return false;
    if ( !undefinedValue( env_in, clear ) )
    {
        double red = 0.0, green = 0.0, blue = 0.0, alpha = 1.0;
        if ( !namedDouble( &red, env_in, clear, "red", false )
             || !namedDouble( &green, env_in, clear, "green", false )
             || !namedDouble( &blue, env_in, clear, "blue", false )
             || !namedDouble( &alpha, env_in, clear, "alpha", false ) ) return false;
        request_out.clear_color = { static_cast<float>( red ), static_cast<float>( green ),
            static_cast<float>( blue ), static_cast<float>( alpha ) };
    }
    napi_value layers = nullptr;
    bool is_array = false;
    if ( !namedValue( &layers, env_in, value_in, "layers" )
         || napi_is_array( env_in, layers, &is_array ) != napi_ok || !is_array ) return false;
    std::uint32_t length = 0U;
    if ( napi_get_array_length( env_in, layers, &length ) != napi_ok ) return false;
    request_out.layers.reserve( length );
    for ( std::uint32_t index = 0U; index < length; ++index )
    {
        napi_value element = nullptr;
        wse::oui::sProjectionImageLayer layer;
        if ( napi_get_element( env_in, layers, index, &element ) != napi_ok
             || !readLayer( &layer, env_in, element ) ) return false;
        request_out.layers.emplace_back( std::move( layer ) );
    }
    return true;
}

struct ProjectionOperation final
{
    napi_async_work work;
    napi_deferred deferred;
    wse::oui::sProjectionRenderRequest request;
    wse::oui::sProjectionRenderFrame frame;
    wse::binding::Error error;

    //! @brief Construct all members with explicit defaults.
    ProjectionOperation(
          const napi_async_work& work_in = nullptr
        , const napi_deferred& deferred_in = nullptr
        , const wse::oui::sProjectionRenderRequest& request_in = {}
        , const wse::oui::sProjectionRenderFrame& frame_in = {}
        , const wse::binding::Error& error_in = {}
    )
        : work     ( work_in )
        , deferred ( deferred_in )
        , request  ( request_in )
        , frame    ( frame_in )
        , error    ( error_in )
    {
    }
};

void executeProjection( napi_env, void* data_in )
{
    auto& operation = *static_cast<ProjectionOperation*>( data_in );
    auto result = wse::oui::renderProjection( operation.request );
    if ( result.succeeded() ) operation.frame = std::move( result.value() );
    else operation.error = rendererError( result.error() );
}

void completeProjection( napi_env env_in, napi_status status_in, void* data_in )
{
    std::unique_ptr<ProjectionOperation> operation(
        static_cast<ProjectionOperation*>( data_in ) );
    if ( status_in != napi_ok && operation->error.ok() )
        operation->error = wse::binding::Error(
            wse::binding::eErrorCategory::Cancellation, 1, "Projection was cancelled." );
    if ( operation->error.ok() )
    {
        napi_value frame = nullptr, data = nullptr;
        napi_create_object( env_in, &frame );
        const auto& bytes = operation->frame.data.bytes();
        napi_create_buffer_copy( env_in, bytes.size(),
            bytes.empty() ? nullptr : bytes.data(), nullptr, &data );
        setProperty( env_in, frame, "width", uint32Value( env_in, operation->frame.width ) );
        setProperty( env_in, frame, "height", uint32Value( env_in, operation->frame.height ) );
        setProperty( env_in, frame, "rowPitch", int64Value( env_in,
            static_cast<std::int64_t>( operation->frame.row_pitch ) ) );
        setProperty( env_in, frame, "adapterName",
            stringValue( env_in, operation->frame.adapter_name ) );
        setProperty( env_in, frame, "data", data );
        napi_resolve_deferred( env_in, operation->deferred, frame );
    }
    else napi_reject_deferred( env_in, operation->deferred,
        errorValue( env_in, operation->error ) );
    napi_delete_async_work( env_in, operation->work );
}

napi_value renderProjection( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[1] = {};
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 1U )
    {
        napi_throw_type_error( env_in, "WSE_PROJECTION_ARGUMENT",
            "renderProjection requires one request object." );
        return nullptr;
    }
    auto* operation = new ( std::nothrow ) ProjectionOperation();
    if ( operation == nullptr )
    {
        napi_throw_error( env_in, "WSE_PROJECTION_MEMORY",
            "Unable to allocate Projection work." );
        return nullptr;
    }
    if ( !readRequest( &operation->request, env_in, arguments[0] ) )
    {
        delete operation;
        napi_throw_type_error( env_in, "WSE_PROJECTION_ARGUMENT",
            "Projection request contains an invalid field." );
        return nullptr;
    }
    napi_value promise = nullptr;
    napi_value name = stringValue( env_in, "wse.projection.render" );
    if ( napi_create_promise( env_in, &operation->deferred, &promise ) != napi_ok
         || napi_create_async_work( env_in, nullptr, name, executeProjection,
                completeProjection, operation, &operation->work ) != napi_ok
         || napi_queue_async_work( env_in, operation->work ) != napi_ok )
    {
        if ( operation->work != nullptr ) napi_delete_async_work( env_in, operation->work );
        delete operation;
        napi_throw_error( env_in, "WSE_PROJECTION_QUEUE",
            "Unable to queue Projection work." );
        return nullptr;
    }
    return promise;
}
#endif // WSE_HAS_OUI

} // namespace

napi_status registerOuiAddon( napi_env env_in, napi_value exports_in )
{
#ifdef WSE_HAS_OUI
    if ( !setProperty( env_in, exports_in, "RendererBackend", enumValue( env_in, {
            { "Automatic", 0U }, { "Direct3D12", 1U }, { "Vulkan12", 2U },
        } ) )
         || !setProperty( env_in, exports_in, "TextureSamplingFilter", enumValue( env_in, {
            { "Nearest", 0U }, { "Linear", 1U },
        } ) )
         || !setProperty( env_in, exports_in, "EdgeBlendCurve", enumValue( env_in, {
            { "Linear", 0U }, { "Smoothstep", 1U },
        } ) ) ) return napi_generic_failure;
    const napi_property_descriptor properties[] = {
        { "renderProjection", nullptr, renderProjection, nullptr, nullptr, nullptr,
            napi_default, nullptr },
    };
    return napi_define_properties( env_in, exports_in,
        sizeof( properties ) / sizeof( properties[0] ), properties );
#else
    (void) env_in;
    (void) exports_in;
    return napi_ok;
#endif
}
