package io.wapitistew.wse;

public enum EdgeBlendCurve {
    LINEAR(0), SMOOTHSTEP(1);
    private final int code;
    EdgeBlendCurve(int code) { this.code = code; }
    public int code() { return code; }
}
