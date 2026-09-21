package io.wapitistew.wse;

public enum CameraPixelFormat {
    UNKNOWN(0), GRAY8(1), RGB8(2), BGR8(3), BGRA8(4), YUYV422(5), NV12(6), MJPEG(7),
    GRAY16(8), RGB16(9), BGR16(10),
    BAYER16_RGGB(11), BAYER16_BGGR(12), BAYER16_GRBG(13), BAYER16_GBRG(14), UYVY422(15);
    private final int code;
    CameraPixelFormat(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraPixelFormat fromCode(int code) {
        for (CameraPixelFormat value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera pixel format: " + code);
    }
}
