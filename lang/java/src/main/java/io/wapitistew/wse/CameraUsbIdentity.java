package io.wapitistew.wse;

import java.util.Objects;

public record CameraUsbIdentity(int vendorId, int productId, String serialNumber, int uvcVersionBcd) {
    public CameraUsbIdentity { Objects.requireNonNull(serialNumber, "serialNumber"); }
    public boolean available() {
        return vendorId != 0 || productId != 0 || !serialNumber.isEmpty() || uvcVersionBcd != 0;
    }
}
