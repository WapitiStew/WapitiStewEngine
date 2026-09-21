package io.wapitistew.wse;

import java.util.Objects;

public record CameraExtensionUnitSelector(
        byte[] unitGuid, int unitId, int selector, long minimumSize, long maximumSize,
        boolean readable, boolean writable, String displayName) {
    public CameraExtensionUnitSelector {
        Objects.requireNonNull(unitGuid, "unitGuid");
        Objects.requireNonNull(displayName, "displayName");
        if (unitGuid.length != 16) throw new IllegalArgumentException("unitGuid must contain exactly 16 bytes.");
        unitGuid = unitGuid.clone();
    }
    @Override public byte[] unitGuid() { return unitGuid.clone(); }
}
