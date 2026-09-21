import io.wapitistew.wse.CameraCapability;
import io.wapitistew.wse.CameraDevice;
import io.wapitistew.wse.CameraFrame;
import io.wapitistew.wse.CameraStreamConfiguration;
import io.wapitistew.wse.CameraStreamProfile;
import io.wapitistew.wse.WebCamera;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;

/**
 * Reads ten consecutive frames from the first usable web camera and reports each frame's sequence
 * number and the delta to the previous frame's monotonic timestamp, followed by a summary line.
 *
 * <p>Hardware-free by design: no camera or no usable stream profile is reported in one plain
 * sentence and the sample returns normally; a stream that stops early prints what did arrive. Build
 * with {@code WSE_BUILD_TMR=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. Like every other WSE Java sample, the native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, so this
 * sample defines no arguments of its own. That load is by absolute path, which on Windows does not
 * add the containing directory to the dependency search path, so the engine directory must also be
 * on {@code PATH}.
 *
 * <p>The C++ counterpart {@code example/cpp/tmr/video_stream.cpp} takes its ten frames through the
 * push callback and converts each one into a Core image. The binding does expose the push form,
 * {@code WebCamera.start(CameraFrameCallback)}, but it exposes no Core image type, so the nearest
 * bound thing is done here: a deterministic pull loop over {@code readFrame} that reports each
 * frame's descriptor and inter-frame timing instead of a converted image.
 */
public final class VideoStreamExample {
    private VideoStreamExample() {}

    /** Frames to pull. Ten is enough to show nine intervals and still finish in about a third of a
     * second at 30 fps; a much larger count only makes the sample run longer. */
    private static final int TARGET_FRAMES = 10;
    /** Deadline for one frame. 2000 ms absorbs the first-frame delay after start(), which is far
     * longer than the steady-state interval; a deadline near one frame period would report a
     * timeout before the camera delivered its first picture. A timeout is a WseException, so the
     * loop below ends and reports what did arrive. */
    private static final int READ_TIMEOUT_MILLISECONDS = 2000;
    /** Frame timestamps are nanoseconds; this divisor is only for readable output. */
    private static final double NANOSECONDS_PER_MILLISECOND = 1_000_000.0;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    public static void main(String[] arguments) {
        if (!WseRuntime.info().hasTmr()) {
            System.out.println("This WSE build does not include the Tmr component.");
            return;
        }

        // Enumeration is static and needs no session; an empty list means no camera, which is a
        // reportable state rather than a failure so the sample runs on a machine without one.
        CameraDevice[] devices = WebCamera.enumerate();
        if (devices.length == 0) {
            System.out.println("No USB/UVC web camera is attached.");
            return;
        }

        // The first profile that advertises at least one output format wins; a device may list a
        // profile with none, which is why both lengths are checked.
        CameraDevice device = null;
        CameraStreamProfile profile = null;
        for (CameraDevice candidate : devices) {
            CameraCapability capability = WebCamera.capabilities(candidate);
            if (capability.streamProfiles().length != 0
                    && capability.streamProfiles()[0].outputFormats().length != 0) {
                device = candidate;
                profile = capability.streamProfiles()[0];
                break;
            }
        }
        if (device == null || profile == null) {
            System.out.println("No enumerated camera has a usable stream profile.");
            return;
        }
        System.out.println("camera: " + device.displayName());

        // The trailing true allows conversion from the native format into the requested output;
        // with false the device itself would have to produce that output.
        CameraStreamConfiguration configuration = new CameraStreamConfiguration(
                profile.nativeFormat(), profile.outputFormats()[0], true);

        // Kept outside the block so the summary can still be printed after the session is closed.
        int received = 0;
        long firstTimestamp = 0L;
        long lastTimestamp = 0L;

        // try-with-resources is the single owner; leaving it closes the camera session.
        try (WebCamera camera = new WebCamera()) {
            camera.open(device, configuration);
            camera.start();
            try {
                long previousTimestamp = 0L;
                while (received < TARGET_FRAMES) {
                    // Each frame owns its pixel buffer, so every iteration frees the previous one
                    // before asking for the next; a loop that let them accumulate would hold ten
                    // frames of native memory until a garbage collection decided otherwise.
                    try (CameraFrame frame = camera.readFrame(READ_TIMEOUT_MILLISECONDS)) {
                        // A monotonic capture-side clock, not wall time: it never steps backwards
                        // and is only meaningful as a difference within one session.
                        long timestamp = frame.monotonicTimestampNanoseconds();
                        long delta = received == 0 ? 0L : timestamp - previousTimestamp;
                        ++received;
                        if (received == 1) firstTimestamp = timestamp;
                        lastTimestamp = timestamp;
                        previousTimestamp = timestamp;
                        System.out.println("frame " + received + "/" + TARGET_FRAMES
                                + ": sequence=" + frame.sequence()
                                + " timestamp_ns=" + timestamp
                                + " delta_ns=" + delta
                                + " delta_ms=" + (delta / NANOSECONDS_PER_MILLISECOND)
                                + " " + frame.width() + "x" + frame.height()
                                + " bytes=" + frame.data().buffer().remaining());
                    }
                }
            } catch (WseException failure) {
                // A stream that stops early is a reportable state, not a sample defect.
                reportFailure("read frame " + (received + 1), failure);
            } finally {
                if (camera.isStreaming()) camera.stop();
            }
        } catch (WseException failure) {
            reportFailure("camera session", failure);
            return;
        }

        if (received == 0) {
            System.out.println("stream complete: 0 of " + TARGET_FRAMES + " frames arrived");
            return;
        }
        // The span covers received-1 intervals, not received, so the average divides by that; the
        // sequence numbers printed above are what reveal a dropped frame, because a gap there with
        // a doubled interval here means the device skipped one rather than delivering late.
        long spanNanoseconds = lastTimestamp - firstTimestamp;
        double spanMilliseconds = spanNanoseconds / NANOSECONDS_PER_MILLISECOND;
        double averageInterval = received > 1 ? spanMilliseconds / (received - 1) : 0.0;
        double framesPerSecond = averageInterval > 0.0 ? 1000.0 / averageInterval : 0.0;
        System.out.println("stream complete: " + received + " of " + TARGET_FRAMES
                + " frames, span=" + spanMilliseconds + " ms"
                + ", average interval=" + averageInterval + " ms"
                + ", effective rate=" + framesPerSecond + " fps");
    }
}
