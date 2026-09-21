//*****************************************************************************************************************
//!
//! @file    D3D12DisplayEnumeration.cpp
//! @brief   \~japanese Direct3D 12向けの読み取り専用Display列挙を提供する.
//! @brief   \~english  Provides read-only display enumeration for Direct3D 12.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "D3D12DisplayEnumeration.h"

#include <Windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Microsoft::WRL::ComPtr;

wse::oui::RendererError makeError(
      const wse::oui::eRendererErrorCategory category_in
    , const wse::oui::eRendererErrorCode     code_in
    , const std::string&                     message_in
    , const HRESULT                          native_code_in = S_OK
)
{
    return wse::oui::RendererError(
          category_in
        , code_in
        , message_in
        , static_cast< std::int64_t >( native_code_in )
    );
}

wse::oui::RendererError backendError( const std::string& message_in, const HRESULT result_in )
{
    const bool device_lost = result_in == DXGI_ERROR_DEVICE_REMOVED ||
        result_in == DXGI_ERROR_DEVICE_RESET;
    return makeError(
          device_lost ? wse::oui::eRendererErrorCategory::Backend
                      : wse::oui::eRendererErrorCategory::Resource
        , device_lost ? wse::oui::eRendererErrorCode::DeviceLost
                      : wse::oui::eRendererErrorCode::BackendFailure
        , message_in
        , result_in
    );
}

std::string wideToUtf8( const wchar_t* const value_in )
{
    if( value_in == nullptr || value_in[ 0U ] == L'\0' )
    {
        return std::string();
    }
    const int required_size = WideCharToMultiByte(
          CP_UTF8
        , WC_ERR_INVALID_CHARS
        , value_in
        , -1
        , nullptr
        , 0
        , nullptr
        , nullptr
    );
    if( required_size <= 1 )
    {
        return std::string();
    }
    std::vector< char > utf8( static_cast< std::size_t >( required_size ) );
    const int converted_size = WideCharToMultiByte(
          CP_UTF8
        , WC_ERR_INVALID_CHARS
        , value_in
        , -1
        , utf8.data()
        , required_size
        , nullptr
        , nullptr
    );
    if( converted_size != required_size )
    {
        return std::string();
    }
    return std::string( utf8.data(), static_cast< std::size_t >( converted_size - 1 ) );
}

std::string makeAdapterId( const LUID& luid_in )
{
    std::ostringstream stream;
    stream << "dxgi:" << std::hex << std::setfill( '0' )
           << std::setw( 8 ) << static_cast< std::uint32_t >( luid_in.HighPart )
           << std::setw( 8 ) << luid_in.LowPart;
    return stream.str();
}

wse::oui::eDisplayRotation toPortableRotation( const DXGI_MODE_ROTATION rotation_in ) noexcept
{
    switch( rotation_in )
    {
        case DXGI_MODE_ROTATION_IDENTITY:
            return wse::oui::eDisplayRotation::Identity;
        case DXGI_MODE_ROTATION_ROTATE90:
            return wse::oui::eDisplayRotation::Rotate90;
        case DXGI_MODE_ROTATION_ROTATE180:
            return wse::oui::eDisplayRotation::Rotate180;
        case DXGI_MODE_ROTATION_ROTATE270:
            return wse::oui::eDisplayRotation::Rotate270;
        default:
            return wse::oui::eDisplayRotation::Unknown;
    }
}

void normalizeDisplayMode( wse::oui::sDisplayMode* const p_mode_inout ) noexcept
{
    if( p_mode_inout == nullptr || p_mode_inout->refresh_rate_numerator == 0U ||
        p_mode_inout->refresh_rate_denominator == 0U )
    {
        return;
    }
    const std::uint32_t divisor = std::gcd(
        p_mode_inout->refresh_rate_numerator, p_mode_inout->refresh_rate_denominator );
    p_mode_inout->refresh_rate_numerator /= divisor;
    p_mode_inout->refresh_rate_denominator /= divisor;
}

wse::oui::sDisplayMode toPortableDisplayMode( const DXGI_MODE_DESC1& mode_in ) noexcept
{
    wse::oui::sDisplayMode mode;
    mode.extent = { mode_in.Width, mode_in.Height };
    mode.refresh_rate_numerator   = mode_in.RefreshRate.Numerator;
    mode.refresh_rate_denominator = mode_in.RefreshRate.Denominator;
    mode.format = mode_in.Format == DXGI_FORMAT_B8G8R8A8_UNORM
        ? wse::oui::eRendererPixelFormat::Bgra8Unorm
        : wse::oui::eRendererPixelFormat::Rgba8Unorm;
    mode.interlaced = mode_in.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST ||
        mode_in.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    normalizeDisplayMode( &mode );
    return mode;
}

bool sameDisplayMode(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in
) noexcept
{
    return left_in.extent.width == right_in.extent.width &&
        left_in.extent.height == right_in.extent.height &&
        left_in.refresh_rate_numerator == right_in.refresh_rate_numerator &&
        left_in.refresh_rate_denominator == right_in.refresh_rate_denominator &&
        left_in.format == right_in.format && left_in.interlaced == right_in.interlaced;
}

bool displayModeLess(
      const wse::oui::sDisplayMode& left_in
    , const wse::oui::sDisplayMode& right_in
) noexcept
{
    if( left_in.extent.width != right_in.extent.width )
    {
        return left_in.extent.width < right_in.extent.width;
    }
    if( left_in.extent.height != right_in.extent.height )
    {
        return left_in.extent.height < right_in.extent.height;
    }
    const std::uint64_t left_refresh =
        static_cast< std::uint64_t >( left_in.refresh_rate_numerator ) *
        right_in.refresh_rate_denominator;
    const std::uint64_t right_refresh =
        static_cast< std::uint64_t >( right_in.refresh_rate_numerator ) *
        left_in.refresh_rate_denominator;
    if( left_refresh != right_refresh )
    {
        return left_refresh < right_refresh;
    }
    if( left_in.interlaced != right_in.interlaced )
    {
        return !left_in.interlaced;
    }
    return static_cast< std::uint8_t >( left_in.format ) <
        static_cast< std::uint8_t >( right_in.format );
}

void sortAndDeduplicateDisplayModes(
    std::vector< wse::oui::sDisplayMode >* const p_modes_inout )
{
    std::sort( p_modes_inout->begin(), p_modes_inout->end(), displayModeLess );
    p_modes_inout->erase(
          std::unique( p_modes_inout->begin(), p_modes_inout->end(), sameDisplayMode )
        , p_modes_inout->end()
    );
}

wse::oui::RendererResult< wse::oui::sDisplayMode > queryCurrentDisplayMode(
    const wchar_t* const device_name_in )
{
    DEVMODEW native_mode = {};
    native_mode.dmSize = sizeof( native_mode );
    SetLastError( ERROR_SUCCESS );
    // EDS_RAWMODE can report timings that the active monitor path does not accept via
    // ChangeDisplaySettingsEx.  The portable contract describes modes available to the
    // current desktop topology, so query the filtered OS view.
    if( EnumDisplaySettingsExW(
            device_name_in, ENUM_CURRENT_SETTINGS, &native_mode, 0U ) == FALSE )
    {
        const DWORD native_error = GetLastError();
        return wse::oui::RendererResult< wse::oui::sDisplayMode >::failure( makeError(
                  wse::oui::eRendererErrorCategory::Backend
                , wse::oui::eRendererErrorCode::BackendFailure
                , "Failed to query the current display mode."
                , native_error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32( native_error )
            )
        );
    }

    wse::oui::sDisplayMode mode;
    mode.extent = { native_mode.dmPelsWidth, native_mode.dmPelsHeight };
    mode.refresh_rate_numerator = native_mode.dmDisplayFrequency;
    mode.refresh_rate_denominator = 1U;
    mode.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    mode.interlaced = ( native_mode.dmDisplayFlags & DM_INTERLACED ) != 0U;
    normalizeDisplayMode( &mode );
    const wse::oui::RendererError validation_error = wse::oui::validateDisplayMode( mode );
    if( !validation_error.ok() )
    {
        return wse::oui::RendererResult< wse::oui::sDisplayMode >::failure( validation_error );
    }
    return wse::oui::RendererResult< wse::oui::sDisplayMode >::success( mode );
}

} // namespace

namespace wse
{
namespace oui
{
namespace internal
{

RendererResult< std::vector< sDisplayDescription > > enumerateD3D12Displays(
      IDXGIFactory6* const factory_in
    , IDXGIAdapter1* const renderer_adapter_in
)
{
    if( factory_in == nullptr || renderer_adapter_in == nullptr )
    {
        return RendererResult< std::vector< sDisplayDescription > >::failure( makeError(
                  eRendererErrorCategory::Validation
                , eRendererErrorCode::InvalidArgument
                , "Display enumeration requires an initialized DXGI factory and renderer adapter."
            )
        );
    }

    DXGI_ADAPTER_DESC1 renderer_adapter_description = {};
    const HRESULT renderer_adapter_result =
        renderer_adapter_in->GetDesc1( &renderer_adapter_description );
    if( FAILED( renderer_adapter_result ) )
    {
        return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to identify the renderer adapter.", renderer_adapter_result ) );
    }
    const std::string renderer_adapter_id =
        makeAdapterId( renderer_adapter_description.AdapterLuid );

    std::vector< sDisplayDescription > displays;
    for( UINT adapter_index = 0U; ; ++adapter_index )
    {
        ComPtr< IDXGIAdapter1 > adapter;
        const HRESULT adapter_result = factory_in->EnumAdapterByGpuPreference(
              adapter_index
            , DXGI_GPU_PREFERENCE_UNSPECIFIED
            , IID_PPV_ARGS( adapter.ReleaseAndGetAddressOf() )
        );
        if( adapter_result == DXGI_ERROR_NOT_FOUND )
        {
            break;
        }
        if( FAILED( adapter_result ) )
        {
            return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to enumerate display adapters.", adapter_result ) );
        }

        DXGI_ADAPTER_DESC1 adapter_description = {};
        const HRESULT adapter_description_result = adapter->GetDesc1( &adapter_description );
        if( FAILED( adapter_description_result ) )
        {
            return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to query a display adapter.", adapter_description_result ) );
        }
        if( ( adapter_description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) != 0U )
        {
            continue;
        }
        const std::string adapter_id = makeAdapterId( adapter_description.AdapterLuid );
        const std::string adapter_name = wideToUtf8( adapter_description.Description );

        for( UINT output_index = 0U; ; ++output_index )
        {
            ComPtr< IDXGIOutput > output;
            const HRESULT output_result = adapter->EnumOutputs(
                output_index, output.ReleaseAndGetAddressOf() );
            if( output_result == DXGI_ERROR_NOT_FOUND )
            {
                break;
            }
            if( FAILED( output_result ) )
            {
                return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to enumerate display outputs.", output_result ) );
            }

            DXGI_OUTPUT_DESC output_description = {};
            const HRESULT output_description_result = output->GetDesc( &output_description );
            if( FAILED( output_description_result ) )
            {
                return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to query a display output.", output_description_result ) );
            }
            if( output_description.AttachedToDesktop == FALSE )
            {
                continue;
            }

            const auto current_mode_result = queryCurrentDisplayMode( output_description.DeviceName );
            if( !current_mode_result.succeeded() )
            {
                return RendererResult< std::vector< sDisplayDescription > >::failure( current_mode_result.error() );
            }
            const LONG desktop_width = output_description.DesktopCoordinates.right -
                output_description.DesktopCoordinates.left;
            const LONG desktop_height = output_description.DesktopCoordinates.bottom -
                output_description.DesktopCoordinates.top;
            if( desktop_width <= 0L || desktop_height <= 0L )
            {
                return RendererResult< std::vector< sDisplayDescription > >::failure( makeError(
                          eRendererErrorCategory::Backend
                        , eRendererErrorCode::BackendFailure
                        , "Active display has invalid desktop coordinates."
                    )
                );
            }

            sDisplayDescription display;
            display.adapter_id = adapter_id;
            display.adapter_name = adapter_name;
            display.display_name = wideToUtf8( output_description.DeviceName );
            display.id = display.adapter_id + ":" + display.display_name;
            display.position = {
                  output_description.DesktopCoordinates.left
                , output_description.DesktopCoordinates.top
            };
            display.desktop_extent = {
                  static_cast< std::uint32_t >( desktop_width )
                , static_cast< std::uint32_t >( desktop_height )
            };
            display.current_mode = current_mode_result.value();
            display.modes.push_back( display.current_mode );
            display.rotation = toPortableRotation( output_description.Rotation );
            const POINT desktop_origin = { 0, 0 };
            display.primary = output_description.Monitor ==
                MonitorFromPoint( desktop_origin, MONITOR_DEFAULTTONULL );
            display.renderer_compatible = display.adapter_id == renderer_adapter_id;

            ComPtr< IDXGIOutput1 > output1;
            if( SUCCEEDED( output.As( &output1 ) ) )
            {
                UINT mode_count = 0U;
                const UINT flags = DXGI_ENUM_MODES_INTERLACED | DXGI_ENUM_MODES_SCALING;
                HRESULT mode_result = output1->GetDisplayModeList1(
                    DXGI_FORMAT_R8G8B8A8_UNORM, flags, &mode_count, nullptr );
                if( mode_result == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE )
                {
                    display.modes_complete = false;
                }
                else if( FAILED( mode_result ) )
                {
                    return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to count display modes.", mode_result ) );
                }
                else
                {
                    std::vector< DXGI_MODE_DESC1 > native_modes( mode_count );
                    if( mode_count > 0U )
                    {
                        mode_result = output1->GetDisplayModeList1(
                            DXGI_FORMAT_R8G8B8A8_UNORM, flags, &mode_count, native_modes.data() );
                    }
                    if( FAILED( mode_result ) )
                    {
                        return RendererResult< std::vector< sDisplayDescription > >::failure( backendError( "Failed to query display modes.", mode_result ) );
                    }
                    native_modes.resize( mode_count );
                    for( const DXGI_MODE_DESC1& native_mode : native_modes )
                    {
                        const sDisplayMode mode = toPortableDisplayMode( native_mode );
                        if( validateDisplayMode( mode ).ok() )
                        {
                            display.modes.push_back( mode );
                        }
                    }
                    display.modes_complete = true;
                }
            }
            sortAndDeduplicateDisplayModes( &display.modes );

            const RendererError validation_error = validateDisplayDescription( display );
            if( !validation_error.ok() )
            {
                return RendererResult< std::vector< sDisplayDescription > >::failure( makeError(
                          eRendererErrorCategory::Backend
                        , eRendererErrorCode::BackendFailure
                        , "Native display data violates the portable display contract: " +
                            validation_error.message()
                    )
                );
            }
            displays.push_back( std::move( display ) );
        }
    }

    std::sort( displays.begin(), displays.end(),
        []( const sDisplayDescription& left_in, const sDisplayDescription& right_in )
        {
            return left_in.id < right_in.id;
        } );
    return RendererResult< std::vector< sDisplayDescription > >::success( std::move( displays ) );
}

RendererResult< sD3D12DisplayTarget > findD3D12DisplayTarget(
      IDXGIFactory6* const      factory_in
    , IDXGIAdapter1* const      renderer_adapter_in
    , const std::string&        display_id_in
)
{
    const auto displays_result = enumerateD3D12Displays( factory_in, renderer_adapter_in );
    if( !displays_result.succeeded() )
    {
        return RendererResult< sD3D12DisplayTarget >::failure( displays_result.error() );
    }
    const auto display_iterator = std::find_if(
          displays_result.value().begin()
        , displays_result.value().end()
        , [&display_id_in]( const sDisplayDescription& display_in )
          {
              return display_in.id == display_id_in;
          }
    );
    if( display_iterator == displays_result.value().end() )
    {
        return RendererResult< sD3D12DisplayTarget >::failure( makeError(
                  eRendererErrorCategory::Resource
                , eRendererErrorCode::ResourceNotFound
                , "Requested display ID is not active."
            )
        );
    }
    if( !display_iterator->renderer_compatible )
    {
        return RendererResult< sD3D12DisplayTarget >::failure( makeError(
                  eRendererErrorCategory::Unsupported
                , eRendererErrorCode::UnsupportedOperation
                , "Requested display is not attached to the active renderer adapter."
            )
        );
    }

    for( UINT output_index = 0U; ; ++output_index )
    {
        ComPtr< IDXGIOutput > output;
        const HRESULT output_result = renderer_adapter_in->EnumOutputs(
            output_index, output.ReleaseAndGetAddressOf() );
        if( output_result == DXGI_ERROR_NOT_FOUND )
        {
            break;
        }
        if( FAILED( output_result ) )
        {
            return RendererResult< sD3D12DisplayTarget >::failure( backendError( "Failed to enumerate renderer display outputs.", output_result ) );
        }
        DXGI_OUTPUT_DESC output_description = {};
        const HRESULT description_result = output->GetDesc( &output_description );
        if( FAILED( description_result ) )
        {
            return RendererResult< sD3D12DisplayTarget >::failure( backendError( "Failed to identify a renderer display output.", description_result ) );
        }
        if( wideToUtf8( output_description.DeviceName ) == display_iterator->display_name )
        {
            sD3D12DisplayTarget target;
            target.description = *display_iterator;
            target.output = std::move( output );
            target.device_name = output_description.DeviceName;
            return RendererResult< sD3D12DisplayTarget >::success( std::move( target ) );
        }
    }
    return RendererResult< sD3D12DisplayTarget >::failure( makeError(
              eRendererErrorCategory::Resource
            , eRendererErrorCode::ResourceNotFound
            , "Requested renderer display output disappeared during enumeration."
        )
    );
}

} // namespace internal
} // namespace oui
} // namespace wse
