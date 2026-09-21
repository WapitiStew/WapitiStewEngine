package io.wapitistew.wse;

import java.util.Objects;

/**
 * One received UDP datagram.
 *
 * @param source endpoint the datagram was received from
 * @param payload datagram payload bytes
 */
public record UdpDatagram(Endpoint source, byte[] payload) {
    public UdpDatagram {
        Objects.requireNonNull(source, "source");
        Objects.requireNonNull(payload, "payload");
    }

    static UdpDatagram fromNative(Object[] values) {
        return new UdpDatagram(
                new Endpoint((String) values[0], (Integer) values[1]), (byte[]) values[2]);
    }
}
