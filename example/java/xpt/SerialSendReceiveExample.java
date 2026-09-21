import io.wapitistew.wse.OperationContext;
import io.wapitistew.wse.SerialPort;
import io.wapitistew.wse.TransportErrorCode;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;
import java.nio.charset.StandardCharsets;

/**
 * Portable XPT serial port: open with fixed 8N1 framing, send a short payload, receive within an
 * explicit deadline, report what came back, and close.
 *
 * <p>Hardware-free by design: with no arguments, or with no such device on this machine, the sample
 * prints one plain sentence with the structured error and returns normally. A deadline that elapses
 * with no answering device is the documented no-device behaviour, not a failure. Build with
 * {@code WSE_BUILD_XPT=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: {@code deviceName baudRate}. The native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, as in every
 * other WSE Java sample, so this sample's own arguments start at the first position. Windows uses
 * {@code COM3}-style names and Linux uses {@code /dev/ttyUSB0}-style paths; the supported rates are
 * 1200, 2400, 4800, 9600, 19200, 38400, 57600, and 115200. The runtime load is by absolute path,
 * which on Windows does not add the containing directory to the dependency search path, so the
 * engine directory must also be on {@code PATH}.
 *
 * <p>Framing is not a parameter: {@code open} takes a device name and a baud rate only, and the
 * port is always 8 data bits, no parity, one stop bit. A device needing different framing cannot be
 * driven through this API, and the rate is the one thing that has to match the device - a mismatch
 * opens successfully and then reads nothing but noise.
 *
 * <p>XPT deliberately has no default timeout: every operation takes an explicit
 * {@link OperationContext} so a caller can never wait forever by accident. Only the portable serial
 * API is bound; the callback-based, Windows-only legacy connector is not part of the binding.
 */
public final class SerialSendReceiveExample {
    private SerialSendReceiveExample() {}

    /** One deadline reused for open, send, and receive. A second is generous for a local port and
     * short enough that a machine with no answering device still finishes quickly; there is no
     * default to fall back on, so a value has to be chosen here. */
    private static final long TIMEOUT_MILLISECONDS = 1000L;
    /** Ceiling for one receive, not a total and not an amount to wait for: the call returns
     * whatever arrived within the deadline, up to this many bytes. A larger ceiling would not make
     * a reply arrive sooner, and a smaller one would leave the rest for the next receive. */
    private static final long RECEIVE_LIMIT = 256L;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    public static void main(String[] arguments) {
        // Missing arguments are a usage message and a normal return: a device name is machine
        // specific, so the sample refuses to guess one and stays runnable on a machine with none.
        if (arguments.length < 2) {
            System.out.println("Pass a device name and a baud rate, for example: "
                    + "SerialSendReceiveExample COM3 115200");
            return;
        }
        int baudRate;
        try {
            baudRate = Integer.parseInt(arguments[1]);
        } catch (NumberFormatException failure) {
            System.out.println("The baud rate must be a number, for example: "
                    + "SerialSendReceiveExample COM3 115200");
            return;
        }
        if (!WseRuntime.info().hasXpt()) {
            System.out.println("This WSE build does not include the Xpt component.");
            return;
        }

        // One explicit deadline is reused for every operation in this sample.
        OperationContext context = new OperationContext(TIMEOUT_MILLISECONDS);
        String deviceName = arguments[0];

        // try-with-resources is the single owner; leaving it closes and releases the port.
        // The constructor takes the native handle for a port that is not yet open, so the block
        // covers the failed-open path too. Without it a Cleaner would perform the same release,
        // but only at some later garbage collection, and a serial port is exclusive - the device
        // would stay unavailable to any other application until then.
        try (SerialPort port = new SerialPort()) {
            try {
                port.open(deviceName, baudRate, context);
            } catch (WseException failure) {
                // No device on this machine is a reportable state, not a sample defect.
                reportFailure("open " + deviceName, failure);
                System.out.println("Point the arguments at a port that exists on this machine.");
                return;
            }
            System.out.println("opened " + port.deviceName() + " at " + port.baudRate()
                    + " baud (8N1)");

            // send() loops until the complete buffer is written or the deadline expires.
            // The trailing carriage return is there because a line-oriented device usually answers
            // only once it sees a terminator; the text itself is arbitrary.
            byte[] payload = "WSE-XPT-SERIAL\r".getBytes(StandardCharsets.UTF_8);
            System.out.println("sent bytes: " + port.send(payload, context));

            // One receive returns whatever the peer produced within the deadline. Without a
            // loopback plug or an answering device, TIMED_OUT is the expected structured result.
            try {
                byte[] reply = port.receive(RECEIVE_LIMIT, context);
                System.out.println("received bytes: " + reply.length);
                System.out.println("received payload: "
                        + new String(reply, StandardCharsets.UTF_8));
            } catch (WseException failure) {
                // TIMED_OUT is a structured outcome of a working call, not a defect, so it is
                // singled out and reported as the ordinary result. Any other code is a real
                // failure and goes through the structured report.
                if (failure.code() == TransportErrorCode.TIMED_OUT.code()) {
                    System.out.println(
                            "No reply within the deadline (expected without an answering device).");
                } else {
                    reportFailure("receive", failure);
                }
            }

            // closePort() is idempotent; close() would do the same.
            // The difference is what survives: closePort() frees the device and keeps the object,
            // so the same port could be opened again, while close() also releases the native
            // resource. Both run here, in that order, and neither minds the other.
            port.closePort();
            System.out.println("closed");
        } catch (WseException failure) {
            reportFailure("serial session", failure);
        }
    }
}
