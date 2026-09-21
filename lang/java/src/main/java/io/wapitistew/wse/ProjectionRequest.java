package io.wapitistew.wse;

import java.util.List;
import java.util.Objects;

/**
 * A complete projection render request.
 *
 * @param adapterName part of the name of the adapter to render on; an empty string chooses
 *     automatically, and no match is a failure rather than a quiet render on another adapter.
 */
public record ProjectionRequest(
        int outputWidth, int outputHeight, RendererColor clearColor,
        int supersampleScale, RendererBackend backend, boolean useSoftwareAdapter,
        boolean enableValidation, String adapterName, int timeoutMilliseconds,
        List<ProjectionLayer> layers) {
    public ProjectionRequest {
        Objects.requireNonNull(clearColor, "clearColor");
        Objects.requireNonNull(backend, "backend");
        adapterName = adapterName == null ? "" : adapterName;
        layers = List.copyOf(Objects.requireNonNull(layers, "layers"));
    }
    public ProjectionRequest(int outputWidth, int outputHeight, List<ProjectionLayer> layers) {
        this(outputWidth, outputHeight, RendererColor.opaqueBlack(), 1,
                RendererBackend.AUTOMATIC, false, false, "", 30_000, layers);
    }
}
