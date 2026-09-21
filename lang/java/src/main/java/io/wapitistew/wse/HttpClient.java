package io.wapitistew.wse;

import java.util.List;
import java.util.Objects;

/**
 * Executes HTTP requests with an explicit deadline and response-size ceiling.
 *
 * <p>The binding never retries on the caller's behalf: a request is treated as non-idempotent and
 * transient-failure retries stay off, so a caller decides when a repeat is safe.
 */
public final class HttpClient {
    private HttpClient() {}

    /**
     * Executes a request without authentication.
     *
     * @param request request to execute
     * @param maximumResponseBodySize accepted response-body ceiling in bytes
     * @param context explicit deadline and optional cancellation
     * @return the response
     * @throws HttpStatusException when the server answered with a 4xx or 5xx status; the response
     *     it answered with is carried by {@link HttpStatusException#response()}
     * @throws WseException when the request failed for any other reason
     */
    public static HttpResponse execute(
            HttpRequest request, long maximumResponseBodySize, OperationContext context) {
        return run(request, maximumResponseBodySize, context, null, null);
    }

    /**
     * Executes a request with server-negotiated authentication.
     *
     * @param request request to execute
     * @param maximumResponseBodySize accepted response-body ceiling in bytes
     * @param username credential user name
     * @param secret credential secret
     * @param context explicit deadline and optional cancellation
     * @return the response
     * @throws HttpStatusException when the server answered with a 4xx or 5xx status; the response
     *     it answered with is carried by {@link HttpStatusException#response()}
     * @throws WseException when the request failed for any other reason
     */
    public static HttpResponse executeAuthenticated(
            HttpRequest request,
            long maximumResponseBodySize,
            String username,
            String secret,
            OperationContext context) {
        Objects.requireNonNull(username, "username");
        Objects.requireNonNull(secret, "secret");
        return run(request, maximumResponseBodySize, context, username, secret);
    }

    private static HttpResponse run(
            HttpRequest request,
            long maximumResponseBodySize,
            OperationContext context,
            String username,
            String secret) {
        Objects.requireNonNull(request, "request");
        Objects.requireNonNull(context, "context");

        List<HttpHeader> headers = request.headers();
        String[] flattened = new String[headers.size() * 2];
        for (int index = 0; index < headers.size(); ++index) {
            flattened[index * 2] = headers.get(index).name();
            flattened[index * 2 + 1] = headers.get(index).value();
        }

        // A 4xx or 5xx status leaves here as an HttpStatusException carrying this same response;
        // the native layer builds it from the array shape returned below.
        Object[] values = Native.httpExecute(
                request.method().code(), request.url(), flattened, request.body(),
                maximumResponseBodySize, username, secret,
                context.timeoutMilliseconds(), context.cancellationHandle());

        return HttpResponse.fromNativeValues(values);
    }
}
