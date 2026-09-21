package io.wapitistew.wse;

import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public final class BindingStress {
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

    private static Properties loadFixture() throws Exception {
        Properties result = new Properties();
        try (var input = Files.newInputStream(Path.of(
                System.getProperty("wse.golden.path")))) {
            result.load(input);
        }
        return result;
    }

    private static ProjectionRequest projection(Properties fixture) {
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
        return new ProjectionRequest(
                Integer.parseInt(fixture.getProperty("output_width")),
                Integer.parseInt(fixture.getProperty("output_height")),
                RendererColor.opaqueBlack(), 1,
                System.getProperty("os.name").startsWith("Windows")
                        ? RendererBackend.DIRECT3D12 : RendererBackend.VULKAN12,
                true, false, "", 30_000, List.of(layer));
    }

    private static void runChild() throws Exception {
        String expectedJava = System.getProperty("wse.expected.java.feature");
        if (expectedJava != null) {
            require(Runtime.version().feature() == Integer.parseInt(expectedJava),
                    "expected Java runtime feature");
        }
        RuntimeInfo info = WseRuntime.info();
        ByteBuffer payload = ByteBuffer.allocateDirect(64 * 1024);

        for (int index = 0; index < 2000; ++index) {
            payload.clear();
            try (WseRuntime runtime = new WseRuntime();
                 NativeBuffer frame = runtime.copyFrame(payload)) {
                require(frame.size() == payload.capacity(), "owned copy size");
            }
        }

        AtomicInteger callbacks = new AtomicInteger();
        for (int index = 0; index < 500; ++index) {
            try (WseRuntime runtime = new WseRuntime();
                 TimerHandle timer = runtime.runAfter(10_000, callbacks::incrementAndGet)) {
                timer.cancel();
            }
        }
        require(callbacks.get() == 0, "cancelled timer callback delivery");

        AtomicInteger cancelledWaits = new AtomicInteger();
        AtomicInteger closedBeforeWaits = new AtomicInteger();
        for (int index = 0; index < 50; ++index) {
            WseRuntime runtime = new WseRuntime();
            AtomicReference<Throwable> failure = new AtomicReference<>();
            Thread worker = new Thread(() -> {
                try {
                    runtime.waitFor(10_000);
                    failure.set(new AssertionError("closed wait completed successfully"));
                } catch (WseException error) {
                    if (error.category() == 6) cancelledWaits.incrementAndGet();
                    else if (error.category() == 3) closedBeforeWaits.incrementAndGet();
                    else failure.set(error);
                } catch (Throwable error) {
                    failure.set(error);
                }
            }, "wse-java-cancellation");
            worker.start();
            Thread.sleep(20);
            runtime.close();
            worker.join(1000);
            require(!worker.isAlive(), "cancelled Java wait must terminate");
            if (failure.get() != null) throw new AssertionError(
                    "Java cancellation stress failed", failure.get());
        }
        require(cancelledWaits.get() > 0,
                "Java stress must observe an in-flight wait cancellation");
        require(cancelledWaits.get() + closedBeforeWaits.get() == 50,
                "every Java close race must terminate with cancellation or closed state");

        if (info.hasTmr()) {
            for (int index = 0; index < 500; ++index) {
                try (WebCamera camera = new WebCamera()) {
                    require(!camera.isOpen(), "new camera session state");
                }
            }
        }

        if (info.hasOui()) {
            Properties fixture = loadFixture();
            ProjectionRequest request = projection(fixture);
            byte[] expected = decodeHex(fixture.getProperty("expected_rgba_hex"));
            for (int index = 0; index < 3; ++index) {
                try (ProjectionFrame frame = Projection.render(request)) {
                    ByteBuffer view = frame.data().buffer();
                    byte[] actual = new byte[view.remaining()];
                    view.get(actual);
                    require(java.util.Arrays.equals(actual, expected),
                            "Java Projection stress frame");
                }
            }
        }
    }

    private static void runParent() throws Exception {
        boolean windows = System.getProperty("os.name").startsWith("Windows");
        Path java = Path.of(System.getProperty("java.home"), "bin",
                windows ? "java.exe" : "java");
        for (int index = 0; index < 3; ++index) {
            List<String> command = new ArrayList<>();
            command.add(java.toString());
            if (Runtime.version().feature() >= 25) {
                command.add("--enable-native-access=ALL-UNNAMED");
            }
            command.add("-Dwse.stress.child=true");
            command.add("-Djava.library.path=" + System.getProperty("java.library.path"));
            for (String property : List.of(
                    "wse.runtime.path", "wse.golden.path", "wse.expected.java.feature")) {
                String value = System.getProperty(property);
                if (value != null) command.add("-D" + property + "=" + value);
            }
            command.add("-cp");
            command.add(System.getProperty("java.class.path"));
            command.add(BindingStress.class.getName());
            Process child = new ProcessBuilder(command).inheritIO().start();
            if (!child.waitFor(60, TimeUnit.SECONDS)) {
                child.destroyForcibly();
                throw new AssertionError("Java binding stress child timed out");
            }
            require(child.exitValue() == 0, "Java binding stress child failed");
        }
    }

    public static void main(String[] args) throws Exception {
        if (Boolean.getBoolean("wse.stress.child")) runChild();
        else runParent();
    }
}
