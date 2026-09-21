"""XPT transport contract for the WSE Python binding.

Hardware-free: both UDP endpoints are loopback sockets inside this process, the serial case opens a
device that cannot exist, and the HTTP cases talk to a closed local port and to a one-shot loopback
listener this test starts itself. No external network, serial device, or pre-existing server is
required.
"""

import importlib.util
import pickle
import socket
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import ModuleType


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location("_wse", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def verify_udp_loopback(wse) -> None:
    context = wse.OperationContext(1000)

    with wse.UdpClient() as receiver, wse.UdpClient() as sender:
        receiver.bind("127.0.0.1", 0, context)
        sender.bind("127.0.0.1", 0, context)
        assert receiver.is_open, "A bound UDP socket must report itself open"

        host, port = receiver.local_endpoint()
        assert port != 0, "Binding to port zero must report the assigned port"

        payload = b"\x01\x02\x03\x04\x05"
        assert sender.send_to(host, port, payload, context) == len(payload)

        source_host, source_port, received = receiver.receive_from(
            wse.UdpClient.maximum_datagram_size(), context
        )
        assert received == payload, "The payload must survive the round trip"
        assert (source_host, source_port) == sender.local_endpoint()

        # Truncation keeps the consumed prefix and sender, as an existing WseError subtype.
        sender.send_to(host, port, b"\x00\x7f\xffsuffix", context)
        try:
            receiver.receive_from(3, context)
        except wse.WseError as failure:
            assert isinstance(failure, wse.WseTransferError)
            assert failure.code == wse.TransportErrorCode.DATAGRAM_TRUNCATED.value
            assert failure.bytes_transferred == 3
            saved = failure
            assert failure.received_data == b"\x00\x7f\xff"
            assert failure.source_endpoint == sender.local_endpoint()
        else:
            raise AssertionError("Truncation must remain an error")
        sender.send_to(host, port, b"next", context)
        assert receiver.receive_from(10, context)[2] == b"next", "Discarded suffix must not be returned"
        assert saved.received_data == b"\x00\x7f\xff", "Next receive cannot alias the prefix"

        # XPT has no default timeout, so an idle receive must fail once its deadline elapses.
        try:
            receiver.receive_from(wse.UdpClient.maximum_datagram_size(), wse.OperationContext(50))
        except wse.WseError as failure:
            assert failure.code == wse.TransportErrorCode.TIMED_OUT.value
            assert failure.category == 5, "TimedOut must normalize to the Timeout category"
            assert not isinstance(failure, wse.WseTransferError)
        else:
            raise AssertionError("An idle receive must fail once its deadline elapses")

        # A cancelled context must be observed cooperatively rather than ignored.
        cancellation = wse.CancellationSource()
        cancellation.cancel()
        assert cancellation.is_cancellation_requested
        try:
            receiver.receive_from(65507, wse.OperationContext(5000, cancellation))
        except wse.WseError as failure:
            assert failure.code == wse.TransportErrorCode.CANCELLED.value
        else:
            raise AssertionError("A cancelled receive must fail")

        receiver.close()
        assert saved.received_data == b"\x00\x7f\xff"
        restored = pickle.loads(pickle.dumps(saved))
        assert type(restored) is wse.WseTransferError
        assert restored.__dict__ == saved.__dict__
        assert restored.args == saved.args
        assert not receiver.is_open, "close must be observable"
        receiver.close()


def verify_send_progress(wse) -> None:
    context = wse.OperationContext(100)
    with wse.TcpClient() as tcp, wse.SerialPort() as serial, wse.UdpClient() as udp:
        calls = (lambda: tcp.send(b"x", context), lambda: serial.send(b"x", context),
                 lambda: udp.send_to("127.0.0.1", 0, b"x", context))
        for call in calls:
            try:
                call()
            except wse.WseError as failure:
                assert isinstance(failure, wse.WseTransferError)
                assert failure.bytes_transferred == 0
                assert failure.received_data is None and failure.source_endpoint is None
                assert failure.category != 0 and failure.code != 0
            else:
                raise AssertionError("A disconnected or invalid-destination send must fail")


def verify_argument_contracts(wse) -> None:
    try:
        wse.OperationContext(-1)
    except wse.WseError as failure:
        assert failure.category == 1, "A negative deadline must be an invalid argument"
    else:
        raise AssertionError("A negative deadline must be rejected")


def verify_serial(wse) -> None:
    context = wse.OperationContext(200)
    with wse.SerialPort() as port:
        assert not port.is_open, "A new serial port must be closed"
        try:
            port.open("WSE_NONEXISTENT_PORT", 9600, context)
        except wse.WseError as failure:
            assert failure.category != 0, "A missing device must report a category"
        else:
            raise AssertionError("Opening a missing serial device must fail")
        assert not port.is_open, "A failed open must leave the port closed"


def verify_http(wse) -> None:
    context = wse.OperationContext(200)
    request = wse.HttpRequest(wse.HttpMethod.POST, "http://127.0.0.1:1/wse")
    request.add_header("Content-Type", "application/octet-stream")
    request.set_body(b"\x00\x01\x02")
    try:
        wse.http_execute(request, 4096, context)
    except wse.WseHttpStatusError:
        raise AssertionError("A refused connection is not an HTTP status failure")
    except wse.WseError as failure:
        assert failure.category != 0, "A request to a closed port must report a category"
        assert not hasattr(failure, "response"), "A failure with no answer must carry no response"
    else:
        raise AssertionError("A request to a closed port must fail")


def serve_http_once(
    listener_in: socket.socket, status_in: int, body_in: bytes
) -> None:
    """Answer one loopback request from a thread in the calling Python process."""
    connection, _address = listener_in.accept()
    with connection:
        connection.settimeout(5)
        received = b""
        while b"\r\n\r\n" not in received:
            chunk = connection.recv(4096)
            assert chunk, "The client must send a complete HTTP request"
            received += chunk
        head = (
            f"HTTP/1.1 {status_in} Test\r\n"
            "Content-Type: application/octet-stream\r\n"
            f"Content-Length: {len(body_in)}\r\n"
            "Connection: close\r\n\r\n"
        ).encode("ascii")
        connection.sendall(head + body_in)


def verify_http_response(
    wse_in: ModuleType, status_in: int, authenticated_in: bool
) -> None:
    """Preserve success/error responses and release the GIL during HTTP work."""
    body = b"\x00binding response\xff"
    with socket.socket() as listener, ThreadPoolExecutor(max_workers=1) as executor:
        listener.settimeout(5)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        server = executor.submit(serve_http_once, listener, status_in, body)
        port = listener.getsockname()[1]
        context = wse_in.OperationContext(5000)
        request = wse_in.HttpRequest(
            wse_in.HttpMethod.GET, f"http://127.0.0.1:{port}/result"
        )
        try:
            if authenticated_in:
                response = wse_in.http_execute_authenticated(
                    request, 4096, "wse", "secret", context
                )
            else:
                response = wse_in.http_execute(request, 4096, context)
        except wse_in.WseHttpStatusError as failure:
            assert status_in >= 400
            assert isinstance(failure, wse_in.WseError)
            assert failure.code == wse_in.TransportErrorCode.HTTP_STATUS_ERROR.value
            assert failure.native_code == status_in
            response = failure.response
        else:
            assert status_in == 200, "An HTTP status failure must raise its error"

        # Both result states retain independently owned bytes after the server exits.
        server.result(timeout=5)
        assert response.status_code == status_in
        assert response.body() == body
        assert response.attempt_count == 1, "The binding must not retry"
        assert any(
            name.lower() == "content-length" for name, _ in response.headers()
        )


def verify_tcp(wse) -> None:
    context = wse.OperationContext(200)
    with wse.TcpClient() as client:
        assert not client.is_connected, "A new TCP client must be disconnected"
        try:
            # Port 1 on loopback is not listening, so the attempt must fail structurally.
            client.connect("127.0.0.1", 1, context)
        except wse.WseError as failure:
            assert failure.category != 0, "A refused connection must report a category"
        else:
            raise AssertionError("Connecting to a closed port must fail")
        assert not client.is_connected, "A failed connect must leave the client disconnected"
        client.disconnect()


def main() -> int:
    if len(sys.argv) > 1:
        wse = load_module(Path(sys.argv[1]).resolve())
    else:
        import wse
    assert wse.WseTransferError.__module__ == wse.WseError.__module__
    assert wse.runtime_info()["components"]["xpt"] is True, "XPT must be reported as available"

    verify_udp_loopback(wse)
    verify_send_progress(wse)
    verify_argument_contracts(wse)
    verify_serial(wse)
    verify_http(wse)
    for status in (200, 404, 503):
        for authenticated in (False, True):
            verify_http_response(wse, status, authenticated)
    verify_tcp(wse)

    print("python XPT contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
