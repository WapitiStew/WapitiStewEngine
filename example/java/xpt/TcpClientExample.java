import io.wapitistew.wse.Endpoint;
import io.wapitistew.wse.OperationContext;
import io.wapitistew.wse.TcpClient;
import io.wapitistew.wse.TransportErrorCode;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;
import java.nio.charset.StandardCharsets;

/**
 * Portable XPT TCP client: connect, report both endpoints, send a short payload, receive one reply,
 * check the peer, and disconnect.
 *
 * <p>Hardware-free by design: XPT has no listener API, so the peer is supplied by the caller. With
 * no arguments, or with nothing listening at the given endpoint, the sample prints one plain
 * sentence with the structured error and returns normally. Build with {@code WSE_BUILD_XPT=ON} and
 * {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: {@code host port}. The native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, as in every
 * other WSE Java sample, so this sample's own arguments start at the first position. Example:
 * {@code java -Dwse.runtime.path=... TcpClientExample 127.0.0.1 7} - TCP port 7 is the conventional
 * echo service, chosen so the reply can be compared with what was sent; any listening service works
 * and a non-echoing one simply reports no reply. The runtime load is by absolute path, which on
 * Windows does not add the containing directory to the dependency search path, so the engine
 * directory must also be on {@code PATH}.
 *
 * <p>XPT deliberately has no default timeout: every operation takes an explicit
 * {@link OperationContext} so a caller can never wait forever by accident.
 *
 * <p>Every call blocks the calling thread until it finishes or its deadline elapses, and one client
 * is used from one thread at a time. There is no asynchronous form and no callback in this binding.
 */
public final class TcpClientExample {
    private TcpClientExample() {}

    /** One deadline reused for connect, send, and receive. A second is ample for a loopback or
     * local-network peer and short enough that an unreachable one fails quickly; a wide-area host
     * behind a slow path would want more. There is no default to fall back on. */
    private static final long TIMEOUT_MILLISECONDS = 1000L;
    /** Ceiling for one receive, not a total and not an amount to wait for: the call returns the
     * one chunk that arrived, up to this many bytes. TCP is a stream, so a real protocol keeps
     * receiving until it has a whole message rather than assuming one call brings it. */
    private static final long RECEIVE_LIMIT = 256L;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    public static void main(String[] arguments) {
        // Missing arguments are a usage message and a normal return: XPT has no listener API, so
        // the sample cannot host its own peer and will not guess at someone else's.
        if (arguments.length < 2) {
            System.out.println("Pass a host and a TCP port, for example: TcpClientExample 127.0.0.1 7");
            return;
        }
        int port;
        try {
            port = Integer.parseInt(arguments[1]);
        } catch (NumberFormatException failure) {
            System.out.println("The port must be a number, for example: TcpClientExample 127.0.0.1 7");
            return;
        }
        // Checked here rather than left to the transport so the message names the real problem;
        // an out-of-range port would otherwise come back as a generic invalid-argument failure.
        if (port <= 0 || port > 65535) {
            System.out.println("The port must be between 1 and 65535.");
            return;
        }
        if (!WseRuntime.info().hasXpt()) {
            System.out.println("This WSE build does not include the Xpt component.");
            return;
        }

        // One explicit deadline is reused for every operation in this sample.
        OperationContext context = new OperationContext(TIMEOUT_MILLISECONDS);
        Endpoint endpoint = new Endpoint(arguments[0], port);

        // try-with-resources is the single owner; leaving it disconnects and releases the socket.
        // The constructor takes the native handle before any connection exists, so the block also
        // covers the failed-connect path. Without it a Cleaner would perform the same release, but
        // only at some later garbage collection, leaving the socket and its handle held until then.
        try (TcpClient client = new TcpClient()) {
            try {
                client.connect(endpoint, context);
            } catch (WseException failure) {
                // No server at that endpoint is a reportable state, not a sample defect.
                reportFailure("connect " + endpoint, failure);
                System.out.println("Point the arguments at a reachable TCP service.");
                return;
            }
            System.out.println("connected: " + client.localEndpoint() + " -> "
                    + client.remoteEndpoint());

            // send() loops until the complete buffer is written or the deadline expires.
            byte[] payload = "WSE-XPT-TCP".getBytes(StandardCharsets.UTF_8);
            System.out.println("sent bytes: " + client.send(payload, context));

            // One receive returns a single chunk; an orderly peer close is REMOTE_CLOSED.
            try {
                byte[] reply = client.receive(RECEIVE_LIMIT, context);
                System.out.println("received bytes: " + reply.length);
                System.out.println("received payload: "
                        + new String(reply, StandardCharsets.UTF_8));
            } catch (WseException failure) {
                if (failure.code() == TransportErrorCode.TIMED_OUT.code()) {
                    System.out.println(
                            "No reply within the deadline (the peer is not an echo service).");
                } else if (failure.code() == TransportErrorCode.REMOTE_CLOSED.code()) {
                    System.out.println("The peer closed the connection without replying.");
                } else {
                    reportFailure("receive", failure);
                }
            }

            // A non-destructive peek for an operating-system-observable peer close or reset.
            try {
                client.checkPeerConnection();
                System.out.println("peer check: no close or reset observed");
            } catch (WseException failure) {
                System.out.println("peer check: peer reported code=" + failure.code());
            }

            // disconnect() is idempotent; close() would do the same.
            client.disconnect();
            System.out.println("disconnected");
        } catch (WseException failure) {
            reportFailure("tcp session", failure);
        }
    }
}
