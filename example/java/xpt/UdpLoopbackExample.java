import io.wapitistew.wse.Endpoint;
import io.wapitistew.wse.OperationContext;
import io.wapitistew.wse.TransportErrorCode;
import io.wapitistew.wse.UdpClient;
import io.wapitistew.wse.UdpDatagram;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.TransferException;
import java.nio.charset.StandardCharsets;

/**
 * Portable XPT UDP send and receive over the loopback interface with explicit deadlines.
 *
 * <p>Hardware-free by design: both endpoints are UDP sockets bound to 127.0.0.1 in this process, so
 * the sample runs anywhere XPT builds. Build with {@code WSE_BUILD_XPT=ON} and
 * {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>XPT deliberately has no default timeout: every operation takes an explicit
 * {@link OperationContext} so a caller can never wait forever by accident.
 */
public final class UdpLoopbackExample {
    private UdpLoopbackExample() {}

    public static void main(String[] arguments) {
        // One explicit deadline is reused for every operation in this sample.
        OperationContext context = new OperationContext(1000L);

        // try-with-resources is the single owner; leaving it closes both sockets.
        try (UdpClient receiver = new UdpClient(); UdpClient sender = new UdpClient()) {
            // Port zero asks the operating system to assign a free port to each socket.
            receiver.bind(new Endpoint("127.0.0.1", 0), context);
            sender.bind(new Endpoint("127.0.0.1", 0), context);

            Endpoint receiverEndpoint = receiver.localEndpoint();
            System.out.println("receiver endpoint: " + receiverEndpoint);
            System.out.println("maximum datagram size: " + UdpClient.maximumDatagramSize());

            byte[] message = "wse-xpt-udp".getBytes(StandardCharsets.UTF_8);
            System.out.println("sent bytes: " + sender.sendTo(receiverEndpoint, message, context));

            UdpDatagram datagram = receiver.receiveFrom(UdpClient.maximumDatagramSize(), context);
            System.out.println("received from: " + datagram.source());
            System.out.println(
                    "received payload: " + new String(datagram.payload(), StandardCharsets.UTF_8));

            sender.sendTo(receiverEndpoint, "prefix-and-suffix".getBytes(StandardCharsets.UTF_8), context);
            try {
                receiver.receiveFrom(3, context);
                throw new AssertionError("Expected a truncated datagram");
            } catch (TransferException error) {
                if (error.code() != TransportErrorCode.DATAGRAM_TRUNCATED.code()) throw error;
                if (error.bytesTransferred() != 3 || error.receivedData()[0] != 'p') throw error;
                System.out.println("truncated prefix: " + new String(error.receivedData(), StandardCharsets.UTF_8));
                // sourceEndpoint identifies the sender. Never automatically replay the datagram.
            }

            // A short deadline with no traffic must report TimedOut rather than blocking.
            try {
                receiver.receiveFrom(UdpClient.maximumDatagramSize(), new OperationContext(50L));
                System.out.println("idle receive timed out: false");
            } catch (WseException failure) {
                boolean timedOut = failure.code() == TransportErrorCode.TIMED_OUT.code();
                System.out.println("idle receive timed out: " + timedOut);
            }
        }
    }
}
