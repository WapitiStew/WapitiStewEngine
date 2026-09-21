import ctypes
import gc
import importlib.util
import os
import sys
import threading
import time
from pathlib import Path


def load_binding(module_path: Path):
    if module_path.is_dir():
        sys.path.insert(0, str(module_path))
        import wse

        return wse
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules["_wse"] = module
    spec.loader.exec_module(module)
    return module


def resident_bytes() -> int:
    if sys.platform == "win32":
        from ctypes import wintypes

        class ProcessMemoryCounters(ctypes.Structure):
            _fields_ = [
                ("cb", wintypes.DWORD),
                ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
            ]

        counters = ProcessMemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        psapi = ctypes.WinDLL("psapi", use_last_error=True)
        kernel32.GetCurrentProcess.argtypes = []
        kernel32.GetCurrentProcess.restype = wintypes.HANDLE
        psapi.GetProcessMemoryInfo.argtypes = [
            wintypes.HANDLE,
            ctypes.POINTER(ProcessMemoryCounters),
            wintypes.DWORD,
        ]
        psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
        handle = kernel32.GetCurrentProcess()
        if not psapi.GetProcessMemoryInfo(
            handle, ctypes.byref(counters), counters.cb
        ):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(counters.WorkingSetSize)
    statm = Path("/proc/self/statm").read_text(encoding="ascii").split()
    return int(statm[1]) * int(os.sysconf("SC_PAGE_SIZE"))


def collect_unused_memory() -> None:
    gc.collect()
    if sys.platform.startswith("linux"):
        try:
            libc = ctypes.CDLL(None)
            malloc_trim = libc.malloc_trim
            malloc_trim.argtypes = [ctypes.c_size_t]
            malloc_trim.restype = ctypes.c_int
            malloc_trim(0)
        except AttributeError:
            pass


def make_projection(wse, fixture):
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
    request.backend = (
        wse.RendererBackend.DIRECT3D12
        if sys.platform == "win32"
        else wse.RendererBackend.VULKAN12
    )
    request.use_software_adapter = True
    request.layers = [layer]
    return request


module_path = Path(sys.argv[1]).resolve()
fixture_path = Path(sys.argv[2]).resolve()
fixture = {}
for line in fixture_path.read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#"):
        key, value = line.split("=", 1)
        fixture[key] = value

wse = load_binding(module_path)
info = wse.runtime_info()
payload = bytes(64 * 1024)

for _ in range(250):
    with wse.Runtime() as runtime:
        frame = runtime.copy_frame(payload)
        assert len(frame) == len(payload)
        del frame
collect_unused_memory()
baseline = resident_bytes()

for _ in range(2000):
    with wse.Runtime() as runtime:
        frame = runtime.copy_frame(payload)
        assert len(frame) == len(payload)
        del frame

for _ in range(50):
    runtime = wse.Runtime()
    errors = []

    def wait_until_closed():
        try:
            runtime.wait(10_000)
        except wse.WseError as error:
            errors.append(error.category)

    worker = threading.Thread(target=wait_until_closed)
    worker.start()
    time.sleep(0.005)
    runtime.close()
    worker.join(timeout=1)
    assert not worker.is_alive()
    assert errors == [6]

if info["components"]["tmr"]:
    for _ in range(500):
        with wse.WebCamera() as camera:
            assert not camera.is_open

if info["components"]["oui"]:
    projection = make_projection(wse, fixture)
    expected = bytes.fromhex(fixture["expected_rgba_hex"])
    for _ in range(20):
        frame = wse.render_projection(projection)
        assert bytes(frame.data) == expected
        del frame

collect_unused_memory()
growth = resident_bytes() - baseline
limit = 96 * 1024 * 1024
assert growth <= limit, f"Python native resident growth exceeded {limit}: {growth}"
print(f"Python stress resident growth: {growth} bytes")
