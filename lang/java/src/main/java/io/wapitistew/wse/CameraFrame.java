package io.wapitistew.wse;

import java.util.Objects;

public record CameraFrame(
        int width, int height, CameraPixelFormat pixelFormat, long rowStride,
        long sequence, long monotonicTimestampNanoseconds, NativeBuffer data) implements AutoCloseable {
    public CameraFrame {
        Objects.requireNonNull(pixelFormat, "pixelFormat");
        Objects.requireNonNull(data, "data");
    }
    @Override public void close() { data.close(); }
}
