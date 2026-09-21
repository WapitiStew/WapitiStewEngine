// @file range2.cpp
// @brief Hardware-free usage of the wse::Range2_ rectangle class, reported through the WSE stream logger.
//
// Build with the always-on Core component and link WSE::Core.
//
// Range2_ is a value type with no allocation and no shared state, so every rectangle below is
// an independent copy and none of them refers to another. What it does have is a validating
// constructor, which is the one thing that can throw.

#include <wse/stew.h>

int main()
{
    wse::registDefaultLog();

    // A rectangle is a pair of inclusive [min, max] intervals on X and Y. The four-argument
    // form runs min X, max X, min Y, max Y — one axis at a time, not corner then corner — and
    // it validates: a min above its max throws std::invalid_argument rather than yielding a
    // backwards rectangle. Writing the corners in the other order here would give min Y 1920
    // against max Y 1080 and throw on the spot, which is the intended way to notice.
    const wse::float64_rect display( 0.0, 1920.0, 0.0, 1080.0 );
    wse::WLog() << "display:" << display.str();
    wse::WLog() << "display size:" << display.size().str();

    // A rectangle can also be defined by a start point and a size. The size is added to the
    // start, so the maximum is the far edge itself and width() gives the size straight back.
    // On an integer instantiation that inclusive maximum means the rectangle covers one more
    // pixel column than the width suggests; on this float64 one it is only a coordinate.
    const wse::float64_xy origin( 480.0, 270.0 );
    const wse::float64_size extent( 960.0, 540.0 );
    const wse::float64_rect region( origin, extent );
    wse::WLog() << "centered region:" << region.str();

    // isInner()/isOver() answer point containment against the inclusive bounds. They are exact
    // complements — isOver() is defined as the negation of isInner() — so a point sitting on an
    // edge counts as inside and never as both or neither. Per-axis forms exist as well when
    // only one coordinate is in question.
    const wse::float64_xy inside( 960.0, 540.0 );
    const wse::float64_xy outside( 100.0, 100.0 );
    wse::WLog() << "point" << inside.str()
                << ( region.isInner( inside ) ? "is inside the region" : "is outside the region" );
    wse::WLog() << "point" << outside.str()
                << ( region.isOver( outside ) ? "is outside the region" : "is inside the region" );

    const bool consistent =
        region.width() == 960.0 && region.height() == 540.0 &&
        region.isInner( inside ) && region.isOver( outside );
    if ( !consistent )
    {
        wse::WLog() << "ERROR: rectangle checks failed";
        return 1;
    }

    wse::WLog() << "rectangle checks succeeded";
    return 0;
}
