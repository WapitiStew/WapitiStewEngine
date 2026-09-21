package io.wapitistew.wse;

public enum ImageOrientation {
    NONE(0), ROTATE_90_CW(1), ROTATE_180(2), ROTATE_90_CCW(3), FLIP_HORIZONTAL(4), FLIP_VERTICAL(5);
    private final int code;
    ImageOrientation(int code) { this.code = code; }
    public int code() { return code; }
    public static ImageOrientation fromCode(int code) {
        for (ImageOrientation value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown ImageOrientation: " + code);
    }
}
