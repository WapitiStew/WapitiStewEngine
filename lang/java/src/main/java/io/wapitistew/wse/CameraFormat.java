package io.wapitistew.wse;

import java.util.Objects;

public record CameraFormat(
        int width, int height, int frameRateNumerator, int frameRateDenominator,
        CameraPixelFormat pixelFormat) {
    public CameraFormat { Objects.requireNonNull(pixelFormat, "pixelFormat"); }
    public double framesPerSecond() {
        return frameRateDenominator == 0 ? 0.0 : (double) frameRateNumerator / frameRateDenominator;
    }
}
