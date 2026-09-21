package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicLong;

/** Native-owned, read-only direct byte buffer with deterministic close. */
public final class NativeBuffer implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.bufferClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;
    private final ByteBuffer buffer;

    NativeBuffer(long handle, ByteBuffer buffer) {
        this.state = new State(handle);
        this.cleanable = CLEANER.register(this, state);
        this.buffer = buffer.asReadOnlyBuffer();
    }

    public ByteBuffer buffer() {
        if (state.handle.get() == 0L) throw new IllegalStateException("NativeBuffer is closed.");
        return buffer.asReadOnlyBuffer();
    }

    public int size() { return buffer.capacity(); }
    public boolean isClosed() { return state.handle.get() == 0L; }
    @Override public void close() { cleanable.clean(); }
}
