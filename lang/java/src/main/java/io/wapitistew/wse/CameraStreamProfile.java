package io.wapitistew.wse;

import java.util.Objects;

public record CameraStreamProfile(CameraFormat nativeFormat, CameraPixelFormat[] outputFormats) {
    public CameraStreamProfile {
        Objects.requireNonNull(nativeFormat, "nativeFormat");
        outputFormats = outputFormats.clone();
    }
    @Override public CameraPixelFormat[] outputFormats() { return outputFormats.clone(); }
    public boolean supportsOutput(CameraPixelFormat output) {
        for (CameraPixelFormat candidate : outputFormats) if (candidate == output) return true;
        return false;
    }
}
