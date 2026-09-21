"""Portable TMR video stream: ten consecutive frames with their sequence numbers and intervals.

Hardware-free by design: a machine with no attached camera reports that and exits without failing.
Build with WSE_BUILD_TMR=ON and WSE_BUILD_PYTHON_BINDING=ON.

The C++ counterpart receives the same ten frames through the push callback that runs on the
session's worker thread. The binding exposes start(callback) as well, but the pull loop below keeps
every frame on the calling thread and keeps the ordering of the report obvious, which is what a
Python caller usually wants. A callback would run on that worker thread, so anything it touched
would need its own protection; the pull loop needs none.

The binding has no log facility, so the report goes through print, and no Core image layer, so the
frames are described rather than converted.

With no argument the sample imports the installed `wse` package; with an argument it loads the
built extension module from that path instead.

Exit status distinguishes the two ways this can end without frames. No camera, or no camera with a
usable profile, is a fact about the machine and returns 0. A camera that was opened and streaming
but did not deliver the full count returns 1, because that is a real failure of the stream.

    python example/python/tmr/video_stream.py [path to the built _wse module]
"""

import importlib.util
import sys
from pathlib import Path

# Ten frames is long enough for the interval to settle after the first capture and short enough to
# finish in well under a second at any advertised frame rate.
TARGET_FRAMES = 10

# Generous next to a 30 fps interval of about 33 ms. A camera that misses this is not merely slow,
# so the sample reports the failure rather than waiting longer. The deadline bounds the wait for
# one frame, not the loop, so the run as a whole can take up to TARGET_FRAMES times this long.
READ_TIMEOUT_MS = 2000


def load_wse():
    """Loads the binding, either as the installed package or from a built module path.

    With no argument the installed `wse` package is imported. With one, the built `_wse` module at
    that path is loaded directly, which is how the sample runs against a build tree.
    """
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
    if not hasattr(wse, "WebCamera"):
        raise SystemExit("This WSE build does not include the TMR component.")

    # An empty enumeration is not a failure. The sample is meant to run on a machine with no camera,
    # so it says so and reports success. enumerate() lists USB/UVC devices only; CSI, network, and
    # virtual cameras are filtered out, so an empty list on a laptop showing a virtual meeting
    # camera is the expected answer rather than a missed device.
    devices = wse.WebCamera.enumerate()
    if not devices:
        print("No USB/UVC web camera is attached.")
        return 0

    # Take the first device that advertises a profile with at least one output format. A profile
    # without one cannot be opened for capture. The static capabilities() inspects a device this
    # process does not own, so no session exists yet and nothing has to be closed if it fails;
    # current_capabilities() is the one that describes an already open session.
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

    # The native format is what the sensor delivers and the output format is what the caller wants.
    # allow_conversion lets the backend bridge the two; without it a mismatch is refused outright.
    configuration = wse.CameraStreamConfiguration()
    configuration.native_format = profile.native_format
    configuration.output_format = profile.output_formats[0]
    configuration.allow_conversion = True

    received = 0
    intervals_ns: list[int] = []
    failure_message = ""

    # The context manager is the single owner; leaving it stops and closes the session. More
    # precisely, __exit__ calls release(), which is the one-way step: it stops, closes, and hands
    # the native camera back, and every later call on the object then reports InvalidState. close()
    # would end only the session and leave the object open()able again.
    with wse.WebCamera() as camera:
        # open() fixes the resolution for the whole session and creates the session only, so
        # start() is still required before any frame arrives.
        camera.open(device, configuration)
        camera.start()
        try:
            previous_timestamp_ns = 0
            while received < TARGET_FRAMES:
                try:
                    frame = camera.read_frame(READ_TIMEOUT_MS)
                except wse.WseError as failure:
                    # Unlike the XPT samples, a timeout here is not an expected outcome: the
                    # session is streaming, so a frame was due. Category and code are the stable
                    # part of the report and are what a caller should branch on; native_code is
                    # backend diagnosis and the message is free text that may be reworded.
                    failure_message = str(failure)
                    print(
                        f"ERROR: frame read failed. category={failure.category}"
                        f" code={failure.code} native={failure.native_code}"
                        f" message={failure}"
                    )
                    break

                # The timestamp comes from a monotonic clock, so it is safe to subtract even if the
                # wall clock is adjusted mid-run. The sequence number is the device's own counter:
                # a gap in it means the pipeline dropped frames, which an even interval alone
                # would not reveal.
                # The first frame has no predecessor, so its delta is reported as zero. That zero
                # is deliberately excluded below, leaving nine intervals to average over ten
                # frames, so one made-up value cannot drag the average down.
                delta_ns = (
                    0 if received == 0 else frame.monotonic_timestamp_ns - previous_timestamp_ns
                )
                previous_timestamp_ns = frame.monotonic_timestamp_ns
                received += 1
                if received > 1:
                    intervals_ns.append(delta_ns)
                print(
                    f"frame {received}/{TARGET_FRAMES}: sequence={frame.sequence}"
                    f" timestamp_ns={frame.monotonic_timestamp_ns}"
                    f" delta_ms={delta_ns / 1_000_000.0:.3f}"
                    f" {frame.width}x{frame.height} bytes={len(frame.data)}"
                )
        finally:
            # stop() only ends streaming and leaves the device open; the release() that __exit__
            # performs is what gives the camera back. Each frame owns a copy of its pixel bytes,
            # so anything kept from the loop stays readable after both.
            if camera.is_streaming:
                camera.stop()

    if received < TARGET_FRAMES:
        print(f"stream incomplete: {received} of {TARGET_FRAMES} frames arrived", failure_message)
        return 1

    average_ms = (sum(intervals_ns) / len(intervals_ns)) / 1_000_000.0 if intervals_ns else 0.0
    print(
        f"stream complete: {received} frames received,"
        f" average interval {average_ms:.3f} ms"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
