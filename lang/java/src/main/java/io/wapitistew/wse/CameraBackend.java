package io.wapitistew.wse;

public enum CameraBackend {
    AUTOMATIC(0), MEDIA_FOUNDATION(1), VIDEO4LINUX2(2), LIBCAMERA(3);
    private final int code;
    CameraBackend(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraBackend fromCode(int code) {
        for (CameraBackend value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera backend: " + code);
    }
}
