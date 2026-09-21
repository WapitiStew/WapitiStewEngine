package io.wapitistew.wse;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Properties;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public final class BindingContract {
    private static void verifyKeyboardFailure(WseException failure) {
        // The error carries its own single observation, independent of earlier status reads.
        int expected;
        switch (failure.code()) {
            case 0: expected = 3; break; // Starting / InvalidState
            case 2: expected = 2; break; // Unavailable / NotFound
            case 3: expected = 8; break; // PermissionDenied / Security
            case 4: expected = 4; break; // Disconnected / InputOutput
            default: throw new AssertionError("invalid failing keyboard state");
        }
        require(failure.category() == expected, "keyboard failure state/category mapping");
    }

    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static byte[] decodeHex(String value) {
        byte[] result = new byte[value.length() / 2];
        for (int index = 0; index < result.length; ++index) {
            result[index] = (byte) Integer.parseInt(value.substring(index * 2, index * 2 + 2), 16);
        }
        return result;
    }

    private static long fnv1a64(byte[] data) {
        long value = 0xcbf29ce484222325L;
        for (byte item : data) {
            value ^= item & 0xffL;
            value *= 0x100000001b3L;
        }
        return value;
    }

    private static Properties loadFixture() throws Exception {
        Properties result = new Properties();
        try (var input = Files.newInputStream(Path.of(
                System.getProperty("wse.golden.path")))) {
            result.load(input);
        }
        return result;
    }

    private static Properties loadCameraFixture() throws Exception {
        Properties result = new Properties();
        try (var input = Files.newInputStream(Path.of(
                System.getProperty("wse.camera.fixture")))) {
            result.load(input);
        }
        return result;
    }

    /**
     * Answers every request on a loopback port with one fixed status and body until the returned
     * socket is closed. Nothing outside this process is contacted.
     */
    private static ServerSocket startStatusServer(int status, String reason, byte[] body)
            throws Exception {
        ServerSocket server = new ServerSocket(0, 4, InetAddress.getByName("127.0.0.1"));
        byte[] head = ("HTTP/1.1 " + status + " " + reason + "\r\n"
                + "Content-Type: text/plain\r\n"
                + "Content-Length: " + body.length + "\r\n"
                + "Connection: close\r\n\r\n").getBytes(StandardCharsets.UTF_8);
        Thread worker = new Thread(() -> {
            while (!server.isClosed()) {
                try (Socket socket = server.accept()) {
                    // Read the request head so the client is never answered mid-send. A GET has no
                    // body, so the blank line ends it.
                    InputStream input = socket.getInputStream();
                    StringBuilder request = new StringBuilder();
                    int value;
                    while ((value = input.read()) >= 0) {
                        request.append((char) value);
                        if (request.length() >= 4
                                && request.lastIndexOf("\r\n\r\n") == request.length() - 4) {
                            break;
                        }
                    }
                    OutputStream output = socket.getOutputStream();
                    output.write(head);
                    output.write(body);
                    output.flush();
                } catch (Exception stopped) {
                    // Closing the server socket ends the loop; the request under test reports any
                    // failure of its own.
                    return;
                }
            }
        });
        worker.setDaemon(true);
        worker.start();
        return server;
    }

    public static void main(String[] args) throws Exception {
        String expectedJava = System.getProperty("wse.expected.java.feature");
        if (expectedJava != null) {
            require(Runtime.version().feature() == Integer.parseInt(expectedJava),
                    "expected Java runtime feature");
        }
        RuntimeInfo info = WseRuntime.info();
        require(info.version().matches("\\d+\\.\\d+\\.\\d+"), "semantic version");
        require(info.bindingAbiVersion() == 1, "binding ABI");

        if (info.hasTmr()) {
            Properties cameraFixture = loadCameraFixture();
            try {
                Class.forName("io.wapitistew.wse.CameraSession");
                throw new AssertionError("legacy CameraSession must not be packaged");
            } catch (ClassNotFoundException expected) {
                // WebCamera is the sole public camera owner.
            }
            CameraDevice[] devices = WebCamera.enumerate(CameraBackend.AUTOMATIC);
            require(devices != null, "camera enumeration");
            CameraDevice fixtureDevice = new CameraDevice(
                    CameraBackend.fromCode(Integer.parseInt(cameraFixture.getProperty("device_backend"))),
                    cameraFixture.getProperty("device_id"),
                    cameraFixture.getProperty("device_display_name"),
                    cameraFixture.getProperty("device_transport"),
                    CameraTransport.fromCode(Integer.parseInt(cameraFixture.getProperty("device_transport_type"))),
                    new CameraUsbIdentity(
                            Integer.parseInt(cameraFixture.getProperty("usb_vendor_id")),
                            Integer.parseInt(cameraFixture.getProperty("usb_product_id")),
                            cameraFixture.getProperty("usb_serial_number"),
                            Integer.parseInt(cameraFixture.getProperty("usb_uvc_version_bcd"))));
            CameraStreamProfile fixtureProfile = new CameraStreamProfile(
                    new CameraFormat(
                            Integer.parseInt(cameraFixture.getProperty("profile_width")),
                            Integer.parseInt(cameraFixture.getProperty("profile_height")),
                            Integer.parseInt(cameraFixture.getProperty("profile_fps_numerator")),
                            Integer.parseInt(cameraFixture.getProperty("profile_fps_denominator")),
                            CameraPixelFormat.fromCode(Integer.parseInt(
                                    cameraFixture.getProperty("profile_native_pixel_format")))),
                    new CameraPixelFormat[] { CameraPixelFormat.fromCode(Integer.parseInt(
                            cameraFixture.getProperty("profile_output_pixel_format"))) });
            require(fixtureDevice.transportType() == CameraTransport.USB_UVC,
                    "fixture device transport");
            require(fixtureProfile.nativeFormat().pixelFormat() == CameraPixelFormat.YUYV422
                            && fixtureProfile.outputFormats()[0] == CameraPixelFormat.BGRA8,
                    "fixture native/output profile");
            try {
                WebCamera.capabilities(new CameraDevice(
                        CameraBackend.AUTOMATIC, "", "", "", CameraTransport.UNKNOWN,
                        new CameraUsbIdentity(0, 0, "", 0)));
                throw new AssertionError("invalid camera must fail");
            } catch (WseException error) {
                require(error.category() != 0, "camera structured error");
            }
            CameraControlCapability control = new CameraControlCapability(
                    CameraControl.fromCode(Integer.parseInt(cameraFixture.getProperty("control"))),
                    Long.parseLong(cameraFixture.getProperty("control_minimum")),
                    Long.parseLong(cameraFixture.getProperty("control_maximum")),
                    Long.parseLong(cameraFixture.getProperty("control_step")),
                    Long.parseLong(cameraFixture.getProperty("control_default")),
                    true, true, CameraControlUnit.fromCode(Integer.parseInt(
                            cameraFixture.getProperty("control_unit"))), 100.0,
                    true, true, "Exposure");
            require(control.valueFromNormalized(0.52) == 50, "camera normalized conversion");
            require(control.physicalFromValue(10) == 1000.0, "camera physical conversion");
            require(CameraControl.POWER_LINE_FREQUENCY.code() == 17, "expanded camera controls");
            require(CameraTransport.USB_UVC.code() == 1, "camera transport");
            CameraExtensionUnitSelector selector = new CameraExtensionUnitSelector(
                    decodeHex(cameraFixture.getProperty("xu_guid_hex")),
                    Integer.parseInt(cameraFixture.getProperty("xu_unit_id")),
                    Integer.parseInt(cameraFixture.getProperty("xu_selector")),
                    Long.parseLong(cameraFixture.getProperty("xu_minimum_size")),
                    Long.parseLong(cameraFixture.getProperty("xu_maximum_size")),
                    true, true, "fixture-xu");
            CameraExtensionUnitValue extension = new CameraExtensionUnitValue(
                    selector, decodeHex(cameraFixture.getProperty("xu_payload_hex")));
            require(extension.payload().length == 4, "owned extension payload");
            WebCamera camera = new WebCamera();
            try {
                camera.start();
                throw new AssertionError("start before open must fail");
            } catch (WseException error) {
                require(error.category() == Integer.parseInt(
                                cameraFixture.getProperty("closed_error_category"))
                                && error.code() == Integer.parseInt(
                                cameraFixture.getProperty("not_open_error_code")),
                        "camera lifecycle error");
            }
            int[] callbackCount = {0};
            for (int attempt = 0; attempt < 2; ++attempt) {
                try {
                    camera.start(frame -> { ++callbackCount[0]; });
                    throw new AssertionError("callback start before open must fail");
                } catch (WseException error) {
                    require(error.category() == Integer.parseInt(
                                    cameraFixture.getProperty("closed_error_category"))
                                    && error.code() == Integer.parseInt(
                                    cameraFixture.getProperty("not_open_error_code")),
                            "callback start preserves the camera error");
                }
            }
            require(callbackCount[0] == 0, "a rejected start delivers no callback");
            // A session close ends the device session and leaves the object reopenable, so the
            // native handle has to survive it. No camera is needed: the same lifecycle error as
            // above proves the handle is still there, since a released one fails as closed.
            camera.closeCamera();
            camera.closeCamera();
            require(!camera.isClosed(), "a session close keeps the camera object alive");
            require(!camera.isOpen() && !camera.isStreaming(), "a session close ends the session");
            try {
                camera.start();
                throw new AssertionError("start after a session close must fail");
            } catch (WseException error) {
                require(error.category() == Integer.parseInt(
                                cameraFixture.getProperty("closed_error_category"))
                                && error.code() == Integer.parseInt(
                                cameraFixture.getProperty("not_open_error_code")),
                        "a session close leaves the camera reopenable");
            }

            camera.close();
            camera.close();
            require(camera.isClosed(), "camera close");
            require(!camera.isOpen() && !camera.isStreaming(), "closed camera state");
            // Releasing the object is terminal, so the session close is refused afterwards.
            try {
                camera.closeCamera();
                throw new AssertionError("a session close after release must fail");
            } catch (WseException error) {
                require(error.category() == 3, "released camera is rejected");
            }

            verifyFrameOperations();
        }

        if (info.hasVpj()) {
            // The contract for an access-controlled component ships with that component, so this
            // class may not be on the classpath. Java has no conditional compilation, and naming
            // the type directly would make this file uncompilable without it. The runtime says
            // the component is present, so its contract has to be there: a miss is a failure.
            try {
                Class.forName("io.wapitistew.wse.VpjBindingContract")
                    .getMethod("verify", java.util.function.BiConsumer.class)
                    .invoke(null, (java.util.function.BiConsumer<Boolean, String>)
                        BindingContract::require);
            } catch (ReflectiveOperationException error) {
                Throwable cause = error.getCause();
                if (cause instanceof RuntimeException runtime) {
                    throw runtime;
                }
                throw new IllegalStateException(
                    "The runtime reports the projector component but its contract is missing.",
                    error);
            }
        }

        if (info.hasXpt()) {
            // Hardware-free: both endpoints are loopback sockets inside this process.
            OperationContext context = new OperationContext(1000L);
            try (UdpClient receiver = new UdpClient(); UdpClient sender = new UdpClient()) {
                receiver.bind(new Endpoint("127.0.0.1", 0), context);
                sender.bind(new Endpoint("127.0.0.1", 0), context);
                require(receiver.isOpen(), "bound UDP socket is open");

                Endpoint local = receiver.localEndpoint();
                require(local.port() != 0, "binding to port zero reports the assigned port");

                byte[] payload = {1, 2, 3, 4, 5};
                require(sender.sendTo(local, payload, context) == payload.length, "udp send count");

                UdpDatagram datagram =
                        receiver.receiveFrom(UdpClient.maximumDatagramSize(), context);
                require(java.util.Arrays.equals(datagram.payload(), payload), "udp payload");
                require(datagram.source().port() == sender.localEndpoint().port(), "udp source");

                sender.sendTo(local, new byte[] {0, 127, (byte)255, 1, 2}, context);
                TransferException saved = null;
                try {
                    receiver.receiveFrom(3, context);
                } catch (WseException failure) {
                    require(failure instanceof TransferException, "truncation subtype");
                    saved = (TransferException) failure;
                    require(saved.code() == TransportErrorCode.DATAGRAM_TRUNCATED.code(), "truncation code");
                    require(saved.bytesTransferred() == 3, "prefix length");
                    require(java.util.Arrays.equals(saved.receivedData(), new byte[] {0, 127, (byte)255}), "prefix bytes");
                    require(saved.sourceEndpoint().equals(sender.localEndpoint()), "prefix source");
                    byte[] copy = saved.receivedData(); copy[0] = 42;
                    require(saved.receivedData()[0] == 0, "defensive prefix copy");
                }
                require(saved != null, "truncation must throw");
                sender.sendTo(local, new byte[] {8}, context);
                require(receiver.receiveFrom(10, context).payload()[0] == 8, "suffix is discarded");

                // XPT has no default timeout, so an idle receive must fail once its deadline ends.
                boolean timedOut = false;
                try {
                    receiver.receiveFrom(UdpClient.maximumDatagramSize(), new OperationContext(50L));
                } catch (WseException failure) {
                    require(!(failure instanceof TransferException), "timeout has no partial packet");
                    timedOut = failure.code() == TransportErrorCode.TIMED_OUT.code()
                            && failure.category() == 5;
                }
                require(timedOut, "idle udp receive reports TimedOut");

                // A cancelled context must be observed cooperatively rather than ignored.
                boolean cancelled = false;
                try (CancellationSource cancellation = new CancellationSource()) {
                    cancellation.cancel();
                    require(cancellation.isCancellationRequested(), "cancellation is observable");
                    try {
                        receiver.receiveFrom(65507L, new OperationContext(5000L, cancellation));
                    } catch (WseException failure) {
                        cancelled = failure.code() == TransportErrorCode.CANCELLED.code();
                    }
                }
                require(cancelled, "cancelled udp receive reports Cancelled");

                receiver.closeSocket();
                require(saved.receivedData()[2] == (byte)255, "prefix survives close");
                require(!receiver.isOpen(), "udp close is observable");
                receiver.closeSocket();
            }

            try (TcpClient tcp = new TcpClient(); SerialPort serial = new SerialPort(); UdpClient udp = new UdpClient()) {
                Runnable[] calls = {() -> tcp.send(new byte[] {1}, context),
                    () -> serial.send(new byte[] {1}, context),
                    () -> udp.sendTo(new Endpoint("127.0.0.1", 0), new byte[] {1}, context)};
                for (Runnable send : calls) {
                    boolean failed = false;
                    try { send.run(); }
                    catch (WseException failure) {
                        require(failure instanceof TransferException, "send subtype");
                        TransferException progress = (TransferException) failure;
                        require(progress.bytesTransferred() == 0, "unopened send count");
                        require(progress.receivedData() == null && progress.sourceEndpoint() == null, "send has no receive data");
                        require(progress.category() != 0 && progress.code() != 0, "send error identity");
                        failed = true;
                    }
                    require(failed, "disconnected or invalid-destination send fails");
                }
            }

            // No serial hardware is required: a device that cannot exist must fail structurally.
            try (SerialPort port = new SerialPort()) {
                require(!port.isOpen(), "a new serial port is closed");
                boolean refused = false;
                try {
                    port.open("WSE_NONEXISTENT_PORT", 9600, new OperationContext(200L));
                } catch (WseException failure) {
                    refused = failure.category() != 0;
                }
                require(refused, "opening a missing serial device fails");
                require(!port.isOpen(), "a failed open leaves the port closed");
            }

            // No network is required: a closed local port must fail structurally.
            try (TcpClient client = new TcpClient()) {
                require(!client.isConnected(), "a new TCP client is disconnected");
                boolean refused = false;
                try {
                    client.connect(new Endpoint("127.0.0.1", 1), new OperationContext(200L));
                } catch (WseException failure) {
                    refused = failure.category() != 0;
                }
                require(refused, "connecting to a closed port fails");
            }

            boolean httpRefused = false;
            try {
                HttpClient.execute(
                        new HttpRequest(HttpMethod.POST, "http://127.0.0.1:1/wse")
                                .addHeader("Content-Type", "application/octet-stream")
                                .setBody(new byte[] {0, 1, 2}),
                        4096L,
                        new OperationContext(200L));
            } catch (WseException failure) {
                httpRefused = failure.category() != 0;
            }
            require(httpRefused, "an HTTP request to a closed port fails");

            // A 4xx answer is a completed exchange: the server replied, and only the status says
            // the reply is a refusal. The failure therefore carries the response instead of
            // throwing it away. Hardware-free: the server is a loopback socket in this process.
            byte[] refusalBody = "missing".getBytes(StandardCharsets.UTF_8);
            try (ServerSocket server = startStatusServer(404, "Not Found", refusalBody)) {
                String url = "http://127.0.0.1:" + server.getLocalPort() + "/absent";
                boolean carried = false;
                try {
                    HttpClient.execute(
                            new HttpRequest(HttpMethod.GET, url), 4096L,
                            new OperationContext(5000L));
                    throw new AssertionError("a 404 answer must fail");
                } catch (HttpStatusException failure) {
                    require(failure.code() == TransportErrorCode.HTTP_STATUS_ERROR.code(),
                            "a 404 answer reports HttpStatusError");
                    HttpResponse response = failure.response();
                    require(response.statusCode() == 404, "the failure carries the status code");
                    require(response.attemptCount() == 1, "the failure carries the attempt count");
                    require(!response.headers().isEmpty(), "the failure carries the headers");
                    require(java.util.Arrays.equals(response.body(), refusalBody),
                            "the failure carries the response body");
                    carried = true;
                }
                require(carried, "a 404 answer carries its response");

                // An existing catch of the common error type still sees a status failure, so
                // adding the response did not move it out of anyone's handler.
                boolean caughtAsCommon = false;
                try {
                    HttpClient.execute(
                            new HttpRequest(HttpMethod.GET, url), 4096L,
                            new OperationContext(5000L));
                } catch (WseException failure) {
                    caughtAsCommon = failure instanceof HttpStatusException;
                }
                require(caughtAsCommon, "a status failure is still a WseException");
            }
        }

        if (info.hasIui()) {
            // Hardware-free: a machine without a readable keyboard must report that through the
            // readiness state and a structured error, never as a silently empty snapshot.
            try (Keyboard keyboard = new Keyboard()) {
                KeyboardAccessState state = keyboard.accessState();
                require(state != null, "keyboard readiness state");
                // Separate observations may straddle startup or hotplug transitions.
                keyboard.isAvailable();
                try {
                    KeyboardState snapshot = keyboard.snapshot();
                    require(snapshot.ascii().length == KeyboardState.ASCII_COUNT
                                    && snapshot.function().length == KeyboardState.FUNCTION_COUNT
                                    && snapshot.arrow().length == KeyboardState.ARROW_COUNT
                                    && snapshot.lock().length == KeyboardState.LOCK_COUNT
                                    && snapshot.command().length == KeyboardState.COMMAND_COUNT,
                            "keyboard snapshot group lengths");
                } catch (WseException failure) {
                    verifyKeyboardFailure(failure);
                }
                try { keyboard.pressedAscii(); }
                catch (WseException failure) { verifyKeyboardFailure(failure); }
            }

            Keyboard closed = new Keyboard();
            closed.close();
            closed.close();
            require(closed.isClosed(), "keyboard close is idempotent and terminal");
            boolean rejected = false;
            try {
                closed.snapshot();
            } catch (WseException failure) {
                rejected = true;
            }
            require(rejected, "closed keyboard is rejected");
        }

        if (info.hasOui()) {
            Properties fixture = loadFixture();
            byte[] expected = decodeHex(fixture.getProperty("expected_rgba_hex"));
            ProjectionLayer layer = new ProjectionLayer(
                    Integer.parseInt(fixture.getProperty("source_width")),
                    Integer.parseInt(fixture.getProperty("source_height")),
                    ByteBuffer.wrap(decodeHex(fixture.getProperty("source_rgba_hex"))), null,
                    new ProjectionVertex[] {
                            new ProjectionVertex(-1.0f, 1.0f, 0.0f, 0.0f),
                            new ProjectionVertex(1.0f, 1.0f, 1.0f, 0.0f),
                            new ProjectionVertex(-1.0f, -1.0f, 0.0f, 1.0f),
                            new ProjectionVertex(1.0f, -1.0f, 1.0f, 1.0f),
                    }, new int[] {0, 1, 2, 2, 1, 3}, TextureSamplingFilter.NEAREST,
                    1.0f, EdgeBlend.none());
            ProjectionRequest projection = new ProjectionRequest(
                    Integer.parseInt(fixture.getProperty("output_width")),
                    Integer.parseInt(fixture.getProperty("output_height")),
                    RendererColor.opaqueBlack(), 1,
                    System.getProperty("os.name").startsWith("Windows")
                            ? RendererBackend.DIRECT3D12 : RendererBackend.VULKAN12,
                    true, false, "", 30_000, List.of(layer));
            try (ProjectionFrame frame = Projection.render(projection)) {
                require(frame.width() == projection.outputWidth()
                                && frame.height() == projection.outputHeight()
                                && frame.rowPitch() == projection.outputWidth() * 4,
                        "projection frame layout");
                ByteBuffer bytes = frame.data().buffer();
                require(bytes.remaining() == expected.length, "projection frame size");
                byte[] actual = new byte[bytes.remaining()];
                bytes.get(actual);
                require(java.util.Arrays.equals(actual, expected), "projection full-frame bytes");
                require(fnv1a64(actual) == Long.parseUnsignedLong(
                                fixture.getProperty("expected_fnv1a64"), 16),
                        "projection FNV-1a hash");
                // Which adapter drew the frame is reported whether or not one was named.
                require(!frame.adapterName().isEmpty(), "projection adapter name");
            }
            try {
                Projection.render(new ProjectionRequest(0, 0, List.of()));
                throw new AssertionError("invalid projection must fail");
            } catch (WseException error) {
                require(error.category() != 0 && error.code() != 0, "projection structured error");
            }
        }

        byte[] source = new byte[] {0, 1, 127, (byte) 255};
        try (WseRuntime runtime = new WseRuntime();
             NativeBuffer frame = runtime.copyFrame(ByteBuffer.wrap(source))) {
            source[1] = 99;
            ByteBuffer view = frame.buffer();
            require(view.isDirect(), "direct buffer");
            require(view.isReadOnly(), "read-only buffer");
            require(view.remaining() == 4, "buffer size");
            require(view.get(1) == 1, "owned copy");
            runtime.waitFor(1);

            CountDownLatch delivered = new CountDownLatch(1);
            try (TimerHandle timer = runtime.runAfter(10, delivered::countDown)) {
                require(delivered.await(1, TimeUnit.SECONDS), "attached callback delivery");
            }
        }

        try (WseRuntime runtime = new WseRuntime()) {
            try {
                runtime.waitFor(-1);
                throw new AssertionError("negative wait must fail");
            } catch (WseException error) {
                require(error.category() == 1 && error.code() == 1, "structured error");
            }
        }

        WseRuntime closed = new WseRuntime();
        closed.close();
        require(closed.isClosed(), "closed state");
        try {
            closed.waitFor(0);
            throw new AssertionError("closed runtime must fail");
        } catch (WseException error) {
            require(error.category() == 3, "closed error");
        }

        for (int index = 0; index < 50; ++index) {
            try (WseRuntime runtime = new WseRuntime();
                 NativeBuffer frame = runtime.copyFrame(ByteBuffer.wrap(new byte[] {1}))) {
                require(frame.buffer().get(0) == 1, "lifecycle copy");
            }
        }
    }
    // Moving or averaging pixels is image work rather than device work, so the operations are
    // static and take a frame. A frame can therefore be built here without any camera.
    private static CameraFrame frameOf(
            WseRuntime runtime, int width, int height, CameraPixelFormat format, byte[] bytes) {
        NativeBuffer data = runtime.copyFrame(ByteBuffer.wrap(bytes));
        return new CameraFrame(width, height, format, 0L, 0L, 0L, data);
    }

    private static void verifyFrameOperations() throws Exception {
        // The formats added for a Bayer sensor reach Java with the same names the C++ side uses.
        require(CameraPixelFormat.fromCode(15) == CameraPixelFormat.UYVY422, "UYVY format code");
        require(CameraPixelFormat.fromCode(14) == CameraPixelFormat.BAYER16_GBRG,
                "Bayer pixel formats");
        require(CameraPixelFormat.fromCode(8) == CameraPixelFormat.GRAY16, "wide grey format");
        require(CameraControl.fromCode(18) == CameraControl.FRAME_RATE, "frame rate control");
        require(ImageOrientation.fromCode(1) == ImageOrientation.ROTATE_90_CW, "orientation");
        require(DemosaicMethod.fromCode(0) == DemosaicMethod.BLOCK_2X2, "demosaic method");

        // Orientation moves whole pixels; averaging combines a site with itself. A Bayer frame
        // therefore refuses the first and accepts the second.
        require(CameraFrameOps.isFrameOperationSupported(CameraPixelFormat.RGB8),
                "colour frames reorient");
        require(!CameraFrameOps.isFrameOperationSupported(CameraPixelFormat.BAYER16_RGGB),
                "Bayer frames do not reorient");
        require(CameraFrameOps.isFrameAveragingSupported(CameraPixelFormat.BAYER16_RGGB),
                "Bayer frames average");
        require(!CameraFrameOps.isFrameAveragingSupported(CameraPixelFormat.MJPEG),
                "compressed frames do not average");
        require(CameraFrameOps.isBayerFormat(CameraPixelFormat.BAYER16_BGGR), "Bayer question");
        require(CameraFrameOps.bayerPatternOf(CameraPixelFormat.BAYER16_GRBG)
                        == BayerPattern.GRBG, "Bayer layout");
        try {
            CameraFrameOps.bayerPatternOf(CameraPixelFormat.RGB8);
            throw new AssertionError("a format with no Bayer layout must fail");
        } catch (WseException expected) {
            require(expected.category() != 0, "Bayer layout error");
        }

        try (WseRuntime runtime = new WseRuntime()) {
            byte[] bytes = new byte[18];
            for (int index = 0; index < bytes.length; ++index) bytes[index] = (byte) (index + 1);
            // 3x2 so that a quarter turn is visible in the extent.
            CameraFrame rgb = frameOf(runtime, 3, 2, CameraPixelFormat.RGB8, bytes);
            CameraFrame turned = CameraFrameOps.applyOrientation(rgb, ImageOrientation.ROTATE_90_CW);
            require(turned.width() == 2 && turned.height() == 3, "a quarter turn swaps the extent");
            require(turned.rowStride() == 6, "the row stride follows the corrected width");

            // Averaging one frame with itself returns that frame.
            try (CameraFrameAccumulator accumulator = new CameraFrameAccumulator()) {
                require(accumulator.count() == 0L, "a new accumulator holds nothing");
                accumulator.add(rgb);
                accumulator.add(rgb);
                require(accumulator.count() == 2L, "two frames accumulate");
                CameraFrame averaged = accumulator.average();
                for (int index = 0; index < bytes.length; ++index) {
                    require(averaged.data().buffer().get(index) == bytes[index],
                            "averaging a frame with itself returns it");
                }
                accumulator.reset();
                require(accumulator.count() == 0L, "reset discards the accumulation");
            }

            CameraFrame bayer = frameOf(
                    runtime, 4, 4, CameraPixelFormat.BAYER16_RGGB, new byte[4 * 4 * 2]);
            CameraFrame colour = CameraFrameOps.demosaicFrame(bayer, CameraPixelFormat.RGB8);
            require(colour.pixelFormat() == CameraPixelFormat.RGB8 && colour.rowStride() == 12,
                    "a Bayer frame converts to colour");
            CameraFrame wide = CameraFrameOps.demosaicFrame(
                    bayer, CameraPixelFormat.BGR16, DemosaicMethod.BLOCK_2X2);
            require(wide.rowStride() == 24, "the result format is the caller choice");
            try {
                CameraFrameOps.applyOrientation(bayer, ImageOrientation.ROTATE_180);
                throw new AssertionError("orientation must refuse a Bayer frame");
            } catch (WseException expected) {
                require(expected.category() != 0, "Bayer orientation error");
            }
        }
    }
}
