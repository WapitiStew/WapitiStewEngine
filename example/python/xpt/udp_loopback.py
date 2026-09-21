"""Portable XPT UDP send and receive over the loopback interface with explicit deadlines.

Hardware-free by design: both endpoints are UDP sockets bound to 127.0.0.1 in this process, so the
sample runs anywhere XPT builds. Build with WSE_BUILD_XPT=ON and WSE_BUILD_PYTHON_BINDING=ON.

XPT deliberately has no default timeout: every operation takes an explicit OperationContext so a
caller can never wait forever by accident.

There is no listener and no accept step, because UDP has neither: two bound sockets are enough for
a complete exchange, which is what makes this the only XPT sample that needs no argument and no
peer of any kind.

With no argument the sample imports the installed `wse` package; with an argument it loads the
built extension module from that path instead.

    python example/python/xpt/udp_loopback.py [path to the built _wse module]
"""

import importlib.util
import sys
from pathlib import Path


def load_wse():
    """Loads the binding, either as the installed package or from a built module path.

    With no argument the installed `wse` package is imported. With one, the built `_wse` module at
    that path is loaded directly, which is how the sample runs against a build tree.
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


def main() -> int:
    wse = load_wse()
    # A build without WSE_BUILD_XPT registers no UdpClient at all, so the name has to be looked for
    # rather than caught; there is no stub that raises a friendlier error later.
    if not hasattr(wse, "UdpClient"):
        raise SystemExit("This WSE build does not include the XPT component.")

    # One explicit deadline is reused for every operation in this sample.
    # A second on the loopback interface is enormous: nothing here leaves the machine, so a
    # datagram that does not arrive in a second is not going to arrive at all. A generous value
    # costs nothing when everything works and keeps a loaded build machine from failing the sample.
    context = wse.OperationContext(1000)

    # The context managers are the single owners; leaving them closes both sockets.
    # Two sockets, not one: a socket may send to itself, but binding both makes the source endpoint
    # printed below a different port from the destination, which is what proves the datagram
    # travelled rather than being echoed back by the operating system.
    with wse.UdpClient() as receiver, wse.UdpClient() as sender:
        # Port zero asks the operating system to assign a free port to each socket.
        # Hard-coding a port would make two concurrent runs of this sample collide, and would fail
        # outright on a machine where something else already holds it.
        receiver.bind("127.0.0.1", 0, context)
        sender.bind("127.0.0.1", 0, context)

        # local_endpoint() is the only way to learn the assigned port, since bind() was given zero.
        host, port = receiver.local_endpoint()
        print(f"receiver endpoint: {host}:{port}")
        # The transport's own ceiling on one datagram, not a tunable. It is used as the receive
        # limit below so nothing can arrive truncated; XPT reports DATAGRAM_TRUNCATED rather than
        # silently dropping the tail when a caller asks for less than the sender wrote.
        print("maximum datagram size:", wse.UdpClient.maximum_datagram_size())

        message = b"wse-xpt-udp"
        # UDP writes a whole datagram or none of it, so the returned count is the payload length or
        # the call raised; there is no partial send to loop over as there is for TCP.
        print("sent bytes:", sender.send_to(host, port, message, context))

        # One call returns one whole datagram plus the sender's endpoint. That endpoint is the
        # sender socket's assigned port, which is why binding the sender was worth the extra line.
        source_host, source_port, payload = receiver.receive_from(
            wse.UdpClient.maximum_datagram_size(), context
        )
        print(f"received from: {source_host}:{source_port}")
        print("received payload:", payload.decode("utf-8"))

        sender.send_to(host, port, b"prefix-and-suffix", context)
        try:
            receiver.receive_from(3, context)
        except wse.WseTransferError as error:
            assert error.code == wse.TransportErrorCode.DATAGRAM_TRUNCATED.value
            assert error.bytes_transferred == 3 and error.received_data == b"pre"
            assert error.source_endpoint == sender.local_endpoint()
            print("truncated prefix:", error.received_data)
            # The suffix is discarded. Do not automatically replay the datagram.
        else:
            raise AssertionError("Expected a truncated datagram")

        # A short deadline with no traffic must report TimedOut rather than blocking.
        # 50 ms is deliberately far below the deadline above: the queue is known to be empty by
        # now, so the only thing being measured is that the deadline is honoured, and a longer wait
        # would only make the sample slower. This is the whole point of XPT having no default
        # timeout - the caller decides, per call, how long forever is.
        try:
            receiver.receive_from(wse.UdpClient.maximum_datagram_size(), wse.OperationContext(50))
            # Reached only if a stray datagram arrived, which is why the line reports False rather
            # than treating the read as a success.
            print("idle receive timed out: False")
        except wse.WseError as failure:
            # Compare against the enumerator, never against the message text: the text is a
            # diagnostic and may be reworded, while the code is the contract.
            timed_out = failure.code == wse.TransportErrorCode.TIMED_OUT.value
            print("idle receive timed out:", timed_out)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
