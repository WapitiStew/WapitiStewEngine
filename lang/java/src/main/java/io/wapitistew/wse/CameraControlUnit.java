package io.wapitistew.wse;

public enum CameraControlUnit {
    DEVICE_NATIVE(0), MICROSECONDS(1), KELVIN(2), DIOPTERS(3), GAIN_MULTIPLIER(4), RELATIVE(5),
    DEGREES(6), HERTZ(7), BOOLEAN(8);
    private final int code;
    CameraControlUnit(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraControlUnit fromCode(int code) {
        for (CameraControlUnit value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera control unit: " + code);
    }
}
