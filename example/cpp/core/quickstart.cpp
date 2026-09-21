// @file quickstart.cpp
// @brief Hardware-free C++ quick start compiled and run by the documentation gate.
//
// Deliberately the smallest program that can fail for a real reason. Because the gate builds
// and runs it, it proves that one include of <wse/stew.h> is enough to reach the Core value
// types and that linking WSE::Core is enough to run them — so a quick start that has drifted
// out of date breaks the documentation build rather than a reader's first attempt.

#include <wse/stew.h>

int main()
{
    // Point2_ stores x and y as public members rather than behind getters, because it carries
    // values and does nothing else. The two exact comparisons are safe here for the same
    // reason they would not be after arithmetic: these doubles are the literals as written.
    const wse::float64_xy point( 10.0, 50.0 );
    return point.x == 10.0 && point.y == 50.0 ? 0 : 1;
}
