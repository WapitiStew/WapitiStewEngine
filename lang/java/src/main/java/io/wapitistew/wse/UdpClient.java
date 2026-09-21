package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/**
 * A UDP socket. Bind it to a local endpoint, then send and receive datagrams with explicit
 * deadlines.
 */
public final class UdpClient implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.udpClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /** Creates an unbound client. */
    public UdpClient() {
        state = new State(Native.udpCreate());
        cleanable = CLEANER.register(this, state);
    }

    /** Returns the maximum number of bytes one datagram can carry. */
    public static long maximumDatagramSize() {
        return Native.udpMaximumDatagramSize();
    }

    /**
     * Binds the socket to a local endpoint. A port of zero requests an operating-system-assigned
     * port, which {@link #localEndpoint()} then reports.
     *
     * @param endpoint local host and port
     * @param context explicit deadline and optional cancellation
     */
    public void bind(Endpoint endpoint, OperationContext context) {
        Objects.requireNonNull(endpoint, "endpoint");
        Objects.requireNonNull(context, "context");
        Native.udpBind(requireHandle(), endpoint.host(), endpoint.port(),
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    /** Closes the socket without releasing the object. Calling this again is safe. */
    public void closeSocket() {
        Native.udpCloseSocket(requireHandle());
    }

    /** Returns true while the socket is open. */
    public boolean isOpen() {
        return Native.udpIsOpen(requireHandle());
    }

    /** Returns the bound local endpoint. */
    public Endpoint localEndpoint() {
        return Endpoint.fromNative(Native.udpLocalEndpoint(requireHandle()));
    }

    /**
     * Sends one datagram to a remote endpoint.
     *
     * @param endpoint remote host and port
     * @param data payload bytes
     * @param context explicit deadline and optional cancellation
     * @return the number of bytes sent
     */
    public long sendTo(Endpoint endpoint, byte[] data, OperationContext context) {
        Objects.requireNonNull(endpoint, "endpoint");
        Objects.requireNonNull(data, "data");
        Objects.requireNonNull(context, "context");
        return Native.udpSendTo(requireHandle(), endpoint.host(), endpoint.port(), data,
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    /**
     * Receives one datagram.
     *
     * @param maximumSize maximum payload size to accept
     * @param context explicit deadline and optional cancellation
     * @return the received datagram
     */
    public UdpDatagram receiveFrom(long maximumSize, OperationContext context) {
        Objects.requireNonNull(context, "context");
        return UdpDatagram.fromNative(Native.udpReceiveFrom(requireHandle(), maximumSize,
                context.timeoutMilliseconds(), context.cancellationHandle()));
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The UDP client is closed.");
        return value;
    }

    /** Closes the socket and releases the native client. Calling this again is safe. */
    @Override public void close() {
        cleanable.clean();
    }
}
