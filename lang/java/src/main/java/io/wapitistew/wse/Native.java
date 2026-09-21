package io.wapitistew.wse;

import java.nio.ByteBuffer;

final class Native {
    static {
        String runtimePath = System.getProperty("wse.runtime.path");
        if (runtimePath != null && !runtimePath.isBlank()) {
            System.load(runtimePath);
        }
        System.loadLibrary("wse_jni");
    }

    private Native() {}

    static native String runtimeVersion();
    static native int bindingAbiVersion();
    static native int componentFlags();
    static native long runtimeCreate();
    static native void runtimeClose(long handle);
    static native NativeBuffer runtimeCopyFrame(long handle, ByteBuffer source);
    static native void runtimeWait(long handle, long milliseconds);
    static native void bufferClose(long handle);
    static native long timerCreate(long runtimeHandle, long milliseconds, Runnable callback);
    static native void timerCancel(long handle);
    static native void timerClose(long handle);
    static native CameraDevice[] webCameraEnumerate(int backend);
    static native CameraCapability webCameraCapabilities(CameraDevice device);
    static native long webCameraCreate();
    static native void webCameraOpen(long handle, CameraDevice device, CameraStreamConfiguration configuration);
    static native void webCameraStart(long handle, CameraFrameCallback callback);
    static native void webCameraStop(long handle);
    static native CameraFrame webCameraReadFrame(long handle, int timeoutMilliseconds);
    static native CameraFrame webCameraReadAveragedFrame(
            long handle, long count, int timeoutMilliseconds);
    static native boolean cameraIsFrameOperationSupported(int pixelFormat);
    static native boolean cameraIsFrameAveragingSupported(int pixelFormat);
    static native boolean cameraIsBayerFormat(int pixelFormat);
    static native int cameraBayerPatternOf(int pixelFormat);
    static native CameraFrame cameraApplyOrientation(
            int width, int height, int pixelFormat, long rowStride, long sequence,
            long timestampNanoseconds, ByteBuffer data, int orientation);
    static native CameraFrame cameraDemosaicFrame(
            int width, int height, int pixelFormat, long rowStride, long sequence,
            long timestampNanoseconds, ByteBuffer data, int outputPixelFormat, int method);
    static native long cameraAccumulatorCreate();
    static native void cameraAccumulatorClose(long handle);
    static native void cameraAccumulatorAdd(
            long handle, int width, int height, int pixelFormat, long rowStride, long sequence,
            long timestampNanoseconds, ByteBuffer data);
    static native long cameraAccumulatorCount(long handle);
    static native CameraFrame cameraAccumulatorAverage(long handle);
    static native void cameraAccumulatorReset(long handle);
    static native CameraCapability webCameraCurrentCapabilities(long handle);
    static native CameraControlCapability webCameraControlCapability(long handle, int control);
    static native CameraControlValue webCameraGetControl(long handle, int control);
    static native void webCameraSetControl(long handle, CameraControlValue value);
    static native CameraExtensionUnitValue webCameraGetExtensionUnit(long handle, CameraExtensionUnitSelector selector);
    static native void webCameraSetExtensionUnit(long handle, CameraExtensionUnitValue value);
    static native boolean webCameraIsOpen(long handle);
    static native boolean webCameraIsStreaming(long handle);
    static native void webCameraCloseCamera(long handle);
    static native void webCameraClose(long handle);
    static native long keyboardCreate();
    static native void keyboardClose(long handle);
    static native int keyboardAccessState(long handle);
    static native boolean keyboardIsAvailable(long handle);
    static native boolean[][] keyboardSnapshot(long handle);
    static native byte keyboardPressedAscii(long handle);
    static native long transportCancellationCreate();
    static native void transportCancellationClose(long handle);
    static native void transportCancellationCancel(long handle);
    static native boolean transportCancellationIsRequested(long handle);
    static native long tcpCreate();
    static native void tcpClose(long handle);
    static native void tcpConnect(long handle, String host, int port, long timeoutMs, long cancellation);
    static native void tcpDisconnect(long handle);
    static native boolean tcpIsConnected(long handle);
    static native void tcpCheckPeerConnection(long handle);
    static native Object[] tcpRemoteEndpoint(long handle);
    static native Object[] tcpLocalEndpoint(long handle);
    static native long tcpSend(long handle, byte[] data, long timeoutMs, long cancellation);
    static native byte[] tcpReceive(long handle, long maximumSize, long timeoutMs, long cancellation);
    static native long udpMaximumDatagramSize();
    static native long udpCreate();
    static native void udpClose(long handle);
    static native void udpBind(long handle, String host, int port, long timeoutMs, long cancellation);
    static native void udpCloseSocket(long handle);
    static native boolean udpIsOpen(long handle);
    static native Object[] udpLocalEndpoint(long handle);
    static native long udpSendTo(
            long handle, String host, int port, byte[] data, long timeoutMs, long cancellation);
    static native Object[] udpReceiveFrom(
            long handle, long maximumSize, long timeoutMs, long cancellation);
    static native long serialCreate();
    static native void serialClose(long handle);
    static native void serialOpen(
            long handle, String deviceName, int baudRate, long timeoutMs, long cancellation);
    static native void serialClosePort(long handle);
    static native boolean serialIsOpen(long handle);
    static native String serialDeviceName(long handle);
    static native int serialBaudRate(long handle);
    static native long serialSend(long handle, byte[] data, long timeoutMs, long cancellation);
    static native byte[] serialReceive(
            long handle, long maximumSize, long timeoutMs, long cancellation);
    static native Object[] httpExecute(
            int method, String url, String[] headers, byte[] body, long maximumBody,
            String username, String secret, long timeoutMs, long cancellation);
    static native int modelProfileSupportedSchemaVersion();
    static native long modelProfileFromJson(String jsonText);
    static native long modelProfileFromFile(String filePath);
    static native void modelProfileClose(long handle);
    static native Object[] modelProfileDescribe(long handle);
    static native long modelRegistryCreate();
    static native void modelRegistryClose(long handle);
    static native void modelRegistryRegister(long handle, long profile, boolean replaceExisting);
    static native void modelRegistryLoadFile(long handle, String filePath, boolean replaceExisting);
    static native int[] modelRegistryLoadDirectory(
            long handle, String directoryPath, boolean replaceExisting);
    static native long modelRegistryFind(long handle, String modelOrAliasId);
    static native String[] modelRegistryResolveIdentity(long handle, String modelOrAliasId);
    static native String[] modelRegistryProfileIds(long handle);
    static native long controlSessionCreate();
    static native void controlSessionClose(long handle);
    static native void controlSessionConfigure(
            long handle, long profile, String protocolId, String address, int port, int baudRate,
            String stableDeviceId, int reconnectAttempts, int reconnectDelayMs);
    static native boolean controlSessionIsConfigured(long handle);
    static native void controlSessionConnect(long handle, long timeoutMs, long cancellation);
    static native void controlSessionDisconnect(long handle);
    static native boolean controlSessionIsConnected(long handle);
    static native Object[] controlSessionExecute(
            long handle, String commandId, byte[] requestBody, long timeoutMs, long cancellation);
    static native ProjectionFrame projectionRender(
            int outputWidth, int outputHeight,
            float clearRed, float clearGreen, float clearBlue, float clearAlpha,
            int supersampleScale, int backend, boolean useSoftwareAdapter,
            boolean enableValidation, String adapterName, int timeoutMilliseconds,
            int[] widths, int[] heights, ByteBuffer[] rgba, ByteBuffer[] alpha,
            float[][] vertices, int[][] indices, int[] samplingFilters,
            float[] opacities, float[][] edgeBlend, int[] edgeBlendCurves);
}
