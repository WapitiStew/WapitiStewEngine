package io.wapitistew.wse;

/**
 * Structured WSE native error.
 *
 * <p>Every failure the binding raises is this type, so one {@code catch (WseException)} covers the
 * whole surface. A failure that carries more than a category and a code extends this class rather
 * than replacing it; see {@link HttpStatusException}.
 */
public class WseException extends RuntimeException {
    private final int category;
    private final int code;
    private final long nativeCode;

    public WseException(int category, int code, long nativeCode, String message) {
        super(message);
        this.category = category;
        this.code = code;
        this.nativeCode = nativeCode;
    }

    public int category() { return category; }
    public int code() { return code; }
    public long nativeCode() { return nativeCode; }
}
