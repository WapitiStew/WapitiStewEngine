# WebCamera Migration Guide

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This guide migrates the historical C++ `WebCamera` methods and the managed `CameraSession` binding
used through Phase 6 to the canonical `WebCamera` API. Applications using ordinary USB/UVC cameras
do not select Media Foundation/DirectShow on Windows or V4L2/libcamera on Linux.

## C++ mapping

| Old operation | Canonical operation | Notes |
| --- | --- | --- |
| `connect(index)` | Select a device from `WebCamera::enumerate()` | Retains stable identity and transport/USB data |
| `setStreamSize()`/`setStreamFPS()` | Select from `capabilities().stream_profiles` | Resolution, FPS, and native pixel format stay together |
| `open()` | `open(device, configuration)` | Separates native and output formats |
| `play()`/`pause()` | `start()`/`stop()` | A stopped open session may be started again |
| `getStreamFrame()` | `readFrame(timeout_ms)`, or `wse::tmr::readImage(camera, timeout_ms, image)` for a Core image | The returned frame or image owns its bytes |
| `getStreamFrame(average_count)` | `wse::tmr::readAveragedFrame(camera, count, timeout_ms)`, or `readAveragedImage()` for a Core image | Reads the frames and averages them; a `CameraFrameAccumulator` does the same from a streaming callback |
| `setStreamCollback()` | `start(CameraFrameCallback)` | New code does not pass a raw object pointer |
| `setExposure()`/`setGain()` | Check `controlCapability()`, then `setControl()`, or `writeExposure()`/`writeGain()`/`writeFocus()` | Validate support, mode, range, step, and unit |
| `getExposureTime()`/`getGain()` | `getControl()`, or `readExposure()`/`readGain()`/`readFocus()` | The convenience accessors name the verb and the control |
| `setStreamDOR()`/`getStreamDOR()` | `wse::tmr::applyOrientation(frame, orientation)` | Adds 90-degree rotation to the legacy 180-degree and mirror operations |
| RGB `img3c08_t` frame | `wse::tmr::toImage(frame, image)` or `readImage()` | A `Bgr8` frame now yields a BGR-ordered image and a `Bgra8` frame keeps four channels, rather than both collapsing to RGB |
| OpenCV conversion | A consumer adapter over `readFrame()`, or a Core image from `toImage()` | Kept out of device control |
| `showSettingsWindow()` | No direct portable replacement | Build application UI from capability data |
| `disconnect()` | `close()` | Idempotent and terminal; construct a new owner to reopen |

WSE 1.0.0 removed the old methods. Code written against a pre-1.0.0 release migrates using the
table above rather than against a compiler warning, because the warning no longer exists. See the
[deprecation and migration guide](DeprecationMigration.md) for what else 1.0.0 removed.

## Frame operations

Orientation correction and averaging are ordinary image operations, so they are free functions
rather than device methods: `WebCamera` still does nothing but talk to the device.

Core owns the operation itself and knows nothing about a camera:

```cpp
const wse::img3c08_t corrected =
    wse::applyOrientation( image, wse::eImageOrientation::Rotate90CW );
const wse::img3c08_t averaged = wse::averageImages< wse::ePixFormat::CH3D8 >( images );
```

Tmr maps a camera frame onto those operations and restores the original pixel format:

```cpp
const auto corrected = wse::tmr::applyOrientation(
    frame, wse::eImageOrientation::Rotate180 );
const auto averaged = wse::tmr::readAveragedFrame( camera, 8U, 1000U );
```

`Gray8`, `Rgb8`, `Bgr8`, and `Bgra8` are supported. Neither operation interprets a channel, so RGB
and BGR need no separate handling. `Yuyv422`, `Uyvy422`, and `Nv12` are subsampled and `Mjpeg` is compressed, so
moving or adding whole pixels has no defined meaning for them and they are refused with
`UnsupportedFormat` rather than silently mishandled.

`CameraFrameAccumulator` holds one accumulation buffer whatever the frame count, so averaging 64
frames costs four times one frame rather than sixty-four, and it accepts frames from a streaming
callback as well as from `readFrame()`.

## Managed languages

The old Python, JavaScript, and Java `CameraSession` and `CameraOpenDescription` bindings were
removed in 7-A5. Each language has one replacement.

| Language | Construction/ownership | Frame read | Shutdown |
| --- | --- | --- | --- |
| JavaScript | `new wse.WebCamera()` | `await camera.readFrame(timeout)` | `camera.close()` |
| Python | `wse.WebCamera()` | `camera.read_frame(timeout)` | `with` or `camera.close()` |
| Java | `new WebCamera()` | `camera.readFrame(timeout)` | try-with-resources or `close()` |

Public values cover devices, USB identity, native/output profiles, control capabilities/values,
extension-unit selectors/values, and owned frames. Do not carry native pointers, COM objects, ioctl
structures, or backend handles into the replacement API.

## Formats and controls

`Uyvy422` (value 15) preserves U0 Y0 V0 Y1 native bytes when conversion is disabled.
It differs from `Yuyv422`; select a reported native profile instead of relabeling its bytes.


- `native_format` is the capture format advertised by the camera.
- `output_format` is returned by WSE. Select the same format when conversion is not required.
- With `allow_conversion=false`, a request that requires conversion fails explicitly.
- Use only enumerated controls; before writing, verify `writable`, mode, range, step, and unit.
- Hardware tests that change a control must restore its original value and mode.
- Use an extension unit only when its known device schema, advertised selector, access mode, and
  payload length agree. Never probe an unknown selector with writes.

On Windows, an advertised odd-height UYVY/YUYV profile can also produce `Bgra8` with
`allow_conversion=true`; WSE selects the internal acquisition route. Keep the native profile
unchanged and choose BGRA as the output. Conversion produces opaque alpha (255), retains row
order and frame metadata, and does not calibrate color. The requested frame rate must be read
back exactly (one 100 ns tick of rounding is allowed); unsupported substitutions fail open.
After a terminal Media Foundation read error, close and reopen before restarting. A generic
`ReadFailed` does not by itself mean that the camera was unplugged. Details and contract tests
are in [Tmr Camera](../design/en/TmrCamera.md#packed-uyvy-frames).

## CameraSession scope

C++ `CameraSession` remains the low-level portable foundation for CSI, virtual cameras, explicit
backend selection, and the `WebCamera` implementation. Use `WebCamera` for ordinary USB/UVC cameras
and for JavaScript, Python, and Java.

## Rollback

The pre-deletion SDK source is preserved by Engine tag
`wse-webcamera-legacy-baseline-20260829`. Create a dedicated branch from a tag and restore the old
implementation in a rollback commit; do not hard-reset the active work branch.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
