package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.nio.ByteBuffer;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/** AutoCloseable owner for WSE Java operations. */
public final class WseRuntime implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.runtimeClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    public WseRuntime() {
        state = new State(Native.runtimeCreate());
        cleanable = CLEANER.register(this, state);
    }

    public static RuntimeInfo info() {
        int flags = Native.componentFlags();
        return new RuntimeInfo(
                Native.runtimeVersion(), Native.bindingAbiVersion(),
                (flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0,
                (flags & 8) != 0, (flags & 16) != 0, (flags & 32) != 0);
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The WSE Java runtime is closed.");
        return value;
    }

    public NativeBuffer copyFrame(ByteBuffer source) {
        Objects.requireNonNull(source, "source");
        ByteBuffer input = source.duplicate();
        ByteBuffer direct = ByteBuffer.allocateDirect(input.remaining());
        direct.put(input).flip();
        return Native.runtimeCopyFrame(requireHandle(), direct);
    }

    public void waitFor(long milliseconds) {
        Native.runtimeWait(requireHandle(), milliseconds);
    }

    public TimerHandle runAfter(long milliseconds, Runnable callback) {
        Objects.requireNonNull(callback, "callback");
        return new TimerHandle(Native.timerCreate(requireHandle(), milliseconds, callback));
    }

    public boolean isClosed() { return state.handle.get() == 0L; }
    @Override public void close() { cleanable.clean(); }
}
