"""Capture one owned frame through the portable WSE WebCamera API.

This is the shortest path from nothing to a frame: enumerate, pick a stream profile, open, start,
read one frame, and let the context manager put everything back. The other TMR samples build on
exactly this shape.

Unlike its neighbours this sample imports the installed `wse` package directly instead of taking
a module path on the command line, so it has to be run against an installed binding. It also
assumes a build that carries TMR; a build without it registers no `wse.WebCamera` at all.

Hardware-free by design in the ordinary case: a machine with no camera prints one line and returns
0, because an absent device is a fact about the machine and not a defect in the sample. Build with
WSE_BUILD_TMR=ON and WSE_BUILD_PYTHON_BINDING=ON.

The binding has no image writer and no log facility, so the frame is reported by shape and byte
count rather than saved, and the report goes through print.

    python example/python/tmr/webcamera.py
"""

import wse


def main() -> int:
    # enumerate() lists USB/UVC devices only; CSI, network, and virtual cameras are filtered out.
    # An empty list on a laptop that shows a virtual meeting camera is therefore the right answer.
    devices = wse.WebCamera.enumerate()
    if not devices:
        print("No USB/UVC web camera is attached.")
        return 0

    # capabilities() is the static form: it inspects a device it does not own, so no session
    # exists yet and there is nothing to close if it fails. Once a camera is open,
    # current_capabilities() is the one that describes the running session. A profile with no
    # output format cannot be streamed, so the first one that advertises one wins.
    device = None
    profile = None
    for candidate in devices:
        capability = wse.WebCamera.capabilities(candidate)
        if capability.stream_profiles and capability.stream_profiles[0].output_formats:
            device = candidate
            profile = capability.stream_profiles[0]
            break
    if device is None or profile is None:
        raise RuntimeError("No enumerated camera has a usable stream profile.")

    # The configuration separates what the sensor captures from what the caller wants to receive.
    # allow_conversion True lets TMR convert between them, which is what makes an MJPEG or YUYV
    # camera deliver the requested output; False demands the device produce output_format natively
    # and turns open() into a failure when it cannot.
    configuration = wse.CameraStreamConfiguration()
    configuration.native_format = profile.native_format
    configuration.output_format = profile.output_formats[0]
    configuration.allow_conversion = True

    # The context manager owns the camera: __exit__ calls release(), which stops the stream, closes
    # the session, and gives the native camera back for good. close() alone would end only the
    # session and leave the object open()able again, which is what resolution_change.py relies on.
    with wse.WebCamera() as camera:
        # open() fixes the resolution for the whole session and creates the session only; start()
        # is still needed before any frame arrives.
        camera.open(device, configuration)
        camera.start()
        try:
            # The timeout bounds the wait for this one frame. Two seconds is many frame periods at
            # any normal rate, so a healthy camera answers well inside it, while a value of a few
            # tens of milliseconds would turn an ordinarily slow first frame into a Timeout. On a
            # stopped session read_frame() reports NotStreaming immediately rather than waiting.
            frame = camera.read_frame(2000)
            print(f"{frame.width}x{frame.height} bytes={len(frame.data)}")
        finally:
            # stop() ends streaming and leaves the device open; the release() at the end of the
            # with block is what actually gives the camera back. The frame owns a copy of its
            # pixel bytes, so it stays readable after both.
            if camera.is_streaming:
                camera.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
