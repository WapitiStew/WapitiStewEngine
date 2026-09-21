package io.wapitistew.wse;

import java.lang.ref.Cleaner;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/** Portable USB/UVC web-camera owner. */
public final class WebCamera implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();
    private static final class State implements Runnable {
        private final AtomicLong handle;
        State(long handle) { this.handle = new AtomicLong(handle); }
        @Override public void run() {
            long value = handle.getAndSet(0L);
            if (value != 0L) Native.webCameraClose(value);
        }
    }
    private final State state;
    private final Cleaner.Cleanable cleanable;

    private static void ensureAvailable() {
        if (!WseRuntime.info().hasTmr())
            throw new WseException(9, 1, 0L, "WSE was built without Tmr.");
    }

    public WebCamera() {
        ensureAvailable();
        state = new State(Native.webCameraCreate());
        cleanable = CLEANER.register(this, state);
    }

    private long requireHandle() {
        long value = state.handle.get();
        if (value == 0L) throw new WseException(3, 1, 0L, "WebCamera is closed.");
        return value;
    }

    public static CameraDevice[] enumerate(CameraBackend backend) {
        ensureAvailable();
        return Native.webCameraEnumerate(Objects.requireNonNull(backend, "backend").code());
    }

    public static CameraDevice[] enumerate() { return enumerate(CameraBackend.AUTOMATIC); }

    public static CameraCapability capabilities(CameraDevice device) {
        ensureAvailable();
        return Native.webCameraCapabilities(Objects.requireNonNull(device, "device"));
    }

    public void open(CameraDevice device) {
        Native.webCameraOpen(requireHandle(), Objects.requireNonNull(device, "device"), null);
    }

    public void open(CameraDevice device, CameraStreamConfiguration configuration) {
        Native.webCameraOpen(requireHandle(), Objects.requireNonNull(device, "device"),
                Objects.requireNonNull(configuration, "configuration"));
    }

    public void start() { Native.webCameraStart(requireHandle(), null); }
    public void start(CameraFrameCallback callback) {
        Native.webCameraStart(requireHandle(), Objects.requireNonNull(callback, "callback"));
    }
    public void stop() { Native.webCameraStop(requireHandle()); }
    public CameraFrame readFrame(int timeoutMilliseconds) {
        return Native.webCameraReadFrame(requireHandle(), timeoutMilliseconds);
    }
    public CameraCapability currentCapabilities() {
        return Native.webCameraCurrentCapabilities(requireHandle());
    }
    public CameraControlCapability controlCapability(CameraControl control) {
        return Native.webCameraControlCapability(requireHandle(), Objects.requireNonNull(control, "control").code());
    }
    public CameraControlValue getControl(CameraControl control) {
        return Native.webCameraGetControl(requireHandle(), Objects.requireNonNull(control, "control").code());
    }
    public void setControl(CameraControlValue value) {
        Native.webCameraSetControl(requireHandle(), Objects.requireNonNull(value, "value"));
    }
    public CameraExtensionUnitValue getExtensionUnit(CameraExtensionUnitSelector selector) {
        return Native.webCameraGetExtensionUnit(requireHandle(), Objects.requireNonNull(selector, "selector"));
    }
    public void setExtensionUnit(CameraExtensionUnitValue value) {
        Native.webCameraSetExtensionUnit(requireHandle(), Objects.requireNonNull(value, "value"));
    }
    public boolean isOpen() {
        long value = state.handle.get();
        return value != 0L && Native.webCameraIsOpen(value);
    }
    public boolean isStreaming() {
        long value = state.handle.get();
        return value != 0L && Native.webCameraIsStreaming(value);
    }
    public boolean isClosed() { return state.handle.get() == 0L; }

    /**
     * Stops streaming and closes the device without releasing the object, exactly as
     * {@link SerialPort#closePort()} does for a port. The resolution is fixed when a session opens,
     * so changing it means ending this session and calling {@code open} again on the same camera.
     * Calling this again is safe.
     */
    public void closeCamera() {
        Native.webCameraCloseCamera(requireHandle());
    }

    /**
     * Closes the device and releases the native camera. This is terminal: a released camera cannot
     * be reopened, so end a session with {@link #closeCamera()} when the object is still needed.
     * Calling this again is safe.
     */
    @Override public void close() { cleanable.clean(); }
}
