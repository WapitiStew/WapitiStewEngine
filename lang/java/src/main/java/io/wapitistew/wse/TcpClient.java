package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/**
 * A caller-confined TCP connection. Use one instance from one thread at a time and close it when
 * finished; closing disconnects an open connection.
 *
 * <p>Every operation takes an {@link OperationContext} because XPT has no default timeout, and each
 * call blocks the calling thread until it finishes or that deadline elapses.
 */
public final class TcpClient implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.tcpClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /** Creates a disconnected client. */
    public TcpClient() {
        state = new State(Native.tcpCreate());
        cleanable = CLEANER.register(this, state);
    }

    /**
     * Connects synchronously to a remote endpoint.
     *
     * @param endpoint remote host and port
     * @param context explicit deadline and optional cancellation
     */
    public void connect(Endpoint endpoint, OperationContext context) {
        Objects.requireNonNull(endpoint, "endpoint");
        Objects.requireNonNull(context, "context");
        Native.tcpConnect(requireHandle(), endpoint.host(), endpoint.port(),
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    /** Closes the connection. Calling this again is safe. */
    public void disconnect() {
        Native.tcpDisconnect(requireHandle());
    }

    /** Returns true while the local connection state is held. */
    public boolean isConnected() {
        return Native.tcpIsConnected(requireHandle());
    }

    /**
     * Checks for an operating-system-observable peer FIN or reset without consuming pending data.
     * Success is not an end-to-end liveness proof.
     */
    public void checkPeerConnection() {
        Native.tcpCheckPeerConnection(requireHandle());
    }

    /** Returns the connected remote endpoint. */
    public Endpoint remoteEndpoint() {
        return Endpoint.fromNative(Native.tcpRemoteEndpoint(requireHandle()));
    }

    /** Returns the operating-system-selected local endpoint. */
    public Endpoint localEndpoint() {
        return Endpoint.fromNative(Native.tcpLocalEndpoint(requireHandle()));
    }

    /**
     * Sends the complete buffer synchronously.
     *
     * @param data bytes to send
     * @param context explicit deadline and optional cancellation
     * @return the number of bytes sent
     */
    public long send(byte[] data, OperationContext context) {
        Objects.requireNonNull(data, "data");
        Objects.requireNonNull(context, "context");
        return Native.tcpSend(requireHandle(), data,
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    /**
     * Receives one chunk up to a maximum size synchronously.
     *
     * @param maximumSize maximum number of bytes to receive
     * @param context explicit deadline and optional cancellation
     * @return the received bytes
     */
    public byte[] receive(long maximumSize, OperationContext context) {
        Objects.requireNonNull(context, "context");
        return Native.tcpReceive(requireHandle(), maximumSize,
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The TCP client is closed.");
        return value;
    }

    /** Disconnects and releases the native client. Calling this again is safe. */
    @Override public void close() {
        cleanable.clean();
    }
}
