package io.wapitistew.wse;

/**
 * The failure raised when the exchange completed and the server answered with an HTTP 4xx or 5xx
 * status.
 *
 * <p>This is the one HTTP failure that still carries a response: the request was sent, the server
 * replied, and only the status says the reply is an error. {@link #response()} returns exactly the
 * {@link HttpResponse} a successful call would have returned, so the status code, headers, and
 * body of a rejected request stay readable.
 *
 * <p>Every other failure, such as an unreachable host or an elapsed deadline, is raised as a plain
 * {@link WseException}. Because this type extends it, an existing {@code catch (WseException)}
 * keeps catching a status failure as well and can branch on
 * {@link TransportErrorCode#HTTP_STATUS_ERROR} or on this type.
 */
public final class HttpStatusException extends WseException {
    private final HttpResponse response;

    // Constructed by the JNI layer, which passes the same value array the success path returns so
    // that both paths build the response the same way.
    HttpStatusException(int category, int code, long nativeCode, String message, Object[] values) {
        super(category, code, nativeCode, message);
        this.response = HttpResponse.fromNativeValues(values);
    }

    /**
     * Returns the response the server sent with the failing status.
     *
     * @return the response a successful request would have returned
     */
    public HttpResponse response() {
        return response;
    }
}
