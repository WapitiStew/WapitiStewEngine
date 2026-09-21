package io.wapitistew.wse;

public enum CameraTransport {
    UNKNOWN(0), USB_UVC(1), CSI(2), VIRTUAL(3), NETWORK(4);
    private final int code;
    CameraTransport(int code) { this.code = code; }
    public int code() { return code; }
    public static CameraTransport fromCode(int code) {
        for (CameraTransport value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown camera transport: " + code);
    }
}
