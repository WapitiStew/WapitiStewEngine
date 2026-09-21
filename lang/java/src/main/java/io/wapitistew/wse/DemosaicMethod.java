package io.wapitistew.wse;

public enum DemosaicMethod {
    BLOCK_2X2(0), BILINEAR(1);
    private final int code;
    DemosaicMethod(int code) { this.code = code; }
    public int code() { return code; }
    public static DemosaicMethod fromCode(int code) {
        for (DemosaicMethod value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown DemosaicMethod: " + code);
    }
}
