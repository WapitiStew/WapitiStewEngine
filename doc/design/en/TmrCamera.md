# WSE Tmr Camera Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document is the normative design for the generic camera API in `WSE::Tmr`. It defines
portable device enumeration, capabilities, stream lifecycle, frame ownership, controls, callbacks,
errors, and calibration values for Windows and Linux. USB Video Class (UVC) cameras retain their
native stream profiles, standard controls, and extension-unit capabilities instead of being reduced
to the smallest common backend feature set. Device-specific access-controlled extensions remain
outside this contract and must not be included in a public package.

The aggregate public header is `<tmr/stew.h>`. The specific headers are
`<tmr/camera/Camera.h>`, `<tmr/camera/CameraTypes.h>`,
`<tmr/camera/CameraError.h>`, and `<tmr/calibration/CameraCalibration.h>`. The high-level web
camera facade is `WebCamera`, and its canonical header is `<tmr/device/WebCamera.h>`. The
`camera/` directory contains portable sessions and contracts; the `device/` directory contains
consumer-facing device classes. There is no `<tmr/camera/WebCamera.h>` header.

## Public API layers

The camera API separates these responsibilities:

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Layer | Public type | Responsibility |
| --- | --- | --- |
| Portable session | `CameraSession` | Backend-independent enumeration, open, streaming, owned frames, errors, and low-level capability operations |
| Web camera facade | `WebCamera` | Web/UVC device selection, native stream profiles, standard controls, automatic modes, and extension-unit operations |
| Calibration | `sCameraCalibration` | Correction values independent of device I/O |
| Private device | Outside the public package | Model-specific protocols, registers, and restricted information |

</div></div>


`WebCamera` owns a `CameraSession`; it does not expose backends through inheritance. Ordinary USB
webcam consumers and language bindings use `WebCamera` as their primary entry point. Advanced users
that need CSI cameras, virtual cameras, or explicit backend selection use `CameraSession`. Neither
surface exposes Media Foundation, DirectShow, V4L2, libcamera, COM types, file descriptors, or
native handles.

`WebCamera` has the same `Closed -> Open -> Streaming -> Open -> Closed` lifecycle as
`CameraSession`. The current surface uses `open(device[, configuration])`, `start`, `stop` and
`close`.

### `WebCamera` facade contract

`WebCamera::enumerate()` returns `UsbUvc` devices and `Unknown` devices whose transport could not be
established by the operating system. It explicitly excludes `Csi`, `Virtual`, and `Network` devices.
Keeping `Unknown` prevents an integrated notebook camera from becoming unusable solely because its
transport cannot be inferred. Consumers must not infer transport from display names or opaque IDs.

Consumers use `open(device)` or `open(device, configuration)`, `start()`/`stop()`, `readFrame()`,
`currentCapabilities()`, `getControl()`/`setControl()`, and the generic XU API. Exposure, gain,
and focus convenience methods use the same capability, range, step, mode, and access validation.
Operational failures use `CameraStatus`/`CameraResult`; `lastError()` exposes the retained error.

Frames reach Core images through `CameraFrameOps.h`, not a camera method; see
[Frames as Core images](#frames-as-core-images). The current public facade has no OS settings
window or one-shot automatic-adjustment convenience API. Unsupported operations exposed by the
current API return structured errors. Device-specific metadata is outside the public package.

## Implementation and owners: TMR-OWNER-01

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Layer | Source | Owns / delegates |
| --- | --- | --- |
| Web facade | [WebCamera.cpp](../../../core/tmr/device/WebCamera.cpp) | Unique Impl and session adapter; selected device, cached capability/configuration and last error; shared enumeration runtime |
| Session | [Camera.cpp](../../../core/tmr/camera/Camera.cpp) | Unique Impl; shared backend; optional callback thread and shared CallbackState |
| Internal seams | [CameraBackend.h](../../../core/tmr/camera/CameraBackend.h), [WebCameraInternal.h](../../../core/tmr/device/WebCameraInternal.h) | Private interfaces/test injection, not installed consumer APIs |
| Windows acquisition | [MediaFoundationCameraBackend.cpp](../../../platform/tmr/win/camera/MediaFoundationCameraBackend.cpp) | Source reader, source and callback COM owners; internal DirectShow route for selected native packed profiles |
| Linux routing/acquisition | [LinuxCameraBackend.cpp](../../../platform/tmr/linux/camera/LinuxCameraBackend.cpp), [V4L2](../../../platform/tmr/linux/camera/V4L2CameraBackend.cpp), [libcamera](../../../platform/tmr/linux/camera/LibcameraCameraBackend.cpp) | Router chooses adapter; each owns descriptors/buffers or camera manager/requests |
| Frame values/processing | [CameraTypes.cpp](../../../core/tmr/camera/CameraTypes.cpp), [CameraFrameOps.cpp](../../../core/tmr/camera/CameraFrameOps.cpp) | Owned byte values and format bridge to Core; no device ownership |

</div></div>


```text
WebCamera unique Impl
  +-- owns --> unique session adapter --> CameraSession unique Impl
                                           +-- retains --> shared backend --> native resources
                                           +-- owns --> callback thread
                                                          +-- retains --> CallbackState
                                                                           +-- retains --> backend
backend -- copies --> owned frame bytes
thread -- borrows loop-local result during invocation --> user callback
```

The worker captures CallbackState and the callable, not the CameraSession object. Shared backend
retention protects the callback-stop lifetime; it does not authorize concurrent owner operations.
CameraSession is neither copyable nor movable. WebCamera is move-only: move transfers its Impl and
session, and move assignment first closes the destination session. A moved-from facade is not an
automatically recreated session. `close()` ends acquisition but keeps an ordinary facade reusable.

<a id="tmr-camera-backends"></a>
## Backends

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Target | Backend | Current implementation scope |
| --- | --- | --- |
| Windows x86-64 | Media Foundation | Enumeration, capabilities, BGRA8 frames, synchronous/callback capture, stop/restart, portable camera controls |
| Linux x86-64/ARM64 | V4L2 | `/dev/video*` enumeration, format/control queries, MMAP streaming, synchronous/callback capture |
| Linux x86-64 | libcamera 0.7.2 | Enumeration, capabilities, stream configuration, requests/buffers, owned frames, stop/restart, disconnection, and portable controls |
| Linux ARM64 | libcamera | Uses the same adapter source; no pinned ARM64 dependency is provisioned by default until the Pi 4 native and hardware gates accept it, and a build without an explicit root returns `UnsupportedBackend` |

</div></div>


`Automatic` selects the platform default. An unavailable backend returns a structured error; it
does not fabricate a device or an empty success. A camera ID is an opaque backend value. Consumers
must not interpret it as a path or symbolic-link format.

An internal Linux router selects a backend independently of V4L2 device I/O. V4L2 and libcamera
have separate adapter seams, so adding another camera stack or device family does not change the
public `CameraSession`. `Automatic` chooses libcamera when the real adapter enumerates a device and
otherwise continues to V4L2. An explicitly selected backend that was not built returns
`UnsupportedBackend` rather than fabricating a device.

Libcamera is pinned to 0.7.2, Git commit
`191e202178f02430b5942397c70d215cdd2056fa`, under LGPL-2.1-or-later. Bootstrap provisions the
source, licenses, and build output under `vendor/libcamera`; CMake configure performs no download.
Because libcamera 0.7.2 requires C++20, only the private adapter source is compiled as C++20. Public
WSE headers, targets, and consumers retain the C++17 requirement.

## Devices and capabilities

`CameraSession::enumerate()` returns `sCameraDeviceInfo` values containing a backend, opaque ID,
display name, and typed transport. Transport distinguishes at least `UsbUvc`, `Csi`, `Virtual`,
`Network`, and `Unknown`. When a UVC device exposes them, optional identity contains its USB vendor
ID, product ID, serial number, and UVC version. Missing identity data is not guessed or replaced by
an empty successful value.

`capabilities()` returns native stream profiles, supported output conversions, and control
capabilities for one device. A native stream profile identifies the width, height, frame-rate
numerator and denominator, and the pixel format emitted by the device. An open request separates the
native profile, the output pixel format delivered to the consumer, and the conversion policy. A
backend that converts MJPEG or YUYV to BGRA8 must not advertise the native profile as BGRA8. A
conversion-disabled request returns `UnsupportedFormat` unless it matches exactly.

`CameraSession::open(sCameraOpenDescription)` is a compatibility path and maps explicitly to a
stream configuration. The open overload that separates the native profile and output format is the
entry point for current code. Backend differences are represented as capabilities or
`UnsupportedFormat`/`UnsupportedControl`, not silent behavior.

Libcamera derives the shortest and longest frame durations, plus a 30 fps candidate when in range,
from `FrameDurationLimits`. A camera that cannot set the requested rate through that control returns
`UnsupportedFormat` instead of advertising a guessed rate.

Beyond the eight-bit formats, a frame may carry `Gray16`, `Rgb16`, `Bgr16`, and the four sixteen-bit
Bayer layouts, whose names give the order of the top-left two-by-two block. A sensor that delivers
its mosaic rather than a picture needs those, and so does anything that wants the sensor samples
instead of an interpretation of them. Adding a future pixel format must not renumber the existing
enumeration values.

`FrameRate` is one of the canonical controls. A device whose frame rate is a register rather than a
rate reports it through `unit` and `physical_scale` like any other control.

A backend may hand a frame over without converting it: asking for the output format the device
already produces, with conversion disabled, delivers the samples as the device sent them. That is
how a mosaic survives the trip, since converting one produces a picture of the mosaic from which no
colour can be raised afterwards.

## Lifecycle and threading

The lifecycle is `Closed -> Open -> Streaming -> Open -> Closed`.

- `open()` requires a valid device and format. A second open returns `AlreadyOpen`.
- `start()` begins a stream for synchronous reads. `start(callback)` adds an owned worker thread.
- `readFrame(timeout_ms)` requires a timeout of at least 1 ms. A synchronous read during callback
  capture returns `ConcurrentRead`.
- `stop()` and `close()` are safe when already stopped or closed. A stopped open session can restart.
- Destruction stops the callback worker and releases backend resources.
- Callbacks are serialized on the session worker. Callback exceptions do not cross the library
  boundary and terminate that callback stream.
- Calling `stop()` from a callback requests shutdown without self-joining or detaching the normally
  owned worker. The next owner-thread `stop()`, `start()`, or `close()` joins and reaps that finished
  worker. Destroying the session from its callback remains memory-safe for compatibility but is
  discouraged because owner-side reaping is impossible. Consumers externally serialize concurrent
  operations on one session and retain callback-owned application objects until the callback ends.

`CameraSession` and `WebCamera` each hold their PIMPL through one `std::unique_ptr`. The Windows
Media Foundation backend owns COM objects with `ComPtr`; no consumer-side manual reference release
remains beyond the COM-mandated `Release()` implementation of the callback object itself. Linux
file descriptors, mappings, and libcamera objects likewise remain within backend RAII owners.

### Open and selection sequence: TMR-OPEN-02

Explicit `WebCamera::open(device, configuration)` checks session availability, AlreadyOpen,
configuration validity, permitted transport, capability query, and exact advertised native-profile /
output-format match before calling the session. Width, height, format and both frame-rate rational
fields must match; equivalent but differently represented rates are not normalized here. The session
creates a candidate backend, opens it, and retains it only on success. Failure destroys the candidate.

`open(device)` selects the first valid advertised profile with a recognized preferred output.
The current preference is BGRA8, BGR8, RGB8, YUYV422, NV12, Gray8, then MJPEG. This is not highest
resolution/rate selection; profiles supporting only formats outside that list need an explicit
configuration. Native/output separation and `allow_conversion` remain part of the request.

Capabilities are cached, not refreshed on every observation. `currentCapabilities()` checks retained
selection, not live device access. The index constructor also selects without opening. Default open
updates selection before forwarding to explicit open, so a failed open or AlreadyOpen can leave
selection metadata that is not a newly opened session. Prefer explicit configuration and use
`isOpen()` plus the operation result; cached metadata alone does not prove ownership. `close()`
clears selection, cached capability/configuration and retained last error.

### Callback and session state: TMR-CALLBACK-03

Backend streaming and callback delivery are separate state axes:

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Call/event | Session/backend effect | Worker/result effect |
| --- | --- | --- |
| Start on Closed | NotOpen | No worker |
| Start on Open | Delegate backend start | No callback worker for synchronous start |
| Start with empty callback | InvalidArgument before backend start | No worker |
| Start with callable | Prepare owned state/callable, then start backend | Publish thread ownership before releasing worker to read or invoke user code |
| State/callable preparation throws | Backend is not started | Original C++ exception propagates; no worker |
| Thread launch/copy throws after backend start | Stop backend; close if stop fails or throws | Clear callback state, propagate original launch exception; restart if still open, otherwise reopen |
| Synchronous read while callback active | ConcurrentRead | Does not compete for frames |
| Worker read returns TimedOut | Continue streaming | Suppress callback and read again |
| Other worker read failure | Backend state follows adapter | Deliver one failure then end delivery |
| Worker read throws | Backend is not implicitly stopped | Deliver one preallocated ResourceExhausted result for bad_alloc, otherwise BackendFailure; end delivery |
| User callback throws | Backend is not implicitly stopped | Catch exception and end delivery |
| Owner-thread stop | Set stop flag; wait for current read; serialize backend stop | Release backend mutex before joining; clear CallbackState; return backend stop status |
| Callback-thread stop | Same stop request/backend stop | Defer join; owner must reap later |
| Close | Stop, close backend, release its reference | Owner-side join first; repeated close is harmless |

</div></div>


The loop requests `readFrame(100 ms)` under the shared backend mutex, releases that mutex, checks
stop again, suppresses TimedOut, and invokes the callback outside all state locks. It terminates
after a non-timeout failure or callback exception. A read completing after cancellation is not
delivered as an unsolicited terminal error. Stop racing after the check can still overlap invocation;
owner-thread join is the completion barrier. The argument borrows a loop-local result: copy a
successful frame to retain it.

At worker exit only `active` becomes false; the backend need not stop. `isStreaming()` queries the
backend, not active delivery, and can remain true after a throwing callback or terminal read error.
Call owner-side `stop()` before restarting to stop the backend and reap the thread. A callback cannot
restart its own worker. WebCamera passes the modern callback through without wrapping its errors into
`lastError()`; consume the delivered result. Successful remembered operations can clear `lastError()`,
and cached capability observers do not universally update it.

100 ms is a backend read request, not a shutdown-time guarantee. User code and native stop add
latency. V4L2 retries its wait against one monotonic deadline, but CameraSession does not
establish a deadline for the entire read/cleanup operation or offer a cancellation token. Owner lifecycle
calls are externally serialized, with the documented callback-stop exception.

The owner prepares CallbackState, terminal-error storage and a copied callable before backend start.
A publication condition keeps the worker idle until the owner stores its `std::thread`. Thread-local
callback identity remains valid through callable/capture destruction and lets self-stop defer reaping without accessing a thread object the owner may be
joining. Both stop callers serialize native stop through CallbackState's backend mutex; neither holds
it across join. A throwing native stop closes the backend, reaps on the owner thread, then rethrows.
`close()` contains that exception and completes cleanup. A throwing native start also closes uncertain
partial state before rethrowing. Public C++ allocation/thread failures can still propagate; this is
rollback, not a universal no-throw or allocation-free result guarantee.

### Backend processing and failure limits

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Adapter | Acquisition / return-to-driver sequence | Stop / partial failure |
| --- | --- | --- |
| Media Foundation | Request sample, await internal callback notification, copy/validate owned payload | Session serializes callback read/stop; adapter stop seals publication and awaits asynchronous Flush completion; close shuts down source and releases COM owners |
| V4L2 | Queue MMAP buffers, STREAMON; deadline poll, DQBUF, lease/copy/validate, QBUF | Partial QBUF/STREAMON failure resets with STREAMOFF. Failed reset/stop closes descriptor before unmapping; successful reset permits restart |
| libcamera | Create requests, start camera, queue; completion queue wakes reader; copy frame, reuse/requeue | Transactional start/read rollback; preallocated completion ring; pending owners retained until stop/return barrier |

</div></div>


V4L2 tracks queue ownership separately from streaming: even a failed STREAMON leaves queued buffers
owned by the driver. STREAMOFF resets those queues before retry, including failure at the first QBUF.
The original operation's errno is retained across cleanup. Cleanup uses non-allocating native calls;
a failed reset closes the device and releases every mapping once. This follows the kernel's
[STREAMON/STREAMOFF contract](https://docs.kernel.org/userspace-api/media/v4l/vidioc-streamon.html).

### Native frame leases and wait budget: TMR-NATIVE-05

The V4L2 open transaction installs a rollback owner immediately after opening the descriptor.
Until format, buffer-vector allocation and every mapping succeed, any return or C++ exception
closes the descriptor and releases all completed mappings. A caller may retry open after failure.
This also covers an exception while constructing an error; error construction is not allocation-free.

For each V4L2 read, compute one `steady_clock` deadline from the positive millisecond budget.
Before each poll, round remaining time up to milliseconds and clamp it to `INT_MAX`. A poll timeout
in a long chunk, EINTR, or DQBUF EAGAIN/EINTR re-enters that same deadline. An exhausted budget
returns `TimedOut` without dequeuing. For example, a 10 ms budget interrupted after 3 ms leaves
7 ms; `UINT32_MAX` ms is split into 2147483647, 2147483647 and 1 ms, never a negative infinite wait.

After successful DQBUF, a valid buffer index creates one scoped return obligation. Copy owned
bytes and validate the frame before returning a fresh zero-initialized QBUF descriptor containing
only type, memory and index. Payload overflow, the driver error flag, truncated storage and copy
exceptions also discharge that obligation. QBUF failure closes the camera before unmapping;
an invalid index or terminal dequeue failure also closes, without guessing which buffer to return.
If copying/validation already failed, that primary result or exception wins over cleanup failure.
If the frame was otherwise valid, a failed QBUF reports its saved native error and publishes no frame.
After a return failure, check `isOpen()` and reopen before restarting. The
[kernel buffer-exchange contract](https://docs.kernel.org/userspace-api/media/v4l/vidioc-qbuf.html)
allows a failed dequeue to consume a buffer without a usable index, which motivates this close policy.

Media Foundation retains the sample and contiguous buffer with COM owners. The private
[frame-copy helper](../../../platform/tmr/win/camera/MediaFoundationFrame.h) creates an Unlock
obligation only after successful Lock. Validate pointer, length/capacity and timestamp scaling;
copy owned bytes; validate storage; then Unlock exactly once before returning success. Invalid
metadata, truncated frames and allocation exceptions still attempt Unlock once. A failed Lock
does not Unlock. A failed Unlock returns `Backend/ReadFailed` for an otherwise valid frame; an
earlier validation error or exception takes precedence. The adapter does not automatically stop
for this sample-copy failure. Buffer owners are released after the attempt; failed native Unlock
is not a proof of driver recovery. This follows the
[Media Foundation Lock lifetime](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock).

These are frame-wait budgets, not wall-time bounds on copying, native calls, cleanup or stop.
V4L2 cleanup ioctls still retry EINTR without a deadline. MF and DirectShow use their own absolute
wait deadlines; libcamera uses a predicate wait. The session has no shared end-to-end deadline or
cancellation token. A successful wait does not promise the whole read returns before its budget.

`wse.tmr.v4l2_read_contract` runs 27 synthetic open/read cases on the actual adapter: vector
allocation failure, each query/map position, error-construction failure, payload allocation failure,
invalid buffers, return failure with failed stop, and virtual-clock EINTR/EAGAIN/large-timeout paths.
It checks exactly-once close/unmap/return, reopen, retained primary errors and detached frame bytes.
`wse.tmr.media_foundation_frame_contract` runs 16 fake-buffer cases on the production helper:
Lock failure (including E_OUTOFMEMORY), payload allocation failure, invalid metadata/storage,
timestamp overflow, Unlock failure and owned-copy lifetime. The Windows allocation fault targets
the payload rather than debug STL bookkeeping. Neither fixture exercises a real driver.

Remaining native gaps include failures inside MF activation/reader factories and libcamera,
physical removal/recovery and all-allocator exhaustion; native ownership is defined in TMR-MF-STOP-08.
Public C++ allocation failures may propagate after cleanup; no universal no-throw result is promised.

### libcamera request and mapping transactions: TMR-LIBCAMERA-06

The private [resource helpers](../../../platform/tmr/linux/camera/LibcameraResources.h) separate
ownership from diagnostics. A plane copy checks used length, zero length, offset/length overflow
and destination capacity before mapping. A scoped owner attempts exactly one `munmap` after copy,
including allocation exceptions; map failure never unmaps. A failed explicit unmap is reported as
`ReadFailed`, but one attempted release cannot certify recovery from a native unmap failure.
Copied bytes never alias the native plane, and partially copied frames are not published.

Start is a transaction: build the entire request list and completion storage, prepare initial
controls, start the native camera, then attach controls and queue every request. Any failed result
or exception rolls back through non-allocating WSE cleanup before the original result/exception
escapes. Stop calls themselves belong to libcamera and may allocate or block. A successful native
stop is the barrier before request destruction; a cleanup error cannot replace the primary error.
Signal connection failure during open also rolls back; the camera reference is released before
stopping its manager.

The shared manager lease in the same resource helper covers enumeration, capability queries and
open sessions within one WSE runtime. libcamera permits only one CameraManager. Lease acquisition
and final destruction share a registry mutex, so a new constructor cannot race final cleanup.
Failed manager construction/start leaves no lease; every camera reference is released before its
lease. `wse.tmr.libcamera_manager_contract` checks failure/retry, nested leases and 1,000 concurrent
leases, plus two leases of the real pinned manager without opening a camera. Applications must not
create an independent libcamera manager alongside WSE or mix independently loaded WSE runtimes.
The manager lease does not repair the upstream disconnected-request completion gap below.

The completion callback runs on the libcamera thread. It only updates a preallocated ring and
pending ledger under a mutex and wakes the reader; it neither grows a container nor interprets
control metadata. Capacity equals the owned request count. Cancelled requests discharge pending
ownership without publishing a frame. Unknown/duplicate completion or an internal callback failure
latches a terminal failure, observed as `BackendFailure` by the reader. Metadata updates run on the
reader instead. The outer callback contains exceptions; this is not a guarantee that libcamera's
own signal dispatch or native allocator cannot throw.

After taking a completed request, the reader updates metadata, copies a frame, reuses the request,
sets controls and requeues once. A normal copy-validation error still attempts requeue. Copy/control/
reuse exceptions or requeue failure stop the stream; a prior copy error wins over a requeue error.
Call `stop()` and inspect its result before restarting after failure. Successful cleanup keeps the
session open; `isStreaming()==false` alone does not prove all native requests have returned.

On stop failure, requests and allocator remain owned. Restart is rejected while native activity or
pending ownership remains. Close retries stop and waits for pending returns before disconnecting
signals and destroying requests, allocator, configuration, camera and manager in that order.
There is no deadline on this wait. In pinned libcamera 0.7.2, `Camera::stop()` normally completes
pending requests synchronously, but `Camera::disconnect()` still documents unresolved handling of
pending requests. A disconnected camera can therefore leave close waiting indefinitely; do not
infer removal/recovery certification from synthetic tests. See the pinned source commit in
[Backends](#tmr-camera-backends) and the [upstream camera API](https://docs.libcamera.org/master/public-api/classlibcamera_1_1Camera.html).

`wse.tmr.libcamera_resources_contract` tests the production helpers with synthetic mapping and
request operations: allocation rollback, overflow, detached bytes, failed release, transactional
start/read failures, retained pending owners, cancellation and 1,000 allocation-free ring cycles.
This does not inject faults inside libcamera itself. An enabled Linux build also requires the real
adapter's enumeration/invalid-ID contract to succeed; the stub cannot satisfy that gate. Neither
check captures a physical camera or certifies driver shutdown timing.

### Media Foundation initialization ownership: TMR-MF-INIT-07

The private [resource owners](../../../platform/tmr/win/camera/MediaFoundationResources.h) cover
enumeration, capability queries and open. Immediately adopt the task-allocated activation array;
reserve the destination vector while that owner still holds every reference. Moving a reference
to `ComPtr` clears its raw slot before another operation can throw. Untransferred references and
the array are released on every exit. An allocated device-name string likewise gains a task-memory
owner before UTF-8 conversion. These match Microsoft's [enumeration ownership](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-mfenumdevicesources)
and [allocated-string contract](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getallocatedstring).

A source owner combines `ComPtr` with one explicit WSE `Shutdown` attempt before reference release.
For capability queries, declare it before reader/control owners so those references are released
first, including when copying a profile or constructing an error throws. Both reader-creation paths
set `MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN=TRUE`; the reader leaves the Shutdown
obligation to WSE, as defined by the [reader ownership attribute](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-source-reader-disconnect-mediasource-on-shutdown).

Runtime ownership pairs each successful COM initialization (including `S_FALSE`) with one
`CoUninitialize`. `RPC_E_CHANGED_MODE` borrows the existing apartment and creates no such obligation.
A failed MF startup immediately releases the COM obligation; a successful startup is paired with
MF shutdown before COM uninitialization. Repeated cleanup is harmless, and the same runtime owner
may initialize again. See Microsoft's [COM initialization balance](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).

Open arms rollback after input/lifecycle checks and before runtime initialization. Every later
failure return or C++ exception closes the partial backend, including the DirectShow fallback;
only a fully configured open commits. Callback allocation is skipped after an earlier native
failure. Preserve the original HRESULT in a successfully constructed error; an allocation failure
while constructing that error may instead propagate after cleanup. A repeated open rejected as
`AlreadyOpen` does not close the existing session.

The dedicated native thread enforces runtime initialization/final cleanup ownership; see
TMR-MF-STOP-08 below. Callback self-stop does not transfer COM ownership. Driver recovery after
failed Shutdown and native calls that block indefinitely remain outside the completion guarantee.

`wse.tmr.media_foundation_resources_contract` runs 29 cases: production owners with synthetic COM
objects/runtime results, actual vector/string allocation failures, partial adoption, source release
order, rollback/commit and retry, plus a real MF attribute store checking the reader setting.
It opens no camera. It does not inject faults into the actual activation/reader factories or every
adapter allocation site, and is not a whole-heap exhaustion or physical-device certification.

### Media Foundation stop and owner thread: TMR-MF-STOP-08

The private [native owner thread](../../../core/tmr/camera/CameraOwnerThread.h) constructs the Windows
backend, dispatches every operation, and destroys it on one dedicated thread. This includes the
DirectShow fallback and COM/MF initialization/finalization. Enumeration and static capability queries
instead use a temporary runtime that is initialized and destroyed on their calling thread. The public
camera callback worker remains separate. A caller may close a quiescent session on another thread;
concurrent destruction and operations remain forbidden. Native code must not synchronously re-enter
the same dispatcher. Dispatch borrows a stack task and the caller's arguments until completion, with
no dispatch heap allocation; owned return values or native operations can still allocate. Factory
failure joins the thread before propagating, and operation exceptions return to the waiting caller.

The [reader state](../../../platform/tmr/win/camera/MediaFoundationReadState.h) closes frame publication
before calling asynchronous `Flush`, including when no request is outstanding, to discard queued
native samples. Successful return from Flush is insufficient: stop waits for `OnFlush` for up to
2,000 ms after the native call returns. This follows Microsoft's [Flush completion contract](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-flush).
Read callbacks during flushing cannot publish a frame or cancel the flush wait. Only successful
completion permits the next start to reset publication. Flush HRESULT failure returns BackendFailure
with the native code; notification timeout returns TimedOut. Both make the reader terminal: start
fails until close and reopen. Close seals callback state before releasing reader/control/source
owners and shutting down MF/COM on the dedicated thread. A late callback retains its own COM state
and cannot borrow a destroyed backend. Repeated stop/close is harmless.

The 2,000 ms notification budget does not bound Flush, Release, Shutdown, a driver call or user
callback execution. The owner-thread contract tests factory failure, exceptions, move-only/void
results, 1,000 concurrent dispatches, allocation-free dispatch and destruction from another thread.
The MF read-state contract covers success, failed HRESULT, missing/late completion, restart rejection,
sample retention and publication boundaries using the production helper and a real MF sample.
These tests do not certify arbitrary native-factory internals, process-wide OOM or physical hot-unplug.

## Frames and ownership

`sCameraFrame` is a copyable owning value. Its bytes are detached from backend buffers and remain
valid after the next capture and after session shutdown. It carries a description, sequence number,
and a monotonic nanosecond timestamp. The timestamp epoch is unspecified; use order and differences
within a stream.

### Frame layout and examples: TMR-FRAME-04

`row_stride` is the byte distance between rows; zero requests the minimum packed stride. Use the
effective stride in byte arithmetic. Packed frame data may exceed, but must not be shorter than,
`effective_stride * height`, including the final row's padding. The renderer instead requires exact
frame byte counts. Sixteen-bit samples use little-endian byte order.
For the 32-byte BGRA example below, 31 bytes is invalid and 33 bytes is valid.

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Format / example | Minimum and required storage |
| --- | --- |
| BGRA8, 3 x 2, stride 16 | Minimum row 12 bytes; required 32 bytes; pixel (2,1) starts at byte 24 |
| NV12, 4 x 2, stride 6 | Y occupies bytes 0..11; interleaved UV starts at 12 and occupies 6 bytes; total 18 |
| YUYV / UYVY | Two pixels share four bytes; width must be even, height may be odd |
| MJPEG | Nonzero dimensions and stride zero; memorySize is zero by design, while data must be nonempty |

</div></div>


NV12 requires even width and height. Y and interleaved UV planes share stride; total storage is
`stride * height + stride * (height / 2)`. Odd height is rejected, not rounded up. Sizing checks
overflow and insufficient stride. `sCameraFrame::valid()` checks storage, not JPEG decodability or
sensor quality. Sequence values are backend supplied; there is no universal restart counter or
uninterrupted-delivery guarantee. Timestamps do not establish cross-device or wall-clock synchronization.

## Error contract

Operations return `CameraResult<T>` or `CameraStatus`. `CameraError` contains a stable category and
code plus a diagnostic message and optional native code. Program logic must branch on category/code;
messages and native codes are diagnostic only. Validation, lifecycle, device, I/O, timeout,
unsupported, and backend failures are distinct. Where detectable, removal maps to
`DeviceDisconnected` rather than hanging or terminating the process.

## Controls

The portable control catalog identifies at least exposure, gain, focus, brightness, contrast,
saturation, white balance, zoom, iris, hue, sharpness, gamma, color enable, backlight compensation,
pan, tilt, roll, and power-line frequency. Unsupported controls are absent from a device capability;
consumers do not assume that every camera implements every control. A capability carries a stable
control ID, display name, range, step, default, manual/automatic modes, read/write access, and unit.
Adding a future standard control must not renumber existing control IDs.

The V4L2 backend queries and sets matching standard controls. The Windows backend converts controls
obtained from the Media Foundation source through an internal adapter. Operating-system interfaces,
property IDs, and flags do not appear in public headers. Consumers enumerate capabilities and use
`getControl()` and `setControl()` without branching on the backend. `WebCamera` also provides typed
convenience methods for common controls such as exposure, gain, and focus; those methods pass through
the same capability validation path.

In Manual mode, `value` follows the advertised range and step. `valueFromNormalized(0.0..1.0)` and
`normalizedFromValue()` support relative UI controls without knowledge of native operating-system
units. Automatic mode ignores `value`. Out-of-range values, step mismatches, and unsupported modes
are rejected before device I/O. Read and write failures use `ControlReadFailed` and
`ControlWriteFailed`; unsupported controls never report success.

Capabilities also provide `unit` and `physical_scale`.
`physicalFromValue()` and `valueFromPhysical()` convert between the integer `value` and the stated
physical quantity. Public units include microseconds for exposure, kelvin for white balance,
diopters for focus, a multiplier for gain, and relative values for brightness, contrast, and
saturation. A control whose conversion is not guaranteed reports `DeviceNative`. Consumers must
inspect `unit` instead of inferring an operating-system-specific unit.

### UVC extension units

An optional advanced API handles UVC extension units (XUs) that cannot be represented by standard
controls. A public selector identifies the UVC unit GUID/unit ID, selector, direction, allowed
payload size, and read/write access. Payloads are owned byte arrays. `IKsControl`, Windows property
sets, Linux ioctls, pointers, and file descriptors never cross the public boundary.

Before XU writes reach the device, the implementation validates that the device advertised the
selector, the payload length matches, the configured maximum is not exceeded, the session state is
valid, and write access is allowed. Unknown selectors are never written automatically, vendor
commands are never guessed, and unsupported operations never report success. Public WSE provides
only the generic XU transport. Model-specific selector names, schemas, commands, and restricted
information belong in private extensions.

### Stream post-processing boundary

Rotation, flipping, frame averaging, color conversion, and OpenCV conversion are not UVC device
controls. They belong to portable image processing over owned frames or to consumer adapters, not
to camera backends. An operating-system camera settings window is also not required by the portable
API and may be offered only as an optional platform service.

`CameraFrameOps.h` is where that image processing lives, as free functions over an owned frame
rather than as methods on a camera, so that driving a device and working on its pixels stay apart.
It offers orientation correction, an accumulator that averages frames of one shape, and conversion
from a Bayer mosaic to colour. The arithmetic itself belongs to Core, in `wse_ImageTransform.h` and
`wse_ImageDemosaic.h`, which know nothing about cameras; this header carries only the camera-side
responsibility of mapping a camera pixel format onto a Core one and refusing what cannot be mapped.

Orientation and averaging answer differently for a Bayer frame, and the two questions are separate
for that reason. Moving a Bayer pixel puts it on a site of another colour, so orientation refuses
one and `isFrameOperationSupported()` reports so. Averaging combines a site with itself across
frames and leaves the layout alone, so it accepts one and `isFrameAveragingSupported()` reports
that. Averaging before the colour is raised is also where noise falls furthest.

Accumulation works one whole sample at a time, so a sixteen-bit format keeps its two bytes
together. A conversion offers block replication, which copies a block of four samples onto its four
pixels and changes no value, and bilinear, which averages neighbours for smoother edges; both write
every pixel including the last row and column.

<a id="frames-as-core-images"></a>
### Frames as Core images

The same header converts an owned frame into a Core `wse::Image_` and back. `toImage()` folds the
row stride away and gives a packed image, `toCameraFrame()` returns a packed frame, and
`readImage()` and `readAveragedImage()` do the read and the conversion in one call. They stay free
functions for the reason above: acquiring pixels is device work, and shaping them is not.

The destination type names the format, which is how a channel order survives the boundary. A camera
format maps as follows.

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Camera format | Core format | Image type |
| --- | --- | --- |
| `Gray8` / `Gray16` | `CH1D8` / `CH1D16` | `img1c08_t` / `img1c16_t` |
| `Rgb8` / `Rgb16` | `CH3D8` / `CH3D16` | `img3c08_t` / `img3c16_t` |
| `Bgr8` / `Bgr16` | `BGR3D8` / `BGR3D16` | `img3c08_bgr_t` / `img3c16_bgr_t` |
| `Bgra8` | `BGRA4D8` | `img4c08_bgra_t` |
| `Bayer16*` | `CH1D16` | `img1c16_t` |
| `Yuyv422` / `Uyvy422` / `Nv12` | `CH3D8` after conversion | `img3c08_t` only |
| `Mjpeg` | none | none |

</div></div>


Three properties of that table are deliberate. A `Bgr8` frame becomes a BGR-ordered image rather
than being reordered on the way through, and asking for an `img3c08_t` from one is
`UnsupportedFormat` rather than a silent swap; `wse::convertChannelOrder()` is the explicit route.
A `Bgra8` frame becomes a four-channel image and keeps its alpha, so nothing on this path discards a
channel. And a Bayer frame maps only as a carrier for its samples: the layout does not travel in the
pixel format, so a caller pairs it with `bayerPatternOf()` or converts with `demosaicFrame()` first.

`Yuyv422`, `Uyvy422`, and `Nv12` are subsampled and have no Core format of their own, so they convert to RGB
with the BT.601 matrix and only into `img3c08_t`. `Mjpeg` is compressed and the engine carries no
decoder, so it reports `Unsupported`/`UnsupportedFormat`. A sixteen-bit sample travels little endian.

Every one of these returns `CameraStatus` or `CameraResult`. The Core image types report an
allocation failure by throwing, so each conversion catches that and reports
`Backend`/`ResourceExhausted` instead; no exception crosses the Tmr boundary.


## Public API boundary

`CameraSession` provides low-level C++ sessions. All four language bindings use `WebCamera` as
their camera owner and do not expose `CameraSession`. `Parameter<T>` is an independent data type. Model-specific XU schemas and private device headers are not public.

### Backend-independent example

```cpp
using namespace wse::tmr;

const auto devices = CameraSession::enumerate();
if (!devices.succeeded() || devices.value().empty())
    return;

const auto capability = CameraSession::capabilities(devices.value().front());
if (!capability.succeeded() || capability.value().formats.empty())
    return;
CameraSession camera;
if (!camera.open({ capability.value().device, capability.value().formats.front() }).succeeded())
    return;

for (const auto& control : capability.value().controls)
{
    if (control.control == eCameraControl::Brightness && control.supports_manual)
    {
        if (!camera.setControl({ control.control, eCameraControlMode::Manual,
                                 control.valueFromNormalized(0.5) }).succeeded())
            return;
    }
}
```

## Calibration boundary

`sCameraCalibration` is an owned value independent of device I/O. It contains a camera ID and
optional domains for intrinsics, Brown-Conrady distortion, a 3 x 3 color matrix, and a lens-shading
map. `valid()` checks finite values, dimensions, positive focal lengths, and map element counts. It
does not certify calibration accuracy or identity. Device storage, correction execution, and
persistence belong to separate adapters.

## Package and verification boundary

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Contract | Executable evidence | Limit |
| --- | --- | --- |
| TMR-OWNER-01 / TMR-OPEN-02 | [WebCamera contract](../../../test/characterization/tmr_webcamera_contract.cpp), [backend contract](../../../test/characterization/tmr_camera_backend_contract.cpp) | Injected seams; not every native partial-open/selection-cache failure |
| TMR-CALLBACK-03 | Backend contract plus [start/stop faults](../../../test/characterization/tmr_camera_start_stop_contract.cpp): callable-copy/launch faults, failed rollback, read exceptions, publication, read/stop exclusion and concurrent owner/self-stop | Injected allocation/thread failure paths, not an exhaustive heap sweep; no arbitrary callback/native-call time bound |
| Native start rollback | [V4L2 lifecycle faults](../../../test/characterization/tmr_v4l2_lifecycle_contract.cpp): each QBUF position, STREAMON/STREAMOFF failure, original errno, retry and cleanup order | Actual adapter with synthetic native calls; no physical camera or MF/libcamera native fault certification |
| TMR-FRAME-04 | [Camera values](../../../test/characterization/tmr_camera_contract.cpp), [frame operations](../../../test/characterization/camera_frame_ops_contract.cpp) | Owned layout/conversions; physical buffer behavior needs hardware evidence |

</div></div>


See [Core Algorithms](CoreAlgorithms.md) for image arithmetic, [Thread Ownership](ThreadOwnership.md)
for owner barriers, and [Design Verification](DesignVerification.md) for evidence scope.

A Tmr-only package installs `WSE::Core`, `WSE::Tmr`, and only the public camera/calibration
headers. Access-controlled legacy extensions are added only by an explicit internal build and must
not leak into public packages.

Linux x86-64 Tmr packages bundle the pinned libcamera runtime, IPA module, licenses, and
dependency metadata. Such packages still depend on the Linux libudev, OpenSSL, and YAML runtimes.
Packages without Tmr, or with `WSE_ENABLE_LIBCAMERA=OFF`, must not contain libcamera.

Automated gates cover Windows Shared/Static, Linux x86-64 GCC/Clang Shared/Static, installed
consumers and Linux ARM64 GCC cross builds. The mock-backend gate covers native/output conversion,
generic XU boundaries, 1,000 streaming-control iterations, stop/restart, independent sessions,
owned frames and disconnection errors without hardware. Public-header and ownership gates reject
native camera types in public headers, manual PIMPL allocation/deallocation, normal callback-stop
detaches and manual COM release in the public Windows backend.

Real libcamera adapter checks cover compilation, empty-manager enumeration, invalid IDs, mocks,
packaged consumers and structured errors. Windows camera smoke covers enumeration, frame output,
controls, restart and callbacks on the selected profile. The Pi 4 V4L2 smoke covers enumeration,
capabilities, readable controls, an owned frame, stop and close in Debug/Release and Shared/Static;
it does not write controls. Exact physical results and configurations belong in the controlled
acceptance record, with status scope defined by [Hardware Validation](HardwareValidation.md).
Higher resolutions/formats, control writes/restoration, physical removal/reconnection and endurance
require their own evidence. Pi 5, physical libcamera and Pi CSI are uncertified. Mock or cross-build
success is not hardware certification.

<a id="packed-uyvy-frames"></a>
## Packed UYVY frames

`Uyvy422` has pixel-format value 15. Each
two-pixel group is four bytes in U0 Y0 V0 Y1 order. Rows require an even width, while odd heights
and padded strides are supported. Media Foundation, V4L2, and libcamera preserve this native
format when conversion is disabled. On Windows, native UYVY/YUYV profiles with an odd height
use a DirectShow graph internally because the MF source may reject their samples. The adapter
matches the selected interface or its unique Windows device instance; ambiguous matches fail.
Only direct pin connections are allowed. The graph copies each sample into owned storage,
keeps the latest sample, and wakes waiting readers before stopping. Camera and video-processing
controls use the selected source's COM interfaces. No platform type enters the public API.

The same acquisition route serves a BGRA8 request for an odd-height packed native profile when
conversion is enabled. An output-only compatibility description resolves the native format from
the same device's advertised profile with exactly matching dimensions and rational frame rate.
It never selects another device. Conversion-disabled requests retain native bytes or fail explicitly.

[DirectShowCaptureFormat.h](../../../platform/tmr/win/camera/DirectShowCaptureFormat.h) selects the
requested interval `10000000 * denominator / numerator` in 100 ns units. A default interval may
be replaced only inside the advertised minimum/maximum interval. After `SetFormat`, both
`GetFormat` and the connected sample type must match subtype, dimensions, 16-bit packed layout,
and interval within one tick. A driver-selected nearby rate is rejected. This follows the
[Windows format-selection contract](https://learn.microsoft.com/en-us/windows/win32/directshow/configure-the-video-output-format);
an interval range alone does not prove every rate is supported.

[CameraYuvConversion.h](../../../core/tmr/camera/CameraYuvConversion.h) validates the owned source
and converts each pair without an intermediate RGB image. It uses the same BT.601 limited-range
integer coefficients as `toImage()`: `C=max(0,Y-16)`, `D=U-128`, `E=V-128`, then
`R=clamp((298*C+409*E+128)>>8)`, `G=clamp((298*C-100*D-208*E+128)>>8)`,
`B=clamp((298*C+516*D+128)>>8)`, with each channel clamped to 0..255. Output is packed B,G,R,255
with stride `width*4`; source padding is skipped, sequence/timestamp and row order are preserved.
Odd heights are valid, odd widths are rejected. Invalid storage returns `ReadFailed` and output
allocation failure returns `ResourceExhausted` (constructing the error itself can still allocate).
This fixed color conversion does not infer device colorimetry or perform optical calibration.

MF sample failures and end-of-stream retain their native status as `Backend/ReadFailed`; only
explicit device invalidation or `ERROR_DEVICE_NOT_CONNECTED` becomes `Device/DeviceDisconnected`.
After a terminal reader failure, streaming stops, further reads/start are rejected, and close skips
reader `Flush` before releasing it and shutting down the source. Recovery requires close/reopen.
This respects the [source-reader error boundary](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/ne-mfreadwrite-mf_source_reader_flag).
The Windows `wse.tmr.directshow_format_contract` and `wse.tmr.media_foundation_error_contract`
verify interval/readback and error-classification decisions with synthetic inputs. The portable
`wse.tmr.camera_frame_ops` checks literal BGRA bytes, padding, metadata, odd height and invalid input;
these contracts do not certify a driver, physical disconnect, or all native allocation failures.

`toImage()` explicitly converts UYVY to RGB8 using BT.601;
orientation and averaging require conversion first. Language bindings expose the same value.

The C ABI guards frame-handle allocation before callback delivery and sends a single NULL/error
event on construction failure, then ends delivery. C# catches managed callback/adoption exceptions
inside its bridge and exposes `WebCamera.CallbackException`; an already delivered frame remains
application-owned. Owner-side Stop/join remains necessary. See [CABI-ADOPT-06](CAbiContract.md)
for ownership transfer, reset rules and synthetic test limits.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
