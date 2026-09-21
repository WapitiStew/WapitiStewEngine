import importlib.util
import sys
from pathlib import Path


module_path = Path(sys.argv[1]).resolve()
fixture = dict(line.split("=", 1) for line in Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
               if line and not line.startswith("#"))
number = lambda name: int(fixture[name])
spec = importlib.util.spec_from_file_location("_wse", module_path)
if spec is None or spec.loader is None:
    raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
wse = importlib.util.module_from_spec(spec)
sys.modules["_wse"] = wse
spec.loader.exec_module(wse)

# Exercise the same package entry point consumers import, not only the native module.
native = wse
package_path = Path(__file__).resolve().parents[2] / "lang/python/wse/__init__.py"
sys.modules["wse._wse"] = native
package_spec = importlib.util.spec_from_file_location("wse", package_path,
    submodule_search_locations=[str(package_path.parent)])
if package_spec is None or package_spec.loader is None:
    raise RuntimeError(f"Unable to load WSE Python package: {package_path}")
wse = importlib.util.module_from_spec(package_spec)
sys.modules["wse"] = wse
package_spec.loader.exec_module(wse)
for name in ("ImageOrientation", "BayerPattern", "DemosaicMethod", "CameraFrameAccumulator",
             "is_frame_operation_supported", "is_frame_averaging_supported", "is_bayer_format",
             "bayer_pattern_of", "apply_orientation", "demosaic_frame"):
    assert name in wse.__all__ and getattr(wse, name) is getattr(native, name), name

assert wse.runtime_info()["components"]["tmr"]
assert not hasattr(wse, "CameraSession")
devices = wse.WebCamera.enumerate(wse.CameraBackend.AUTOMATIC)
assert isinstance(devices, list)

invalid = wse.CameraDevice()
try:
    wse.WebCamera.capabilities(invalid)
    raise AssertionError("invalid camera must fail")
except wse.WseError as error:
    assert error.category != 0
    assert error.code != 0

control = wse.CameraControlCapability()
control.control = wse.CameraControl(number("control"))
control.minimum = number("control_minimum")
control.maximum = number("control_maximum")
control.step = number("control_step")
control.default_value = number("control_default")
control.unit = wse.CameraControlUnit(number("control_unit"))
control.physical_scale = 100.0
control.supports_manual = True
assert control.value_from_normalized(0.52) == 50
assert control.normalized_from_value(50) == 0.5
assert control.physical_from_value(10) == 1000.0
assert control.value_from_physical(5200.0) == 50

assert wse.CameraTransport.USB_UVC.value == 1
assert wse.CameraControl.POWER_LINE_FREQUENCY.value == 17

fixture_device = wse.CameraDevice()
fixture_device.backend = wse.CameraBackend(number("device_backend"))
fixture_device.id = fixture["device_id"]
fixture_device.display_name = fixture["device_display_name"]
fixture_device.transport = fixture["device_transport"]
fixture_device.transport_type = wse.CameraTransport(number("device_transport_type"))
fixture_device.usb.vendor_id = number("usb_vendor_id")
fixture_device.usb.product_id = number("usb_product_id")
fixture_device.usb.serial_number = fixture["usb_serial_number"]
fixture_device.usb.uvc_version_bcd = number("usb_uvc_version_bcd")
profile = wse.CameraStreamProfile()
profile.native_format.width = number("profile_width")
profile.native_format.height = number("profile_height")
profile.native_format.frame_rate_numerator = number("profile_fps_numerator")
profile.native_format.frame_rate_denominator = number("profile_fps_denominator")
profile.native_format.pixel_format = wse.CameraPixelFormat(number("profile_native_pixel_format"))
profile.output_formats = [wse.CameraPixelFormat(number("profile_output_pixel_format"))]
assert fixture_device.transport_type == wse.CameraTransport.USB_UVC
assert profile.native_format.pixel_format == wse.CameraPixelFormat.YUYV422
assert profile.output_formats == [wse.CameraPixelFormat.BGRA8]

selector = wse.CameraExtensionUnitSelector()
selector.unit_guid = bytes.fromhex(fixture["xu_guid_hex"])
selector.unit_id = number("xu_unit_id")
selector.selector = number("xu_selector")
selector.minimum_size = number("xu_minimum_size")
selector.maximum_size = number("xu_maximum_size")
selector.readable = True
selector.writable = True
selector.display_name = "fixture-xu"
value = wse.CameraExtensionUnitValue()
value.selector = selector
value.payload = bytes.fromhex(fixture["xu_payload_hex"])
assert value.payload == bytes.fromhex(fixture["xu_payload_hex"])


def open_invalid(camera):
    """Returns the structured error an invalid device produces, which must not depend on close()."""
    try:
        camera.open(invalid)
    except wse.WseError as failure:
        return (failure.category, failure.code)
    raise AssertionError("opening an invalid device must fail")


with wse.WebCamera() as camera:
    assert not camera.is_open
    assert not camera.is_streaming
    assert not camera.released
    try:
        camera.start()
        raise AssertionError("start before open must fail")
    except wse.WseError as error:
        assert error.category == number("closed_error_category")
        assert error.code == number("not_open_error_code")

    callbacks = []
    for _ in range(2):
        try:
            camera.start(callbacks.append)
            raise AssertionError("callback start before open must fail")
        except wse.WseError as error:
            assert error.category == number("closed_error_category")
            assert error.code == number("not_open_error_code")
    assert callbacks == [], "A rejected start must never deliver a callback"

    # close() ends the session the way the C++ WebCamera does; the object stays open()able, so the
    # second attempt must reach the device layer and report exactly what the first one did.
    before_close = open_invalid(camera)
    camera.close()
    assert not camera.is_open
    assert not camera.released, "close() must not retire the object"
    assert open_invalid(camera) == before_close, "close() must leave the WebCamera reopenable"
    camera.close()

# Leaving the with block releases the native camera, and that step is the terminal one.
assert camera.released
camera.close()
camera.release()
try:
    camera.start()
    raise AssertionError("released WebCamera must reject operations")
except wse.WseError as error:
    assert error.category == 3


# ---------------------------------------------------------------------------------------------
# Frame operations
# ---------------------------------------------------------------------------------------------

# The formats added for a Bayer sensor reach Python with the same names the C++ side uses.
for name in ("GRAY16", "RGB16", "BGR16",
             "BAYER16_RGGB", "BAYER16_BGGR", "BAYER16_GRBG", "BAYER16_GBRG", "UYVY422"):
    assert hasattr(wse.CameraPixelFormat, name), name
assert hasattr(wse.CameraControl, "FRAME_RATE")

# Orientation moves whole pixels; averaging combines a site with itself. A Bayer frame therefore
# refuses the first and accepts the second, and the two questions have two answers.
assert wse.is_frame_operation_supported(wse.CameraPixelFormat.RGB8)
assert not wse.is_frame_operation_supported(wse.CameraPixelFormat.BAYER16_RGGB)
assert wse.is_frame_averaging_supported(wse.CameraPixelFormat.BAYER16_RGGB)
assert not wse.is_frame_averaging_supported(wse.CameraPixelFormat.MJPEG)
assert wse.is_bayer_format(wse.CameraPixelFormat.BAYER16_BGGR)
assert not wse.is_bayer_format(wse.CameraPixelFormat.RGB8)
assert wse.bayer_pattern_of(wse.CameraPixelFormat.BAYER16_GRBG) == wse.BayerPattern.GRBG
try:
    wse.bayer_pattern_of(wse.CameraPixelFormat.RGB8)
    raise AssertionError("a format with no Bayer layout must fail")
except wse.WseError as error:
    assert error.category != 0

# 3x2 so that a quarter turn is visible in the extent.
rgb = wse.CameraFrame(
    width=3, height=2, pixel_format=wse.CameraPixelFormat.RGB8,
    data=bytes(range(1, 3 * 2 * 3 + 1)))
assert rgb.row_stride == 3 * 3
assert len(rgb.data) == 18

turned = wse.apply_orientation(rgb, wse.ImageOrientation.ROTATE_90_CW)
assert turned.width == 2 and turned.height == 3
assert turned.row_stride == 2 * 3
half = wse.apply_orientation(rgb, wse.ImageOrientation.ROTATE_180)
assert bytes(wse.apply_orientation(half, wse.ImageOrientation.ROTATE_180).data) == bytes(rgb.data)

# Averaging one frame with itself returns that frame.
accumulator = wse.CameraFrameAccumulator()
assert accumulator.count == 0
accumulator.add(rgb)
accumulator.add(rgb)
assert accumulator.count == 2
assert bytes(accumulator.average().data) == bytes(rgb.data)
accumulator.reset()
assert accumulator.count == 0

# A Bayer frame converts to colour, and the eight-bit result keeps the high byte of each sample.
bayer = wse.CameraFrame(
    width=4, height=4, pixel_format=wse.CameraPixelFormat.BAYER16_RGGB,
    data=bytes(4 * 4 * 2))
colour = wse.demosaic_frame(bayer, wse.CameraPixelFormat.RGB8)
assert colour.pixel_format == wse.CameraPixelFormat.RGB8
assert colour.width == 4 and colour.height == 4
assert colour.row_stride == 4 * 3
wide = wse.demosaic_frame(bayer, wse.CameraPixelFormat.BGR16, wse.DemosaicMethod.BLOCK_2X2)
assert wide.row_stride == 4 * 3 * 2

try:
    wse.apply_orientation(bayer, wse.ImageOrientation.ROTATE_180)
    raise AssertionError("orientation must refuse a Bayer frame")
except wse.WseError as error:
    assert error.category != 0
try:
    wse.demosaic_frame(rgb, wse.CameraPixelFormat.RGB8)
    raise AssertionError("demosaic must refuse a frame that is not Bayer")
except wse.WseError as error:
    assert error.category != 0

# Averaging a Bayer frame is allowed, which is what a sensor needs before the colour is raised.
bayer_accumulator = wse.CameraFrameAccumulator()
bayer_accumulator.add(bayer)
assert bayer_accumulator.average().pixel_format == wse.CameraPixelFormat.BAYER16_RGGB
