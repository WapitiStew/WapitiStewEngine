package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Reads physical keyboard input through IUI. Construction starts a monitoring thread, so create
 * one instance and close it when finished.
 *
 * <p>The Java binding exposes polling only. Read {@link #snapshot()} for one consistent point in
 * time rather than combining several independent reads.
 */
public final class Keyboard implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.keyboardClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /** Creates a keyboard and starts monitoring. */
    public Keyboard() {
        state = new State(Native.keyboardCreate());
        cleanable = CLEANER.register(this, state);
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The WSE keyboard is closed.");
        return value;
    }

    /**
     * Returns the current backend readiness state. Reading the state succeeds even when the
     * keyboard is not readable.
     */
    public KeyboardAccessState accessState() {
        return KeyboardAccessState.fromValue(Native.keyboardAccessState(requireHandle()));
    }

    /** Returns true for Ready. Windows polling readiness does not prove physical presence. */
    public boolean isAvailable() {
        return Native.keyboardIsAvailable(requireHandle());
    }

    /**
     * Reads every key group from one update point.
     *
     * @return a consistent snapshot of the current key states
     * @throws WseException when the keyboard is not readable; the category distinguishes a backend
     *     that is still starting, unavailable, refused, or disconnected
     */
    public KeyboardState snapshot() {
        boolean[][] groups = Native.keyboardSnapshot(requireHandle());
        return new KeyboardState(groups[0], groups[1], groups[2], groups[3], groups[4]);
    }

    /**
     * Reads one pressed ASCII code. Multiple simultaneous input is unsupported and the smaller
     * code wins.
     *
     * @return the pressed ASCII code, or zero when no ASCII key is pressed
     * @throws WseException when the keyboard is not readable
     */
    public byte pressedAscii() {
        return Native.keyboardPressedAscii(requireHandle());
    }

    /** Returns true once this keyboard has been closed. */
    public boolean isClosed() {
        return state.handle.get() == 0L;
    }

    /** Releases the native keyboard and stops monitoring. Calling this again is safe. */
    @Override public void close() {
        cleanable.clean();
    }
}
