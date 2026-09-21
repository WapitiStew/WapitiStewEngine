package io.wapitistew.wse;

public record RendererColor(float red, float green, float blue, float alpha) {
    public static RendererColor opaqueBlack() { return new RendererColor(0.0f, 0.0f, 0.0f, 1.0f); }
}
