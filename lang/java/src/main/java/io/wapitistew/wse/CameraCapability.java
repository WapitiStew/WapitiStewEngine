package io.wapitistew.wse;

import java.util.Objects;

public record CameraCapability(
        CameraDevice device, CameraFormat[] formats, CameraControlCapability[] controls,
        CameraStreamProfile[] streamProfiles, CameraExtensionUnitSelector[] extensionUnits) {
    public CameraCapability {
        Objects.requireNonNull(device, "device");
        formats = formats.clone();
        controls = controls.clone();
        streamProfiles = streamProfiles.clone();
        extensionUnits = extensionUnits.clone();
    }
    @Override public CameraFormat[] formats() { return formats.clone(); }
    @Override public CameraControlCapability[] controls() { return controls.clone(); }
    @Override public CameraStreamProfile[] streamProfiles() { return streamProfiles.clone(); }
    @Override public CameraExtensionUnitSelector[] extensionUnits() { return extensionUnits.clone(); }
}
