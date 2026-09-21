import importlib.util
import re
import sys
import threading
import time
from pathlib import Path


def load_binding():
    if len(sys.argv) == 1:
        import wse

        return wse

    module_path = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules["_wse"] = module
    spec.loader.exec_module(module)
    return module


wse = load_binding()
info = wse.runtime_info()
assert re.fullmatch(r"\d+\.\d+\.\d+", info["version"])
assert info["binding_abi_version"] == 1
assert set(info["components"]) == {"xpt", "tmr", "oui", "gef", "iui", "vpj"}
assert all(isinstance(value, bool) for value in info["components"].values())

source = bytearray([0, 1, 127, 255])
frame = wse.copy_frame(source)
source[1] = 99
assert bytes(frame) == bytes([0, 1, 127, 255])
assert frame.tobytes() == bytes(frame)
assert len(frame) == 4
assert frame.size == 4
view = memoryview(frame)
assert view.readonly
assert view.tobytes() == bytes(frame)
assert bytes(wse.copy_frame(b"")) == b""
assert bytes(wse.copy_frame(memoryview(bytearray([1, 2, 3])))) == b"\x01\x02\x03"

try:
    wse.copy_frame(memoryview(bytearray(range(8)))[::2])
    raise AssertionError("non-contiguous input must fail")
except wse.WseError as error:
    assert error.category == 1
    assert error.code == 2
    assert error.native_code == 0

try:
    wse.wait(-1)
    raise AssertionError("negative wait must fail")
except wse.WseError as error:
    assert error.category == 1
    assert error.code == 1

wse.wait(1)
with wse.Runtime() as runtime:
    assert not runtime.closed
    assert runtime.info() == info
    assert bytes(runtime.copy_frame(b"runtime")) == b"runtime"
assert runtime.closed

try:
    runtime.wait(0)
    raise AssertionError("closed runtime must fail")
except wse.WseError as error:
    assert error.category == 3

runtime = wse.Runtime()
cancelled = []


def wait_until_cancelled():
    try:
        runtime.wait(10_000)
    except wse.WseError as error:
        cancelled.append(error.category)


thread = threading.Thread(target=wait_until_cancelled)
started = time.monotonic()
thread.start()
time.sleep(0.02)
runtime.close()
thread.join(timeout=1.0)
assert not thread.is_alive()
assert time.monotonic() - started < 1.0
assert cancelled == [6]
runtime.close()
