package io.wapitistew.wse;

/**
 * Stable transport error codes. A {@link WseException#code()} raised by an XPT operation carries
 * one of these values, while {@link WseException#category()} is normalized to the common binding
 * categories.
 */
public enum TransportErrorCode {
    /** No failure. */
    NONE(0),
    /** A public API argument was invalid. */
    INVALID_ARGUMENT(1),
    /** The host could not be resolved. */
    HOST_NOT_FOUND(2),
    /** No usable address was available. */
    ADDRESS_UNAVAILABLE(3),
    /** The remote refused the connection. */
    CONNECTION_REFUSED(4),
    /** The connection was reset or aborted. */
    CONNECTION_RESET(5),
    /** The network or host was unreachable. */
    NETWORK_UNREACHABLE(6),
    /** The operation requires a connection that is not established. */
    NOT_CONNECTED(7),
    /** The remote closed the connection normally. */
    REMOTE_CLOSED(8),
    /** The explicit deadline elapsed. */
    TIMED_OUT(9),
    /** Cancellation was requested. */
    CANCELLED(10),
    /** Binding to the local endpoint failed. */
    BIND_FAILED(11),
    /** Sending failed. */
    SEND_FAILED(12),
    /** Receiving failed. */
    RECEIVE_FAILED(13),
    /** The message exceeded the transport limit. */
    MESSAGE_TOO_LARGE(14),
    /** The datagram was truncated to the receive buffer. */
    DATAGRAM_TRUNCATED(15),
    /** A handle, socket, or memory resource was exhausted. */
    RESOURCE_EXHAUSTED(16),
    /** The platform or backend does not support the operation. */
    UNSUPPORTED(17),
    /** The failure did not fall into any other code. */
    UNKNOWN(18),
    /** Opening the device or transport resource failed. */
    OPEN_FAILED(19),
    /** Configuring the device or transport resource failed. */
    CONFIGURATION_FAILED(20),
    /** The server returned an HTTP 4xx or 5xx status. */
    HTTP_STATUS_ERROR(21),
    /** The response body exceeded the caller ceiling. */
    RESPONSE_TOO_LARGE(22),
    /** TLS verification or another security step failed. */
    SECURITY_FAILED(23);

    private final int code;

    TransportErrorCode(int code) {
        this.code = code;
    }

    /** Returns the numeric code shared with the other language bindings. */
    public int code() {
        return code;
    }

    /**
     * Returns the code matching a numeric value.
     *
     * @param value numeric code reported by a failed operation
     * @return the matching code
     * @throws IllegalArgumentException when no code matches
     */
    public static TransportErrorCode fromCode(int value) {
        for (TransportErrorCode candidate : values()) {
            if (candidate.code == value) return candidate;
        }
        throw new IllegalArgumentException("Unknown transport error code: " + value);
    }
}
