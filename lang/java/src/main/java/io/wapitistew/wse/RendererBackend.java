package io.wapitistew.wse;

public enum RendererBackend {
    AUTOMATIC(0), DIRECT3D12(1), VULKAN12(2);
    private final int code;
    RendererBackend(int code) { this.code = code; }
    public int code() { return code; }
    public static RendererBackend fromCode(int code) {
        for (RendererBackend value : values()) if (value.code == code) return value;
        throw new IllegalArgumentException("Unknown renderer backend: " + code);
    }
}
