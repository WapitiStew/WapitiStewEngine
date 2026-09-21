//*****************************************************************************************************************
//!
//! @file    oui_d3d12_display_enumeration.cpp
//! @brief   \~japanese Portable D3D12 Display列挙Snapshotの契約を検証する.
//! @brief   \~english  Verifies the portable D3D12 display-enumeration snapshot contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/renderer/Renderer.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

//! @brief Test failureを記録する.
//! @param [in] message_in Failure message.
void fail( const std::string& message_in )
{
    std::cerr << "FAILED: " << message_in << '\n';
    ++g_failures;
}

//! @return Display modeが完全一致する場合true.
bool sameMode(
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

//! @return Display snapshotが完全一致する場合true.
bool sameDisplay(
      const wse::oui::sDisplayDescription& left_in
    , const wse::oui::sDisplayDescription& right_in
)
{
    if( left_in.id != right_in.id || left_in.adapter_id != right_in.adapter_id ||
        left_in.adapter_name != right_in.adapter_name ||
        left_in.display_name != right_in.display_name ||
        left_in.position.x != right_in.position.x || left_in.position.y != right_in.position.y ||
        left_in.desktop_extent.width != right_in.desktop_extent.width ||
        left_in.desktop_extent.height != right_in.desktop_extent.height ||
        !sameMode( left_in.current_mode, right_in.current_mode ) ||
        left_in.rotation != right_in.rotation || left_in.primary != right_in.primary ||
        left_in.renderer_compatible != right_in.renderer_compatible ||
        left_in.modes_complete != right_in.modes_complete ||
        left_in.modes.size() != right_in.modes.size() )
    {
        return false;
    }
    for( std::size_t index = 0U; index < left_in.modes.size(); ++index )
    {
        if( !sameMode( left_in.modes[ index ], right_in.modes[ index ] ) )
        {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    // WARP自体にOutputは無いが、SystemのActive Hardware Outputは読み取り専用で列挙できる.
    wse::oui::Renderer renderer;
    wse::oui::sRendererConfiguration configuration;
    configuration.backend              = wse::oui::eRendererBackend::Direct3D12;
    configuration.use_software_adapter = true;
    const auto initialize_result = renderer.initialize( configuration );
    if( !initialize_result.succeeded() )
    {
        fail( "Renderer initialization failed: " + initialize_result.error().message() );
        return 1;
    }
    const auto capabilities_result = renderer.getCapabilities();
    if( !capabilities_result.succeeded() ||
        !capabilities_result.value().supports_display_enumeration )
    {
        fail( "D3D12 display-enumeration capability is unavailable." );
        return 1;
    }

    const auto first_result = renderer.enumerateDisplays();
    const auto second_result = renderer.enumerateDisplays();
    if( !first_result.succeeded() || !second_result.succeeded() )
    {
        fail( "Display enumeration failed." );
        return 1;
    }
    const auto& first = first_result.value();
    const auto& second = second_result.value();
    if( first.size() != second.size() )
    {
        fail( "Repeated display snapshots have different sizes." );
    }

    std::set< std::string > display_ids;
    std::size_t primary_count = 0U;
    const std::size_t comparable_count = ( std::min )( first.size(), second.size() );
    for( std::size_t index = 0U; index < first.size(); ++index )
    {
        const auto& display = first[ index ];
        if( !wse::oui::validateDisplayDescription( display ).ok() )
        {
            fail( "Enumerated display violates the portable contract." );
        }
        if( !display_ids.insert( display.id ).second )
        {
            fail( "Display IDs are not unique within one topology snapshot." );
        }
        if( index > 0U && first[ index - 1U ].id >= display.id )
        {
            fail( "Display snapshot ordering is not deterministic." );
        }
        if( display.renderer_compatible )
        {
            fail( "A hardware display must not claim compatibility with the WARP adapter." );
        }
        primary_count += display.primary ? 1U : 0U;
    }
    if( primary_count > 1U )
    {
        fail( "Display snapshot contains more than one primary display." );
    }
    for( std::size_t index = 0U; index < comparable_count; ++index )
    {
        if( !sameDisplay( first[ index ], second[ index ] ) )
        {
            fail( "Repeated display enumeration is unstable without a topology change." );
        }
    }

    renderer.shutdown();
    if( renderer.enumerateDisplays().succeeded() )
    {
        fail( "Display enumeration after shutdown must fail." );
    }
    std::cout << "D3D12 active display snapshot count: " << first.size() << '\n';
    return g_failures == 0 ? 0 : 1;
}
