package io.wapitistew.wse;

import java.util.Objects;

/**
 * One rendered projection frame.
 *
 * @param adapterName the adapter that actually drew the frame; when no adapter was named on the
 *     request, this is the only place the choice is reported.
 */
public record ProjectionFrame(
        int width, int height, long rowPitch, String adapterName, NativeBuffer data)
        implements AutoCloseable {
    public ProjectionFrame {
        adapterName = adapterName == null ? "" : adapterName;
        Objects.requireNonNull(data, "data");
    }
    @Override public void close() { data.close(); }
}
