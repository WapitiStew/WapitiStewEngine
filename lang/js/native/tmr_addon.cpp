//*****************************************************************************************************************
//!
//! @file    tmr_addon.cpp
//! @brief   \~japanese Tmr Camera面のNode-API addon. Tmr無しのBuildでは登録関数が何もせず成功する.
//! @brief   \~english  Node-API addon for the Tmr camera surface; in a build without Tmr the registration
//!                     function succeeds without registering anything.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#define NAPI_VERSION 8
#include "component_addons.h"

#include <wse/binding/stew.h>
#ifdef WSE_HAS_TMR
#include <tmr/camera/CameraFrameOps.h>
#include <tmr/stew.h>
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_includes.inc"
#endif

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace
{

napi_value stringValue( napi_env env_in, const std::string& value_in )
{
    napi_value result = nullptr;
    napi_create_string_utf8( env_in, value_in.c_str(), value_in.size(), &result );
    return result;
}

bool setProperty(
      napi_env env_in, napi_value object_in, const char* name_in, napi_value value_in )
{
    return value_in != nullptr
        && napi_set_named_property( env_in, object_in, name_in, value_in ) == napi_ok;
}

napi_value uint32Value( napi_env env_in, const std::uint32_t value_in )
{
    napi_value value = nullptr;
    napi_create_uint32( env_in, value_in, &value );
    return value;
}

napi_value int64Value( napi_env env_in, const std::int64_t value_in )
{
    napi_value value = nullptr;
    napi_create_int64( env_in, value_in, &value );
    return value;
}

napi_value doubleValue( napi_env env_in, const double value_in )
{
    napi_value value = nullptr;
    napi_create_double( env_in, value_in, &value );
    return value;
}

napi_value boolValue( napi_env env_in, const bool value_in )
{
    napi_value value = nullptr;
    napi_get_boolean( env_in, value_in, &value );
    return value;
}

napi_value nullValue( napi_env env_in )
{
    napi_value value = nullptr;
    napi_get_null( env_in, &value );
    return value;
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
    napi_value message = stringValue( env_in, error_in.message() );
    if ( napi_create_error( env_in, nullptr, message, &result ) != napi_ok ) return nullptr;
    setProperty( env_in, result, "category",
        uint32Value( env_in, static_cast<std::uint32_t>( error_in.category() ) ) );
    setProperty( env_in, result, "code", int64Value( env_in, error_in.code() ) );
    setProperty( env_in, result, "nativeCode", int64Value( env_in, error_in.nativeCode() ) );
    return result;
}

void throwError( napi_env env_in, const wse::binding::Error& error_in )
{
    napi_value error = errorValue( env_in, error_in );
    if ( error != nullptr ) napi_throw( env_in, error );
}

wse::binding::Error unsupportedError()
{
    return wse::binding::Error(
        wse::binding::eErrorCategory::Unsupported, 1,
        "WSE was built without Tmr." );
}

#ifdef WSE_HAS_TMR

wse::binding::Error cameraError( const wse::tmr::CameraError& error_in )
{
    return wse::binding::fromTmrError( error_in );
}

bool namedValue(
      napi_value* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in )
{
    napi_value& value_out = *p_value_out;

    return napi_get_named_property( env_in, object_in, name_in, &value_out ) == napi_ok;
}

bool hasNamedValue( napi_env env_in, napi_value object_in, const char* name_in )
{
    bool result = false;
    return napi_has_named_property( env_in, object_in, name_in, &result ) == napi_ok && result;
}

bool namedUint32(
      std::uint32_t* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in )
{
    std::uint32_t& value_out = *p_value_out;

    napi_value value = nullptr;
    return namedValue( &value, env_in, object_in, name_in )
        && napi_get_value_uint32( env_in, value, &value_out ) == napi_ok;
}

bool namedInt64(
      std::int64_t* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in )
{
    std::int64_t& value_out = *p_value_out;

    napi_value value = nullptr;
    return namedValue( &value, env_in, object_in, name_in )
        && napi_get_value_int64( env_in, value, &value_out ) == napi_ok;
}

bool namedString(
      std::string* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in )
{
    std::string& value_out = *p_value_out;

    napi_value value = nullptr;
    std::size_t size = 0U;
    if ( !namedValue( &value, env_in, object_in, name_in )
         || napi_get_value_string_utf8( env_in, value, nullptr, 0U, &size ) != napi_ok )
    {
        return false;
    }
    std::vector<char> buffer( size + 1U, '\0' );
    if ( napi_get_value_string_utf8(
             env_in, value, buffer.data(), buffer.size(), &size ) != napi_ok )
    {
        return false;
    }
    value_out.assign( buffer.data(), size );
    return true;
}

bool namedBool(
      bool* const p_value_out
    , napi_env env_in, napi_value object_in, const char* name_in )
{
    bool& value_out = *p_value_out;

    napi_value value = nullptr;
    return namedValue( &value, env_in, object_in, name_in )
        && napi_get_value_bool( env_in, value, &value_out ) == napi_ok;
}

bool readDevice(
      wse::tmr::sCameraDeviceInfo* const p_device_out
    , napi_env env_in, napi_value value_in )
{
    wse::tmr::sCameraDeviceInfo& device_out = *p_device_out;

    std::uint32_t backend = 0U;
    const bool basic = namedUint32( &backend, env_in, value_in, "backend" )
        && namedString( &device_out.id, env_in, value_in, "id" )
        && namedString( &device_out.display_name, env_in, value_in, "displayName" )
        && namedString( &device_out.transport, env_in, value_in, "transport" )
        && ( device_out.backend = static_cast<wse::tmr::eCameraBackend>( backend ), true );
    if ( !basic ) return false;
    if ( hasNamedValue( env_in, value_in, "transportType" ) )
    {
        std::uint32_t transport = 0U;
        if ( !namedUint32( &transport, env_in, value_in, "transportType" ) ) return false;
        device_out.transport_type = static_cast<wse::tmr::eCameraTransport>( transport );
    }
    if ( hasNamedValue( env_in, value_in, "usb" ) )
    {
        napi_value usb = nullptr;
        std::uint32_t vendor = 0U, product = 0U, version = 0U;
        if ( !namedValue( &usb, env_in, value_in, "usb" )
             || !namedUint32( &vendor, env_in, usb, "vendorId" )
             || !namedUint32( &product, env_in, usb, "productId" )
             || !namedString( &device_out.usb.serial_number, env_in, usb, "serialNumber" )
             || !namedUint32( &version, env_in, usb, "uvcVersionBcd" ) ) return false;
        device_out.usb.vendor_id = static_cast<std::uint16_t>( vendor );
        device_out.usb.product_id = static_cast<std::uint16_t>( product );
        device_out.usb.uvc_version_bcd = static_cast<std::uint16_t>( version );
    }
    return true;
}

bool readFormat(
      wse::tmr::sCameraFormat* const p_format_out
    , napi_env env_in, napi_value value_in )
{
    wse::tmr::sCameraFormat& format_out = *p_format_out;

    std::uint32_t pixel_format = 0U;
    return namedUint32( &format_out.width, env_in, value_in, "width" )
        && namedUint32( &format_out.height, env_in, value_in, "height" )
        && namedUint32( &format_out.frame_rate_numerator, env_in, value_in, "frameRateNumerator" )
        && namedUint32( &format_out.frame_rate_denominator, env_in, value_in, "frameRateDenominator" )
        && namedUint32( &pixel_format, env_in, value_in, "pixelFormat" )
        && ( format_out.pixel_format = static_cast<wse::tmr::eCameraPixelFormat>( pixel_format ), true );
}

bool readExtensionSelector(
      wse::tmr::sCameraExtensionUnitSelector* const p_selector_out
    , napi_env env_in, napi_value value_in )
{
    wse::tmr::sCameraExtensionUnitSelector& selector_out = *p_selector_out;

    napi_value guid = nullptr;
    void* guid_data = nullptr;
    std::size_t guid_size = 0U;
    std::uint32_t unit_id = 0U, selector = 0U;
    std::int64_t minimum = 0, maximum = 0;
    if ( !namedValue( &guid, env_in, value_in, "unitGuid" )
         || napi_get_buffer_info( env_in, guid, &guid_data, &guid_size ) != napi_ok
         || guid_size != selector_out.unit_guid.size()
         || !namedUint32( &unit_id, env_in, value_in, "unitId" )
         || !namedUint32( &selector, env_in, value_in, "selector" )
         || !namedInt64( &minimum, env_in, value_in, "minimumSize" )
         || !namedInt64( &maximum, env_in, value_in, "maximumSize" )
         || !namedBool( &selector_out.readable, env_in, value_in, "readable" )
         || !namedBool( &selector_out.writable, env_in, value_in, "writable" )
         || !namedString( &selector_out.display_name, env_in, value_in, "displayName" ) ) return false;
    std::memcpy( selector_out.unit_guid.data(), guid_data, guid_size );
    selector_out.unit_id = static_cast<std::uint8_t>( unit_id );
    selector_out.selector = static_cast<std::uint8_t>( selector );
    selector_out.minimum_size = static_cast<std::size_t>( minimum );
    selector_out.maximum_size = static_cast<std::size_t>( maximum );
    return minimum >= 0 && maximum >= 0;
}

napi_value deviceValue( napi_env env_in, const wse::tmr::sCameraDeviceInfo& device_in )
{
    napi_value value = nullptr, usb = nullptr;
    napi_create_object( env_in, &value );
    napi_create_object( env_in, &usb );
    setProperty( env_in, value, "backend", uint32Value( env_in, static_cast<std::uint32_t>( device_in.backend ) ) );
    setProperty( env_in, value, "id", stringValue( env_in, device_in.id ) );
    setProperty( env_in, value, "displayName", stringValue( env_in, device_in.display_name ) );
    setProperty( env_in, value, "transport", stringValue( env_in, device_in.transport ) );
    setProperty( env_in, value, "transportType", uint32Value( env_in,
        static_cast<std::uint32_t>( device_in.transport_type ) ) );
    setProperty( env_in, usb, "vendorId", uint32Value( env_in, device_in.usb.vendor_id ) );
    setProperty( env_in, usb, "productId", uint32Value( env_in, device_in.usb.product_id ) );
    setProperty( env_in, usb, "serialNumber", stringValue( env_in, device_in.usb.serial_number ) );
    setProperty( env_in, usb, "uvcVersionBcd", uint32Value( env_in, device_in.usb.uvc_version_bcd ) );
    setProperty( env_in, usb, "available", boolValue( env_in, device_in.usb.available() ) );
    setProperty( env_in, value, "usb", usb );
    return value;
}

napi_value formatValue( napi_env env_in, const wse::tmr::sCameraFormat& format_in )
{
    napi_value value = nullptr;
    napi_create_object( env_in, &value );
    setProperty( env_in, value, "width", uint32Value( env_in, format_in.width ) );
    setProperty( env_in, value, "height", uint32Value( env_in, format_in.height ) );
    setProperty( env_in, value, "frameRateNumerator", uint32Value( env_in, format_in.frame_rate_numerator ) );
    setProperty( env_in, value, "frameRateDenominator", uint32Value( env_in, format_in.frame_rate_denominator ) );
    setProperty( env_in, value, "pixelFormat", uint32Value( env_in, static_cast<std::uint32_t>( format_in.pixel_format ) ) );
    setProperty( env_in, value, "framesPerSecond", doubleValue( env_in, format_in.framesPerSecond() ) );
    return value;
}

napi_value controlCapabilityValue(
      napi_env env_in, const wse::tmr::sCameraControlCapability& control_in )
{
    napi_value value = nullptr;
    napi_create_object( env_in, &value );
    setProperty( env_in, value, "control", uint32Value( env_in, static_cast<std::uint32_t>( control_in.control ) ) );
    setProperty( env_in, value, "minimum", int64Value( env_in, control_in.minimum ) );
    setProperty( env_in, value, "maximum", int64Value( env_in, control_in.maximum ) );
    setProperty( env_in, value, "step", int64Value( env_in, control_in.step ) );
    setProperty( env_in, value, "defaultValue", int64Value( env_in, control_in.default_value ) );
    setProperty( env_in, value, "supportsManual", boolValue( env_in, control_in.supports_manual ) );
    setProperty( env_in, value, "supportsAutomatic", boolValue( env_in, control_in.supports_automatic ) );
    setProperty( env_in, value, "unit", uint32Value( env_in, static_cast<std::uint32_t>( control_in.unit ) ) );
    setProperty( env_in, value, "physicalScale", doubleValue( env_in, control_in.physical_scale ) );
    setProperty( env_in, value, "readable", boolValue( env_in, control_in.readable ) );
    setProperty( env_in, value, "writable", boolValue( env_in, control_in.writable ) );
    setProperty( env_in, value, "displayName", stringValue( env_in, control_in.display_name ) );
    return value;
}

napi_value streamProfileValue( napi_env env_in, const wse::tmr::sCameraStreamProfile& profile_in )
{
    napi_value value = nullptr, outputs = nullptr;
    napi_create_object( env_in, &value );
    napi_create_array_with_length( env_in, profile_in.output_formats.size(), &outputs );
    for ( std::size_t index = 0U; index < profile_in.output_formats.size(); ++index )
        napi_set_element( env_in, outputs, static_cast<std::uint32_t>( index ),
            uint32Value( env_in, static_cast<std::uint32_t>( profile_in.output_formats[index] ) ) );
    setProperty( env_in, value, "nativeFormat", formatValue( env_in, profile_in.native_format ) );
    setProperty( env_in, value, "outputFormats", outputs );
    return value;
}

napi_value extensionSelectorValue( napi_env env_in,
    const wse::tmr::sCameraExtensionUnitSelector& selector_in )
{
    napi_value value = nullptr, guid = nullptr;
    napi_create_object( env_in, &value );
    napi_create_buffer_copy( env_in, selector_in.unit_guid.size(), selector_in.unit_guid.data(), nullptr, &guid );
    setProperty( env_in, value, "unitGuid", guid );
    setProperty( env_in, value, "unitId", uint32Value( env_in, selector_in.unit_id ) );
    setProperty( env_in, value, "selector", uint32Value( env_in, selector_in.selector ) );
    setProperty( env_in, value, "minimumSize", int64Value( env_in, static_cast<std::int64_t>( selector_in.minimum_size ) ) );
    setProperty( env_in, value, "maximumSize", int64Value( env_in, static_cast<std::int64_t>( selector_in.maximum_size ) ) );
    setProperty( env_in, value, "readable", boolValue( env_in, selector_in.readable ) );
    setProperty( env_in, value, "writable", boolValue( env_in, selector_in.writable ) );
    setProperty( env_in, value, "displayName", stringValue( env_in, selector_in.display_name ) );
    return value;
}

napi_value capabilityValue( napi_env env_in, const wse::tmr::sCameraCapability& capability_in )
{
    napi_value value = nullptr, formats = nullptr, controls = nullptr,
        profiles = nullptr, extensions = nullptr;
    napi_create_object( env_in, &value );
    napi_create_array_with_length( env_in, capability_in.formats.size(), &formats );
    napi_create_array_with_length( env_in, capability_in.controls.size(), &controls );
    napi_create_array_with_length( env_in, capability_in.stream_profiles.size(), &profiles );
    napi_create_array_with_length( env_in, capability_in.extension_units.size(), &extensions );
    for ( std::size_t index = 0U; index < capability_in.formats.size(); ++index )
        napi_set_element( env_in, formats, static_cast<std::uint32_t>( index ), formatValue( env_in, capability_in.formats[index] ) );
    for ( std::size_t index = 0U; index < capability_in.controls.size(); ++index )
        napi_set_element( env_in, controls, static_cast<std::uint32_t>( index ), controlCapabilityValue( env_in, capability_in.controls[index] ) );
    for ( std::size_t index = 0U; index < capability_in.stream_profiles.size(); ++index )
        napi_set_element( env_in, profiles, static_cast<std::uint32_t>( index ), streamProfileValue( env_in, capability_in.stream_profiles[index] ) );
    for ( std::size_t index = 0U; index < capability_in.extension_units.size(); ++index )
        napi_set_element( env_in, extensions, static_cast<std::uint32_t>( index ), extensionSelectorValue( env_in, capability_in.extension_units[index] ) );
    setProperty( env_in, value, "device", deviceValue( env_in, capability_in.device ) );
    setProperty( env_in, value, "formats", formats );
    setProperty( env_in, value, "controls", controls );
    setProperty( env_in, value, "streamProfiles", profiles );
    setProperty( env_in, value, "extensionUnits", extensions );
    return value;
}

napi_value frameValue( napi_env env_in, wse::tmr::sCameraFrame frame_in )
{
    napi_value value = nullptr;
    napi_value data = nullptr;
    napi_create_object( env_in, &value );
    napi_create_buffer_copy( env_in, frame_in.data.size(),
        frame_in.data.empty() ? nullptr : frame_in.data.data(), nullptr, &data );
    setProperty( env_in, value, "width", uint32Value( env_in, frame_in.description.width ) );
    setProperty( env_in, value, "height", uint32Value( env_in, frame_in.description.height ) );
    setProperty( env_in, value, "pixelFormat", uint32Value( env_in, static_cast<std::uint32_t>( frame_in.description.pixel_format ) ) );
    setProperty( env_in, value, "rowStride", int64Value( env_in, static_cast<std::int64_t>( frame_in.description.row_stride ) ) );
    setProperty( env_in, value, "sequence", int64Value( env_in, static_cast<std::int64_t>( frame_in.sequence ) ) );
    setProperty( env_in, value, "monotonicTimestampNs", int64Value( env_in, frame_in.monotonic_timestamp_ns ) );
    setProperty( env_in, value, "data", data );
    return value;
}

//! \~japanese JS側のFrame ObjectをCameraのFrameへ読み取る.
//! \~english  Reads a JavaScript frame object into a camera frame.
bool readFrame(
      wse::tmr::sCameraFrame* const p_frame_out
    , napi_env env_in, napi_value value_in )
{
    wse::tmr::sCameraFrame& frame_out = *p_frame_out;

    napi_value data = nullptr;
    void* bytes = nullptr;
    std::size_t size = 0U;
    std::uint32_t pixel_format = 0U;
    std::int64_t row_stride = 0;
    if ( !namedUint32( &frame_out.description.width, env_in, value_in, "width" )
         || !namedUint32( &frame_out.description.height, env_in, value_in, "height" )
         || !namedUint32( &pixel_format, env_in, value_in, "pixelFormat" )
         || !namedValue( &data, env_in, value_in, "data" )
         || napi_get_buffer_info( env_in, data, &bytes, &size ) != napi_ok ) return false;
    frame_out.description.pixel_format =
        static_cast<wse::tmr::eCameraPixelFormat>( pixel_format );
    // A caller who does not say what the row stride is means the rows are packed.
    frame_out.description.row_stride =
        hasNamedValue( env_in, value_in, "rowStride" )
            && namedInt64( &row_stride, env_in, value_in, "rowStride" ) && row_stride > 0
        ? static_cast<std::size_t>( row_stride )
        : frame_out.description.minimumRowStride();
    if ( hasNamedValue( env_in, value_in, "sequence" ) )
    {
        std::int64_t sequence = 0;
        if ( namedInt64( &sequence, env_in, value_in, "sequence" ) && sequence >= 0 )
            frame_out.sequence = static_cast<std::uint64_t>( sequence );
    }
    if ( hasNamedValue( env_in, value_in, "monotonicTimestampNs" ) )
    {
        (void) namedInt64( &frame_out.monotonic_timestamp_ns,
            env_in, value_in, "monotonicTimestampNs" );
    }
    frame_out.data.assign(
        static_cast<const std::uint8_t*>( bytes ),
        static_cast<const std::uint8_t*>( bytes ) + size );
    return true;
}

struct CameraState final
{
    std::mutex mutex;
    std::unique_ptr<wse::tmr::WebCamera> camera;
    napi_threadsafe_function callback;
    //! \~japanese Finalizerのみが立てる終端状態. `close()`はここを変えない.
    //! \~english  Terminal state raised only by the finalizer; `close()` never sets it.
    bool released;

    //! @brief Construct all members with explicit defaults.
    CameraState()
        : mutex    ()
        , camera   ( std::make_unique<wse::tmr::WebCamera>() )
        , callback ( nullptr )
        , released ( false )
    {
    }
};

struct CameraHolder final
{
    std::shared_ptr<CameraState> state;
};

struct CameraCallbackData final
{
    bool succeeded;
    wse::tmr::sCameraFrame frame;
    wse::binding::Error error;

    //! @brief Construct all members with explicit defaults.
    CameraCallbackData(
          bool succeeded_in = false
        , const wse::tmr::sCameraFrame& frame_in = {}
        , const wse::binding::Error& error_in = {}
    )
        : succeeded ( succeeded_in )
        , frame     ( frame_in )
        , error     ( error_in )
    {
    }
};

struct CameraReadOperation final
{
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    std::shared_ptr<CameraState> state;
    std::uint32_t timeout_ms;
    wse::tmr::sCameraFrame frame;
    wse::binding::Error error;

    //! @brief Construct all members with explicit defaults.
    CameraReadOperation(
          const napi_env& env_in = nullptr
        , const napi_async_work& work_in = nullptr
        , const napi_deferred& deferred_in = nullptr
        , const std::shared_ptr<CameraState>& state_in = {}
        , std::uint32_t timeout_ms_in = 0U
        , const wse::tmr::sCameraFrame& frame_in = {}
        , const wse::binding::Error& error_in = {}
    )
        : env        ( env_in )
        , work       ( work_in )
        , deferred   ( deferred_in )
        , state      ( state_in )
        , timeout_ms ( timeout_ms_in )
        , frame      ( frame_in )
        , error      ( error_in )
    {
    }
};

CameraHolder* cameraHolder(
      std::size_t* p_argument_count_inout, napi_value* p_arguments_out
    , napi_env env_in, napi_callback_info info_in )
{
    napi_value receiver = nullptr;
    void* data = nullptr;
    std::size_t count = p_argument_count_inout == nullptr ? 0U : *p_argument_count_inout;
    if ( napi_get_cb_info( env_in, info_in, &count, p_arguments_out, &receiver, &data ) != napi_ok )
        return nullptr;
    if ( p_argument_count_inout != nullptr ) *p_argument_count_inout = count;
    CameraHolder* holder = nullptr;
    return napi_unwrap( env_in, receiver, reinterpret_cast<void**>( &holder ) ) == napi_ok ? holder : nullptr;
}

void callCameraCallback( napi_env env_in, napi_value callback_in, void*, void* data_in )
{
    std::unique_ptr<CameraCallbackData> data( static_cast<CameraCallbackData*>( data_in ) );
    if ( env_in == nullptr || callback_in == nullptr ) return;
    napi_value receiver = nullptr;
    napi_value ignored = nullptr;
    napi_get_undefined( env_in, &receiver );
    napi_value arguments[ 2 ] = { nullValue( env_in ), nullValue( env_in ) };
    if ( data->succeeded ) arguments[ 1 ] = frameValue( env_in, std::move( data->frame ) );
    else arguments[ 0 ] = errorValue( env_in, data->error );
    napi_call_function( env_in, receiver, callback_in, 2U, arguments, &ignored );
}

//! \~japanese Streamingとdeviceを閉じるが、後続の`open()`のためにObjectは使える状態で残す.
//! \~english  Ends the session but leaves the object usable, so a later `open()` works.
//!
//! The threadsafe callback is released the same graceful way `stop()` releases it, and the
//! pointer is cleared so a later `start(callback)` creates a fresh one.
void closeCameraSession( const std::shared_ptr<CameraState>& state_in )
{
    if ( !state_in ) return;
    std::lock_guard<std::mutex> lock( state_in->mutex );
    if ( state_in->released ) return;
    state_in->camera->stop();
    state_in->camera->close();
    if ( state_in->callback != nullptr )
    {
        napi_release_threadsafe_function( state_in->callback, napi_tsfn_release );
        state_in->callback = nullptr;
    }
}

//! \~japanese Finalizerの終端経路. 以後この状態は再利用できない.
//! \~english  The finalizer's terminal path; the state is never usable again.
void releaseCameraState( const std::shared_ptr<CameraState>& state_in )
{
    if ( !state_in ) return;
    std::lock_guard<std::mutex> lock( state_in->mutex );
    if ( state_in->released ) return;
    state_in->camera->stop();
    state_in->camera->close();
    state_in->released = true;
    if ( state_in->callback != nullptr )
    {
        napi_release_threadsafe_function( state_in->callback, napi_tsfn_abort );
        state_in->callback = nullptr;
    }
}

void finalizeCamera( napi_env, void* data_in, void* )
{
    auto* holder = static_cast<CameraHolder*>( data_in );
    releaseCameraState( holder->state );
    delete holder;
}

napi_value cameraDevices( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[ 1 ] = {};
    napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr );
    std::uint32_t backend = 0U;
    if ( count != 0U && napi_get_value_uint32( env_in, arguments[ 0 ], &backend ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "backend must be an integer." );
        return nullptr;
    }
    const auto result = wse::tmr::WebCamera::enumerate(
        static_cast<wse::tmr::eCameraBackend>( backend ) );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    napi_value array = nullptr;
    napi_create_array_with_length( env_in, result.value().size(), &array );
    for ( std::size_t index = 0U; index < result.value().size(); ++index )
        napi_set_element( env_in, array, static_cast<std::uint32_t>( index ), deviceValue( env_in, result.value()[ index ] ) );
    return array;
}

// ----------------------------------------------------------------------------------------------
// Frame operations
// ----------------------------------------------------------------------------------------------

//! \~japanese Pixel Formatを1つ受け取り真偽を返す3つの述語をまとめる.
//! \~english  Shares the shape of the three predicates that take one pixel format.
template< bool ( *_Predicate )( wse::tmr::eCameraPixelFormat ) noexcept >
napi_value pixelFormatPredicate( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[ 1 ] = {};
    std::uint32_t format = 0U;
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 1U
         || napi_get_value_uint32( env_in, arguments[ 0 ], &format ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
            "A pixel format must be given as an integer." );
        return nullptr;
    }
    return boolValue( env_in,
        _Predicate( static_cast<wse::tmr::eCameraPixelFormat>( format ) ) );
}

napi_value cameraBayerPatternOf( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[ 1 ] = {};
    std::uint32_t format = 0U;
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 1U
         || napi_get_value_uint32( env_in, arguments[ 0 ], &format ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
            "A pixel format must be given as an integer." );
        return nullptr;
    }
    wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
    if ( !wse::tmr::bayerPatternOf(
             &pattern, static_cast<wse::tmr::eCameraPixelFormat>( format ) ) )
    {
        throwError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument, 1
            , "This pixel format carries no Bayer layout." ) );
        return nullptr;
    }
    return uint32Value( env_in, static_cast<std::uint32_t>( pattern ) );
}

napi_value cameraApplyOrientation( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 2U;
    napi_value arguments[ 2 ] = {};
    wse::tmr::sCameraFrame frame;
    std::uint32_t orientation = 0U;
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 2U || !readFrame( &frame, env_in, arguments[ 0 ] )
         || napi_get_value_uint32( env_in, arguments[ 1 ], &orientation ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
            "applyOrientation requires a frame and an orientation." );
        return nullptr;
    }
    auto result = wse::tmr::applyOrientation(
        frame, static_cast<wse::eImageOrientation>( orientation ) );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return frameValue( env_in, std::move( result.value() ) );
}

napi_value cameraDemosaicFrame( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 3U;
    napi_value arguments[ 3 ] = {};
    wse::tmr::sCameraFrame frame;
    std::uint32_t output_format = 0U;
    std::uint32_t method = static_cast<std::uint32_t>( wse::eDemosaicMethod::Bilinear );
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count < 2U || !readFrame( &frame, env_in, arguments[ 0 ] )
         || napi_get_value_uint32( env_in, arguments[ 1 ], &output_format ) != napi_ok
         || ( count >= 3U
              && napi_get_value_uint32( env_in, arguments[ 2 ], &method ) != napi_ok ) )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
            "demosaicFrame requires a frame and a result format." );
        return nullptr;
    }
    auto result = wse::tmr::demosaicFrame( frame,
        static_cast<wse::tmr::eCameraPixelFormat>( output_format ),
        static_cast<wse::eDemosaicMethod>( method ) );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return frameValue( env_in, std::move( result.value() ) );
}

//! \~japanese 累積器は状態を持つため、Frameの配列を受け取り平均を返す形にする.
//! \~english  The accumulator holds state, so it is offered as a call that averages an array.
napi_value cameraAverageFrames( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[ 1 ] = {};
    std::uint32_t length = 0U;
    bool is_array = false;
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 1U
         || napi_is_array( env_in, arguments[ 0 ], &is_array ) != napi_ok || !is_array
         || napi_get_array_length( env_in, arguments[ 0 ], &length ) != napi_ok )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
            "averageFrames requires an array of frames." );
        return nullptr;
    }

    wse::tmr::CameraFrameAccumulator accumulator;
    for ( std::uint32_t index = 0U; index < length; ++index )
    {
        napi_value element = nullptr;
        wse::tmr::sCameraFrame frame;
        if ( napi_get_element( env_in, arguments[ 0 ], index, &element ) != napi_ok
             || !readFrame( &frame, env_in, element ) )
        {
            napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
                "Every element of the array must be a frame." );
            return nullptr;
        }
        const auto status = accumulator.add( frame );
        if ( !status.succeeded() )
        {
            throwError( env_in, cameraError( status.error() ) );
            return nullptr;
        }
    }
    auto result = accumulator.average();
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return frameValue( env_in, std::move( result.value() ) );
}

napi_value cameraCapabilities( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U;
    napi_value arguments[ 1 ] = {};
    wse::tmr::sCameraDeviceInfo device;
    if ( napi_get_cb_info( env_in, info_in, &count, arguments, nullptr, nullptr ) != napi_ok
         || count != 1U || !readDevice( &device, env_in, arguments[ 0 ] ) )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "cameraCapabilities requires a CameraDevice." );
        return nullptr;
    }
    const auto result = wse::tmr::WebCamera::capabilities( device );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return capabilityValue( env_in, result.value() );
}

napi_value cameraOpen( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 2U; napi_value arguments[ 2 ] = {};
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    wse::tmr::sCameraDeviceInfo device;
    if ( holder == nullptr || count == 0U || !readDevice( &device, env_in, arguments[ 0 ] ) )
    {
        napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "open requires a CameraDevice." ); return nullptr;
    }
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    // `close()` ends the session only; reopening is the supported way to change the frame size.
    // Only the finalizer's release is terminal.
    if ( holder->state->released )
    {
        throwError( env_in, wse::binding::Error(
            wse::binding::eErrorCategory::InvalidState, 1, "WebCamera has been released." ) );
        return nullptr;
    }
    auto status = wse::tmr::CameraStatus::success();
    if ( count == 1U ) status = holder->state->camera->open( device );
    else
    {
        wse::tmr::sCameraStreamConfiguration configuration;
        napi_value native_format = nullptr;
        std::uint32_t output_format = 0U;
        bool allow_conversion = true;
        napi_value allow_value = nullptr;
        if ( !namedValue( &native_format, env_in, arguments[ 1 ], "nativeFormat" )
             || !readFormat( &configuration.native_format, env_in, native_format )
             || !namedUint32( &output_format, env_in, arguments[ 1 ], "outputFormat" ) )
        {
            napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT",
                "configuration requires nativeFormat and outputFormat." ); return nullptr;
        }
        if ( namedValue( &allow_value, env_in, arguments[ 1 ], "allowConversion" ) )
            napi_get_value_bool( env_in, allow_value, &allow_conversion );
        configuration.output_format = static_cast<wse::tmr::eCameraPixelFormat>( output_format );
        configuration.allow_conversion = allow_conversion;
        status = holder->state->camera->open( device, configuration );
    }
    if ( !status.succeeded() ) { throwError( env_in, cameraError( status.error() ) ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value cameraStart( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[ 1 ] = {};
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    auto status = wse::tmr::CameraStatus::success();
    if ( count == 0U ) status = holder->state->camera->start();
    else
    {
        napi_valuetype type = napi_undefined;
        if ( napi_typeof( env_in, arguments[ 0 ], &type ) != napi_ok || type != napi_function )
        { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "start callback must be a function." ); return nullptr; }
        // A previous session may have left one behind; a threadsafe function is per-start.
        if ( holder->state->callback != nullptr )
        { napi_release_threadsafe_function( holder->state->callback, napi_tsfn_release ); holder->state->callback = nullptr; }
        napi_value name = stringValue( env_in, "wse.camera.callback" );
        if ( napi_create_threadsafe_function( env_in, arguments[ 0 ], nullptr, name, 4U, 1U,
                nullptr, nullptr, nullptr, callCameraCallback, &holder->state->callback ) != napi_ok )
        { napi_throw_error( env_in, "WSE_CAMERA_CALLBACK", "Unable to create camera callback." ); return nullptr; }
        const auto state = holder->state;
        status = state->camera->start( [state]( const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& result_in )
        {
            auto* data = new ( std::nothrow ) CameraCallbackData();
            if ( data == nullptr ) return;
            data->succeeded = result_in.succeeded();
            if ( data->succeeded ) data->frame = result_in.value();
            else data->error = cameraError( result_in.error() );
            if ( napi_call_threadsafe_function( state->callback, data, napi_tsfn_nonblocking ) != napi_ok ) delete data;
        } );
    }
    if ( !status.succeeded() )
    {
        if ( holder->state->callback != nullptr )
        { napi_release_threadsafe_function( holder->state->callback, napi_tsfn_abort ); holder->state->callback = nullptr; }
        throwError( env_in, cameraError( status.error() ) ); return nullptr;
    }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value cameraStop( napi_env env_in, napi_callback_info info_in )
{
    CameraHolder* holder = cameraHolder( nullptr, nullptr, env_in, info_in ); if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto status = holder->state->camera->stop();
    if ( holder->state->callback != nullptr )
    { napi_release_threadsafe_function( holder->state->callback, napi_tsfn_release ); holder->state->callback = nullptr; }
    if ( !status.succeeded() ) { throwError( env_in, cameraError( status.error() ) ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

void executeCameraRead( napi_env, void* data_in )
{
    auto& operation = *static_cast<CameraReadOperation*>( data_in );
    std::lock_guard<std::mutex> lock( operation.state->mutex );
    if ( operation.state->released )
    { operation.error = wse::binding::Error( wse::binding::eErrorCategory::InvalidState, 1, "WebCamera has been released." ); return; }
    auto result = operation.state->camera->readFrame( operation.timeout_ms );
    if ( result.succeeded() ) operation.frame = std::move( result.value() );
    else operation.error = cameraError( result.error() );
}

void completeCameraRead( napi_env env_in, napi_status status_in, void* data_in )
{
    auto* operation = static_cast<CameraReadOperation*>( data_in );
    if ( status_in != napi_ok && operation->error.ok() )
        operation->error = wse::binding::Error( wse::binding::eErrorCategory::Cancellation, 1, "Camera read was cancelled." );
    if ( operation->error.ok() ) napi_resolve_deferred( env_in, operation->deferred, frameValue( env_in, std::move( operation->frame ) ) );
    else napi_reject_deferred( env_in, operation->deferred, errorValue( env_in, operation->error ) );
    napi_delete_async_work( env_in, operation->work ); delete operation;
}

napi_value cameraReadFrame( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[ 1 ] = {};
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    std::uint32_t timeout = 0U;
    if ( holder == nullptr || count != 1U || napi_get_value_uint32( env_in, arguments[ 0 ], &timeout ) != napi_ok )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "readFrame requires a timeout." ); return nullptr; }
    auto* operation = new ( std::nothrow ) CameraReadOperation();
    if ( operation == nullptr ) { napi_throw_error( env_in, "WSE_CAMERA_MEMORY", "Unable to allocate camera read." ); return nullptr; }
    operation->env = env_in; operation->state = holder->state; operation->timeout_ms = timeout;
    napi_value promise = nullptr, name = stringValue( env_in, "wse.camera.read" );
    if ( napi_create_promise( env_in, &operation->deferred, &promise ) != napi_ok
         || napi_create_async_work( env_in, nullptr, name, executeCameraRead, completeCameraRead,
                operation, &operation->work ) != napi_ok
         || napi_queue_async_work( env_in, operation->work ) != napi_ok )
    { if ( operation->work ) napi_delete_async_work( env_in, operation->work ); delete operation; napi_throw_error( env_in, "WSE_CAMERA_QUEUE", "Unable to queue camera read." ); return nullptr; }
    return promise;
}

napi_value cameraGetControl( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[ 1 ] = {}; std::uint32_t control = 0U;
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    if ( holder == nullptr || count != 1U || napi_get_value_uint32( env_in, arguments[ 0 ], &control ) != napi_ok )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "getControl requires a control integer." ); return nullptr; }
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto result = holder->state->camera->getControl( static_cast<wse::tmr::eCameraControl>( control ) );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    napi_value value = nullptr; napi_create_object( env_in, &value );
    setProperty( env_in, value, "control", uint32Value( env_in, static_cast<std::uint32_t>( result.value().control ) ) );
    setProperty( env_in, value, "mode", uint32Value( env_in, static_cast<std::uint32_t>( result.value().mode ) ) );
    setProperty( env_in, value, "value", int64Value( env_in, result.value().value ) ); return value;
}

napi_value cameraSetControl( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[ 1 ] = {}; std::uint32_t control = 0U, mode = 0U; std::int64_t value_number = 0;
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    if ( holder == nullptr || count != 1U || !namedUint32( &control, env_in, arguments[ 0 ], "control" )
         || !namedUint32( &mode, env_in, arguments[ 0 ], "mode" ) || !namedInt64( &value_number, env_in, arguments[ 0 ], "value" ) )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "setControl requires control, mode, and value." ); return nullptr; }
    wse::tmr::sCameraControlValue value; value.control = static_cast<wse::tmr::eCameraControl>( control );
    value.mode = static_cast<wse::tmr::eCameraControlMode>( mode ); value.value = value_number;
    std::lock_guard<std::mutex> lock( holder->state->mutex ); const auto status = holder->state->camera->setControl( value );
    if ( !status.succeeded() ) { throwError( env_in, cameraError( status.error() ) ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value cameraCurrentCapabilities( napi_env env_in, napi_callback_info info_in )
{
    CameraHolder* holder = cameraHolder( nullptr, nullptr, env_in, info_in ); if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto result = holder->state->camera->currentCapabilities();
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return capabilityValue( env_in, result.value() );
}

napi_value cameraControlCapability( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[1] = {}; std::uint32_t control = 0U;
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    if ( holder == nullptr || count != 1U
         || napi_get_value_uint32( env_in, arguments[0], &control ) != napi_ok )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "controlCapability requires a control integer." ); return nullptr; }
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto result = holder->state->camera->controlCapability(
        static_cast<wse::tmr::eCameraControl>( control ) );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    return controlCapabilityValue( env_in, result.value() );
}

napi_value cameraGetExtensionUnit( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[1] = {};
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    wse::tmr::sCameraExtensionUnitSelector selector;
    if ( holder == nullptr || count != 1U
         || !readExtensionSelector( &selector, env_in, arguments[0] ) )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "getExtensionUnit requires a valid selector." ); return nullptr; }
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto result = holder->state->camera->getExtensionUnit( selector );
    if ( !result.succeeded() ) { throwError( env_in, cameraError( result.error() ) ); return nullptr; }
    napi_value value = nullptr, payload = nullptr;
    napi_create_object( env_in, &value );
    napi_create_buffer_copy( env_in, result.value().payload.size(),
        result.value().payload.empty() ? nullptr : result.value().payload.data(), nullptr, &payload );
    setProperty( env_in, value, "selector", extensionSelectorValue( env_in, result.value().selector ) );
    setProperty( env_in, value, "payload", payload );
    return value;
}

napi_value cameraSetExtensionUnit( napi_env env_in, napi_callback_info info_in )
{
    std::size_t count = 1U; napi_value arguments[1] = {};
    CameraHolder* holder = cameraHolder( &count, arguments, env_in, info_in );
    napi_value selector_value = nullptr, payload_value = nullptr;
    wse::tmr::sCameraExtensionUnitValue value;
    void* payload = nullptr; std::size_t payload_size = 0U;
    if ( holder == nullptr || count != 1U
         || !namedValue( &selector_value, env_in, arguments[0], "selector" )
         || !readExtensionSelector( &value.selector, env_in, selector_value )
         || !namedValue( &payload_value, env_in, arguments[0], "payload" )
         || napi_get_buffer_info( env_in, payload_value, &payload, &payload_size ) != napi_ok )
    { napi_throw_type_error( env_in, "WSE_CAMERA_ARGUMENT", "setExtensionUnit requires selector and Buffer payload." ); return nullptr; }
    const auto* bytes = static_cast<const std::uint8_t*>( payload );
    if ( payload_size != 0U ) value.payload.assign( bytes, bytes + payload_size );
    std::lock_guard<std::mutex> lock( holder->state->mutex );
    const auto status = holder->state->camera->setExtensionUnit( value );
    if ( !status.succeeded() ) { throwError( env_in, cameraError( status.error() ) ); return nullptr; }
    napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value cameraIsOpen( napi_env env_in, napi_callback_info info_in )
{
    CameraHolder* holder = cameraHolder( nullptr, nullptr, env_in, info_in ); if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex ); return boolValue( env_in, holder->state->camera->isOpen() );
}
napi_value cameraIsStreaming( napi_env env_in, napi_callback_info info_in )
{
    CameraHolder* holder = cameraHolder( nullptr, nullptr, env_in, info_in ); if ( holder == nullptr ) return nullptr;
    std::lock_guard<std::mutex> lock( holder->state->mutex ); return boolValue( env_in, holder->state->camera->isStreaming() );
}
napi_value cameraClose( napi_env env_in, napi_callback_info info_in )
{
    CameraHolder* holder = cameraHolder( nullptr, nullptr, env_in, info_in ); if ( holder == nullptr ) return nullptr;
    closeCameraSession( holder->state ); napi_value result = nullptr; napi_get_undefined( env_in, &result ); return result;
}

napi_value createWebCamera( napi_env env_in, napi_callback_info )
{
    auto* holder = new ( std::nothrow ) CameraHolder();
    if ( holder == nullptr ) return nullptr;
    try { holder->state = std::make_shared<CameraState>(); }
    catch ( ... ) { delete holder; return nullptr; }
    napi_value object = nullptr; napi_create_object( env_in, &object );
    const napi_property_descriptor methods[] = {
        { "open", nullptr, cameraOpen, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "start", nullptr, cameraStart, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stop", nullptr, cameraStop, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "readFrame", nullptr, cameraReadFrame, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "currentCapabilities", nullptr, cameraCurrentCapabilities, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "controlCapability", nullptr, cameraControlCapability, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getControl", nullptr, cameraGetControl, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setControl", nullptr, cameraSetControl, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getExtensionUnit", nullptr, cameraGetExtensionUnit, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setExtensionUnit", nullptr, cameraSetExtensionUnit, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "isOpen", nullptr, cameraIsOpen, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "isStreaming", nullptr, cameraIsStreaming, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "close", nullptr, cameraClose, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    if ( napi_define_properties( env_in, object, sizeof( methods ) / sizeof( methods[ 0 ] ), methods ) != napi_ok
         || napi_wrap( env_in, object, holder, finalizeCamera, nullptr, nullptr ) != napi_ok )
    { delete holder; return nullptr; }
    return object;
}
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_node_body.inc"
#endif
#endif // WSE_HAS_TMR

} // namespace

napi_status registerTmrAddon( napi_env env_in, napi_value exports_in )
{
#ifdef WSE_HAS_TMR
#ifdef WSE_EXTENSION_TMR_BINDING
    const auto extension_status = registerExtensionTmrAddon(env_in, exports_in);
    if (extension_status != napi_ok) return extension_status;
#endif
    if ( !setProperty( env_in, exports_in, "CameraBackend", enumValue( env_in, {
            { "Automatic", 0U }, { "MediaFoundation", 1U },
            { "Video4Linux2", 2U }, { "Libcamera", 3U },
        } ) )
         || !setProperty( env_in, exports_in, "CameraPixelFormat", enumValue( env_in, {
            { "Unknown", 0U }, { "Gray8", 1U }, { "Rgb8", 2U },
            { "Bgr8", 3U }, { "Bgra8", 4U }, { "Yuyv422", 5U },
            { "Nv12", 6U }, { "Mjpeg", 7U }, { "Gray16", 8U },
            { "Rgb16", 9U }, { "Bgr16", 10U },
            { "Bayer16Rggb", 11U }, { "Bayer16Bggr", 12U },
            { "Bayer16Grbg", 13U }, { "Bayer16Gbrg", 14U },
            { "Uyvy422", 15U },
        } ) )
         || !setProperty( env_in, exports_in, "ImageOrientation", enumValue( env_in, {
            { "None", 0U }, { "Rotate90Cw", 1U }, { "Rotate180", 2U },
            { "Rotate90Ccw", 3U }, { "FlipHorizontal", 4U }, { "FlipVertical", 5U },
        } ) )
         || !setProperty( env_in, exports_in, "BayerPattern", enumValue( env_in, {
            { "Rggb", 0U }, { "Bggr", 1U }, { "Grbg", 2U }, { "Gbrg", 3U },
        } ) )
         || !setProperty( env_in, exports_in, "DemosaicMethod", enumValue( env_in, {
            { "Block2x2", 0U }, { "Bilinear", 1U },
        } ) )
         || !setProperty( env_in, exports_in, "CameraTransport", enumValue( env_in, {
            { "Unknown", 0U }, { "UsbUvc", 1U }, { "Csi", 2U },
            { "Virtual", 3U }, { "Network", 4U },
        } ) )
         || !setProperty( env_in, exports_in, "CameraControl", enumValue( env_in, {
            { "Exposure", 0U }, { "Gain", 1U }, { "Focus", 2U },
            { "Brightness", 3U }, { "Contrast", 4U },
            { "Saturation", 5U }, { "WhiteBalance", 6U }, { "Zoom", 7U },
            { "Iris", 8U }, { "Hue", 9U }, { "Sharpness", 10U },
            { "Gamma", 11U }, { "ColorEnable", 12U },
            { "BacklightCompensation", 13U }, { "Pan", 14U },
            { "Tilt", 15U }, { "Roll", 16U }, { "PowerLineFrequency", 17U },
            { "FrameRate", 18U },
        } ) )
         || !setProperty( env_in, exports_in, "CameraControlMode", enumValue( env_in, {
            { "Manual", 0U }, { "Automatic", 1U },
        } ) )
         || !setProperty( env_in, exports_in, "CameraControlUnit", enumValue( env_in, {
            { "DeviceNative", 0U }, { "Microseconds", 1U },
            { "Kelvin", 2U }, { "Diopters", 3U },
            { "GainMultiplier", 4U }, { "Relative", 5U },
            { "Degrees", 6U }, { "Hertz", 7U }, { "Boolean", 8U },
        } ) ) ) return napi_generic_failure;
    const napi_property_descriptor properties[] = {
        { "_webCameraDevices", nullptr, cameraDevices, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_webCameraCapabilities", nullptr, cameraCapabilities, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_createWebCamera", nullptr, createWebCamera, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_isFrameOperationSupported", nullptr, pixelFormatPredicate< wse::tmr::isFrameOperationSupported >, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_isFrameAveragingSupported", nullptr, pixelFormatPredicate< wse::tmr::isFrameAveragingSupported >, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_isBayerFormat", nullptr, pixelFormatPredicate< wse::tmr::isBayerFormat >, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_bayerPatternOf", nullptr, cameraBayerPatternOf, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_applyOrientation", nullptr, cameraApplyOrientation, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_demosaicFrame", nullptr, cameraDemosaicFrame, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "_averageFrames", nullptr, cameraAverageFrames, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    return napi_define_properties( env_in, exports_in,
        sizeof( properties ) / sizeof( properties[ 0 ] ), properties );
#else
    (void) env_in; (void) exports_in;
    return napi_ok;
#endif
}
