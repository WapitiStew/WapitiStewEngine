package io.wapitistew.wse;

import java.util.Objects;

/**
 * Operations on a camera frame.
 *
 * <p>Moving or averaging pixels is image work rather than device work, so these are static
 * operations on a frame instead of methods on a camera.
 */
public final class CameraFrameOps {
    private CameraFrameOps() {}

    private static void ensureAvailable() {
        if (!WseRuntime.info().hasTmr())
            throw new WseException(9, 1, 0L, "WSE was built without Tmr.");
    }

    /** Whether a pixel format can be reoriented. A Bayer format cannot. */
    public static boolean isFrameOperationSupported(CameraPixelFormat pixelFormat) {
        ensureAvailable();
        return Native.cameraIsFrameOperationSupported(
                Objects.requireNonNull(pixelFormat, "pixelFormat").code());
    }

    /** Whether a pixel format can be averaged. A Bayer format can. */
    public static boolean isFrameAveragingSupported(CameraPixelFormat pixelFormat) {
        ensureAvailable();
        return Native.cameraIsFrameAveragingSupported(
                Objects.requireNonNull(pixelFormat, "pixelFormat").code());
    }

    public static boolean isBayerFormat(CameraPixelFormat pixelFormat) {
        ensureAvailable();
        return Native.cameraIsBayerFormat(
                Objects.requireNonNull(pixelFormat, "pixelFormat").code());
    }

    /** Throws for a format that carries no Bayer layout. */
    public static BayerPattern bayerPatternOf(CameraPixelFormat pixelFormat) {
        ensureAvailable();
        return BayerPattern.fromCode(Native.cameraBayerPatternOf(
                Objects.requireNonNull(pixelFormat, "pixelFormat").code()));
    }

    /**
     * Returns a frame whose orientation has been corrected.
     *
     * <p>A quarter turn exchanges the width and the height. A Bayer frame is refused, because
     * moving its pixels puts them on sites of other colours; convert it first.
     */
    public static CameraFrame applyOrientation(CameraFrame frame, ImageOrientation orientation) {
        ensureAvailable();
        Objects.requireNonNull(frame, "frame");
        Objects.requireNonNull(orientation, "orientation");
        return Native.cameraApplyOrientation(
                frame.width(), frame.height(), frame.pixelFormat().code(), frame.rowStride(),
                frame.sequence(), frame.monotonicTimestampNanoseconds(),
                frame.data().buffer(), orientation.code());
    }

    /** The result format is one of {@code RGB8}, {@code BGR8}, {@code RGB16}, and {@code BGR16}. */
    public static CameraFrame demosaicFrame(
            CameraFrame frame, CameraPixelFormat outputFormat, DemosaicMethod method) {
        ensureAvailable();
        Objects.requireNonNull(frame, "frame");
        Objects.requireNonNull(outputFormat, "outputFormat");
        Objects.requireNonNull(method, "method");
        return Native.cameraDemosaicFrame(
                frame.width(), frame.height(), frame.pixelFormat().code(), frame.rowStride(),
                frame.sequence(), frame.monotonicTimestampNanoseconds(),
                frame.data().buffer(), outputFormat.code(), method.code());
    }

    public static CameraFrame demosaicFrame(CameraFrame frame, CameraPixelFormat outputFormat) {
        return demosaicFrame(frame, outputFormat, DemosaicMethod.BILINEAR);
    }
}
