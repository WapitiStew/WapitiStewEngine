//*****************************************************************************************************************
//!
//! @file    wse_capi_tmr.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Tmr WebCameraの平坦C ABI実装.
//! @brief   \~english  Implementation of the flat C ABI for the Tmr WebCamera.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <wse/capi/wse_capi_tmr.h>

#include "capi_internal.h"
#include "camera_callback.h"

#include <wse/binding/TmrErrorAdapter.h>
#include <tmr/camera/CameraFrameOps.h>
#include <tmr/stew.h>
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_includes.inc"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

//! \~japanese 不透明Tmr Handleの実体.
//! \~english  Concrete bodies behind the opaque Tmr handles.
struct wse_capi_camera_device_list_t final
{
    std::vector<wse::tmr::sCameraDeviceInfo> devices;

    explicit wse_capi_camera_device_list_t(
        std::vector<wse::tmr::sCameraDeviceInfo> devices_in )
        : devices ( std::move( devices_in ) )
    {
    }
};

struct wse_capi_camera_device_t final
{
    wse::tmr::sCameraDeviceInfo device;

    explicit wse_capi_camera_device_t( wse::tmr::sCameraDeviceInfo device_in )
        : device ( std::move( device_in ) )
    {
    }
};

struct wse_capi_camera_capability_t final
{
    wse::tmr::sCameraCapability capability;

    explicit wse_capi_camera_capability_t( wse::tmr::sCameraCapability capability_in )
        : capability ( std::move( capability_in ) )
    {
    }
};

struct wse_capi_camera_frame_t final
{
    wse::tmr::sCameraFrame frame;

    explicit wse_capi_camera_frame_t( wse::tmr::sCameraFrame frame_in )
        : frame ( std::move( frame_in ) )
    {
    }
};

struct wse_capi_camera_xu_selector_t final
{
    wse::tmr::sCameraExtensionUnitSelector selector;

    explicit wse_capi_camera_xu_selector_t(
        wse::tmr::sCameraExtensionUnitSelector selector_in )
        : selector ( std::move( selector_in ) )
    {
    }
};

struct wse_capi_camera_t final
{
    wse::tmr::WebCamera camera;
};

struct wse_capi_camera_frame_accumulator_t final
{
    wse::tmr::CameraFrameAccumulator accumulator;
};

namespace
{

//! \~japanese TmrのStructured ErrorをC ABIのStatusへ写す.
//! \~english  Maps a Tmr structured error onto a C ABI status.
wse_capi_status fromCameraError( const wse::tmr::CameraError& error_in )
{
    return wse::capi::fromBindingError( wse::binding::fromTmrError( error_in ) );
}

//! \~japanese Statusの成否をC ABIのStatusへ写す. 成功時はerror()へ触れない.
//! \~english  Maps a status outcome onto a C ABI status without touching error() on success.
wse_capi_status fromCameraStatus( const wse::tmr::CameraStatus& status_in )
{
    return status_in.succeeded()
        ? wse::capi::makeSuccess()
        : fromCameraError( status_in.error() );
}

// The C ABI carries an enumerator as the integer it is, so a value outside the enumeration would
// name nothing. Each of these turns an unknown integer into the value the C++ side refuses, which
// is what makes an out-of-range argument a structured error rather than undefined behaviour.
wse::tmr::eCameraPixelFormat toCameraPixelFormat( const std::int32_t value_in )
{
    return value_in >= 0
        && value_in <= static_cast<std::int32_t>( wse::tmr::eCameraPixelFormat::Uyvy422 )
        ? static_cast<wse::tmr::eCameraPixelFormat>( value_in )
        : wse::tmr::eCameraPixelFormat::Unknown;
}

wse::eImageOrientation toImageOrientation( const std::int32_t value_in )
{
    return value_in >= 0
        && value_in <= static_cast<std::int32_t>( wse::eImageOrientation::FlipVertical )
        ? static_cast<wse::eImageOrientation>( value_in )
        : wse::eImageOrientation::None;
}

wse::eDemosaicMethod toDemosaicMethod( const std::int32_t value_in )
{
    return value_in == static_cast<std::int32_t>( wse::eDemosaicMethod::Block2x2 )
        ? wse::eDemosaicMethod::Block2x2
        : wse::eDemosaicMethod::Bilinear;
}

//! \~japanese Byte列を2回呼出方式で複製する.
//! \~english  Copies bytes using the two-call pattern.
wse_capi_status copyBytes(
      std::uint8_t*                    p_buffer_out
    , std::size_t*                     p_size_out
    , const std::vector<std::uint8_t>& source_in
    , const std::size_t                capacity_in )
{
    if ( p_size_out == nullptr )
    {
        return wse::capi::makeInvalidArgument( "The output size pointer must not be null." );
    }
    *p_size_out = source_in.size();
    if ( p_buffer_out == nullptr )
    {
        return wse::capi::makeSuccess();
    }
    if ( capacity_in < source_in.size() )
    {
        return wse::capi::makeStatus(
            WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, "Destination buffer is too small." );
    }
    if ( !source_in.empty() )
    {
        std::memcpy( p_buffer_out, source_in.data(), source_in.size() );
    }
    return wse::capi::makeSuccess();
}

void writeFormat(
      wse_capi_camera_format* const  p_format_out
    , const wse::tmr::sCameraFormat& source_in )
{
    wse_capi_camera_format& format_out = *p_format_out;

    format_out.width = source_in.width;
    format_out.height = source_in.height;
    format_out.frame_rate_numerator = source_in.frame_rate_numerator;
    format_out.frame_rate_denominator = source_in.frame_rate_denominator;
    format_out.pixel_format = static_cast<std::int32_t>( source_in.pixel_format );
}

void writeControlCapability(
      wse_capi_camera_control_capability* const  p_capability_out
    , const wse::tmr::sCameraControlCapability&  source_in )
{
    wse_capi_camera_control_capability& capability_out = *p_capability_out;

    capability_out.control = static_cast<std::int32_t>( source_in.control );
    capability_out.minimum = source_in.minimum;
    capability_out.maximum = source_in.maximum;
    capability_out.step = source_in.step;
    capability_out.default_value = source_in.default_value;
    capability_out.supports_manual = source_in.supports_manual ? 1 : 0;
    capability_out.supports_automatic = source_in.supports_automatic ? 1 : 0;
    capability_out.unit = static_cast<std::int32_t>( source_in.unit );
    capability_out.physical_scale = source_in.physical_scale;
    capability_out.readable = source_in.readable ? 1 : 0;
    capability_out.writable = source_in.writable ? 1 : 0;
}

} // namespace

extern "C"
{

// ----------------------------------------------------------------------------------------------
// Enumeration and device description
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_camera_enumerate(
      wse_capi_camera_device_list* p_list_out
    , int32_t                      backend_in )
{
    return wse::capi::guard( [backend_in, p_list_out]() -> wse_capi_status
    {
        if ( p_list_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_list_out must not be null." );
        }
        auto result = wse::tmr::WebCamera::enumerate(
            static_cast<wse::tmr::eCameraBackend>( backend_in ) );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_list_out = new wse_capi_camera_device_list_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_device_list_destroy( wse_capi_camera_device_list list_inout )
{
    delete list_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_count(
      wse_capi_camera_device_list list_in
    , size_t*                     p_count_out )
{
    return wse::capi::guard( [list_in, p_count_out]() -> wse_capi_status
    {
        if ( list_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "list_in and p_count_out must not be null." );
        }
        *p_count_out = list_in->devices.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_at(
      wse_capi_camera_device_list list_in
    , wse_capi_camera_device*     p_device_out
    , size_t                      index_in )
{
    return wse::capi::guard( [list_in, index_in, p_device_out]() -> wse_capi_status
    {
        if ( list_in == nullptr || p_device_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "list_in and p_device_out must not be null." );
        }
        if ( index_in >= list_in->devices.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the device range." );
        }
        *p_device_out = new wse_capi_camera_device_t( list_in->devices[ index_in ] );
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_device_destroy( wse_capi_camera_device device_inout )
{
    delete device_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_backend(
      wse_capi_camera_device device_in
    , int32_t*               p_backend_out )
{
    return wse::capi::guard( [device_in, p_backend_out]() -> wse_capi_status
    {
        if ( device_in == nullptr || p_backend_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in and p_backend_out must not be null." );
        }
        *p_backend_out = static_cast<std::int32_t>( device_in->device.backend );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_transport_type(
      wse_capi_camera_device device_in
    , int32_t*               p_transport_out )
{
    return wse::capi::guard( [device_in, p_transport_out]() -> wse_capi_status
    {
        if ( device_in == nullptr || p_transport_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "device_in and p_transport_out must not be null." );
        }
        *p_transport_out = static_cast<std::int32_t>( device_in->device.transport_type );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_id(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in )
{
    return wse::capi::guard( [device_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( device_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in must not be null." );
        }
        return wse::capi::copyString( p_buffer_out, p_size_out, device_in->device.id, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_display_name(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in )
{
    return wse::capi::guard( [device_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( device_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in must not be null." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, device_in->device.display_name, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_transport(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in )
{
    return wse::capi::guard( [device_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( device_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in must not be null." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, device_in->device.transport, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_usb(
      wse_capi_camera_device        device_in
    , wse_capi_camera_usb_identity* p_usb_out )
{
    return wse::capi::guard( [device_in, p_usb_out]() -> wse_capi_status
    {
        if ( device_in == nullptr || p_usb_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in and p_usb_out must not be null." );
        }
        p_usb_out->vendor_id = device_in->device.usb.vendor_id;
        p_usb_out->product_id = device_in->device.usb.product_id;
        p_usb_out->uvc_version_bcd = device_in->device.usb.uvc_version_bcd;
        p_usb_out->available = device_in->device.usb.available() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_usb_serial_number(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in )
{
    return wse::capi::guard( [device_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( device_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "device_in must not be null." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, device_in->device.usb.serial_number, capacity_in );
    } );
}

// ----------------------------------------------------------------------------------------------
// Capability
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capabilities(
      wse_capi_camera_device      device_in
    , wse_capi_camera_capability* p_capability_out )
{
    return wse::capi::guard( [device_in, p_capability_out]() -> wse_capi_status
    {
        if ( device_in == nullptr || p_capability_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "device_in and p_capability_out must not be null." );
        }
        auto result = wse::tmr::WebCamera::capabilities( device_in->device );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_capability_out = new wse_capi_camera_capability_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_capability_destroy(
    wse_capi_camera_capability capability_inout )
{
    delete capability_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_format_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out )
{
    return wse::capi::guard( [capability_in, p_count_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_count_out must not be null." );
        }
        *p_count_out = capability_in->capability.formats.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_format_at(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_format*    p_format_out
    , size_t                     index_in )
{
    return wse::capi::guard( [capability_in, index_in, p_format_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_format_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_format_out must not be null." );
        }
        if ( index_in >= capability_in->capability.formats.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the format range." );
        }
        writeFormat( p_format_out, capability_in->capability.formats[ index_in ] );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out )
{
    return wse::capi::guard( [capability_in, p_count_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_count_out must not be null." );
        }
        *p_count_out = capability_in->capability.stream_profiles.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_native_format(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_format*    p_format_out
    , size_t                     index_in )
{
    return wse::capi::guard( [capability_in, index_in, p_format_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_format_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_format_out must not be null." );
        }
        if ( index_in >= capability_in->capability.stream_profiles.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the profile range." );
        }
        writeFormat(
            p_format_out, capability_in->capability.stream_profiles[ index_in ].native_format );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_output_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out
    , size_t                     index_in )
{
    return wse::capi::guard( [capability_in, index_in, p_count_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_count_out must not be null." );
        }
        if ( index_in >= capability_in->capability.stream_profiles.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the profile range." );
        }
        *p_count_out =
            capability_in->capability.stream_profiles[ index_in ].output_formats.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_output_at(
      wse_capi_camera_capability capability_in
    , int32_t*                   p_pixel_format_out
    , size_t                     profile_index_in
    , size_t                     output_index_in )
{
    return wse::capi::guard(
        [capability_in, profile_index_in, output_index_in, p_pixel_format_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_pixel_format_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_pixel_format_out must not be null." );
        }
        if ( profile_index_in >= capability_in->capability.stream_profiles.size() )
        {
            return wse::capi::makeInvalidArgument(
                "profile_index_in is outside the profile range." );
        }
        const auto& outputs =
            capability_in->capability.stream_profiles[ profile_index_in ].output_formats;
        if ( output_index_in >= outputs.size() )
        {
            return wse::capi::makeInvalidArgument( "output_index_in is outside the output range." );
        }
        *p_pixel_format_out = static_cast<std::int32_t>( outputs[ output_index_in ] );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out )
{
    return wse::capi::guard( [capability_in, p_count_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_count_out must not be null." );
        }
        *p_count_out = capability_in->capability.controls.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_at(
      wse_capi_camera_capability          capability_in
    , wse_capi_camera_control_capability* p_control_out
    , size_t                              index_in )
{
    return wse::capi::guard( [capability_in, index_in, p_control_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_control_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_control_out must not be null." );
        }
        if ( index_in >= capability_in->capability.controls.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the control range." );
        }
        writeControlCapability( p_control_out, capability_in->capability.controls[ index_in ] );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_display_name(
      wse_capi_camera_capability capability_in
    , char*                      p_buffer_out
    , size_t*                    p_size_out
    , size_t                     index_in
    , size_t                     capacity_in )
{
    return wse::capi::guard(
        [capability_in, index_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "capability_in must not be null." );
        }
        if ( index_in >= capability_in->capability.controls.size() )
        {
            return wse::capi::makeInvalidArgument( "index_in is outside the control range." );
        }
        return wse::capi::copyString(
              p_buffer_out
            , p_size_out
            , capability_in->capability.controls[ index_in ].display_name
            , capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out )
{
    return wse::capi::guard( [capability_in, p_count_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_count_out must not be null." );
        }
        *p_count_out = capability_in->capability.extension_units.size();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_at(
      wse_capi_camera_capability   capability_in
    , wse_capi_camera_xu_selector* p_selector_out
    , size_t                       index_in )
{
    return wse::capi::guard( [capability_in, index_in, p_selector_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_selector_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_selector_out must not be null." );
        }
        if ( index_in >= capability_in->capability.extension_units.size() )
        {
            return wse::capi::makeInvalidArgument(
                "index_in is outside the extension-unit range." );
        }
        *p_selector_out = new wse_capi_camera_xu_selector_t(
            capability_in->capability.extension_units[ index_in ] );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_device(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_device*    p_device_out )
{
    return wse::capi::guard( [capability_in, p_device_out]() -> wse_capi_status
    {
        if ( capability_in == nullptr || p_device_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "capability_in and p_device_out must not be null." );
        }
        *p_device_out = new wse_capi_camera_device_t( capability_in->capability.device );
        return wse::capi::makeSuccess();
    } );
}

// ----------------------------------------------------------------------------------------------
// Extension unit
// ----------------------------------------------------------------------------------------------

void WSE_CAPI_CALL wse_capi_camera_xu_selector_destroy(
    wse_capi_camera_xu_selector selector_inout )
{
    delete selector_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_describe(
      wse_capi_camera_xu_selector selector_in
    , uint8_t*                    p_unit_id_out
    , uint8_t*                    p_selector_out
    , size_t*                     p_minimum_size_out
    , size_t*                     p_maximum_size_out
    , wse_capi_bool*              p_readable_out
    , wse_capi_bool*              p_writable_out )
{
    return wse::capi::guard(
        [selector_in, p_unit_id_out, p_selector_out, p_minimum_size_out, p_maximum_size_out,
         p_readable_out, p_writable_out]() -> wse_capi_status
    {
        if ( selector_in == nullptr || p_unit_id_out == nullptr || p_selector_out == nullptr
             || p_minimum_size_out == nullptr || p_maximum_size_out == nullptr
             || p_readable_out == nullptr || p_writable_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "No output pointer may be null." );
        }
        *p_unit_id_out = selector_in->selector.unit_id;
        *p_selector_out = selector_in->selector.selector;
        *p_minimum_size_out = selector_in->selector.minimum_size;
        *p_maximum_size_out = selector_in->selector.maximum_size;
        *p_readable_out = selector_in->selector.readable ? 1 : 0;
        *p_writable_out = selector_in->selector.writable ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_guid(
      wse_capi_camera_xu_selector selector_in
    , uint8_t*                    p_buffer_out
    , size_t                      capacity_in )
{
    return wse::capi::guard( [selector_in, p_buffer_out, capacity_in]() -> wse_capi_status
    {
        if ( selector_in == nullptr || p_buffer_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "selector_in and p_buffer_out must not be null." );
        }
        if ( capacity_in < selector_in->selector.unit_guid.size() )
        {
            return wse::capi::makeStatus(
                WSE_CAPI_ERROR_INVALID_ARGUMENT, 0, "The GUID buffer must hold 16 bytes." );
        }
        std::memcpy(
              p_buffer_out
            , selector_in->selector.unit_guid.data()
            , selector_in->selector.unit_guid.size() );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_display_name(
      wse_capi_camera_xu_selector selector_in
    , char*                       p_buffer_out
    , size_t*                     p_size_out
    , size_t                      capacity_in )
{
    return wse::capi::guard(
        [selector_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( selector_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "selector_in must not be null." );
        }
        return wse::capi::copyString(
            p_buffer_out, p_size_out, selector_in->selector.display_name, capacity_in );
    } );
}

// ----------------------------------------------------------------------------------------------
// Frame
// ----------------------------------------------------------------------------------------------

void WSE_CAPI_CALL wse_capi_camera_frame_destroy( wse_capi_camera_frame frame_inout )
{
    delete frame_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_describe(
      wse_capi_camera_frame              frame_in
    , wse_capi_camera_frame_description* p_description_out
    , uint64_t*                          p_sequence_out
    , int64_t*                           p_monotonic_timestamp_ns_out )
{
    return wse::capi::guard(
        [frame_in, p_description_out, p_sequence_out,
         p_monotonic_timestamp_ns_out]() -> wse_capi_status
    {
        if ( frame_in == nullptr || p_description_out == nullptr
             || p_sequence_out == nullptr || p_monotonic_timestamp_ns_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "No output pointer may be null." );
        }
        p_description_out->width = frame_in->frame.description.width;
        p_description_out->height = frame_in->frame.description.height;
        p_description_out->pixel_format =
            static_cast<std::int32_t>( frame_in->frame.description.pixel_format );
        p_description_out->row_stride = frame_in->frame.description.row_stride;
        *p_sequence_out = frame_in->frame.sequence;
        *p_monotonic_timestamp_ns_out = frame_in->frame.monotonic_timestamp_ns;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_data(
      wse_capi_camera_frame frame_in
    , uint8_t*              p_buffer_out
    , size_t*               p_size_out
    , size_t                capacity_in )
{
    return wse::capi::guard( [frame_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( frame_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "frame_in must not be null." );
        }
        return copyBytes( p_buffer_out, p_size_out, frame_in->frame.data, capacity_in );
    } );
}

// ----------------------------------------------------------------------------------------------
// Camera owner
// ----------------------------------------------------------------------------------------------

wse_capi_status WSE_CAPI_CALL wse_capi_camera_create( wse_capi_camera* p_camera_out )
{
    return wse::capi::guard( [p_camera_out]() -> wse_capi_status
    {
        if ( p_camera_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_camera_out must not be null." );
        }
        *p_camera_out = new wse_capi_camera_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_destroy( wse_capi_camera camera_inout )
{
    if ( camera_inout == nullptr ) return;
    // close() already stops and contains stop failures; destroy must not let an
    // allocating status/exception cross the C ABI or a SafeHandle finalizer.
    camera_inout->camera.close();
    delete camera_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_open(
      wse_capi_camera                             camera_inout
    , wse_capi_camera_device                      device_in
    , const wse_capi_camera_stream_configuration* configuration_in )
{
    return wse::capi::guard( [camera_inout, device_in, configuration_in]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || device_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout and device_in must not be null." );
        }
        if ( configuration_in == nullptr )
        {
            return fromCameraStatus( camera_inout->camera.open( device_in->device ) );
        }

        wse::tmr::sCameraStreamConfiguration native;
        native.native_format.width = configuration_in->native_format.width;
        native.native_format.height = configuration_in->native_format.height;
        native.native_format.frame_rate_numerator =
            configuration_in->native_format.frame_rate_numerator;
        native.native_format.frame_rate_denominator =
            configuration_in->native_format.frame_rate_denominator;
        native.native_format.pixel_format = static_cast<wse::tmr::eCameraPixelFormat>(
            configuration_in->native_format.pixel_format );
        native.output_format =
            static_cast<wse::tmr::eCameraPixelFormat>( configuration_in->output_format );
        native.allow_conversion = configuration_in->allow_conversion != 0;
        return fromCameraStatus( camera_inout->camera.open( device_in->device, native ) );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_start( wse_capi_camera camera_inout )
{
    return wse::capi::guard( [camera_inout]() -> wse_capi_status
    {
        if ( camera_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout must not be null." );
        }
        return fromCameraStatus( camera_inout->camera.start() );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_start_with_callback(
      wse_capi_camera                camera_inout
    , wse_capi_camera_frame_callback callback_in
    , wse_capi_user_data             user_data_in )
{
    return wse::capi::guard( [camera_inout, callback_in, user_data_in]() -> wse_capi_status
    {
        if ( camera_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout must not be null." );
        }
        if ( callback_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "callback_in must not be null." );
        }
        // Ownership of the frame passes to the callback, exactly as it does for a read, so a
        // callback that keeps a frame is holding something it owns rather than a dangling borrow.
        const wse::tmr::CameraFrameCallback bridge =
            [callback_in, user_data_in]( const wse::tmr::CameraResult< wse::tmr::sCameraFrame >& result_in )
        {
            if ( !result_in.succeeded() )
            {
                callback_in( nullptr, fromCameraError( result_in.error() ), user_data_in );
                return;
            }
            wse::capi::deliverCameraFrame<wse_capi_camera_frame_t>(
                result_in.value(), callback_in, user_data_in );
        };
        return fromCameraStatus( camera_inout->camera.start( bridge ) );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_stop( wse_capi_camera camera_inout )
{
    return wse::capi::guard( [camera_inout]() -> wse_capi_status
    {
        if ( camera_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout must not be null." );
        }
        return fromCameraStatus( camera_inout->camera.stop() );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_frame(
      wse_capi_camera        camera_inout
    , wse_capi_camera_frame* p_frame_out
    , uint32_t               timeout_milliseconds_in )
{
    return wse::capi::guard(
        [camera_inout, timeout_milliseconds_in, p_frame_out]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_inout and p_frame_out must not be null." );
        }
        auto result = camera_inout->camera.readFrame( timeout_milliseconds_in );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_frame_out = new wse_capi_camera_frame_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_current_capabilities(
      wse_capi_camera             camera_in
    , wse_capi_camera_capability* p_capability_out )
{
    return wse::capi::guard( [camera_in, p_capability_out]() -> wse_capi_status
    {
        if ( camera_in == nullptr || p_capability_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_in and p_capability_out must not be null." );
        }
        auto result = camera_in->camera.currentCapabilities();
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_capability_out = new wse_capi_camera_capability_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_query_control_capability(
      wse_capi_camera                     camera_in
    , wse_capi_camera_control_capability* p_capability_out
    , int32_t                             control_in )
{
    return wse::capi::guard( [camera_in, control_in, p_capability_out]() -> wse_capi_status
    {
        if ( camera_in == nullptr || p_capability_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_in and p_capability_out must not be null." );
        }
        auto result = camera_in->camera.controlCapability(
            static_cast<wse::tmr::eCameraControl>( control_in ) );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        writeControlCapability( p_capability_out, result.value() );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_get_control(
      wse_capi_camera                camera_inout
    , wse_capi_camera_control_value* p_value_out
    , int32_t                        control_in )
{
    return wse::capi::guard( [camera_inout, control_in, p_value_out]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || p_value_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_inout and p_value_out must not be null." );
        }
        auto result = camera_inout->camera.getControl(
            static_cast<wse::tmr::eCameraControl>( control_in ) );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        p_value_out->control = static_cast<std::int32_t>( result.value().control );
        p_value_out->mode = static_cast<std::int32_t>( result.value().mode );
        p_value_out->value = result.value().value;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_set_control(
      wse_capi_camera                      camera_inout
    , const wse_capi_camera_control_value* value_in )
{
    return wse::capi::guard( [camera_inout, value_in]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || value_in == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout and value_in must not be null." );
        }
        wse::tmr::sCameraControlValue native;
        native.control = static_cast<wse::tmr::eCameraControl>( value_in->control );
        native.mode = static_cast<wse::tmr::eCameraControlMode>( value_in->mode );
        native.value = value_in->value;
        return fromCameraStatus( camera_inout->camera.setControl( native ) );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_get_extension_unit(
      wse_capi_camera             camera_inout
    , uint8_t*                    p_buffer_out
    , size_t*                     p_size_out
    , wse_capi_camera_xu_selector selector_in
    , size_t                      capacity_in )
{
    return wse::capi::guard(
        [camera_inout, selector_in, p_buffer_out, capacity_in, p_size_out]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || selector_in == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_inout and selector_in must not be null." );
        }
        auto result = camera_inout->camera.getExtensionUnit( selector_in->selector );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        return copyBytes( p_buffer_out, p_size_out, result.value().payload, capacity_in );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_set_extension_unit(
      wse_capi_camera             camera_inout
    , wse_capi_camera_xu_selector selector_in
    , const uint8_t*              payload_in
    , size_t                      size_in )
{
    return wse::capi::guard(
        [camera_inout, selector_in, payload_in, size_in]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || selector_in == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_inout and selector_in must not be null." );
        }
        if ( payload_in == nullptr && size_in != 0U )
        {
            return wse::capi::makeInvalidArgument(
                "payload_in must not be null when size_in is non-zero." );
        }
        wse::tmr::sCameraExtensionUnitValue value;
        value.selector = selector_in->selector;
        value.payload.assign(
            payload_in, ( payload_in == nullptr ) ? payload_in : ( payload_in + size_in ) );
        return fromCameraStatus( camera_inout->camera.setExtensionUnit( value ) );
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_is_open(
      wse_capi_camera camera_in
    , wse_capi_bool*  p_open_out )
{
    return wse::capi::guard( [camera_in, p_open_out]() -> wse_capi_status
    {
        if ( camera_in == nullptr || p_open_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_in and p_open_out must not be null." );
        }
        *p_open_out = camera_in->camera.isOpen() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_is_streaming(
      wse_capi_camera camera_in
    , wse_capi_bool*  p_streaming_out )
{
    return wse::capi::guard( [camera_in, p_streaming_out]() -> wse_capi_status
    {
        if ( camera_in == nullptr || p_streaming_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_in and p_streaming_out must not be null." );
        }
        *p_streaming_out = camera_in->camera.isStreaming() ? 1 : 0;
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_close( wse_capi_camera camera_inout )
{
    return wse::capi::guard( [camera_inout]() -> wse_capi_status
    {
        if ( camera_inout == nullptr )
        {
            return wse::capi::makeInvalidArgument( "camera_inout must not be null." );
        }
        camera_inout->camera.stop();
        // close() is noexcept and terminal, so there is no status to translate.
        camera_inout->camera.close();
        return wse::capi::makeSuccess();
    } );
}

// ----------------------------------------------------------------------------------------------
// Frame operations
// ----------------------------------------------------------------------------------------------

wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_supports_orientation(
    int32_t pixel_format_in )
{
    return wse::tmr::isFrameOperationSupported( toCameraPixelFormat( pixel_format_in ) ) ? 1 : 0;
}

wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_supports_averaging(
    int32_t pixel_format_in )
{
    return wse::tmr::isFrameAveragingSupported( toCameraPixelFormat( pixel_format_in ) ) ? 1 : 0;
}

wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_is_bayer( int32_t pixel_format_in )
{
    return wse::tmr::isBayerFormat( toCameraPixelFormat( pixel_format_in ) ) ? 1 : 0;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_pixel_format_bayer_pattern(
      int32_t* p_pattern_out
    , int32_t  pixel_format_in )
{
    return wse::capi::guard( [pixel_format_in, p_pattern_out]() -> wse_capi_status
    {
        if ( p_pattern_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_pattern_out must not be null." );
        }
        wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
        if ( !wse::tmr::bayerPatternOf( &pattern, toCameraPixelFormat( pixel_format_in ) ) )
        {
            return wse::capi::makeInvalidArgument( "This pixel format carries no Bayer layout." );
        }
        *p_pattern_out = static_cast<std::int32_t>( pattern );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_apply_orientation(
      wse_capi_camera_frame  frame_in
    , wse_capi_camera_frame* p_frame_out
    , int32_t                orientation_in )
{
    return wse::capi::guard( [frame_in, orientation_in, p_frame_out]() -> wse_capi_status
    {
        if ( frame_in == nullptr || p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "frame_in and p_frame_out must not be null." );
        }
        auto result = wse::tmr::applyOrientation(
            frame_in->frame, toImageOrientation( orientation_in ) );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_frame_out = new wse_capi_camera_frame_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_demosaic(
      wse_capi_camera_frame  frame_in
    , wse_capi_camera_frame* p_frame_out
    , int32_t                output_pixel_format_in
    , int32_t                method_in )
{
    return wse::capi::guard(
        [frame_in, output_pixel_format_in, method_in, p_frame_out]() -> wse_capi_status
    {
        if ( frame_in == nullptr || p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "frame_in and p_frame_out must not be null." );
        }
        auto result = wse::tmr::demosaicFrame( frame_in->frame,
            toCameraPixelFormat( output_pixel_format_in ), toDemosaicMethod( method_in ) );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_frame_out = new wse_capi_camera_frame_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_create(
    wse_capi_camera_frame_accumulator* p_accumulator_out )
{
    return wse::capi::guard( [p_accumulator_out]() -> wse_capi_status
    {
        if ( p_accumulator_out == nullptr )
        {
            return wse::capi::makeInvalidArgument( "p_accumulator_out must not be null." );
        }
        *p_accumulator_out = new wse_capi_camera_frame_accumulator_t();
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_frame_accumulator_destroy(
    wse_capi_camera_frame_accumulator accumulator_inout )
{
    delete accumulator_inout;
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_add(
      wse_capi_camera_frame_accumulator accumulator_inout
    , wse_capi_camera_frame             frame_in )
{
    return wse::capi::guard( [accumulator_inout, frame_in]() -> wse_capi_status
    {
        if ( accumulator_inout == nullptr || frame_in == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "accumulator_inout and frame_in must not be null." );
        }
        const auto status = accumulator_inout->accumulator.add( frame_in->frame );
        if ( !status.succeeded() ) return fromCameraError( status.error() );
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_count(
      wse_capi_camera_frame_accumulator accumulator_in
    , size_t*                           p_count_out )
{
    return wse::capi::guard( [accumulator_in, p_count_out]() -> wse_capi_status
    {
        if ( accumulator_in == nullptr || p_count_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "accumulator_in and p_count_out must not be null." );
        }
        *p_count_out = accumulator_in->accumulator.count();
        return wse::capi::makeSuccess();
    } );
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_average(
      wse_capi_camera_frame_accumulator accumulator_in
    , wse_capi_camera_frame*            p_frame_out )
{
    return wse::capi::guard( [accumulator_in, p_frame_out]() -> wse_capi_status
    {
        if ( accumulator_in == nullptr || p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "accumulator_in and p_frame_out must not be null." );
        }
        auto result = accumulator_in->accumulator.average();
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_frame_out = new wse_capi_camera_frame_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

void WSE_CAPI_CALL wse_capi_camera_frame_accumulator_reset(
    wse_capi_camera_frame_accumulator accumulator_inout )
{
    if ( accumulator_inout != nullptr ) accumulator_inout->accumulator.reset();
}

wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_averaged_frame(
      wse_capi_camera        camera_inout
    , wse_capi_camera_frame* p_frame_out
    , size_t                 count_in
    , uint32_t               timeout_milliseconds_in )
{
    return wse::capi::guard(
        [camera_inout, count_in, timeout_milliseconds_in, p_frame_out]() -> wse_capi_status
    {
        if ( camera_inout == nullptr || p_frame_out == nullptr )
        {
            return wse::capi::makeInvalidArgument(
                "camera_inout and p_frame_out must not be null." );
        }
        auto result = wse::tmr::readAveragedFrame(
            &camera_inout->camera, count_in, timeout_milliseconds_in );
        if ( !result.succeeded() ) return fromCameraError( result.error() );
        *p_frame_out = new wse_capi_camera_frame_t( std::move( result.value() ) );
        return wse::capi::makeSuccess();
    } );
}

} // extern "C"

#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_capi_body.inc"
#endif
