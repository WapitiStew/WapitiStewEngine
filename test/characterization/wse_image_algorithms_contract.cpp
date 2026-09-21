// @brief Pins the Core design's independent numerical vectors without optional components.
// Literal outputs distinguish rotation direction, integer rounding, Bayer edge sampling and
// storage-channel color transforms. No camera or renderer is needed to exercise these contracts.
#include <wse/data/wse_ImageTransform.h>
#include <wse/data/wse_ImageDemosaic.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    int failures = 0;

    void expect( const bool condition_in, const char* const message_in )
    {
        if( !condition_in )
        {
            std::cerr << "FAILED: " << message_in << '\n';
            ++failures;
        }
    }

    template< typename Exception, typename Callable >
    void expectException( const Callable& operation_in, const char* const message_in )
    {
        bool rejected = false;
        try { operation_in(); }
        catch( const Exception& ) { rejected = true; }
        expect( rejected, message_in );
    }

    void expectImage(
          const wse::img1c08_t& image_in
        , const size_t width_in
        , const size_t height_in
        , const std::vector< std::uint8_t >& expected_in
        , const char* const message_in )
    {
        bool matches = image_in.width() == width_in && image_in.height() == height_in
                    && image_in.size() == expected_in.size();
        if( matches )
        {
            for( size_t index = 0U; index < expected_in.size(); ++index )
            {
                matches = matches && image_in.elements()[ index ][ 0U ] == expected_in[ index ];
            }
        }
        expect( matches, message_in );
    }

    void expectRgb(
          const wse::img3c08_t& image_in
        , const size_t x_in
        , const size_t y_in
        , const std::array< std::uint8_t, 3U >& expected_in
        , const char* const message_in )
    {
        const auto& pixel = image_in[ y_in ][ x_in ];
        expect( pixel[ 0U ] == expected_in[ 0U ] && pixel[ 1U ] == expected_in[ 1U ]
             && pixel[ 2U ] == expected_in[ 2U ], message_in );
    }
}

int main()
{
    // CORE-ALG-04: a rectangular image catches dimension and direction mistakes.
    wse::img1c08_t source( 3U, 2U );
    for( size_t index = 0U; index < 6U; ++index )
    {
        source.elements()[ index ][ 0U ] = static_cast< std::uint8_t >( index + 1U );
    }
    using Orientation = wse::eImageOrientation;
    expectImage( wse::applyOrientation( source, Orientation::Rotate90CW ),
        2U, 3U, { 4U, 1U, 5U, 2U, 6U, 3U }, "Clockwise design vector" );
    expectImage( wse::applyOrientation( source, Orientation::Rotate90CCW ),
        2U, 3U, { 3U, 6U, 2U, 5U, 1U, 4U }, "Counterclockwise design vector" );
    expectImage( wse::applyOrientation( source, Orientation::Rotate180 ),
        3U, 2U, { 6U, 5U, 4U, 3U, 2U, 1U }, "Half-turn vector" );
    expectImage( wse::applyOrientation( source, Orientation::FlipHorizontal ),
        3U, 2U, { 3U, 2U, 1U, 6U, 5U, 4U }, "Horizontal reflection vector" );
    expectImage( wse::applyOrientation( source, Orientation::FlipVertical ),
        3U, 2U, { 4U, 5U, 6U, 1U, 2U, 3U }, "Vertical reflection vector" );
    auto copy = wse::applyOrientation( source, Orientation::None );
    copy[ 0U ][ 0U ][ 0U ] = 99U;
    expectImage( source, 3U, 2U, { 1U, 2U, 3U, 4U, 5U, 6U },
        "Orientation results do not alias their source" );
    expectException< std::invalid_argument >( [&source]() {
        (void)wse::applyOrientation( source, static_cast< Orientation >( 255U ) );
    }, "Unknown orientation is refused" );
    expectException< std::invalid_argument >( []() {
        (void)wse::applyOrientation( wse::img1c08_t(), Orientation::None );
    }, "Empty orientation input is refused" );

    // CORE-ALG-05: pin rounding and state preservation independently of camera adaptation.
    wse::ImageAccumulator< wse::ePixFormat::CH1D8 > accumulator;
    expectException< std::logic_error >( [&accumulator]() {
        (void)accumulator.average();
    }, "An empty accumulator has no average" );
    wse::img1c08_t sample( 1U, 1U );
    sample[ 0U ][ 0U ][ 0U ] = 10U;
    accumulator.add( sample );
    sample[ 0U ][ 0U ][ 0U ] = 21U;
    accumulator.add( sample );
    expectImage( accumulator.average(), 1U, 1U, { 16U }, "10 and 21 round to 16" );
    expectException< std::invalid_argument >( [&accumulator]() {
        accumulator.add( wse::img1c08_t( 2U, 1U ) );
    }, "A different shape is refused" );
    expect( accumulator.count() == 2U, "A rejected shape preserves frame count" );
    expectImage( accumulator.average(), 1U, 1U, { 16U },
        "A rejected shape preserves sums and average does not consume them" );
    accumulator.reset();
    expect( accumulator.count() == 0U && accumulator.width() == 0U && accumulator.height() == 0U,
        "Reset clears count and shape" );
    expectException< std::logic_error >( []() {
        (void)wse::averageImages< wse::ePixFormat::CH1D8 >( {} );
    }, "An empty image list has no average" );

    // CORE-ALG-06: nonuniform samples expose first-green and clipped-neighborhood policies.
    wse::img1c08_t bayer( 3U, 3U );
    for( size_t index = 0U; index < 9U; ++index )
    {
        bayer.elements()[ index ][ 0U ] = static_cast< std::uint8_t >( ( index + 1U ) * 10U );
    }
    const auto bilinear = wse::demosaic< wse::ePixFormat::CH1D8, wse::ePixFormat::CH3D8 >(
        bayer, wse::eBayerPattern::Rggb );
    expectRgb( bilinear, 0U, 0U, { 10U, 30U, 50U }, "Bilinear clipped corner" );
    expectRgb( bilinear, 1U, 0U, { 20U, 40U, 50U }, "Bilinear averages the site's green too" );
    expectRgb( bilinear, 1U, 1U, { 50U, 50U, 50U }, "Bilinear center design vector" );
    const auto blocks = wse::demosaic< wse::ePixFormat::CH1D8, wse::ePixFormat::CH3D8 >(
        bayer, wse::eBayerPattern::Rggb, wse::eColorChannelOrder::Rgb,
        wse::eDemosaicMethod::Block2x2 );
    expectRgb( blocks, 0U, 0U, { 10U, 20U, 50U }, "Block2x2 keeps first green" );
    expectRgb( blocks, 1U, 1U, { 10U, 20U, 50U }, "Block2x2 shares one triple" );
    expectRgb( blocks, 2U, 2U, { 90U, 60U, 50U }, "Odd edge shifts the sample window" );
    const auto bgr = wse::demosaic< wse::ePixFormat::CH1D8, wse::ePixFormat::BGR3D8 >(
        bayer, wse::eBayerPattern::Rggb, wse::eColorChannelOrder::Bgr );
    expect( bgr[ 0U ][ 0U ][ 0U ] == 50U && bgr[ 0U ][ 0U ][ 2U ] == 10U,
        "Explicit BGR order changes physical channel placement" );
    expectException< std::invalid_argument >( []() {
        (void)wse::demosaic< wse::ePixFormat::CH1D8, wse::ePixFormat::CH3D8 >(
            wse::img1c08_t( 1U, 1U ), wse::eBayerPattern::Rggb );
    }, "Bayer interpolation requires a 2x2 neighborhood" );

    wse::img3c08_t color( 1U, 1U );
    color[ 0U ][ 0U ][ 0U ] = 10U;
    color[ 0U ][ 0U ][ 1U ] = 20U;
    color[ 0U ][ 0U ][ 2U ] = 30U;
    const std::array< std::array< double, 3U >, 3U > coefficients{{
        {{ 30.0, 0.0, 0.0 }}, {{ 0.0, -1.0, 0.0 }}, {{ 0.0, 0.0, 0.5 }}
    }};
    expectRgb( wse::applyColorMatrix( color, coefficients ), 0U, 0U, { 255U, 0U, 15U },
        "Color matrix clamps each output channel" );
    color[ 0U ][ 0U ][ 2U ] = 31U;
    expectRgb( wse::applyColorMatrix( color, coefficients ), 0U, 0U, { 255U, 0U, 16U },
        "Color matrix rounds a half sample upward" );
    return failures == 0 ? 0 : 1;
}
