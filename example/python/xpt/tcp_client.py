"""Portable XPT TCP client: connect, send, receive, peer check, disconnect.

XPT provides no listener API, so the peer comes from the command line; point it at any TCP echo
service. Without a host argument, or without a server listening there, the sample reports that in
one sentence and exits cleanly. Build with WSE_BUILD_XPT=ON and WSE_BUILD_PYTHON_BINDING=ON.

XPT deliberately has no default timeout: every operation takes an explicit OperationContext so a
caller can never wait forever by accident.

Both arguments are required, and the module path is the first of them, so this sample always loads
the built extension rather than the installed `wse` package.

    python example/python/xpt/tcp_client.py <path to the built _wse module> <host> <port>
"""

import importlib.util
import sys
from pathlib import Path

# One second is generous for a connect on a local network and short enough that pointing the sample
# at an unreachable host still returns promptly. A production client would size this from the route
# it expects rather than reuse one number for connect and receive alike.
TIMEOUT_MS = 1000
# The sample prints what came back, so the limit only has to hold a short echo. It is a ceiling on
# one receive, not a buffer that is always filled: a smaller reply returns as soon as it arrives.
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
    untranslated errno or WSAGetLastError value that a platform-specific diagnosis needs, and the
    message is prose for a human. A caller branches on category and code, never on the message.
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
            "No host was given; pass the built _wse module path, then a host and a port:"
            " python example/python/xpt/tcp_client.py <module path> <host> <port>"
        )
        return 0
    host = sys.argv[2]
    try:
        port = int(sys.argv[3])
    except ValueError:
        print(f"The port argument is not a number: {sys.argv[3]}")
        return 0

    wse = load_wse()
    # A build without WSE_BUILD_XPT registers no TcpClient at all, so the name has to be looked for
    # rather than caught; there is no stub that raises a friendlier error later.
    if not hasattr(wse, "TcpClient"):
        raise SystemExit("This WSE build does not include the XPT component.")

    # One explicit deadline is reused for every operation in this sample.
    # A context is a plain value and carries no socket state, so sharing one across connect, send,
    # and receive is safe; each call starts its own countdown from that same duration rather than
    # spending one shared budget.
    context = wse.OperationContext(TIMEOUT_MS)

    # The context manager is the single owner; leaving it disconnects the socket.
    with wse.TcpClient() as client:
        try:
            client.connect(host, port, context)
        except wse.WseError as failure:
            # No server on the far end is a reportable state, not a sample defect.
            # CONNECTION_REFUSED, HOST_NOT_FOUND, NETWORK_UNREACHABLE, and TIMED_OUT all land here
            # and all mean the same thing to this sample: nothing is listening where the caller
            # pointed it. Returning 0 is deliberate so the sample runs on a machine with no network
            # at all; a real client would branch on the code and decide whether a retry helps.
            report_transport_error(f"connect {host}:{port}", failure)
            print("point the host and port arguments at a reachable TCP service")
            return 0

        # Both endpoints are printed because only the remote one was supplied by the caller. The
        # local pair is what the operating system chose, and it is the only way to identify this
        # connection in a packet capture or in the peer's own logs.
        local_host, local_port = client.local_endpoint()
        remote_host, remote_port = client.remote_endpoint()
        print(f"connected: {local_host}:{local_port} -> {remote_host}:{remote_port}")

        # send() loops until the complete buffer is written or the deadline expires.
        # That is the asymmetry worth remembering: send is all-or-deadline, while the receive below
        # returns whatever one read produced. A caller that needs a whole message back has to loop.
        print("sent bytes:", client.send(b"WSE-XPT-TCP", context))

        # One receive returns a single chunk; an orderly peer close is REMOTE_CLOSED.
        try:
            payload = client.receive(RECEIVE_LIMIT, context)
            print(f"received {len(payload)} bytes:", payload.decode("utf-8", "replace"))
        except wse.WseError as failure:
            # A silent peer is the normal outcome when the argument points at something that is not
            # an echo service, so a timeout is reported and the sample carries on. Anything else is
            # a genuine transport failure and is printed in full, but still without failing the run.
            if failure.code == wse.TransportErrorCode.TIMED_OUT.value:
                print("no reply within the deadline (the peer is not an echo service)")
            else:
                report_transport_error("receive", failure)

        # A non-destructive peek for an OS-observable peer close or reset.
        # It consumes nothing, so calling it does not disturb a reply still in flight. It can only
        # see what the operating system already knows: a peer that vanished without sending a FIN
        # or a RST is indistinguishable from a healthy idle one until a send fails.
        try:
            client.check_peer_connection()
            print("peer check: no close or reset observed")
        except wse.WseError as failure:
            print("peer check: peer reported code =", failure.code)

        # disconnect() is idempotent; leaving the context manager would do the same.
        # It is written out because the sample wants to print after the socket is down, which is
        # the one thing the context manager cannot express.
        client.disconnect()
        print("disconnected")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
