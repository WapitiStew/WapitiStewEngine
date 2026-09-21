import io.wapitistew.wse.HttpClient;
import io.wapitistew.wse.HttpHeader;
import io.wapitistew.wse.HttpMethod;
import io.wapitistew.wse.HttpRequest;
import io.wapitistew.wse.HttpResponse;
import io.wapitistew.wse.HttpStatusException;
import io.wapitistew.wse.OperationContext;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;
import java.nio.charset.StandardCharsets;

/**
 * Portable XPT HTTP client: one GET request with an explicit deadline and response-body ceiling,
 * reporting the status code, attempt count, header count, and body size.
 *
 * <p>Hardware-free by design: with no argument, or with no network access to the given URL, the
 * sample prints one plain sentence with the structured error and returns normally. Build with
 * {@code WSE_BUILD_XPT=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: {@code url}. The native library is located through the
 * {@code -Dwse.runtime.path=...} system property rather than a command-line argument, as in every
 * other WSE Java sample, so this sample's own argument is the first position. Example:
 * {@code java -Dwse.runtime.path=... HttpGetExample http://example.com/}. That load is by absolute
 * path, which on Windows does not add the containing directory to the dependency search path, so
 * the engine directory must also be on {@code PATH}, since an enabled component may bring its own
 * runtime library beside the engine.
 *
 * <p>Nothing here has to be closed. {@code HttpClient} is a static entry point holding no handle,
 * and the response is an ordinary record whose headers and body are already copied onto the Java
 * heap, so it is the one XPT sample with no try-with-resources block.
 *
 * <p>XPT deliberately has no default timeout: every operation takes an explicit
 * {@link OperationContext} so a caller can never wait forever by accident. The binding also never
 * retries on the caller's behalf, so {@code attemptCount} shows what the transport actually did.
 *
 * <p>One difference from the C++ counterpart {@code example/cpp/xpt/http_get.cpp}, which inspects
 * the response carried alongside a failure on the returned result object. The Java binding raises
 * an exception instead of returning a result, so the response a 4xx or 5xx answer carries arrives
 * on {@link HttpStatusException#response()}; every other failure has no response and reports its
 * structured category and code.
 */
public final class HttpGetExample {
    private HttpGetExample() {}

    /** One deadline for the whole exchange - name resolution, connect, TLS, request, and reading
     * the body all come out of these 5000 ms, so it is not a per-byte or per-stage budget. A much
     * smaller value would report TIMED_OUT against a slow but healthy server; there is no default
     * to fall back on, so a value has to be chosen here. */
    private static final long TIMEOUT_MILLISECONDS = 5000L;
    /** The same 8 MiB ceiling the C++ default execution options use. */
    // It is a refusal, not a truncation: a larger response fails with RESPONSE_TOO_LARGE and no
    // body, which is what keeps an unbounded download from becoming an unbounded allocation.
    private static final long MAXIMUM_BODY_BYTES = 8L * 1024L * 1024L;
    /** Only the printed preview is cut to this length; the whole body is still received. */
    private static final int BODY_PREVIEW_BYTES = 80;

    private static void reportFailure(String operation, WseException failure) {
        // Branch on category and code, never on the message text.
        System.out.println(operation + " failed: category=" + failure.category()
                + " code=" + failure.code() + " native=" + failure.nativeCode()
                + " message=" + failure.getMessage());
    }

    // Decoding as UTF-8 and folding the line breaks keeps the preview to one printed line. A body
    // that is not text, or is cut mid-character by the ceiling, still prints without throwing.
    private static String preview(byte[] body) {
        int length = Math.min(body.length, BODY_PREVIEW_BYTES);
        String text = new String(body, 0, length, StandardCharsets.UTF_8);
        return text.replace('\r', ' ').replace('\n', ' ');
    }

    public static void main(String[] arguments) {
        // No argument is a usage message and a normal return, not an error: the sample has to run
        // on a machine with no network, and it will not invent a URL to reach out to.
        if (arguments.length < 1) {
            System.out.println(
                    "Pass a URL, for example: HttpGetExample http://example.com/");
            return;
        }
        if (!WseRuntime.info().hasXpt()) {
            System.out.println("This WSE build does not include the Xpt component.");
            return;
        }

        String url = arguments[0];
        // The context is the only place a deadline can be stated; the second constructor argument
        // would attach a CancellationSource, which this sample has no other thread to cancel from.
        OperationContext context = new OperationContext(TIMEOUT_MILLISECONDS);
        // The request is mutable and addHeader returns the same instance for chaining, so this is
        // one object, not two. Accept is sent because a server may otherwise negotiate a form the
        // preview cannot show; a GET carries no body.
        HttpRequest request = new HttpRequest(HttpMethod.GET, url).addHeader("Accept", "*/*");

        HttpResponse response;
        try {
            response = HttpClient.execute(request, MAXIMUM_BODY_BYTES, context);
        } catch (HttpStatusException failure) {
            // A 4xx or 5xx answer is a completed exchange, so the server's own reply is readable
            // here. Report the failure, then go on to print that reply.
            reportFailure("GET " + url, failure);
            response = failure.response();
        } catch (WseException failure) {
            // An unreachable host or a TLS problem arrives here; the code says which, and there is
            // no response to read. No network access is a reportable state, not a sample defect.
            reportFailure("GET " + url, failure);
            System.out.println("Point the argument at a reachable URL.");
            return;
        }

        System.out.println("GET " + url);
        System.out.println("status code: " + response.statusCode());
        // One, unless the transport itself retried: the binding turns caller-level retry off and
        // treats every request as non-idempotent, so a repeat is the caller's decision to make.
        System.out.println("attempt count: " + response.attemptCount());
        System.out.println("header count: " + response.headers().size());
        System.out.println("body bytes: " + response.body().length);
        // Header names are compared without case because HTTP does not distinguish them, and the
        // list keeps the server's order and may repeat a name, so it is scanned rather than mapped.
        for (HttpHeader header : response.headers()) {
            if (header.name().equalsIgnoreCase("Content-Type")) {
                System.out.println("content type: " + header.value());
            }
        }
        System.out.println("body preview: " + preview(response.body()));
    }
}
