import io.wapitistew.wse.CameraCapability;
import io.wapitistew.wse.CameraDevice;
import io.wapitistew.wse.CameraFormat;
import io.wapitistew.wse.CameraFrame;
import io.wapitistew.wse.CameraPixelFormat;
import io.wapitistew.wse.CameraStreamConfiguration;
import io.wapitistew.wse.CameraStreamProfile;
import io.wapitistew.wse.WebCamera;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;

/**
 * Captures one frame at the first advertised stream profile, closes the session, reopens the camera
 * at a profile with a different frame size, captures again, and reports both sizes.
 *
 * <p>The resolution is fixed when the session opens, so changing it means closing and reopening.
 *
 * <p>Hardware-free by design: no camera, no usable stream profile, or a camera that advertises only
 * one resolution is reported in one plain sentence and the sample returns normally. Build with
 * {@code WSE_BUILD_TMR=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. Like every other WSE Java sample, the native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, so this
 * sample defines no arguments of its own. That load is by absolute path, which on Windows does not
 * add the containing directory to the dependency search path, so the engine directory must also be
 * on {@code PATH}.
 *
 * <p>One difference from the C++ counterpart {@code example/cpp/tmr/resolution_change.cpp}: it
 * saves each capture as a BMP, and the Java binding exposes no image encoder, so the nearest bound
 * thing is done here and each capture is reported by its frame descriptor. Like the C++ sample,
 * this one reuses a single {@code WebCamera} across both sessions; {@code closeCamera()} ends a
 * session and leaves the camera reopenable, while {@code close()} releases the native object.
 */
public final class ResolutionChangeExample {
    private ResolutionChangeExample() {}

    /** Deadline for one frame. 2000 ms absorbs the first-frame delay after start(); each capture
     * here is the first frame of a freshly opened session, which is the slowest one a session ever
     * delivers, so a deadline near the frame interval would time out on both captures. */
    private static final int READ_TIMEOUT_MILLISECONDS = 2000;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    private static String describeFormat(CameraFormat format) {
        return format.width() + "x" + format.height();
    }

    /**
     * Opens one session on the given camera, captures one frame, and returns its description.
     *
     * <p>Any failure leaves as a {@code WseException} for the caller to report, and the session is
     * ended either way. The frame's own dimensions are read rather than the requested ones, because
     * the descriptor is what proves the device honoured the profile.
     */
    private static String captureOnce(
            WebCamera camera, CameraDevice device, CameraStreamConfiguration configuration) {
        camera.open(device, configuration);
        try {
            camera.start();
            // The frame owns its pixel buffer; the block frees it before this method returns, so
            // nothing native outlives the session it came from.
            try (CameraFrame frame = camera.readFrame(READ_TIMEOUT_MILLISECONDS)) {
                return frame.width() + "x" + frame.height()
                        + " format=" + frame.pixelFormat()
                        + " sequence=" + frame.sequence()
                        + " bytes=" + frame.data().buffer().remaining();
            }
        } finally {
            // Ends this session only. The camera object stays usable, so the next profile opens
            // on the same instance. It also stops streaming, and calling it twice is safe, so it
            // is correct on the failure path as well - which is why it sits in a finally rather
            // than after the capture.
            camera.closeCamera();
        }
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

        // The whole capability is kept here, not just one profile: the second resolution is chosen
        // from the same advertised list further down.
        CameraDevice device = null;
        CameraCapability capability = null;
        for (CameraDevice candidate : devices) {
            CameraCapability candidateCapability = WebCamera.capabilities(candidate);
            if (candidateCapability.streamProfiles().length != 0
                    && candidateCapability.streamProfiles()[0].outputFormats().length != 0) {
                device = candidate;
                capability = candidateCapability;
                break;
            }
        }
        if (device == null || capability == null) {
            System.out.println("No enumerated camera has a usable stream profile.");
            return;
        }
        System.out.println("camera: " + device.displayName());

        // The trailing true allows conversion from the native format into the requested output;
        // with false the device itself would have to produce that output, which narrows how many
        // profiles are usable and would work against picking a second one below.
        CameraStreamProfile firstProfile = capability.streamProfiles()[0];
        CameraStreamConfiguration first = new CameraStreamConfiguration(
                firstProfile.nativeFormat(), firstProfile.outputFormats()[0], true);

        // A second profile with a different frame size; keep the same output format when it is
        // offered so only the resolution differs between the two captures.
        CameraStreamConfiguration second = null;
        for (CameraStreamProfile profile : capability.streamProfiles()) {
            boolean differentSize =
                    profile.nativeFormat().width() != first.nativeFormat().width()
                    || profile.nativeFormat().height() != first.nativeFormat().height();
            if (!differentSize || profile.outputFormats().length == 0) continue;
            // A profile that offers the first capture's output format is preferred so the two
            // descriptors differ in size alone; otherwise the format changes too and the
            // comparison at the end is less direct.
            CameraPixelFormat output = profile.supportsOutput(first.outputFormat())
                    ? first.outputFormat()
                    : profile.outputFormats()[0];
            second = new CameraStreamConfiguration(profile.nativeFormat(), output, true);
            break;
        }

        // One camera object serves both sessions: closeCamera() ends a session and leaves the
        // object reopenable, and try-with-resources releases it once for good at the end.
        // Getting this pair the wrong way round is the mistake this sample exists to prevent:
        // close() is terminal, so calling it between the two captures would leave the second open
        // to fail on a released camera.
        try (WebCamera camera = new WebCamera()) {
            try {
                System.out.println("first profile: " + describeFormat(first.nativeFormat())
                        + " at " + first.nativeFormat().framesPerSecond() + " fps"
                        + " output=" + first.outputFormat());
                System.out.println("first capture: " + captureOnce(camera, device, first));
            } catch (WseException failure) {
                reportFailure("capture at " + describeFormat(first.nativeFormat()), failure);
                return;
            }

            // A fixed-resolution camera is a real device, not a broken one, so this returns
            // normally after the first capture has already been reported.
            if (second == null) {
                System.out.println("The camera advertises only one resolution; nothing to change.");
                return;
            }

            try {
                System.out.println("second profile: " + describeFormat(second.nativeFormat())
                        + " at " + second.nativeFormat().framesPerSecond() + " fps"
                        + " output=" + second.outputFormat());
                System.out.println("second capture: " + captureOnce(camera, device, second));
                System.out.println("captured both resolutions: "
                        + describeFormat(first.nativeFormat()) + " and "
                        + describeFormat(second.nativeFormat()));
            } catch (WseException failure) {
                reportFailure("capture at " + describeFormat(second.nativeFormat()), failure);
            }
        }
    }
}
