"""Portable TMR exposure control: report the range, capture, change the exposure, capture again.

Hardware-free by design: a machine with no attached camera reports that and exits without failing.
Build with WSE_BUILD_TMR=ON and WSE_BUILD_PYTHON_BINDING=ON.

The C++ counterpart writes a BMP before and after the change. The Python binding hands out frame
bytes and nothing that saves an image file, so this sample reports each frame's shape, sequence,
and byte count instead; the change itself is visible in the reported control values. The C++ side
also has readExposure()/writeExposure() shorthands and a lastError() accessor; the binding offers
neither, so the generic get_control()/set_control() pair and the raised WseError stand in for them.
There is no log facility either, so the report goes through print.

A control is not a property a caller may simply write. Ask control_capability() first: it says
whether the control exists on this device at all, whether it is writable, whether it accepts a
manual mode, and what range and step a value has to land on. A value outside the advertised range
is refused as a validation error before it ever reaches the driver. Anything the sample changes,
it puts back before it leaves.

With no argument the sample imports the installed `wse` package; with an argument it loads the
built extension module from that path instead.

    python example/python/tmr/exposure_control.py [path to the built _wse module]
"""

import importlib.util
import sys
from pathlib import Path

# Bounds the wait for one frame, not the whole sample. Two seconds is many frame periods at any
# normal rate, so a healthy camera never reaches it, while a few tens of milliseconds would turn an
# ordinarily slow frame into a Timeout failure.
READ_TIMEOUT_MS = 2000
# Frames discarded after the write so the sensor settles on the new exposure. A UVC device applies
# an exposure change over the next few frames, so reading immediately would show the old picture
# under the new reported value. Fewer frames risks exactly that; more only costs time.
SETTLE_FRAMES = 5


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


def describe_frame(label: str, frame) -> None:
    print(
        f"{label}: {frame.width}x{frame.height} format={frame.pixel_format}"
        f" sequence={frame.sequence} bytes={len(frame.data)}"
    )


def main() -> int:
    wse = load_wse()
    if not hasattr(wse, "WebCamera"):
        raise SystemExit("This WSE build does not include the TMR component.")

    # enumerate() lists USB/UVC devices only; CSI, network, and virtual cameras are filtered out.
    # An empty list is a fact about the machine, so the sample says so and still reports success.
    devices = wse.WebCamera.enumerate()
    if not devices:
        print("No USB/UVC web camera is attached.")
        return 0

    # The static capabilities() inspects a device this process does not own, so no session exists
    # yet and nothing has to be closed if it fails. A profile with no output format cannot be
    # streamed, so the first one that advertises one wins.
    device = None
    profile = None
    for candidate in devices:
        capability = wse.WebCamera.capabilities(candidate)
        if capability.stream_profiles and capability.stream_profiles[0].output_formats:
            device = candidate
            profile = capability.stream_profiles[0]
            break
    if device is None or profile is None:
        print("No enumerated camera has a usable stream profile.")
        return 0

    # native_format is what the sensor delivers, output_format what the caller wants to receive.
    # allow_conversion True lets TMR bridge the two; False demands the device produce the output
    # format natively and makes open() fail when it cannot.
    configuration = wse.CameraStreamConfiguration()
    configuration.native_format = profile.native_format
    configuration.output_format = profile.output_formats[0]
    configuration.allow_conversion = True

    # The context manager is the single owner; leaving it stops and closes the session. __exit__
    # calls release(), the one-way step that also hands the native camera back, so every early
    # return below still ends with the device given up rather than merely idle.
    with wse.WebCamera() as camera:
        # open() fixes the stream for the session and creates the session only; start() is still
        # required before read_frame() can return anything. Controls are readable either way, but
        # a write is only observable in the pictures once frames are flowing.
        camera.open(device, configuration)
        camera.start()
        try:
            # The advertised capability carries the valid range, step, unit, and access.
            try:
                exposure = camera.control_capability(wse.CameraControl.EXPOSURE)
            except wse.WseError as failure:
                print("this camera reports no exposure control:", failure)
                return 0
            print(
                f"exposure capability: range=[{exposure.minimum},{exposure.maximum}]"
                f" step={exposure.step} default={exposure.default_value}"
                f" unit={exposure.unit} manual={exposure.supports_manual}"
                f" writable={exposure.writable}"
            )
            if not exposure.writable or not exposure.supports_manual:
                print("this camera does not accept a manual exposure; nothing to change")
                return 0

            original = camera.get_control(wse.CameraControl.EXPOSURE)
            print(
                f"exposure before: value={original.value}"
                f" physical={exposure.physical_from_value(original.value)}"
                f" mode={original.mode}"
            )
            describe_frame("frame before", camera.read_frame(READ_TIMEOUT_MS))

            # Move to a clearly different position on the normalized 0.0-1.0 scale.
            target = exposure.value_from_normalized(0.25)
            if target == original.value:
                target = exposure.value_from_normalized(0.75)

            request = wse.CameraControlValue()
            request.control = wse.CameraControl.EXPOSURE
            request.mode = wse.CameraControlMode.MANUAL
            request.value = target
            camera.set_control(request)
            print(
                f"exposure after: value={target}"
                f" physical={exposure.physical_from_value(target)} mode=MANUAL"
            )

            for _ in range(SETTLE_FRAMES):
                camera.read_frame(READ_TIMEOUT_MS)
            describe_frame("frame after", camera.read_frame(READ_TIMEOUT_MS))

            # Restore the setting the device had before this sample ran.
            camera.set_control(original)
            print("restored the original exposure")
        finally:
            if camera.is_streaming:
                camera.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
