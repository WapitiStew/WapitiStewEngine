package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.concurrent.atomic.AtomicLong;

/** Cancelable native timer callback. */
public final class TimerHandle implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.timerClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    TimerHandle(long handle) {
        state = new State(handle);
        cleanable = CLEANER.register(this, state);
    }

    public void cancel() {
        long value = state.handle.get();
        if (value != 0L) Native.timerCancel(value);
    }

    public boolean isClosed() { return state.handle.get() == 0L; }
    @Override public void close() { cleanable.clean(); }
}
