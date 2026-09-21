import importlib.util
import sys
from pathlib import Path


module_path = Path(sys.argv[1]).resolve()
fixture_path = Path(sys.argv[2]).resolve()
fixture = {}
for line in fixture_path.read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#"):
        key, value = line.split("=", 1)
        fixture[key] = value


def fnv1a64(data: bytes) -> int:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


if module_path.is_dir():
    sys.path.insert(0, str(module_path))
    import wse
else:
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    wse = importlib.util.module_from_spec(spec)
    sys.modules["_wse"] = wse
    spec.loader.exec_module(wse)

assert wse.runtime_info()["components"]["oui"]
layer = wse.ProjectionLayer(
    int(fixture["source_width"]),
    int(fixture["source_height"]),
    bytes.fromhex(fixture["source_rgba_hex"]),
    [
        wse.ProjectionVertex(-1.0, 1.0, 0.0, 0.0),
        wse.ProjectionVertex(1.0, 1.0, 1.0, 0.0),
        wse.ProjectionVertex(-1.0, -1.0, 0.0, 1.0),
        wse.ProjectionVertex(1.0, -1.0, 1.0, 1.0),
    ],
    [0, 1, 2, 2, 1, 3],
)
layer.sampling_filter = wse.TextureSamplingFilter.NEAREST
request = wse.ProjectionRequest()
request.output_width = int(fixture["output_width"])
request.output_height = int(fixture["output_height"])
request.backend = wse.RendererBackend.DIRECT3D12 if sys.platform == "win32" else wse.RendererBackend.VULKAN12
request.use_software_adapter = True
request.layers = [layer]
frame = wse.render_projection(request)
# Which adapter drew the frame is reported whether or not one was named on the request.
assert isinstance(frame.adapter_name, str) and frame.adapter_name
expected = bytes.fromhex(fixture["expected_rgba_hex"])
assert (frame.width, frame.height, frame.row_pitch, len(frame.data)) == (
    request.output_width,
    request.output_height,
    request.output_width * 4,
    len(expected),
)
data = bytes(frame.data)
assert data == expected
assert fnv1a64(data) == int(fixture["expected_fnv1a64"], 16)

invalid = wse.ProjectionRequest()
try:
    wse.render_projection(invalid)
    raise AssertionError("invalid projection must fail")
except wse.WseError as error:
    assert error.category != 0
    assert error.code != 0
