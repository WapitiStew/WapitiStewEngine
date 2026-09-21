package io.wapitistew.wse;

import java.util.Objects;

public record CameraDevice(
        CameraBackend backend, String id, String displayName, String transport,
        CameraTransport transportType, CameraUsbIdentity usb) {
    public CameraDevice {
        Objects.requireNonNull(backend, "backend");
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(displayName, "displayName");
        Objects.requireNonNull(transport, "transport");
        Objects.requireNonNull(transportType, "transportType");
        Objects.requireNonNull(usb, "usb");
    }
}
