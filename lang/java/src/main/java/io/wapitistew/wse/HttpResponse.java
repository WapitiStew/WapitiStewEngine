package io.wapitistew.wse;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * A received HTTP response.
 *
 * @param statusCode HTTP status code
 * @param attemptCount number of attempts, including transport retries
 * @param headers response header fields in the order the server sent them
 * @param body response body bytes
 */
public record HttpResponse(
        int statusCode, int attemptCount, List<HttpHeader> headers, byte[] body) {
    public HttpResponse {
        headers = List.copyOf(Objects.requireNonNull(headers, "headers"));
        Objects.requireNonNull(body, "body");
    }

    /**
     * Builds a response from the value array the native layer produces:
     * {@code { Integer statusCode, Integer attemptCount, String[] headers, byte[] body }}.
     *
     * <p>A success and a 4xx/5xx failure both arrive in that shape, so both go through here and a
     * failed request reports the same response a successful one would.
     */
    static HttpResponse fromNativeValues(Object[] values) {
        String[] responseHeaders = (String[]) values[2];
        List<HttpHeader> parsed = new ArrayList<>(responseHeaders.length / 2);
        for (int index = 0; index + 1 < responseHeaders.length; index += 2) {
            parsed.add(new HttpHeader(responseHeaders[index], responseHeaders[index + 1]));
        }
        return new HttpResponse(
                (Integer) values[0], (Integer) values[1], List.copyOf(parsed), (byte[]) values[3]);
    }
}
