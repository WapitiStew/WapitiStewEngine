import io.wapitistew.wse.EdgeBlend;
import io.wapitistew.wse.Projection;
import io.wapitistew.wse.ProjectionFrame;
import io.wapitistew.wse.ProjectionLayer;
import io.wapitistew.wse.ProjectionRequest;
import io.wapitistew.wse.ProjectionVertex;
import io.wapitistew.wse.RendererBackend;
import io.wapitistew.wse.RendererColor;
import io.wapitistew.wse.TextureSamplingFilter;
import io.wapitistew.wse.WseRuntime;
import java.nio.ByteBuffer;
import java.util.List;

/**
 * Handle-free OUI projection: render one warped RGBA frame without touching a graphics API.
 *
 * <p>Hardware-free by design: the sample asks for the software adapter, so it runs on machines
 * with no discrete GPU. Build with {@code WSE_BUILD_OUI=ON} and {@code WSE_BUILD_JAVA_BINDING=ON}.
 *
 * <p>Arguments: none. The native library is located through the {@code -Dwse.runtime.path=...}
 * system property. That load is by absolute path, which on Windows does not add the containing
 * directory to the dependency search path, so the engine directory must also be on {@code PATH} -
 * the graphics backend brings its own libraries beside the engine.
 *
 * <p>The binding is deliberately narrower than the C++ API: the C++ counterparts under
 * {@code example/cpp/oui/} drive a {@code Renderer} directly, creating textures, meshes, surfaces,
 * and fences and waiting on them. None of those handles is bound. Java submits one whole request
 * and receives one finished frame, so a render is a single blocking call with no fence to wait on
 * and no handle to destroy.
 */
public final class ProjectionExample {
    private ProjectionExample() {}

    public static void main(String[] arguments) {
        // Asking first keeps a build without OUI a plain sentence: render() would otherwise raise
        // an Unsupported WseException, and returning success here is deliberate.
        if (!WseRuntime.info().hasOui()) {
            System.err.println("This WSE build does not include the OUI component.");
            return;
        }

        // A 2x2 RGBA source: red, green, blue, white.
        // Four bytes per pixel, tightly packed with no row padding, so the layer's width and height
        // fully describe the layout. The layer copies these bytes into a read-only direct buffer of
        // its own during construction, so the buffer below is not aliased afterwards and a heap
        // buffer would serve as well; the copy runs from position to limit, which is what flip()
        // sets up.
        byte[] source = {
            (byte) 255, 0, 0, (byte) 255,   0, (byte) 255, 0, (byte) 255,
            0, 0, (byte) 255, (byte) 255,   (byte) 255, (byte) 255, (byte) 255, (byte) 255,
        };
        ByteBuffer rgba = ByteBuffer.allocateDirect(source.length);
        rgba.put(source).flip();

        // The mesh maps the source texture onto the output quad in normalized device coordinates.
        // Each vertex is x, y, u, v: the position spans -1 to 1 with y up, the texture coordinate
        // spans 0 to 1 with v down, so (-1, 1) carries (0, 0) and the source arrives upright. This
        // is where a warp would be expressed - moving a corner here is what "projection mapping"
        // means, and the four corners below are simply the unwarped case.
        ProjectionVertex[] vertices = {
            new ProjectionVertex(-1.0f, 1.0f, 0.0f, 0.0f),
            new ProjectionVertex(1.0f, 1.0f, 1.0f, 0.0f),
            new ProjectionVertex(-1.0f, -1.0f, 0.0f, 1.0f),
            new ProjectionVertex(1.0f, -1.0f, 1.0f, 1.0f),
        };
        // 2, 2 is the source size in pixels and must agree with the buffer above. The null is the
        // optional alpha mask, so the layer is fully opaque. The indices list two triangles over
        // the four vertices. NEAREST keeps each source texel a hard 2x2 block in the 4x4 output;
        // LINEAR would blend the four texels and no output pixel would stay pure red. The 1.0f is
        // layer opacity, and EdgeBlend.none() means no soft edge - edge blending exists for
        // overlapping projectors, and a single layer wants none of it.
        ProjectionLayer layer = new ProjectionLayer(
                2, 2, rgba, null, vertices, new int[] {0, 1, 2, 2, 1, 3},
                TextureSamplingFilter.NEAREST, 1.0f, EdgeBlend.none());

        // OUI has one backend per platform. Naming it rather than taking AUTOMATIC keeps the
        // sample's output comparable with the C++ and C# counterparts, which name it too.
        RendererBackend backend = System.getProperty("os.name", "").startsWith("Windows")
                ? RendererBackend.DIRECT3D12
                : RendererBackend.VULKAN12;
        // 4 by 4 output from a 2 by 2 source, so the mapping is visible one block per texel; the
        // clear colour shows wherever no layer covers the target. The 1 is the supersample scale,
        // meaning off - a larger scale renders that many times bigger and downsamples, which costs
        // memory and time and is what a warped edge needs to stop aliasing. The false disables the
        // backend validation layer, which is a debug facility that needs its SDK installed. The
        // 30_000 is the whole render-and-read-back deadline in milliseconds, generous because the
        // software adapter is slow; it is not a per-frame budget.
        ProjectionRequest request = new ProjectionRequest(
                4, 4, RendererColor.opaqueBlack(), 1, backend,
                // Software rendering keeps the sample runnable on machines without a discrete GPU.
                true, false, 30_000, List.of(layer));

        // Renderer, surface, texture, mesh, and readback objects stay owned inside OUI; only the
        // packed RGBA8 result frame crosses the language boundary, and it is closed explicitly.
        // The frame owns the one native allocation this sample holds, and leaving the block frees
        // it there and then. Without the block a Cleaner would still free it, but only at some
        // later garbage collection, so a render loop would pile up whole frames of native memory
        // the collector has no reason to hurry over.
        try (ProjectionFrame frame = Projection.render(request)) {
            System.out.println("frame size: " + frame.width() + " x " + frame.height());
            // The row pitch is the byte distance between two rows, which the backend may pad beyond
            // width times four; index a row through it rather than assuming a tight layout.
            System.out.println("row pitch: " + frame.rowPitch());
            // A read-only view onto memory the frame owns, not a copy. It must not outlive the
            // block: after close() the view still exists in Java but the bytes behind it are gone.
            ByteBuffer data = frame.data().buffer();
            System.out.println("byte count: " + data.remaining());
            // With NEAREST and this mesh the top-left output pixel is the source's top-left texel,
            // so a run that prints anything but pure red says the mapping or the filter changed.
            // The mask is there because a Java byte is signed and 255 reads back as -1 without it.
            System.out.println("top-left pixel RGBA: ["
                    + (data.get(0) & 0xFF) + ", " + (data.get(1) & 0xFF) + ", "
                    + (data.get(2) & 0xFF) + ", " + (data.get(3) & 0xFF) + "]");
        }
    }
}
