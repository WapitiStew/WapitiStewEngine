// Saturation and parallel reservations must never reissue a renderer lifetime.
#include "../../core/oui/renderer/RendererIdentity.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    using wse::oui::internal::RendererGenerationSource;
    RendererGenerationSource source;
    constexpr std::size_t workers = 8U, each = 1024U;
    std::array< std::vector< std::uint32_t >, workers > values;
    std::vector< std::thread > threads;
    for( std::size_t worker = 0; worker < workers; ++worker )
    {
        values[ worker ].resize( each );
        threads.emplace_back( [&,worker] {
            for( auto& value : values[ worker ] ) value = source.reserve();
        } );
    }
    for( auto& thread : threads ) thread.join();
    std::vector< std::uint32_t > all;
    for( const auto& part : values ) all.insert( all.end(), part.begin(), part.end() );
    std::sort( all.begin(), all.end() );
    for( std::size_t index = 0; index < all.size(); ++index )
        if( all[ index ] != index + 1U ) { std::cerr << "Duplicate or missing generation\n"; return 1; }

    const auto maximum = ( std::numeric_limits< std::uint32_t >::max )();
    RendererGenerationSource ending( maximum - 1U );
    std::array< std::uint32_t, workers > final{};
    threads.clear();
    for( std::size_t worker = 0; worker < workers; ++worker )
        threads.emplace_back( [&,worker] { final[ worker ] = ending.reserve(); } );
    for( auto& thread : threads ) thread.join();
    if( std::count( final.begin(), final.end(), maximum ) != 1
        || std::count( final.begin(), final.end(), 0U ) != workers - 1U || ending.reserve() != 0U )
    { std::cerr << "Generation exhaustion reused an identity\n"; return 1; }
    std::cout << "Generation uniqueness and permanent exhaustion passed\n";
    return 0;
}
