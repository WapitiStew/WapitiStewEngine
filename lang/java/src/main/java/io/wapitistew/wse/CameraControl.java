package io.wapitistew.wse;

public enum CameraControl {
    EXPOSURE(0), GAIN(1), FOCUS(2), BRIGHTNESS(3), CONTRAST(4), SATURATION(5), WHITE_BALANCE(6),
    ZOOM(7), IRIS(8), HUE(9), SHARPNESS(10), GAMMA(11), COLOR_ENABLE(12),
    BACKLIGHT_COMPENSATION(13), PAN(14), TILT(15), ROLL(16), POWER_LINE_FREQUENCY(17),
    FRAME_RATE(18);
    private final int code;
    CameraControl(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraControl fromCode(int code) {
        for (CameraControl value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera control: " + code);
    }
}
