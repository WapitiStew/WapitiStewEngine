// Internal identity source. Handles are scoped to one loaded WSE runtime, not persisted.
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

namespace wse::oui::internal
{
class RendererGenerationSource final
{
    std::atomic< std::uint32_t > m_last;
  public:
    explicit RendererGenerationSource( std::uint32_t last_issued_in = 0U ) noexcept
        : m_last( last_issued_in ) {}

    // Zero means permanently exhausted. No wrap or reuse, including failed initialization.
    std::uint32_t reserve() noexcept
    {
        auto last = m_last.load( std::memory_order_relaxed );
        while( last != ( std::numeric_limits< std::uint32_t >::max )() )
        {
            if( m_last.compare_exchange_weak( last, last + 1U, std::memory_order_relaxed ) )
                return last + 1U;
        }
        return 0U;
    }
};

inline std::uint32_t reserveRendererGeneration() noexcept
{
    static RendererGenerationSource source;
    return source.reserve();
}
}
