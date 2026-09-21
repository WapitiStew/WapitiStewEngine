package io.wapitistew.wse;

import java.util.Objects;

public record CameraControlValue(CameraControl control, CameraControlMode mode, long value) {
    public CameraControlValue {
        Objects.requireNonNull(control, "control");
        Objects.requireNonNull(mode, "mode");
    }
}
