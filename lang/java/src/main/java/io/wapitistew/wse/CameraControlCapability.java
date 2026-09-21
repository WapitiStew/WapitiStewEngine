package io.wapitistew.wse;

import java.util.Objects;

public record CameraControlCapability(
        CameraControl control, long minimum, long maximum, long step, long defaultValue,
        boolean supportsManual, boolean supportsAutomatic, CameraControlUnit unit, double physicalScale,
        boolean readable, boolean writable, String displayName) {
    public CameraControlCapability {
        Objects.requireNonNull(control, "control");
        Objects.requireNonNull(unit, "unit");
        Objects.requireNonNull(displayName, "displayName");
    }
    public long valueFromNormalized(double normalized) {
        double clamped = Math.max(0.0, Math.min(1.0, normalized));
        long raw = Math.round(minimum + (maximum - minimum) * clamped);
        long increment = step <= 0 ? 1 : step;
        return Math.max(minimum, Math.min(maximum, minimum + Math.round((double)(raw - minimum) / increment) * increment));
    }
    public double normalizedFromValue(long value) {
        if (maximum == minimum) return 0.0;
        return Math.max(0.0, Math.min(1.0, (double)(value - minimum) / (maximum - minimum)));
    }
    public double physicalFromValue(long value) { return value * physicalScale; }
    public long valueFromPhysical(double physical) {
        return valueFromNormalized(normalizedFromValue(Math.round(physical / physicalScale)));
    }
}
