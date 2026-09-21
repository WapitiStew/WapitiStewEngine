package io.wapitistew.wse;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * An HTTP request. Build it with a method and URL, then add headers and an optional body before
 * handing it to {@link HttpClient}.
 */
public final class HttpRequest {
    private final HttpMethod method;
    private final String url;
    private final List<HttpHeader> headers = new ArrayList<>();
    private byte[] body = new byte[0];

    /**
     * Creates a request.
     *
     * @param method request method
     * @param url absolute request URL
     */
    public HttpRequest(HttpMethod method, String url) {
        this.method = Objects.requireNonNull(method, "method");
        this.url = Objects.requireNonNull(url, "url");
    }

    /**
     * Adds one header field.
     *
     * @param name field name
     * @param value field value
     * @return this request, for chaining
     */
    public HttpRequest addHeader(String name, String value) {
        headers.add(new HttpHeader(name, value));
        return this;
    }

    /**
     * Sets the request body. The bytes are copied, so later changes to the array are not observed.
     *
     * @param value body bytes
     * @return this request, for chaining
     */
    public HttpRequest setBody(byte[] value) {
        Objects.requireNonNull(value, "value");
        body = value.clone();
        return this;
    }

    /** Returns the request method. */
    public HttpMethod method() {
        return method;
    }

    /** Returns the request URL. */
    public String url() {
        return url;
    }

    /** Returns the header fields added so far. */
    public List<HttpHeader> headers() {
        return List.copyOf(headers);
    }

    /** Returns a copy of the request body. */
    public byte[] body() {
        return body.clone();
    }
}
