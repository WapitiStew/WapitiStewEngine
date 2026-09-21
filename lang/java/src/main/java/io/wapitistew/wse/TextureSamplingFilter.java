package io.wapitistew.wse;

public enum TextureSamplingFilter {
    NEAREST(0), LINEAR(1);
    private final int code;
    TextureSamplingFilter(int code) { this.code = code; }
    public int code() { return code; }
}
