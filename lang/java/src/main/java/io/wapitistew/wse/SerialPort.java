package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/**
 * A serial port. Open it by device name and baud rate, then send and receive with explicit
 * deadlines.
 *
 * <p>Only the portable serial-port API is exposed. The callback-based, Windows-only legacy
 * connector is not part of this binding.
 */
public final class SerialPort implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.serialClose(value);
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /** Creates a closed port. */
    public SerialPort() {
        state = new State(Native.serialCreate());
        cleanable = CLEANER.register(this, state);
    }

    /**
     * Opens the port.
     *
     * @param deviceName platform device name, such as {@code COM3} or {@code /dev/ttyUSB0}
     * @param baudRate baud rate
     * @param context explicit deadline and optional cancellation
     */
    public void open(String deviceName, int baudRate, OperationContext context) {
        Objects.requireNonNull(deviceName, "deviceName");
        Objects.requireNonNull(context, "context");
        Native.serialOpen(requireHandle(), deviceName, baudRate,
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    /** Closes the port without releasing the object. Calling this again is safe. */
    public void closePort() {
        Native.serialClosePort(requireHandle());
    }

    /** Returns true while the port is open. */
    public boolean isOpen() {
        return Native.serialIsOpen(requireHandle());
    }

    /** Returns the opened device name, or an empty string while the port is closed. */
    public String deviceName() {
        return Native.serialDeviceName(requireHandle());
    }

    /** Returns the configured baud rate. */
    public int baudRate() {
        return Native.serialBaudRate(requireHandle());
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
        return Native.serialSend(requireHandle(), data,
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
        return Native.serialReceive(requireHandle(), maximumSize,
                context.timeoutMilliseconds(), context.cancellationHandle());
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "The serial port is closed.");
        return value;
    }

    /** Closes the port and releases the native resource. Calling this again is safe. */
    @Override public void close() {
        cleanable.clean();
    }
}
