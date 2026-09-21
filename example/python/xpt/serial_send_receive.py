"""Portable XPT serial port: open with fixed 8N1 framing, send, receive, close.

The device name and baud rate come from the command line. Without them, or without the physical
device, the sample reports that in one sentence and exits cleanly; the structured OpenFailed error
is itself the documented no-device behavior. Build with WSE_BUILD_XPT=ON and
WSE_BUILD_PYTHON_BINDING=ON.

XPT deliberately has no default timeout: every operation takes an explicit OperationContext so a
caller can never wait forever by accident.

    python example/python/xpt/serial_send_receive.py <path to the built _wse module> <device> <baud>

Windows uses "COM3"-style names; Linux uses "/dev/ttyUSB0"-style paths. The supported rates are
1200/2400/4800/9600/19200/38400/57600/115200.
"""

import importlib.util
import sys
from pathlib import Path

# A second is many character times even at the slowest supported rate, so a device that answers at
# all answers within it. Shortening this would start reporting a slow device as a missing one;
# lengthening it only makes the no-device case, which is the common one here, take longer to report.
TIMEOUT_MS = 1000
# The sample prints what came back, so the limit only has to hold a short echo. It is a ceiling on
# one receive, not a buffer that is always filled: a shorter burst returns as soon as it arrives.
RECEIVE_LIMIT = 256


def load_wse():
    """Loads the binding, either as the installed package or from a built module path.

    With no argument the installed `wse` package is imported. With one, the built `_wse` module at
    that path is loaded directly, which is how the sample runs against a build tree. main() below
    requires the path, so only the second mode is reachable here.
    """
    if len(sys.argv) == 1:
        import wse

        return wse
    module_path = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def report_transport_error(operation: str, failure) -> None:
    """Prints one structured line for a WseError: the operation, then the whole error identity.

    Every field is printed because they answer different questions. `category` says which layer
    refused, `code` is the portable TransportErrorCode a caller may branch on, `native_code` is the
    untranslated errno or Win32 status a platform-specific diagnosis needs, and the message is
    prose for a human. A caller branches on category and code, never on the message text.
    """
    print(
        f"ERROR: {operation} failed. category={failure.category} code={failure.code}"
        f" native={failure.native_code} message={failure}"
    )


def main() -> int:
    # Argument handling first, before the binding is loaded: a usage mistake should not depend on
    # whether the native module is present. Every bad-argument path prints and returns 0, because
    # running the sample wrongly is not a build failure.
    if len(sys.argv) < 4:
        print(
            "No serial device was given; pass the built _wse module path, then a device name and a"
            " baud rate: python example/python/xpt/serial_send_receive.py <module path>"
            " <device> <baud>"
        )
        return 0
    device_name = sys.argv[2]
    try:
        baud_rate = int(sys.argv[3])
    except ValueError:
        print(f"The baud rate argument is not a number: {sys.argv[3]}")
        return 0

    wse = load_wse()
    # A build without WSE_BUILD_XPT registers no SerialPort at all, so the name has to be looked
    # for rather than caught; there is no stub that raises a friendlier error later.
    if not hasattr(wse, "SerialPort"):
        raise SystemExit("This WSE build does not include the XPT component.")

    # One explicit deadline is reused for every operation in this sample.
    # A context is a plain value and carries no port state, so sharing one across open, send, and
    # receive is safe; each call starts its own countdown from that same duration rather than
    # spending one shared budget.
    context = wse.OperationContext(TIMEOUT_MS)

    # The context manager is the single owner; leaving it closes the port.
    # That matters more here than for a socket: a serial device is usually exclusive, so a port
    # left open by a crashed sample keeps every other program off it until the process dies.
    with wse.SerialPort() as port:
        try:
            port.open(device_name, baud_rate, context)
        except wse.WseError as failure:
            # No device on this machine is a reportable state, not a sample defect.
            # OPEN_FAILED covers a name that does not exist and a port another program already
            # holds; CONFIGURATION_FAILED covers a device that exists but refuses the rate.
            # Returning 0 is deliberate so the sample runs on a machine with no serial hardware.
            report_transport_error(f"open {device_name}", failure)
            print("point the device argument at a port that exists on this machine")
            return 0
        # Read back rather than echo the arguments: these are what the port actually settled on.
        # The framing is fixed at 8 data bits, no parity, one stop bit, so only the rate is a
        # choice; a device expecting anything else cannot be talked to through this API.
        print(f"opened {port.device_name} at {port.baud_rate} baud (8N1)")

        # send() writes the whole buffer or fails on the deadline, so the returned count equals the
        # payload length on success. The trailing carriage return is the line terminator most simple
        # serial devices expect before they treat a command as complete.
        print("sent bytes:", port.send(b"WSE-XPT-SERIAL\r", context))

        # One receive returns whatever the peer produced within the deadline. Without a loopback
        # plug or an answering device, a timeout is the expected structured result.
        try:
            payload = port.receive(RECEIVE_LIMIT, context)
            print(f"received {len(payload)} bytes:", payload.decode("utf-8", "replace"))
        except wse.WseError as failure:
            if failure.code == wse.TransportErrorCode.TIMED_OUT.value:
                print("no reply within the deadline (expected without an answering device)")
            else:
                report_transport_error("receive", failure)
                return 1

        # close() is idempotent; leaving the context manager would do the same.
        port.close()
        print("closed")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
