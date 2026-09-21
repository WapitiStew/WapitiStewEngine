package io.wapitistew.wse;

public enum BayerPattern {
    RGGB(0), BGGR(1), GRBG(2), GBRG(3);
    private final int code;
    BayerPattern(int code) { this.code = code; }
    public int code() { return code; }
    public static BayerPattern fromCode(int code) {
        for (BayerPattern value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown BayerPattern: " + code);
    }
}
