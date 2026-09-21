package io.wapitistew.wse;

import java.util.Objects;

public record EdgeBlend(float left, float right, float top, float bottom, EdgeBlendCurve curve) {
    public EdgeBlend { Objects.requireNonNull(curve, "curve"); }
    public static EdgeBlend none() { return new EdgeBlend(0.0f, 0.0f, 0.0f, 0.0f, EdgeBlendCurve.LINEAR); }
}
