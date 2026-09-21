"""Portable TMR resolution change: capture one frame per stream profile.

The resolution is fixed at open(), so the sample closes the session and opens a second one with a
different stream profile. Hardware-free by design: a machine with no attached camera reports that
and exits without failing. Build with WSE_BUILD_TMR=ON and WSE_BUILD_PYTHON_BINDING=ON.

The C++ counterpart saves each capture as a BMP and reuses one WebCamera object. The Python binding
has no image writer, so this sample reports each frame's shape and byte count instead; it reuses one
WebCamera the same way, because close() ends the session and leaves the object open()able again.
Leaving the `with` block releases the native camera for good.

    python example/python/tmr/resolution_change.py [path to the built _wse module]
"""

import importlib.util
import sys
from pathlib import Path

READ_TIMEOUT_MS = 2000


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


def describe_format(camera_format) -> str:
    return f"{camera_format.width}x{camera_format.height}"


def capture_once(camera, device, configuration) -> None:
    """Opens one session on the given camera, reads a single frame, and closes that session."""
    camera.open(device, configuration)
    camera.start()
    try:
        frame = camera.read_frame(READ_TIMEOUT_MS)
        print(
            f"captured {describe_format(configuration.native_format)}:"
            f" {frame.width}x{frame.height} format={frame.pixel_format}"
            f" bytes={len(frame.data)}"
        )
    finally:
        if camera.is_streaming:
            camera.stop()
        # The resolution is fixed at open(), so the session ends here and the next one begins on
        # the same object.
        camera.close()


def main() -> int:
    wse = load_wse()
    if not hasattr(wse, "WebCamera"):
        raise SystemExit("This WSE build does not include the TMR component.")

    devices = wse.WebCamera.enumerate()
    if not devices:
        print("No USB/UVC web camera is attached.")
        return 0

    device = devices[0]
    capability = wse.WebCamera.capabilities(device)

    # Collect two profiles with different frame sizes; both must offer a usable output.
    first_profile = None
    for profile in capability.stream_profiles:
        if profile.output_formats:
            first_profile = profile
            break
    if first_profile is None:
        print("The camera has no usable stream profile.")
        return 0

    first = wse.CameraStreamConfiguration()
    first.native_format = first_profile.native_format
    first.output_format = first_profile.output_formats[0]
    first.allow_conversion = True

    second = None
    for profile in capability.stream_profiles:
        different_size = (
            profile.native_format.width != first.native_format.width
            or profile.native_format.height != first.native_format.height
        )
        if not different_size or not profile.output_formats:
            continue
        second = wse.CameraStreamConfiguration()
        second.native_format = profile.native_format
        second.output_format = (
            first.output_format
            if profile.supports_output(first.output_format)
            else profile.output_formats[0]
        )
        second.allow_conversion = True
        break

    with wse.WebCamera() as camera:
        print("first resolution:", describe_format(first.native_format))
        capture_once(camera, device, first)

        if second is None:
            print("The camera advertises only one resolution; nothing to change.")
            return 0

        print("second resolution:", describe_format(second.native_format))
        capture_once(camera, device, second)
    print(
        "captured both resolutions:",
        describe_format(first.native_format),
        "and",
        describe_format(second.native_format),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
