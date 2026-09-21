package io.wapitistew.wse;

/** Readiness state of the keyboard backend. */
public enum KeyboardAccessState {
    /** The backend is still starting and has produced no reading yet. */
    STARTING(0),
    /** Readable sources on Linux; enabled polling on Windows, without a physical-device probe. */
    READY(1),
    /** No readable keyboard source exists on this machine. */
    UNAVAILABLE(2),
    /** The operating system refused to let this process read the keyboard. */
    PERMISSION_DENIED(3),
    /** The keyboard source was disconnected after it had been readable. */
    DISCONNECTED(4);

    private final int value;

    KeyboardAccessState(int value) {
        this.value = value;
    }

    /** Returns the numeric value shared with the other language bindings. */
    public int value() {
        return value;
    }

    /**
     * Returns the state matching a numeric value.
     *
     * @param value numeric state reported by the native binding
     * @return the matching state
     * @throws IllegalArgumentException when no state matches
     */
    public static KeyboardAccessState fromValue(int value) {
        for (KeyboardAccessState state : values()) {
            if (state.value == value) return state;
        }
        throw new IllegalArgumentException("Unknown keyboard access state: " + value);
    }
}
