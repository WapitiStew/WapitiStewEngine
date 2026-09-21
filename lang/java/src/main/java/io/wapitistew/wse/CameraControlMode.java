package io.wapitistew.wse;

public enum CameraControlMode {
    MANUAL(0), AUTOMATIC(1);
    private final int code;
    CameraControlMode(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraControlMode fromCode(int code) {
        for (CameraControlMode value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera control mode: " + code);
    }
}
