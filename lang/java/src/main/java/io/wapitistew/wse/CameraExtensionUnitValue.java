package io.wapitistew.wse;

import java.util.Objects;

public record CameraExtensionUnitValue(CameraExtensionUnitSelector selector, byte[] payload) {
    public CameraExtensionUnitValue {
        Objects.requireNonNull(selector, "selector");
        Objects.requireNonNull(payload, "payload");
        payload = payload.clone();
    }
    @Override public byte[] payload() { return payload.clone(); }
}
