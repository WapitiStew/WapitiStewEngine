package io.wapitistew.wse;

public interface CameraFrameCallback {
    void onFrame(CameraFrame frame);
    default void onError(WseException error) {}
}
