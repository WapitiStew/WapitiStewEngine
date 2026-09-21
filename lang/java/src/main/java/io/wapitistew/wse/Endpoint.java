package io.wapitistew.wse;

import java.util.Objects;

/**
 * A host name or numeric address paired with a TCP or UDP port.
 *
 * @param host host name, IPv4 address, or IPv6 address
 * @param port port number
 */
public record Endpoint(String host, int port) {
    public Endpoint {
        Objects.requireNonNull(host, "host");
    }

    /** Returns true for a non-empty host and non-zero port usable as a remote endpoint. */
    public boolean isValid() {
        return !host.isEmpty() && port != 0;
    }

    @Override public String toString() {
        return host + ":" + port;
    }

    static Endpoint fromNative(Object[] values) {
        return new Endpoint((String) values[0], (Integer) values[1]);
    }
}
