// @file size.cpp
// @brief Hardware-free usage of the wse::Size_ data class, reported through the WSE stream logger.
//
// Build with the always-on Core component and link WSE::Core.

#include <wse/stew.h>

int main()
{
    // Register the default "WSE" console profile once; every WLog() stream below goes through it.
    wse::registDefaultLog();

    // Construction with explicit width and height.
    const wse::float64_size panel( 1920.0, 1080.0 );
    wse::WLog() << "panel:" << panel.str();

    // area() derives a value from the stored pair. It always returns double whatever the
    // element type is, so an integer size cannot silently overflow its own type here.
    wse::WLog() << "panel area:" << panel.area() << "square pixels";

    // setup() replaces both members at once, which is the only way to change a Size_ after
    // construction; there is no per-member setter to leave the pair half updated.
    wse::float64_size preview;
    preview.setup( panel.width() / 4.0, panel.height() / 4.0 );
    wse::WLog() << "quarter preview:" << preview.str();

    // The casting constructor converts between element types (float64 -> uint32). It is
    // explicit, so a narrowing conversion never happens by accident at a call boundary, and it
    // is a plain static_cast per member: a fractional width truncates toward zero rather than
    // rounding, and a negative or out-of-range value is not diagnosed. The quarter of 1920x1080
    // divides exactly, which is why the check below can compare for equality.
    const wse::uint32_size integral( preview );
    wse::WLog() << "integral preview:" << integral.str();

    const bool consistent =
        panel.width() == 1920.0 && panel.height() == 1080.0 &&
        integral.width() == 480U && integral.height() == 270U;
    if ( !consistent )
    {
        wse::WLog() << "ERROR: size round trip failed";
        return 1;
    }

    wse::WLog() << "size round trip succeeded";
    return 0;
}
