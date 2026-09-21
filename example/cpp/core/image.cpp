// @file image.cpp
// @brief The wse::Image_ data class: adopting an externally strided buffer, channel order
//        in the type, orientation, and writing back out.
//
// Hardware-free. Build with the always-on Core component and link WSE::Core.
//
// Two rules run through the whole tour. An Image_ is always packed: it carries a width and a
// height but no stride, so every entry point that meets foreign memory either folds a row
// margin away on the way in or re-applies one on the way out. And the channel order lives in
// the ePixFormat template argument rather than in a runtime field, so BGR3D8 and CH3D8 are
// different types and confusing them is a compile error rather than a wrong-looking image.
//
// The exit code names the step that failed: 1 an unreadable view, 2 a refused write-back,
// 3 a value that did not survive the tour.

#include <wse/stew.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{

constexpr std::size_t WIDTH  = 4U;
constexpr std::size_t HEIGHT = 3U;
//! A device usually hands over rows with a margin; this one pads each row by five bytes.
//! Only the total matters: a stride below the packed row size makes the view invalid, and a
//! larger one just moves the next row further along. Five is small enough to keep the numbers
//! in the log readable.
constexpr std::size_t PADDED_STRIDE = WIDTH * 3U + 5U;

//! Builds the kind of buffer a device produces: interleaved BGR with a row margin.
//! The whole buffer starts at 0xEE and only the packed part of each row is overwritten, so the
//! margin keeps a value that no pixel below ever produces.
std::vector< std::uint8_t > makePaddedBgrBuffer()
{
    std::vector< std::uint8_t > buffer( PADDED_STRIDE * HEIGHT, 0xEEU );
    for ( std::size_t y = 0U; y < HEIGHT; ++y )
    {
        for ( std::size_t x = 0U; x < WIDTH; ++x )
        {
            const std::size_t offset = y * PADDED_STRIDE + x * 3U;
            buffer[ offset + 0U ] = static_cast< std::uint8_t >( 10U + y * 10U + x ); // blue
            buffer[ offset + 1U ] = static_cast< std::uint8_t >( 20U + y * 10U + x ); // green
            buffer[ offset + 2U ] = static_cast< std::uint8_t >( 30U + y * 10U + x ); // red
        }
    }
    return buffer;
}

//! Names what an image carries. Both answers come from the type, not from a stored field.
//! pixel_size() is the channel count rather than a byte count, and bit_depth() the bits in one
//! sample; a caller sizing a buffer wants pixel_byte(), which combines the two.
template< wse::ePixFormat _Pf >
std::string describe( const wse::Image_< _Pf >& image_in )
{
    const std::string order =
        wse::getChannelOrder( _Pf ) == wse::eColorChannelOrder::Bgr ? "BGR" : "RGB";
    return std::to_string( image_in.width() ) + "x" + std::to_string( image_in.height() )
         + " channels=" + std::to_string( image_in.pixel_size() )
         + " depth=" + std::to_string( image_in.bit_depth() )
         + " order=" + order;
}

} // namespace

int main()
{
    wse::registDefaultLog();

    const std::vector< std::uint8_t > buffer = makePaddedBgrBuffer();

    // A view describes someone else's memory: `buffer` stays the owner, and the view holds a
    // bare pointer that has to stay good for the duration of each call made through it.
    // Validating it first means the only thing that can still fail during construction is the
    // allocation. accessible_bytes is what proves the read stays inside the buffer, so it has
    // to be the real readable size and not a hopeful one; the check only demands that the last
    // row's pixels be reachable, not the margin that follows them.
    wse::sInterleavedView view;
    view.data             = buffer.data();
    view.width            = WIDTH;
    view.height           = HEIGHT;
    view.row_stride       = PADDED_STRIDE;
    view.accessible_bytes = buffer.size();
    if ( !wse::isInterleavedViewValid< wse::ePixFormat::BGR3D8 >( view ) )
    {
        wse::WLog() << "ERROR: the view does not describe a readable BGR8 buffer";
        return 1;
    }

    // The image is always packed, so adopting the buffer folds the row margin away. "Adopting"
    // is a copy and not an alias: once this returns, `bgr` owns its own storage and holds no
    // pointer back into `buffer`. Because the view was validated above this cannot throw for a
    // layout reason, only for a failed allocation.
    const wse::img3c08_bgr_t bgr =
        wse::makeImageFromInterleaved< wse::ePixFormat::BGR3D8 >( view );
    wse::WLog() << "adopted image:" << describe( bgr );
    wse::WLog() << "packed row bytes:" << wse::packedRowBytes< wse::ePixFormat::BGR3D8 >( WIDTH )
                << "against a source stride of" << PADDED_STRIDE;
    // Subscripting runs row, then column, then channel, and none of the three is bounds
    // checked. Channel zero is blue here only because the type says BGR3D8.
    wse::WLog() << "first pixel: blue=" << bgr[ 0U ][ 0U ][ 0U ]
                << "green=" << bgr[ 0U ][ 0U ][ 1U ]
                << "red=" << bgr[ 0U ][ 0U ][ 2U ];

    // Orientation moves whole pixels and never interprets a channel, so a BGR image and an
    // RGB one behave identically; the ninety-degree turn exchanges width and height. The
    // source is left alone and a new image comes back, so the cost is a second buffer; an
    // empty source is the one input that throws.
    const wse::img3c08_bgr_t rotated =
        wse::applyOrientation( bgr, wse::eImageOrientation::Rotate90CW );
    wse::WLog() << "rotated image:" << describe( rotated );

    // Changing the channel order is an explicit call. The type changes with it, so a later
    // reader cannot mistake one order for the other. The two formats have to agree on channel
    // count and bit depth — only the order may differ — and the swap is unconditional: it
    // exchanges channel zero with channel two whatever the two names happen to be.
    const wse::img3c08_t rgb =
        wse::convertChannelOrder< wse::ePixFormat::CH3D8, wse::ePixFormat::BGR3D8 >( bgr );
    wse::WLog() << "reordered image:" << describe( rgb );
    const bool swapped = rgb[ 0U ][ 0U ][ 0U ] == bgr[ 0U ][ 0U ][ 2U ]
                      && rgb[ 0U ][ 0U ][ 2U ] == bgr[ 0U ][ 0U ][ 0U ];
    wse::WLog() << "channel zero and channel two exchanged:" << swapped;

    // Adding an alpha channel is also explicit; castData changes the channel count by index
    // and never reorders, so an RGB image has to be reordered before or after, not by it.
    // That is why `rgb` and not `bgr` is the argument: castData is instantiated for the
    // order-neutral CHxDyy formats, so handing it the BGR type does not resolve. The new
    // channel is filled fully opaque, which is where the 255 logged below comes from.
    const wse::img4c08_t rgba =
        wse::castData::image< wse::ePixFormat::CH4D08, wse::ePixFormat::CH3D08 >( rgb );
    wse::WLog() << "four-channel image:" << describe( rgba )
                << "alpha=" << rgba[ 0U ][ 0U ][ 3U ];

    // Writing back out fills someone else's buffer at whatever stride it asks for, and the
    // margin bytes are left exactly as they were. `destination` is the owner; the target only
    // names it. The call never throws, and it refuses rather than writes when the extents
    // disagree or the buffer is too small, so a false return leaves the 0x11 fill untouched
    // and there is no half-written frame to clean up.
    std::vector< std::uint8_t > destination( PADDED_STRIDE * HEIGHT, 0x11U );
    wse::sInterleavedTarget target;
    target.data             = destination.data();
    target.width            = WIDTH;
    target.height           = HEIGHT;
    target.row_stride       = PADDED_STRIDE;
    target.accessible_bytes = destination.size();
    if ( !wse::writeImageToInterleaved( &target, bgr ) )
    {
        wse::WLog() << "ERROR: writing the image back out failed";
        return 2;
    }

    // Compare the packed part of each row only: the inner bound stops at WIDTH * 3 so a margin
    // byte never enters the comparison and the round trip is judged on pixels alone.
    bool pixels_match = true;
    for ( std::size_t y = 0U; y < HEIGHT; ++y )
    {
        for ( std::size_t x = 0U; x < WIDTH * 3U; ++x )
        {
            pixels_match = pixels_match &&
                destination[ y * PADDED_STRIDE + x ] == buffer[ y * PADDED_STRIDE + x ];
        }
    }
    // The first byte past row zero's pixels is enough to show the write respected the stride:
    // had it treated the buffer as packed, this byte would carry row one's blue sample.
    const bool margin_untouched = destination[ WIDTH * 3U ] == 0x11U;
    wse::WLog() << "round trip restored every pixel:" << pixels_match;
    wse::WLog() << "the row margin was left alone:" << margin_untouched;

    // One verdict over the whole tour: the channel exchange, the stride round trip, the
    // untouched margin, and the transposed extents the ninety-degree turn promised.
    const bool consistent = swapped && pixels_match && margin_untouched
                         && rotated.width() == HEIGHT && rotated.height() == WIDTH;
    wse::WLog() << ( consistent ? "image tour succeeded" : "ERROR: image tour failed" );
    return consistent ? 0 : 3;
}
