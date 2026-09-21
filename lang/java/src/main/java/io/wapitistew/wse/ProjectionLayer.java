package io.wapitistew.wse;

import java.nio.ByteBuffer;
import java.util.Objects;

public final class ProjectionLayer {
    private final int width;
    private final int height;
    private final ByteBuffer rgba;
    private final ByteBuffer alpha;
    private final ProjectionVertex[] vertices;
    private final int[] indices;
    private final TextureSamplingFilter samplingFilter;
    private final float opacity;
    private final EdgeBlend edgeBlend;

    public ProjectionLayer(int width, int height, ByteBuffer rgba, ByteBuffer alpha,
            ProjectionVertex[] vertices, int[] indices, TextureSamplingFilter samplingFilter,
            float opacity, EdgeBlend edgeBlend) {
        this.width = width;
        this.height = height;
        this.rgba = ownedDirect(Objects.requireNonNull(rgba, "rgba"));
        this.alpha = alpha == null ? null : ownedDirect(alpha);
        this.vertices = Objects.requireNonNull(vertices, "vertices").clone();
        for (ProjectionVertex vertex : this.vertices) Objects.requireNonNull(vertex, "vertex");
        this.indices = Objects.requireNonNull(indices, "indices").clone();
        this.samplingFilter = Objects.requireNonNull(samplingFilter, "samplingFilter");
        this.opacity = opacity;
        this.edgeBlend = Objects.requireNonNull(edgeBlend, "edgeBlend");
    }

    public ProjectionLayer(int width, int height, ByteBuffer rgba,
            ProjectionVertex[] vertices, int[] indices) {
        this(width, height, rgba, null, vertices, indices,
                TextureSamplingFilter.LINEAR, 1.0f, EdgeBlend.none());
    }

    private static ByteBuffer ownedDirect(ByteBuffer source) {
        ByteBuffer input = source.slice();
        ByteBuffer copy = ByteBuffer.allocateDirect(input.remaining());
        copy.put(input).flip();
        return copy.asReadOnlyBuffer();
    }

    public int width() { return width; }
    public int height() { return height; }
    public ByteBuffer rgba() { return rgba.asReadOnlyBuffer(); }
    public ByteBuffer alpha() { return alpha == null ? null : alpha.asReadOnlyBuffer(); }
    public ProjectionVertex[] vertices() { return vertices.clone(); }
    public int[] indices() { return indices.clone(); }
    public TextureSamplingFilter samplingFilter() { return samplingFilter; }
    public float opacity() { return opacity; }
    public EdgeBlend edgeBlend() { return edgeBlend; }
}
