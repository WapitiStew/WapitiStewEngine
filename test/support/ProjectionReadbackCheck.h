// Four RGB landmarks from the display smoke's 2x2 source texture.
// Sample 3x3 patches inside the unblended, clamp-to-edge corner regions.
#pragma once
#include <oui/renderer/RendererTypes.h>
#include <array>
#include <cstdlib>
#include <ostream>

namespace wse_test
{
inline bool matchesProjectionPattern( const wse::oui::sRendererFrame& frame_in,
    std::ostream& diagnostics_inout )
{
    using namespace wse::oui;
    const auto& description = frame_in.description;
    if( description.memorySize() == 0U || frame_in.data.size() != description.memorySize() || description.extent.width < 32U || description.extent.height < 32U
        || ( description.format != eRendererPixelFormat::Rgba8Unorm
            && description.format != eRendererPixelFormat::Bgra8Unorm ) )
    { diagnostics_inout << "Invalid projection readback layout.\n"; return false; }
    const std::array< std::array< int, 3 >, 4 > colors{{
        {{255,96,0}}, {{0,255,96}}, {{0,96,255}}, {{255,255,255}}
    }};
    const auto width = description.extent.width, height = description.extent.height;
    const std::size_t stride = description.effectiveRowPitch();
    const bool bgra = description.format == eRendererPixelFormat::Bgra8Unorm;
    for( unsigned region = 0U; region < 4U; ++region )
    {
        const unsigned center_x = ( region % 2U == 0U ) ? width / 5U : width - width / 5U;
        const unsigned center_y = ( region / 2U == 0U ) ? height / 5U : height - height / 5U;
        for( unsigned y = center_y - 1U; y <= center_y + 1U; ++y )
        for( unsigned x = center_x - 1U; x <= center_x + 1U; ++x )
        {
            const auto* pixel = frame_in.data.data() + y * stride + x * 4U;
            const std::array< int, 3 > rgb{{ pixel[ bgra ? 2U : 0U ], pixel[ 1U ], pixel[ bgra ? 0U : 2U ] }};
            for( unsigned channel = 0U; channel < 3U; ++channel )
            {
                if( std::abs( rgb[ channel ] - colors[ region ][ channel ] ) > 2 )
                {
                    diagnostics_inout << "RGB pattern mismatch at " << x << ',' << y
                        << " channel=" << channel << " expected=" << colors[ region ][ channel ]
                        << " actual=" << rgb[ channel ] << '\n';
                    return false;
                }
            }
        }
    }
    return true;
}
} // namespace wse_test
