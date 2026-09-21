package io.wapitistew.wse;

import java.time.Duration;
import java.util.Objects;

/**
 * The explicit controls that every XPT operation requires. There is deliberately no default
 * timeout, so a caller always states its own deadline.
 *
 * @param timeoutMilliseconds non-negative operation deadline
 * @param cancellation cancellation source to observe, or null
 */
public record OperationContext(long timeoutMilliseconds, CancellationSource cancellation) {
    public OperationContext {
        if (timeoutMilliseconds < 0L) {
            throw new IllegalArgumentException("The operation timeout must not be negative.");
        }
    }

    /** Creates a context with a deadline and no cancellation. */
    public OperationContext(long timeoutMilliseconds) {
        this(timeoutMilliseconds, null);
    }

    /**
     * Creates a context from a duration.
     *
     * @param timeout non-negative operation deadline
     * @return the context
     */
    public static OperationContext of(Duration timeout) {
        Objects.requireNonNull(timeout, "timeout");
        return new OperationContext(timeout.toMillis());
    }

    long cancellationHandle() {
        return cancellation == null ? 0L : cancellation.handle();
    }
}
