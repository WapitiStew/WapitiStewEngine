package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Accumulates camera frames of one shape and produces their average.
 *
 * <p>Only an accumulation buffer is held, independently of how many frames were added. Adding a
 * frame whose description differs fails and leaves the accumulation unchanged.
 */
public final class CameraFrameAccumulator implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.cameraAccumulatorClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    public CameraFrameAccumulator() {
        if (!WseRuntime.info().hasTmr())
            throw new WseException(9, 1, 0L, "WSE was built without Tmr.");
        state = new State(Native.cameraAccumulatorCreate());
        cleanable = CLEANER.register(this, state);
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "CameraFrameAccumulator is closed.");
        return value;
    }

    /** Adds one frame. */
    public void add(CameraFrame frame) {
        Objects.requireNonNull(frame, "frame");
        Native.cameraAccumulatorAdd(requireHandle(),
                frame.width(), frame.height(), frame.pixelFormat().code(), frame.rowStride(),
                frame.sequence(), frame.monotonicTimestampNanoseconds(), frame.data().buffer());
    }

    /** Number of accumulated frames. */
    public long count() { return Native.cameraAccumulatorCount(requireHandle()); }

    /** Returns the averaged frame, rounded to nearest. */
    public CameraFrame average() { return Native.cameraAccumulatorAverage(requireHandle()); }

    /** Discards the accumulation. */
    public void reset() { Native.cameraAccumulatorReset(requireHandle()); }

    @Override public void close() { cleanable.clean(); }
}
