"""Handle-free OUI projection: render one warped RGBA frame without touching a graphics API.

Hardware-free by design: the sample asks for the software adapter, so it runs on machines with
no discrete GPU. Build with WSE_BUILD_OUI=ON and WSE_BUILD_PYTHON_BINDING=ON.

The C++ counterpart drives the handle-based renderer itself: it initializes a Renderer, creates a
texture, a mesh and an offscreen surface, uploads, waits on fences, reads the texture back, then
destroys every handle in order. The binding exposes none of those handles. It offers only the
one-shot `render_projection(request)`, which does the whole sequence inside OUI and hands back a
result frame, so a Python caller has nothing to destroy and nothing to leak. The price is that the
per-step controls, the capability query and the explicit fence timeouts are C++ only; the request's
`timeout_ms` field is the single deadline that stands in for all of them.

The binding also has no image writer and no log facility, so the sample prints the frame's shape,
pitch, byte count and one pixel instead of saving a file, and prints instead of logging.

With no argument the sample imports the installed `wse` package; with an argument it loads the
built extension module from that path instead.

    python example/python/oui/projection.py [path to the built _wse module]
"""

import importlib.util
import sys
from pathlib import Path


def load_wse():
    if len(sys.argv) == 1:
        import wse

        return wse
    module_path = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    wse = load_wse()
    if not hasattr(wse, "render_projection"):
        raise SystemExit("This WSE build does not include the OUI component.")

    # A 2x2 RGBA source: red, green, blue, white. Four distinct colours are the smallest texture in
    # which a sampling or orientation mistake is visible in the printed corner pixel. The bytes are
    # tightly packed RGBA8 with no row padding, which is the layout the layer expects.
    source = bytes(
        [
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255,
        ]
    )

    # The mesh maps the source texture onto the output quad in normalized device coordinates.
    # Positions run -1..1 with y upwards; texture coordinates run 0..1 with v downwards, so the
    # first vertex is the top-left corner in both spaces. The six indices are two triangles over
    # the four corners. The constructor copies the pixel bytes, the vertices and the indices into
    # the layer, so none of the Python objects above has to outlive the call.
    layer = wse.ProjectionLayer(
        2,
        2,
        source,
        [
            wse.ProjectionVertex(-1.0, 1.0, 0.0, 0.0),
            wse.ProjectionVertex(1.0, 1.0, 1.0, 0.0),
            wse.ProjectionVertex(-1.0, -1.0, 0.0, 1.0),
            wse.ProjectionVertex(1.0, -1.0, 1.0, 1.0),
        ],
        [0, 1, 2, 2, 1, 3],
    )
    # NEAREST keeps every output pixel equal to exactly one source texel, so the corner printed at
    # the end is pure red and a reader can check it by eye. LINEAR would blend the four texels and
    # give a value that depends on the rasterizer. The layer's other knobs keep their defaults:
    # opacity 1.0, and an edge blend of zero width, which disables blending altogether.
    layer.sampling_filter = wse.TextureSamplingFilter.NEAREST

    # 4x4 is the smallest output that still magnifies the 2x2 source; a larger one only costs time
    # and memory. Left at their defaults are supersample_scale 1 (a higher scale renders larger and
    # downsamples, which would smear the NEAREST result), timeout_ms 30000, an opaque black clear
    # colour, and an empty adapter_name, which means OUI picks the adapter rather than requiring a
    # name to match.
    request = wse.ProjectionRequest()
    request.output_width = 4
    request.output_height = 4
    # Naming the backend the platform actually ships keeps one code path per platform. AUTOMATIC
    # would let OUI choose, which is the right default for an application but hides which path a
    # sample exercised.
    request.backend = (
        wse.RendererBackend.DIRECT3D12 if sys.platform == "win32" else wse.RendererBackend.VULKAN12
    )
    # Software rendering keeps the sample runnable on machines without a discrete GPU.
    request.use_software_adapter = True
    request.layers = [layer]

    # Renderer, surface, texture, mesh, and readback objects stay owned inside OUI; only the
    # packed RGBA8 result frame crosses the language boundary.
    # A failure anywhere in that sequence raises WseError; there is no partial frame to inspect.
    frame = wse.render_projection(request)
    # frame.data is an owned immutable copy, not a view into anything OUI still holds, so it stays
    # valid for as long as Python keeps a reference. It supports the buffer protocol as well, so a
    # caller that wants zero-copy reads can wrap it in memoryview instead of calling bytes().
    print("frame size:", frame.width, "x", frame.height)
    # The readback is packed on the way out, so row_pitch is width * 4 and row y starts at
    # y * row_pitch. It is printed because a caller indexing rows must use it rather than assume.
    print("row pitch:", frame.row_pitch)
    print("byte count:", len(frame.data))
    print("top-left pixel RGBA:", list(bytes(frame.data)[0:4]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
