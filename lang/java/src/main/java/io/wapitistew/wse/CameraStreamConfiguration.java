package io.wapitistew.wse;

import java.util.Objects;

public record CameraStreamConfiguration(
        CameraFormat nativeFormat, CameraPixelFormat outputFormat, boolean allowConversion) {
    public CameraStreamConfiguration {
        Objects.requireNonNull(nativeFormat, "nativeFormat");
        Objects.requireNonNull(outputFormat, "outputFormat");
    }
}
