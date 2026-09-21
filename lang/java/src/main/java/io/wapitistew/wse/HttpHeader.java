package io.wapitistew.wse;

import java.util.Objects;

/**
 * One HTTP header field.
 *
 * @param name field name
 * @param value field value
 */
public record HttpHeader(String name, String value) {
    public HttpHeader {
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(value, "value");
    }
}
