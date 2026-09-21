# WonderStewEngine Public API Reference

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This reference covers headers installed under `include/wse/api` and the optional installed language
bindings. Implementation details, device-specific extensions, and components absent from the public
package are out of scope.

See the [deprecation and migration guide](DeprecationMigration.md) for every deprecated declaration,
its canonical replacement, and removal conditions.
Cross-cutting platform, ownership, thread, callback, shutdown, and error rules are defined by the
[Architecture](../design/en/Architecture.md), [Thread and Ownership Model](../design/en/ThreadOwnership.md),
and [Public API Policy](../design/en/PublicApiPolicy.md); component sections below link their detailed contract.

C ABI diagnostics retain at most 1023 UTF-8 bytes plus NUL without allocation. C# native calls
retain SafeHandle inputs until return; Dispose does not cancel ongoing work. Mutable operations
still require caller serialization. See [C ABI](../design/en/CAbiContract.md) for layouts and lifetime rules.

## Core indexed data access

Use `matrix[y][x]`, `homography[y][x]`, `image[y][x][channel]`, `pixel[channel]`, and `mesh[y][x]`
for direct mutable or const access. The [Portable Core contract](../design/en/PortableCore.md#fast-data-access)
defines valid indices, storage lifetime, and checked alternatives.
The [Core Data Model](../design/en/CoreDataModel.md) connects the types to their row-major
storage, channel order, ownership and external-buffer layout.
Use [Core Algorithms](../design/en/CoreAlgorithms.md) for formulas and numerical examples,
and [Core Services](../design/en/CoreServices.md) for Timer, logging and License lifecycle rules.

```cpp
wse::Matrix matrix(2, 3); // rows, columns
matrix[1][2] = 42.0;
const auto& view = matrix;
const auto* row = view[1];
wse::img3c8_t image(640, 480);
image[10][20][2] = 255;
```

## Package entry points

See [Build and Packaging](../design/en/BuildPackaging.md) for installed layout, dependency propagation
and explicit component-target checks; [C ABI](../design/en/CAbiContract.md) defines handle ownership,
status layout, thread-local diagnostics and two-call buffers for the flat interface.

For binary block layout and the supported setting-table grammar, see
[GEF File Formats](../design/en/GefFileFormats.md), including its read/write limitations.

GEF adds `ReadLimits`, `readWithLimits`, CSV `readReplace` and both controllers’ `writeAtomic`.
Legacy CSV `read` still appends; BIN reads replace transactionally and support read-then-write.
Use bounded reads for external input and atomic replacement to retain the original after a failed save.

| Component | Header/entry point | Purpose |
| --- | --- | --- |
| Core | `<wse/stew.h>` / `WSE::Core` | Data, logging, utilities |
| XPT | `<xpt/stew.h>` / `WSE::Xpt` | TCP, UDP, HTTP, serial, retry |
| GEF | `<gef/stew.h>` / `WSE::Gef` | Generic file formats |
| IUI | `<iui/stew.h>` / `WSE::Iui` | Input devices |
| OUI | `<oui/stew.h>` / `WSE::Oui` | Display, rendering, projection |
| Tmr | `<tmr/stew.h>` / `WSE::Tmr` | Portable camera control |
| CV adapter | `<cv/OpenCvAdapter.h>` (header-only, opt-in) | OpenCV interop; requires consumer-provided OpenCV |

Only targets enabled in the package manifest exist. Shared/static linkage and component selection are
defined by the [Build and Install Guide](BuildGuide.md).

## Portable transport (XPT)

- `TcpClient`, `UdpClient`, and `SerialPort` are move-only RAII resource owners with structured
  `TransportResult<T>` errors, explicit timeout, and cancellation.
- `TcpClient::checkPeerConnection()` observes an OS-visible FIN/reset without consuming pending
  bytes. Success is not a remote liveness guarantee.
- `HttpClient` does not expose libcurl types and does not implicitly retry non-idempotent requests.
- TCP reconnect closes the old socket after validation; UDP rebind and Serial reopen commit a
  replacement only on success. HTTP takes request, execution options and context separately.
- `SerialPort` is the only serial surface; the legacy `SerialConnector` was removed on 2026-09-13
  (see the [deprecation and migration guide](DeprecationMigration.md#xpt-transport)).

See the [XPT Transport design](../design/en/XptTransport.md) for owner/state tables, partial progress,
deadline/cleanup limits, HTTP attempt order and retry calculation.

## Core threads, timer, and IUI

Linux reports `PermissionDenied` when all key sources fail and a key query reports EACCES/EPERM;
otherwise total capture loss reports `Disconnected`. A later scan can recover without replacing the owner.

- Worker threads are internal: no public class exposes a thread, atomic, or mutex, and the legacy
  `BMultiThread` worker base was removed on 2026-09-13
  (see the [deprecation and migration guide](DeprecationMigration.md#core-worker-timer)).
- `Timer` is final and owns its worker: `start(std::chrono::milliseconds, TimerCallback)`,
  `setInterval()`, `stop()`, `isRunning()`. It no longer derives from `BMultiThread`; the old
  `initialize()`/`updateTimerConfig()`/`setTimerCallback()` surface was removed on 2026-09-13
  (see the [deprecation and migration guide](DeprecationMigration.md#core-worker-timer)).
- Windows and Linux `Keyboard::snapshot()` return all key groups from one synchronized update. The
  owned `KeyCallback` is installed with `setCallback()`. `accessState()` reports `Starting`, `Ready`,
  `Unavailable`, `PermissionDenied`, or `Disconnected`; `isAvailable()` is true only for `Ready`.
  A non-ready all-released snapshot is not evidence of physical release. Linux uses read-only evdev,
  does not grab devices or alter permissions, and maps a US-layout ASCII compatibility state rather
  than localized text. Windows Ready is not a physical-device/access probe. Status and snapshot
  reads are separate observations. Copies share the owned callable, and `clearCallback()` does
  not wait for an acquired callable; mutable captures require application synchronization.

See the [Portable Core design](../design/en/PortableCore.md) for move, readiness, callback-thread,
exception, clear, and destruction rules, and the [IUI Keyboard design](../design/en/IuiKeyboard.md)
for platform-specific availability, polling order, callback/copy lifetime and hardware-validation rules.

Python WseTransferError, Java TransferException, JavaScript TransferError fields and C#
TransferException preserve failed-send counts and truncated UDP prefix/source (BIND-TRANSFER-05).
Existing base exception handlers still catch them. Read the count before deciding how to recover;
never automatically replay the original payload. See [binding error values](../design/en/LanguageBindings.md).

## Tmr WebCamera

- Discovery/capability: `WebCamera::enumerate()` and `WebCamera::capabilities()`
- Profile selection: combine `sCameraStreamProfile::native_format` with one advertised
  `output_formats` value in `sCameraStreamConfiguration`
- C++ lifecycle: `open()`, `start()`, `readFrame()` or callback, `stop()`, and idempotent `close()`;
  closing the device session permits reopening the same ordinary owner
- Controls: `currentCapabilities()`, `controlCapability()`, `getControl()`, and `setControl()`
- UVC extension units: `getExtensionUnit()` and `setExtensionUnit()` for advertised selectors whose
  schema and payload length are known
- State: `isOpen()` and `isStreaming()`

Check every `CameraResult<T>`/`CameraStatus`. Failures provide structured category, code, native code,
and message values. Frames and XU payloads own their bytes. Media Foundation, DirectShow, V4L2,
libcamera, COM, ioctl, and native handles do not cross the public boundary.

C++ `CameraSession` remains the low-level portable foundation. `WebCamera` is the canonical owner for
ordinary USB/UVC cameras and all managed languages. Historical lifecycle, raw callback, image
processing, and settings-window methods were removed from the public API; see the
[migration guide](DeprecationMigration.md#tmr-webcamera). Frame processing uses CameraFrameOps.
[Tmr design](../design/en/TmrCamera.md) specifies selection, callback completion and byte layout;
callback delivery ending does not necessarily stop the backend. Binding session close and terminal
owner release follow the language-specific [ownership contract](../design/en/LanguageBindings.md).

Libcamera uses transactional request/mapping ownership (TMR-LIBCAMERA-06). After a copy/control
exception or requeue failure, stop and check the result before restarting. Pending requests remain
owned after failed stop; close can wait indefinitely for a disconnected pipeline. See
[Tmr resource and shutdown limits](../design/en/TmrCamera.md).

Windows MF enumeration/name allocations, capability sources and partial open have scoped cleanup
(TMR-MF-INIT-07). An allocation exception can still propagate after cleanup. Perform final close
on the opening owner thread; a Shutdown attempt is not proof of driver recovery. See
[Tmr initialization ownership](../design/en/TmrCamera.md).

## Portable rendering (OUI)

- The canonical move-only owner is `Renderer`: `initialize()`, create resources/surfaces, render or
  present, then `shutdown()`.
- Resource operations include `createTexture`, `uploadTexture`, `createMesh`, `updateMesh`,
  `destroyTexture`, and `destroyMesh`.
- Handles belong to one active renderer lifetime; discard them at shutdown and never exchange them
  between renderer instances. See [OUI resource and submission details](../design/en/OuiRenderer.md)
  and [Projection formulas and pass sequencing](../design/en/OuiProjection.md).
- Surface/display operations include `createSurface`, `getSurfaceTexture`, `resizeSurface`,
  `setSurfaceWindowMode`, `enumerateDisplays`, event processing, presentation, and destruction.
- `executeRenderPass`, `waitFence`, and `readTexture` provide the portable execution boundary.
- Projection code uses `ProjectionPipeline` and `ProjectionMeshAdapter`.

`renderProjection()` supplies the output extent for scale two. Low-level `ProjectionPipeline` attempts
both temporary-handle releases even on exceptions; a failed destroy can retain resources until Renderer
shutdown. See [Projection failure order](../design/en/OuiProjection.md).

Texture, mesh, surface, and fence values are generation-bearing opaque identities, not native GPU
handles. Callers serialize access to one `Renderer` and handle failures through
`RendererResult<T>`/`RendererError`.

`InterfaceGPU`, `RenderTexture2D`, `ScreenRenderer`, `ProjectionRenderer`, and `WindowRenderer` remain
legacy compatibility APIs. `InterfaceGPU::device()`/`factory()` are non-owning views and must not be
released by callers. Copies of `InterfaceGPU` and `RenderTexture2D` share internal RAII owners, so
releasing/finalizing one copy does not invalidate another. Do not add their legacy `void*` GPU aliases
to new code, language bindings, or module boundaries. See the
[OUI Renderer design](../design/en/OuiRenderer.md).

## Language bindings

| Language | Package entry | Camera owner | Owned frame |
| --- | --- | --- | --- |
| JavaScript | `lang/js/index.js` | `new WebCamera()` | Node `Buffer` |
| Python | `lang/python/wse` | `WebCamera()` | `FrameBuffer` |
| Java | `lang/java/wse.jar` | `new WebCamera()` | `CameraFrame`/`NativeBuffer` |
| C# | `lang/cs/WapitiStew.Wse.dll` | `new WebCamera()` | `CameraFrame` |

JavaScript closes explicitly, Python uses a context manager or `close()`, Java uses
try-with-resources or `close()`, and C# uses `using` or `Dispose()`. The authoritative
generated/type surfaces are `index.d.ts`, `__init__.pyi`, package `io.wapitistew.wse` in the JAR,
and the XML documentation beside `WapitiStew.Wse.dll`.

The C# binding calls the flat C ABI in `api/wse/capi` rather than the C++ classes directly, because
P/Invoke cannot bind C++ classes. It exposes the same components as the other three bindings: Core,
XPT, IUI, OUI, and Tmr.

Every binding exposes the current API only. The deprecated `WebCamera` methods and the
callback-based legacy serial connector are not bound; see the
[deprecation and migration guide](DeprecationMigration.md).

All four bindings expose IUI through a `Keyboard` owner:

| Language | Owner | Snapshot | Close |
| --- | --- | --- | --- |
| JavaScript | `new Keyboard()` | `snapshot()` returns an owned object | `close()` |
| Python | `Keyboard()` | `snapshot()` returns a `dict` of `list[bool]` | context manager or `close()` |
| Java | `new Keyboard()` | `snapshot()` returns a `KeyboardState` | try-with-resources or `close()` |
| C# | `new Keyboard()` | `Snapshot()` returns a `KeyboardState` | `using` or `Dispose()` |

The bindings poll rather than deliver callbacks. `snapshot()` reads all five key groups from one
update point, and it fails with the structured error derived from the readiness state when the
keyboard is not readable, so an unreadable keyboard never looks like an empty snapshot.

All four bindings expose XPT through `TcpClient`, `UdpClient`, `SerialPort`, and an HTTP execute
entry point. Every operation takes an explicit deadline because XPT has no default timeout, and a
failure keeps the stable transport code in `code` while `category` is normalized.

Camera frame timeouts budget waiting, not copying, native cleanup or stop. V4L2 closes on
failed buffer return; check `isOpen()` before restarting. MF always attempts Unlock after a
successful Lock, including copy exceptions. See [native frame ownership](../design/en/TmrCamera.md).

## Safety and unsupported operations

- Do not infer support from a control enum; use the selected device's capability.
- Save and restore a control's value and mode when a hardware test changes it.
- Do not write to unknown extension-unit selectors.
- Do not treat a requested native/output conversion as successful when it is unsupported.
- Do not expose or persist operating-system camera objects in application code.
- Raspberry Pi and physical Linux camera certification remain separate from cross-build success.

See the [How-to](HOWTO.md), [WebCamera migration guide](WebCameraMigration.md),
[Tmr Camera design](../design/en/TmrCamera.md), and
[Language Binding design](../design/en/LanguageBindings.md).

C# owns pending native outputs until SafeHandle adoption and rolls back partial collections/results.
`WebCamera.CallbackException` records the first managed callback/adoption exception and suppresses
later delivery; inspect it after owner-side Stop/join. A delivered frame remains caller-owned,
including when the callback throws. See [C ABI adoption](../design/en/CAbiContract.md).

C# now exposes failed-send progress and truncated UDP prefix/source in `TransferException`,
still catchable as WseException. Read BytesTransferred, ReceivedData and SourceEndpoint; these
managed values require no Dispose. Existing C receive_from remains success-only. Use the
additive `wse_capi_udp_client_receive_from_with_progress` to receive an owned datagram on
DatagramTruncated, and destroy it even on failure. No symbol is removed and ABI version 1
is unchanged. Use matched managed/native artifacts; old shims lack the new entry point.
See [C ABI partial transfer](../design/en/CAbiContract.md) for guarantees and limits.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
