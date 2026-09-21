package io.wapitistew.wse;

import java.nio.ByteBuffer;
import java.util.List;
import java.util.Objects;

/** High-level Projection entry point that never exposes GPU handles or fences. */
public final class Projection {
    private Projection() {}

    public static ProjectionFrame render(ProjectionRequest request) {
        Objects.requireNonNull(request, "request");
        if (!WseRuntime.info().hasOui()) {
            throw new WseException(9, 1, 0L, "WSE was built without OUI.");
        }
        List<ProjectionLayer> layers = request.layers();
        int count = layers.size();
        int[] widths = new int[count];
        int[] heights = new int[count];
        ByteBuffer[] rgba = new ByteBuffer[count];
        ByteBuffer[] alpha = new ByteBuffer[count];
        float[][] vertices = new float[count][];
        int[][] indices = new int[count][];
        int[] filters = new int[count];
        float[] opacities = new float[count];
        float[][] edges = new float[count][4];
        int[] curves = new int[count];
        for (int layerIndex = 0; layerIndex < count; ++layerIndex) {
            ProjectionLayer layer = layers.get(layerIndex);
            widths[layerIndex] = layer.width();
            heights[layerIndex] = layer.height();
            rgba[layerIndex] = layer.rgba();
            alpha[layerIndex] = layer.alpha();
            ProjectionVertex[] layerVertices = layer.vertices();
            vertices[layerIndex] = new float[layerVertices.length * 4];
            for (int vertexIndex = 0; vertexIndex < layerVertices.length; ++vertexIndex) {
                ProjectionVertex vertex = layerVertices[vertexIndex];
                int offset = vertexIndex * 4;
                vertices[layerIndex][offset] = vertex.x();
                vertices[layerIndex][offset + 1] = vertex.y();
                vertices[layerIndex][offset + 2] = vertex.u();
                vertices[layerIndex][offset + 3] = vertex.v();
            }
            indices[layerIndex] = layer.indices();
            filters[layerIndex] = layer.samplingFilter().code();
            opacities[layerIndex] = layer.opacity();
            EdgeBlend edge = layer.edgeBlend();
            edges[layerIndex] = new float[] {edge.left(), edge.right(), edge.top(), edge.bottom()};
            curves[layerIndex] = edge.curve().code();
        }
        RendererColor clear = request.clearColor();
        return Native.projectionRender(request.outputWidth(), request.outputHeight(),
                clear.red(), clear.green(), clear.blue(), clear.alpha(),
                request.supersampleScale(), request.backend().code(), request.useSoftwareAdapter(),
                request.enableValidation(), request.adapterName(), request.timeoutMilliseconds(),
                widths, heights, rgba,
                alpha, vertices, indices, filters, opacities, edges, curves);
    }
}
