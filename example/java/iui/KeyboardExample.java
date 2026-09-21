import io.wapitistew.wse.Keyboard;
import io.wapitistew.wse.KeyboardAccessState;
import io.wapitistew.wse.KeyboardState;
import io.wapitistew.wse.WseException;

/**
 * Portable IUI keyboard readiness check and one consistent key snapshot.
 *
 * <p>Hardware-free by design: a machine without a readable keyboard reports that through the
 * readiness state instead of failing. Build with {@code WSE_BUILD_IUI=ON} and
 * {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. The native library is located through the {@code -Dwse.runtime.path=...}
 * system property. That load is by absolute path, which on Windows does not add the containing
 * directory to the dependency search path, so the engine directory must also be on {@code PATH}.
 *
 * <p>The binding is narrower than the C++ API here in two ways. It exposes polling only, so there
 * is no key-event callback to subscribe to. And the C++ counterpart
 * {@code example/cpp/iui/keyboard.cpp} turns an unreadable state into a reason through
 * {@code wse::binding::fromIuiKeyboardState}; that adapter is not bound, so the nearest bound thing
 * is done below - {@code snapshot()} is called for the exception it raises, whose category and
 * message carry the same reason.
 */
public final class KeyboardExample {
    private KeyboardExample() {}

    // A snapshot hands back one fixed-size flag array per key group, one entry per key in that
    // group, so counting the set entries is the whole summary a sample needs.
    private static int countPressed(boolean[] group) {
        int pressed = 0;
        for (boolean state : group) {
            if (state) ++pressed;
        }
        return pressed;
    }

    public static void main(String[] arguments) throws InterruptedException {
        // try-with-resources is the single owner; leaving it stops the monitoring thread.
        // Construction starts that thread and takes the native handle, and close() releases both.
        // Without the block nothing is leaked for good - a Cleaner still runs the same release -
        // but it runs at some later garbage collection, so the thread would outlive its use.
        try (Keyboard keyboard = new Keyboard()) {
            // The backend needs a moment before it reports its first reading.
            // Twenty attempts of 25 ms give it half a second. A shorter budget makes a slow backend
            // look unavailable; a longer one only delays the same answer, because a backend that is
            // refused or disconnected never becomes available however long this waits.
            for (int attempt = 0; attempt < 20 && !keyboard.isAvailable(); ++attempt) {
                Thread.sleep(25L);
            }

            // Reading the readiness state succeeds even when the keyboard cannot be read, so this
            // line is safe before the availability check below and says which of starting,
            // unavailable, refused, or disconnected applies.
            KeyboardAccessState state = keyboard.accessState();
            System.out.println("keyboard state: " + state);

            if (!keyboard.isAvailable()) {
                // Not a failure of this sample: snapshot() explains why the keyboard is unreadable.
                // A headless or remote machine lands here, and returning success is deliberate so
                // the sample runs anywhere. The call is made purely for the exception it raises,
                // which is the only bound way to obtain the reason text.
                try {
                    keyboard.snapshot();
                } catch (WseException failure) {
                    System.out.println("keyboard is not readable: " + failure.getMessage());
                }
                return;
            }

            // One snapshot is one consistent point in time; do not combine several separate reads.
            KeyboardState snapshot = keyboard.snapshot();
            System.out.println("pressed ascii keys: " + countPressed(snapshot.ascii()));
            System.out.println("pressed function keys: " + countPressed(snapshot.function()));
            System.out.println("pressed arrow keys: " + countPressed(snapshot.arrow()));
            System.out.println("pressed lock keys: " + countPressed(snapshot.lock()));
            System.out.println("pressed command keys: " + countPressed(snapshot.command()));
            // A separate read, and deliberately outside the snapshot: it reports one ASCII code,
            // zero when no ASCII key is down, which is the normal result for an unattended run.
            System.out.println("pressed ascii code: " + keyboard.pressedAscii());
        }
    }
}
