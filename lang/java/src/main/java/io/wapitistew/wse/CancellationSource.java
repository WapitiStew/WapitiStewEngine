package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Requests cooperative cancellation of an in-flight XPT operation. Cancellation is observed, never
 * forced: the operation returns with {@link TransportErrorCode#CANCELLED} at its next observation
 * point.
 */
public final class CancellationSource implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.transportCancellationClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /** Creates a source that has not yet been cancelled. */
    public CancellationSource() {
        state = new State(Native.transportCancellationCreate());
        cleanable = CLEANER.register(this, state);
    }

    long handle() {
        return state.handle.get();
    }

    /** Requests cancellation. Calling this more than once is safe. */
    public void cancel() {
        Native.transportCancellationCancel(requireHandle());
    }

    /** Returns true once cancellation has been requested. */
    public boolean isCancellationRequested() {
        return Native.transportCancellationIsRequested(requireHandle());
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The cancellation source is closed.");
        return value;
    }

    /** Releases the native source. Calling this again is safe. */
    @Override public void close() {
        cleanable.clean();
    }
}
