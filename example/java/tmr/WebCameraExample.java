import io.wapitistew.wse.CameraCapability;
import io.wapitistew.wse.CameraDevice;
import io.wapitistew.wse.CameraFrame;
import io.wapitistew.wse.CameraStreamConfiguration;
import io.wapitistew.wse.CameraStreamProfile;
import io.wapitistew.wse.WebCamera;

/**
 * Captures one owned frame through the portable WSE WebCamera API.
 *
 * <p>This is the shortest of the four camera samples and the one to read first: enumerate, pick a
 * device and a stream profile, open, start, read one frame, and let the block close everything.
 * The other three build on this same opening. Build with {@code WSE_BUILD_TMR=ON} and
 * {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. The native library is located through the {@code -Dwse.runtime.path=...}
 * system property. That load is by absolute path, which on Windows does not add the containing
 * directory to the dependency search path, so the engine directory must also be on {@code PATH}.
 *
 * <p>Hardware-free in part: no camera attached is reported in one plain sentence and the sample
 * returns normally, so it runs on a machine that has none. A camera that is attached but advertises
 * no usable profile is treated as a real error instead, because something is present and cannot be
 * used.
 *
 * <p>Ownership runs in two layers, and both are released by leaving their block. The camera owns
 * the native session; the frame owns the pixel bytes, which are a native allocation the frame hands
 * out as a read-only view rather than copying into the Java heap.
 */
public final class WebCameraExample {
    private WebCameraExample() {}

    public static void main(String[] arguments) {
        // Enumeration is static and needs no session: it inspects devices this process does not
        // own. It raises an Unsupported WseException on a build without TMR.
        CameraDevice[] devices = WebCamera.enumerate();
        if (devices.length == 0) {
            System.out.println("No USB/UVC web camera is attached.");
            return;
        }

        // The first profile that advertises at least one output format wins. A profile is a native
        // format plus the formats TMR can hand back for it, and a device may list a profile with
        // none, which is why both lengths are checked. A real application would choose by size and
        // frame rate instead of taking the first the driver happens to list.
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
        if (device == null || profile == null)
            throw new IllegalStateException("No enumerated camera has a usable stream profile.");

        // The trailing true allows conversion: TMR may decode or repack the native format into the
        // requested output. With false the device would have to produce that output itself, and a
        // camera that cannot would fail to open.
        CameraStreamConfiguration configuration = new CameraStreamConfiguration(
                profile.nativeFormat(), profile.outputFormats()[0], true);
        // try-with-resources is the single owner. close() stops streaming, closes the device, and
        // releases the native camera, and it is terminal - a released camera cannot be reopened.
        // Use closeCamera() instead to end a session and keep the object, as the resolution sample
        // does. Without the block a Cleaner performs the same release, but only at some later
        // garbage collection, which means holding the device open against other applications.
        try (WebCamera camera = new WebCamera()) {
            camera.open(device, configuration);
            camera.start();
            // 2000 ms is the deadline for this one frame. It has to absorb the first-frame delay
            // after start(), which covers exposure settling and buffer negotiation and far exceeds
            // one frame interval; a deadline near 33 ms would report a timeout before the camera
            // ever delivered. A timeout arrives as a WseException, which this sample lets escape.
            // The frame owns its buffer, so leaving the block frees the pixels at once.
            try (CameraFrame frame = camera.readFrame(2000)) {
                System.out.println(frame.width() + "x" + frame.height()
                        + " bytes=" + frame.data().buffer().remaining());
            } finally {
                // Stopping is explicit for symmetry with start(); close() would stop as well. The
                // guard matters because a stream that already failed may have stopped itself.
                if (camera.isStreaming()) camera.stop();
            }
        }
    }
}
