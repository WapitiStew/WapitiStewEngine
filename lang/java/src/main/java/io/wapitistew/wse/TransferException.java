package io.wapitistew.wse;

/**
 * A failed send with its completed byte count, or a truncated UDP receive with its prefix.
 * Existing {@code catch (WseException)} handlers continue to catch this exception.
 * Progress does not prove peer acceptance and must not trigger automatic replay.
 */
public final class TransferException extends WseException {
    private final long bytesTransferred;
    private final byte[] receivedData;
    private final Endpoint sourceEndpoint;

    // JNI supplies independent Java storage; construction takes a defensive copy as well.
    TransferException(int category, int code, long nativeCode, String message,
            long bytesTransferred, byte[] receivedData, String sourceHost, int sourcePort) {
        super(category, code, nativeCode, message);
        this.bytesTransferred = bytesTransferred;
        this.receivedData = receivedData == null ? null : receivedData.clone();
        this.sourceEndpoint = sourceHost == null ? null : new Endpoint(sourceHost, sourcePort);
    }

    /** Returns the completed send count, or the length of the received UDP prefix. */
    public long bytesTransferred() { return bytesTransferred; }

    /** Returns a fresh copy of the UDP prefix; null for send failures. */
    public byte[] receivedData() { return receivedData == null ? null : receivedData.clone(); }

    /** Returns the UDP sender; null for send failures. */
    public Endpoint sourceEndpoint() { return sourceEndpoint; }
}
