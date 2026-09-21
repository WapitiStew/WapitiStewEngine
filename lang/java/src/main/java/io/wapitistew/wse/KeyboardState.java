package io.wapitistew.wse;

/**
 * A consistent keyboard-input snapshot taken at one update point. {@code true} means pressed.
 * Index meanings follow {@code iui/depend/Metadata.h}.
 *
 * @param ascii ASCII key states indexed by ASCII code
 * @param function function key states, F1 first
 * @param arrow arrow key states in left, up, down, right order
 * @param lock lock key states in CapsLock, NumLock, ScrollLock order
 * @param command command key states such as Space, Enter, and Tab
 */
public record KeyboardState(
        boolean[] ascii,
        boolean[] function,
        boolean[] arrow,
        boolean[] lock,
        boolean[] command) {

    /** Number of ASCII key slots. */
    public static final int ASCII_COUNT = 128;
    /** Number of function key slots, F1 through F24. */
    public static final int FUNCTION_COUNT = 24;
    /** Number of arrow key slots. */
    public static final int ARROW_COUNT = 4;
    /** Number of lock key slots. */
    public static final int LOCK_COUNT = 3;
    /** Number of command key slots. */
    public static final int COMMAND_COUNT = 9;

    /** Returns true when no key in this snapshot is pressed. */
    public boolean isIdle() {
        return !hasAny(ascii) && !hasAny(function) && !hasAny(arrow)
                && !hasAny(lock) && !hasAny(command);
    }

    private static boolean hasAny(boolean[] states) {
        for (boolean state : states) {
            if (state) return true;
        }
        return false;
    }
}
