import io.wapitistew.wse.CameraCapability;
import io.wapitistew.wse.CameraControl;
import io.wapitistew.wse.CameraControlCapability;
import io.wapitistew.wse.CameraControlMode;
import io.wapitistew.wse.CameraControlValue;
import io.wapitistew.wse.CameraDevice;
import io.wapitistew.wse.CameraFrame;
import io.wapitistew.wse.CameraStreamConfiguration;
import io.wapitistew.wse.CameraStreamProfile;
import io.wapitistew.wse.WebCamera;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;

/**
 * Reads the TMR exposure capability of the first usable web camera, captures one frame, moves the
 * exposure to a different value inside the advertised range, captures a second frame, and reports
 * both so the change is visible. The original setting is restored before the sample returns.
 *
 * <p>Hardware-free by design: no camera, no usable stream profile, or a camera that refuses a
 * manual exposure is reported in one plain sentence and the sample returns normally. Build with
 * {@code WSE_BUILD_TMR=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. Like every other WSE Java sample, the native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, so this
 * sample defines no arguments of its own. That load is by absolute path, which on Windows does not
 * add the containing directory to the dependency search path, so the engine directory must also be
 * on {@code PATH}.
 *
 * <p>The contract worth taking away: a control is described before it is used. The advertised
 * capability carries the range, the step, the unit, and whether the control can be read or written
 * at all, and a value outside that range is rejected as a validation error before the driver ever
 * sees it. Nothing below assumes a fixed range - every value is derived from what the device said.
 *
 * <p>The C++ counterpart {@code example/cpp/tmr/exposure_control.cpp} saves a BMP before and after
 * the change. The Java binding exposes no image encoder, so the nearest bound thing is done here:
 * each captured frame is reported by its descriptor - sequence, size, pixel format, and byte count
 * - and the exposure value read back from the device is printed alongside it.
 */
public final class ExposureControlExample {
    private ExposureControlExample() {}

    /** Deadline for one frame. 2000 ms absorbs the first-frame delay after start(), which is far
     * longer than one frame interval; a deadline near the frame period would report a timeout
     * before the camera delivered anything. A longer exposure also slows the sensor down, so this
     * value has to stay comfortably above the slowest exposure the range below can select. */
    private static final int READ_TIMEOUT_MILLISECONDS = 2000;
    /** Frames discarded after the write so the sensor settles on the new exposure. */
    // Five is a practical figure for UVC hardware: a write reaches the sensor a few frames later,
    // so a smaller count risks reporting the old exposure as if it were the new one, and a larger
    // one only costs time.
    private static final int SETTLE_FRAMES = 5;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    private static String describe(CameraFrame frame) {
        return "sequence=" + frame.sequence()
                + " " + frame.width() + "x" + frame.height()
                + " format=" + frame.pixelFormat()
                + " bytes=" + frame.data().buffer().remaining();
    }

    /**
     * Returns the current exposure value, or null when the device refuses the read.
     *
     * <p>Reading talks to the driver rather than to a cached value, so it can fail on its own even
     * when the capability said the control is readable. Null is returned instead of rethrowing
     * because every caller below has something sensible to do without the value.
     */
    private static CameraControlValue readExposure(WebCamera camera) {
        try {
            return camera.getControl(CameraControl.EXPOSURE);
        } catch (WseException failure) {
            reportFailure("read exposure", failure);
            return null;
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

        // The first profile that advertises at least one output format wins; a device may list a
        // profile with none, which is why both lengths are checked. The choice is incidental here,
        // because exposure is a device control and does not depend on the profile.
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

        // The trailing true allows conversion from the native format into the requested output;
        // with false the device itself would have to produce that output.
        CameraStreamConfiguration configuration = new CameraStreamConfiguration(
                profile.nativeFormat(), profile.outputFormats()[0], true);
        System.out.println("camera: " + device.displayName());

        // try-with-resources is the single owner; leaving it closes the camera session.
        // close() is terminal: it stops streaming, closes the device, and releases the native
        // camera. Streaming has to be running before a frame can be read, and the controls below
        // are read and written on the open session, not on the enumerated device.
        try (WebCamera camera = new WebCamera()) {
            camera.open(device, configuration);
            camera.start();
            try {
                changeExposure(camera);
            } finally {
                // Stopping runs only after changeExposure has returned, so the exposure it restores
                // is written while the session is still streaming, exactly as the change was.
                if (camera.isStreaming()) camera.stop();
            }
        } catch (WseException failure) {
            // A camera that disappears or refuses the session is a reportable state, not a defect.
            reportFailure("camera session", failure);
        }
    }

    private static void changeExposure(WebCamera camera) {
        // The advertised capability carries the valid range, step, unit, and access.
        CameraControlCapability exposure;
        try {
            exposure = camera.controlCapability(CameraControl.EXPOSURE);
        } catch (WseException failure) {
            reportFailure("exposure capability", failure);
            return;
        }
        System.out.println("exposure capability: range=[" + exposure.minimum() + ", "
                + exposure.maximum() + "] step=" + exposure.step()
                + " default=" + exposure.defaultValue()
                + " unit=" + exposure.unit()
                + " manual=" + exposure.supportsManual()
                + " automatic=" + exposure.supportsAutomatic()
                + " readable=" + exposure.readable()
                + " writable=" + exposure.writable());

        CameraControlValue original = exposure.readable() ? readExposure(camera) : null;
        if (original != null) {
            System.out.println("exposure before: value=" + original.value()
                    + " physical=" + exposure.physicalFromValue(original.value())
                    + " mode=" + original.mode());
        }

        // The first frame is captured with whatever exposure the device already had.
        // The frame owns its pixel buffer, so the block frees it as soon as the descriptor is read.
        try (CameraFrame before = camera.readFrame(READ_TIMEOUT_MILLISECONDS)) {
            System.out.println("frame before: " + describe(before));
        }

        if (!exposure.writable() || !exposure.supportsManual()) {
            System.out.println("This camera does not accept a manual exposure; nothing to change.");
            return;
        }

        // Move to a clearly different position on the normalized 0.0-1.0 scale.
        // Normalized means a fraction of the advertised range, snapped to the advertised step, so
        // the same 0.25 lands on a valid value on any device without this code knowing the units.
        // 0.75 is the fallback for the case where the device already sits at a quarter scale; both
        // are arbitrary, and any two clearly separated fractions would serve.
        long current = original == null ? exposure.defaultValue() : original.value();
        long target = exposure.valueFromNormalized(0.25);
        if (target == current) {
            target = exposure.valueFromNormalized(0.75);
        }
        if (target == current) {
            System.out.println("The exposure range offers no second value; nothing to change.");
            return;
        }

        // MANUAL is what makes the value stick: under AUTOMATIC the device keeps driving exposure
        // itself and a written value would be overwritten within a frame or two.
        try {
            camera.setControl(
                    new CameraControlValue(CameraControl.EXPOSURE, CameraControlMode.MANUAL, target));
        } catch (WseException failure) {
            reportFailure("write exposure", failure);
            return;
        }

        // The sensor needs a few frames before the new exposure reaches the output.
        for (int skipped = 0; skipped < SETTLE_FRAMES; ++skipped) {
            // Explicit close rather than try-with-resources: the frame is discarded unread.
            // Dropping it without the close would still be released eventually by a Cleaner, but
            // five frames of native pixels would sit there until a garbage collection.
            camera.readFrame(READ_TIMEOUT_MILLISECONDS).close();
        }

        // Reading back rather than trusting the write: a device may snap the value to its own grid,
        // so what it reports is the truth and the requested target is only the fallback for a
        // device that refuses the read.
        CameraControlValue applied = exposure.readable() ? readExposure(camera) : null;
        System.out.println("exposure after: value=" + (applied == null ? target : applied.value())
                + " physical=" + exposure.physicalFromValue(applied == null ? target : applied.value())
                + " mode=" + (applied == null ? CameraControlMode.MANUAL : applied.mode()));

        try (CameraFrame after = camera.readFrame(READ_TIMEOUT_MILLISECONDS)) {
            System.out.println("frame after: " + describe(after));
        }

        System.out.println("exposure changed from " + current + " to " + target
                + " (normalized " + exposure.normalizedFromValue(current)
                + " -> " + exposure.normalizedFromValue(target) + ")");

        // Restore the setting the device had before this sample ran.
        // A camera control is device state, not process state: it survives close() and the next
        // application to open the camera inherits it. Restoring is why the original value and its
        // mode were captured as a whole above and are written back unchanged.
        if (original != null) {
            try {
                camera.setControl(original);
                System.out.println("Restored the original exposure.");
            } catch (WseException failure) {
                reportFailure("restore exposure", failure);
            }
        }
    }
}
