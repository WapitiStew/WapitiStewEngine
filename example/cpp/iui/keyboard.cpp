// @file keyboard.cpp
// @brief Portable IUI keyboard readiness check and one consistent key snapshot.
//
// Hardware-free by design: a machine without a readable keyboard reports that through the
// readiness state instead of failing. Build with WSE_BUILD_IUI=ON and link WSE::Iui.
//
// The whole file returns 0. A headless build agent, a container with no input device, and a
// session that was refused read permission are all expected outcomes rather than errors, so
// the readiness state is the report and the exit code stays a build-level verdict. Only an
// unavailable keyboard leaves early; nothing here is a swallowed failure.
//
// The one thing a reader must take away: a snapshot that arrives while the monitor is not
// Ready reads as every key released. That is the absence of a reading, not a reading of
// nothing, and treating it as proof that no key is held is the mistake this sample avoids by
// checking isAvailable() first.

#include <iui/stew.h>
#include <wse/binding/IuiErrorAdapter.h>
#include <wse/stew.h>

#include <chrono>
#include <cstddef>
#include <thread>

namespace
{

const char* stateName( const wse::iui::KeyboardAccessState state_in )
{
    switch ( state_in )
    {
        case wse::iui::KeyboardAccessState::Starting:         return "Starting";
        case wse::iui::KeyboardAccessState::Ready:            return "Ready";
        case wse::iui::KeyboardAccessState::Unavailable:      return "Unavailable";
        case wse::iui::KeyboardAccessState::PermissionDenied: return "PermissionDenied";
        case wse::iui::KeyboardAccessState::Disconnected:     return "Disconnected";
    }
    return "Unknown";
}

//! Counts the pressed entries of one key group. Every group is a fixed-size array of flags
//! indexed by key rather than a list of events, so its length is the group's key count:
//! 128 ascii, 24 function, 4 arrow, 3 lock, 9 command.
template <std::size_t Count>
std::size_t countPressed( const std::array<bool, Count>& group_in )
{
    std::size_t pressed = 0U;
    for ( const bool state : group_in )
    {
        if ( state ) ++pressed;
    }
    return pressed;
}

} // namespace

int main()
{
    wse::registDefaultLog();

    // Construction starts the monitoring thread; the object is the single RAII owner, and
    // destruction stops and joins that thread, so the keyboard must outlive every read of it.
    // Copying is permitted but spawns a second independent worker rather than sharing this
    // one, which is why the object is kept by name and never passed around by value.
    wse::iui::Keyboard keyboard;

    // Linux starts at Starting and needs a worker probe; Windows starts at Ready, meaning
    // polling is enabled, not that a physical device was probed. Twenty attempts of 25 ms is a
    // half-second budget, and the loop leaves the instant the state turns Ready, so the wait
    // costs nothing on a machine that is ready at once. A shorter budget would report Starting
    // on a loaded machine and look like a missing keyboard; a longer one only delays the
    // report, because the worker keeps re-probing either way and a hotplug or a corrected
    // permission can reach Ready later without rebuilding the object.
    for ( int attempt = 0; attempt < 20 && !keyboard.isAvailable(); ++attempt )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
    }

    const wse::iui::KeyboardAccessState state = keyboard.accessState();
    wse::WLog() << "keyboard state:" << stateName( state );

    if ( !keyboard.isAvailable() )
    {
        // Not a failure of this sample: report why the keyboard cannot be read and stop.
        // IUI has no error type of its own, so the adapter turns the readiness state into the
        // portable category a binding would branch on — Ready alone maps to success. Branch on
        // category() or ok(); the numeric code() is the state value, and because Starting is
        // zero it collides with the success code.
        const wse::binding::Error reason = wse::binding::fromIuiKeyboardState( state );
        wse::WLog() << "keyboard is not readable:" << reason.message();
        return 0;
    }

    // One snapshot is one consistent point in time; do not combine several separate reads.
    // All five groups are copied under one lock here. The per-group accessors each take their
    // own snapshot internally, so reading ascii_state() and then command_state() would report
    // two different moments and could show a chord that was never actually held together.
    // The result is an owned copy, so the worker may publish again without disturbing it.
    const wse::iui::KeyboardState snapshot = keyboard.snapshot();
    wse::WLog() << "pressed ascii keys:" << countPressed( snapshot.ascii );
    wse::WLog() << "pressed function keys:" << countPressed( snapshot.function );
    wse::WLog() << "pressed arrow keys:" << countPressed( snapshot.arrow );
    wse::WLog() << "pressed lock keys:" << countPressed( snapshot.lock );
    wse::WLog() << "pressed command keys:" << countPressed( snapshot.command );

    // getASCII() is a fresh read rather than a lookup into the snapshot above, so it can differ
    // from it. It returns int8_t and reports zero when nothing is pressed; on a chord it
    // returns the lowest code, because it takes the first set flag and stops. Widening to int
    // states at the call site that the value is wanted as a code and not as a character.
    const int ascii = static_cast<int>( keyboard.getASCII() );
    wse::WLog() << "pressed ascii code:" << ascii;
    return 0;
}
