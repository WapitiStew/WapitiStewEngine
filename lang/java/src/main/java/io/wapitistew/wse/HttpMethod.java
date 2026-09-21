package io.wapitistew.wse;

/** HTTP request method. */
public enum HttpMethod {
    /** GET. */
    GET(0),
    /** HEAD. */
    HEAD(1),
    /** POST. */
    POST(2),
    /** PUT. */
    PUT(3),
    /** PATCH. */
    PATCH(4),
    /** DELETE. */
    DELETE(5);

    private final int code;

    HttpMethod(int code) {
        this.code = code;
    }

    /** Returns the numeric method value shared with the other language bindings. */
    public int code() {
        return code;
    }
}
