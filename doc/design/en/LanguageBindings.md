# WSE Language Binding Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document is the normative design for the native facade shared by the Python, JavaScript,
Java, and C# bindings. The facade is part of `WSE::Core`; each language adapter is optional. The
pybind11 Python, Node-API JavaScript, JNI Java, and P/Invoke C# adapters share this contract and do
not wrap platform backends directly.

The aggregate C++ header is `<wse/binding/stew.h>`. The Python entry point is installed under
`lang/python`; the JavaScript entry point is the installed `lang/js` package, the Java
entry point is `lang/java/wse.jar`, and the C# entry point is `lang/cs/WapitiStew.Wse.dll`. Native
operating-system handles, graphics handles, backend objects, and
raw owning pointers are not part of any public surface.

## Native facade

`wse::binding::Runtime` exposes a small stable boundary:

- `info()` returns the WSE version, binding ABI version, and compiled XPT/Tmr/OUI capabilities.
- `copyFrame()` copies caller bytes into an immutable, reference-counted `FrameBuffer`.
- `wait()` is finite and observes a copyable `CancellationToken` owned by a
  `CancellationSource`.
- `Error` carries a stable category, stable operation code, diagnostic message, and optional native
  code. Program logic uses category and code, not the message or native code.
- `fromXptError()`, `fromOuiError()`, and `fromTmrError()` preserve component stable codes while
  mapping to the common binding categories. Language adapters do not duplicate those mappings.

`BINDING_ABI_VERSION` starts at 1. Adding an operation without changing an existing representation
does not require an ABI increment. Removing or reinterpreting a field, numeric error category, or
ownership rule requires a new ABI version and a migration path.

The common categories are `None`, `InvalidArgument`, `NotFound`, `InvalidState`, `InputOutput`,
`Timeout`, `Cancellation`, `Protocol`, `Security`, `Unsupported`, `ResourceExhausted`, and
`Internal`. Language adapters translate them to idiomatic exceptions while retaining numeric
`category`, `code`, and `nativeCode` properties for cross-language tests and diagnostics.

## Ownership, async, and shutdown

Native adapters follow the [Result and status contract](ResultContract.md): strict results are
constructed through `success()` / `failure()`, and `error()` is read only after failure. XPT
transfers retain their `TransferResult<T>` type and HTTP retains `HttpResult`; neither is converted
to a strict `TransportResult<T>`. Python acquires native results inside the GIL-release scope and
reacquires the GIL before building Python values or raising exceptions.

Input buffers are copied before asynchronous work begins. Output buffers own their bytes and do not
refer to caller memory or a camera/renderer backend buffer. A future zero-copy path requires a
separate API with an explicit lifetime owner; it must not weaken the current copy contract.

Scheduling and close barriers are operation-specific. Node Core copy/wait/timer work uses native
workers and runtime dispatch; the XPT wrappers are synchronous. Python releases the GIL around
blocking native work, which does not turn a call into an asynchronous operation. A cancellation
request is not necessarily a join or removal of an already queued callback. Use the detailed
sequences below and [Thread Ownership](ThreadOwnership.md); do not infer one barrier for every runtime.

## Node-API adapter

The JavaScript adapter uses the C Node-API surface with `NAPI_VERSION=8`; it does not link to V8 C++
types. The package requires Node.js 18 or later and exports:

| JavaScript API | Contract |
| --- | --- |
| `runtimeInfo()` | Synchronous version, binding ABI, and compiled-component query |
| `copyFrame(Buffer or ArrayBufferView)` | Promise resolving to an independent `Buffer` copy |
| `wait(milliseconds)` | Promise completing after a non-negative finite delay |
| `runAfter(milliseconds, callback)` | Thread-safe callback and a `cancel()` / `close()` handle |

Invalid arguments throw synchronously. Native asynchronous failures reject their Promise with the
portable error properties. `cancel()` is idempotent. `close()` cancels pending delivery, joins the
owned timer thread, and is safe after callback delivery. Addon cleanup performs the same worker
cleanup for handles the application did not close; it does not bound arbitrary application execution.

The addon is context-aware so Node worker threads receive independent instance state. Tests must
cover buffer detachment, error values, callback cancellation, repeated worker load/unload, and an
outstanding long timer during worker termination.

## Python adapter

The Python adapter uses pybind11 3.1.0 and targets CPython 3.11 through 3.14.

When Tmr is enabled, `import wse` exposes `ImageOrientation`, `BayerPattern`, `DemosaicMethod`,
`CameraFrameAccumulator`, `is_frame_operation_supported`, `is_frame_averaging_supported`,
`is_bayer_format`, `bayer_pattern_of`, `apply_orientation` and `demosaic_frame`.
These operations accept the owned `CameraFrame` used by capture; callers do not import the
internal native module. The typing file includes the frame constructor, 16-bit/Bayer formats
and frame-rate control. The Python Tmr contract exercises this package entry point.

| Python API | Contract |
| --- | --- |
| `runtime_info()` | Synchronous version, binding ABI, and compiled-component dictionary |
| `copy_frame(buffer)` | Independent read-only `FrameBuffer` from a contiguous buffer-protocol input |
| `wait(milliseconds)` | Finite zero-to-24-hour wait with the GIL released during native work |
| `Runtime` | Context manager and idempotent `close()` that cancels its outstanding `wait()` |

`FrameBuffer` supports `bytes()`, `tobytes()`, `len()`, and the read-only buffer protocol without
retaining input memory. `WseError` derives from `RuntimeError` and preserves numeric `category`,
`code`, and `native_code`. Operations on a closed runtime report `InvalidState`; a wait interrupted
by `close()` reports `Cancellation`. Only native regions that do not touch Python objects release
the GIL, and every blocking one does: the copy and wait paths, camera capture, projection
rendering, and each XPT operation that runs to its deadline. A Python thread therefore keeps
running while another sits in an HTTP exchange, a socket read, or a serial read, and a responder
in the same process can answer a request the same process made.

The Python adapter supports the main interpreter and process-level load/unload.
Subinterpreters and free-threaded CPython are unsupported; interpreter-local state management
and a verification matrix for those configurations are not provided.

## Java adapter

The Java adapter uses JNI and compiles classes with `--release 17`. `WseRuntime`, `TimerHandle`,
`NativeBuffer`, `WebCamera`, and Projection output owners implement `AutoCloseable`; `Cleaner`
is only a safety net for missed closes. `WseException` retains numeric `category`, `code`, and
`nativeCode` fields.

JNI never shares a thread-local `JNIEnv`. Native callback threads attach to the JVM as daemon
threads and detach before termination. Input `ByteBuffer` content is copied into native ownership;
an output `NativeBuffer` exposes a read-only direct buffer and retains its explicit native owner.
No native pointer or operating-system/graphics handle is exposed.

## C# adapter and the flat C ABI

P/Invoke cannot bind C++ classes, so the C# adapter calls a flat C ABI instead of `wse::binding`
directly. The aggregate C header is `<wse/capi/stew.h>` under `api/wse/capi`; its implementation
lives in `lang/cs/native` and delegates to the same `wse::binding` facade, so the C ABI adds no
second source of truth for semantics.

The C ABI is normative in the following respects:

- Handles are opaque struct pointers (`wse_capi_runtime`, `wse_capi_frame_buffer`,
  `wse_capi_cancellation`). Neither raw `void *` nor operating-system handles cross the boundary.
- Fallible operations return `wse_capi_status`, whose `category` values equal
  `wse::binding::eErrorCategory`. The failure message is thread-local and read with
  `wse_capi_last_error_message()`.
- Status operations use the shared exception guard. Pure getters/destructors have separate return
  shapes. Diagnostic storage uses a fixed 1024-byte TLS buffer without allocation; see the C ABI design for truncation and injection scope.
- An HTTP 4xx/5xx returns an owned response together with failure; the caller destroys it on that
  path too. Transfers can retain progress counts with failure. Other failures do not universally
  overwrite output slots: initialize handles to NULL and follow each operation's output rule.
- A callback takes its caller token as `wse_capi_user_data`, an `intptr_t` the library never reads.
  The flat C ABI has no raw `void *` anywhere, and a callback does not introduce one.
- String and buffer getters use the two-call pattern: passing `NULL` for the destination reports the
  required size.
- Parameters follow the same convention as the C++ API. The opaque handle keeps position one as the
  C spelling of `this` and says whether the call mutates the object it names; the out parameters
  follow it as `p_<name>_out`; the inputs come last as `<name>_in`. A caller-provided destination
  buffer is an out parameter, so the two-call pattern reads
  `( handle, char* p_buffer_out, size_t* p_size_out, size_t capacity_in )`.
- `WSE_CAPI_ABI_VERSION` versions the flat C ABI itself. It is a number of its own, independent of
  the binding ABI version `info()` reports, and it stands at 1. A caller reads the version of the
  library it actually loaded with `wse_capi_abi_version()` and checks it against the
  `WSE_CAPI_ABI_VERSION` it was built against. Adding a function is compatible and leaves the
  number alone; growing a structure or changing an argument list is not, and raises it.
- The C ABI export macro is decided independently of `WSE_STATIC`. The shim always builds as a
  shared library because P/Invoke can only resolve shared libraries, so a statically linked Core
  must not suppress its exports.

On the managed side, `WseRuntime`, `FrameBuffer`, and `CancellationSource` own their handle through
one `SafeHandle` and implement `IDisposable`; `Dispose()` is idempotent, and use after disposal
raises `ObjectDisposedException`. `WseException` retains numeric `Category`, `Code`, and
`NativeCode`. The native library resolves through an explicit path, then the `WSE_CAPI_LIBRARY`
environment variable, then the default probing order. Each opaque input uses SafeHandle marshalling;
optional cancellation uses an explicit lease. Native release waits for the last active reference,
without cancelling or joining unrelated calls. Mutable operations remain caller-confined;
see [C ABI lifetime rules](CAbiContract.md).

The C# adapter exposes Core, XPT, IUI, OUI, and Tmr, matching
the other three bindings.

## High-level Tmr and OUI boundary

All four bindings use `WebCamera` as the sole public camera owner and expose Tmr enumeration,
capabilities, open, start/stop, frames, controls, extension units, and close. The C++ `CameraSession`
is a low-level substrate for hiding operating-system backends and is not a camera owner in any
language package. Consumers do not call Media Foundation, V4L2, or libcamera directly. Callback and frame
ownership is independent from the backend lifetime and follows each runtime's thread rules.

Closing a camera ends the device session; it does not retire the object. The same instance opens
again, which is how a sample changes the stream resolution. Where a language also needs a terminal
release, the two are separate calls: `Close` and `Dispose` in C#, `closeCamera()` and `close()` in
Java, `close()` and `release()` in Python, and in JavaScript `close()` with the collection of the
object. The keyboard is not split this way, because Core gives it no open and close pair.

| Contract | JavaScript | Python | Java | C# |
| --- | --- | --- | --- | --- |
| Owner | `new WebCamera()` | `WebCamera()` / `with` | `new WebCamera()` / try-with-resources | `new WebCamera()` / `using` |
| Device/profile | plain owned object | pybind owned value | immutable record | disposable `CameraDevice` / `CameraStreamProfile` record |
| Frame bytes | copied `Buffer` | `FrameBuffer` | `NativeBuffer` | owned `CameraFrame`; `ToArray()` copies bytes |
| XU payload | copied `Buffer` | copied `bytes` | cloned `byte[]` | copied `byte[]` |
| Close | idempotent, session only; reopens | idempotent, session only; reopens | idempotent, session only; reopens | idempotent, session only; reopens |

OUI uses the C++ `renderProjection()` operation as the common boundary. Callers provide RGBA layers,
optional alpha, meshes, sampling, opacity, edge blend, output size, and backend policy. Renderer,
surface, texture, mesh, fence, and readback objects remain internally owned; only the packed RGBA8
result frame crosses the language boundary.

A request may name the adapter to render on, and the result reports the adapter that drew it.
Naming an adapter that no machine has is a failure rather than a quiet render on another one, and
naming one while also asking for the software adapter is refused because those are two different
requests.

What that boundary does not carry is the renderer itself. `Renderer`, its textures, and therefore
`eRendererPixelFormat` are C++ only: `Rgba16Unorm` and `Rgba16Float` have no binding surface,
because a projection is always rendered to and returned as packed RGBA8. Reaching a wider format
means using `Renderer` from C++, and exposing one through the bindings would mean either changing
what a projection returns or giving four language runtimes ownership of GPU resources.

## IUI keyboard boundary

All four bindings expose `Keyboard` as the sole public keyboard owner. Construction starts the
monitoring thread and the owner is released deterministically: `close()` in JavaScript, a context
manager or `close()` in Python, try-with-resources or `close()` in Java, and `using` or `Dispose()`
in C#. Closing is idempotent and terminal.

The bindings expose polling, not callbacks. `snapshot()` returns all five key groups from one
update point so a caller never stitches together several independent reads. `accessState()` always
succeeds and reports `Starting`, `Ready`, `Unavailable`, `PermissionDenied`, or `Disconnected`;
`snapshot()` and the pressed-ASCII getter fail with the structured error derived from that state,
so an unreadable keyboard is never reported as an empty snapshot. `fromIuiKeyboardState()` owns
that mapping and language adapters do not duplicate it.

| Contract | JavaScript | Python | Java | C# |
| --- | --- | --- | --- | --- |
| Owner | `new Keyboard()` | `Keyboard()` / `with` | `new Keyboard()` | `new Keyboard()` |
| Snapshot | plain owned object | `dict` of `list[bool]` | `KeyboardState` record | `KeyboardState` |
| Readiness | `accessState()` | `access_state` | `accessState()` | `AccessState` |
| Close | idempotent and terminal | idempotent and terminal | idempotent and terminal | idempotent and terminal |

## XPT transport boundary

All four bindings expose `TcpClient`, `UdpClient`, `SerialPort`, and an HTTP execute entry point.
XPT has no default timeout, so every operation takes an explicit deadline and an optional
cancellation. Deadline observation has the resolver/OS-call limits in [XPT](XptTransport.md);
it is not a hard wall-clock bound. A status keeps the stable XPT code
while its category is normalized through `fromXptError()`.

The bindings are synchronous: a call blocks the calling thread until completion or an observed
deadline/cancellation. JavaScript callers keep deadlines short on the main thread or use a Worker.
The binding never retries on the caller's behalf: an HTTP request is classified as non-idempotent
and transient-failure retries stay off, so a caller decides when a repeat is safe.

A 4xx or 5xx answer is reported as a failure that still carries its response, because the exchange
finished and the server replied. Every binding raises its structured error and attaches the
response to it: `HttpStatusException.Response` in C#, `HttpStatusException.response()` in Java,
`WseHttpStatusError.response` in Python, and `error.response` in JavaScript. Each derives from, or
is, the ordinary error type, so an existing catch of that type still works. Every other failure
produces no response and leaves the property absent.

Only the canonical transport API is bound.

## Build, package, and verification boundary

`WSE_BUILD_NODE_BINDING`, `WSE_BUILD_PYTHON_BINDING`, `WSE_BUILD_JAVA_BINDING`, and
`WSE_BUILD_DOTNET_BINDING` default to `OFF`. Before Node is enabled, the pinned Node 24.19.0 header archive and
MIT license must already be provisioned by `bootstrap.py --package node-api-headers`; CMake never
downloads them. The addon target is `WSE::Node` and the output name is `wse.node`.

Before Python is enabled, provision the pinned pybind11 3.1.0 source archive and BSD-3-Clause license
with `bootstrap.py --package pybind11`, and provide a CPython 3.11+ interpreter plus
`Development.Module`. The target is `WSE::Python`, and the module name is `_wse`. Cross builds must
provide target CPython headers through `WSE_PYTHON_TARGET_INCLUDE_DIR` and the target ABI suffix
through `WSE_PYTHON_EXTENSION_SUFFIX`.

Java requires JDK 17 or later; `WSE_JAVA_HOME` may select the build JDK. Tests may set
`WSE_JAVA_17_EXECUTABLE`, `WSE_JAVA_21_EXECUTABLE`, and `WSE_JAVA_25_EXECUTABLE` to run the same
Java 17-compatible classes on all three runtimes. The target is `WSE::Java`,
the native module is `wse_jni`, and `wse.jar` is Java 17 compatible. Cross builds provide target JNI
headers through `WSE_JNI_TARGET_INCLUDE_DIRS`. Java 25 runs with
`--enable-native-access=ALL-UNNAMED`. WSE neither vendors nor packages a JDK runtime.

An installed package places `wse.node` beside the WSE runtime libraries in `bin`, the JavaScript and
TypeScript entry files in `lang/js`, and the Node license/identity files in
`vendor/node-api`. The JavaScript loader resolves the installed layout without requiring a current
working directory or a platform-specific path. `WSE_NODE_ADDON` is an explicit testing/development
override.

The Python module and typing files are installed under `lang/python/wse`; pybind11 license and
identity files are installed under `vendor/pybind11`. The Windows loader adds the package `bin`
directory to DLL search, while the Linux shared module resolves WSE through `$ORIGIN/../../../lib`.
The Java native module is installed under `bin` and `wse.jar` under `lang/java`.

C# requires the .NET 8 SDK at build time; `WSE_DOTNET_EXECUTABLE` may select it. The native target
is `WSE::Dotnet` with output name `wse_capi`, and the managed assembly is `WapitiStew.Wse.dll`
targeting `net8.0`. The native library is installed under `bin`, and the managed assembly and its
XML documentation under `lang/cs`. WSE neither vendors nor packages a .NET runtime.

## Implementation and data flow: BIND-STRUCT-01

| Layer | Source | Responsibility |
| --- | --- | --- |
| Shared Core boundary | [Runtime.cpp](../../../core/wse/binding/Runtime.cpp), [FrameBuffer.cpp](../../../core/wse/binding/FrameBuffer.cpp), [Cancellation.cpp](../../../core/wse/binding/Cancellation.cpp) | Runtime metadata, copied immutable bytes, shared atomic cancellation flags |
| JavaScript | [addon.cpp](../../../lang/js/native/addon.cpp), [package loader](../../../lang/js/index.js) | Per-environment Core operation registries; separate Tmr/OUI/IUI/XPT adapters; Node-API conversion |
| Python | [module.cpp](../../../lang/python/native/module.cpp), [package entry](../../../lang/python/wse/__init__.py) | pybind types, GIL scopes, Python-owned wrappers and exception translation |
| Java | [jni.cpp](../../../lang/java/native/jni.cpp), [WseRuntime.java](../../../lang/java/src/main/java/io/wapitistew/wse/WseRuntime.java) | Private native holders, JNI references, AutoCloseable owners |
| C# | [NativeMethods.cs](../../../lang/cs/Wse/NativeMethods.cs), [C ABI design](CAbiContract.md) | Sequential marshalling, shared native shim, SafeHandle release |

```text
language API -> runtime adapter -> public Core / XPT / IUI / Tmr / OUI facade
                                      -> platform adapter where needed
C# API -> P/Invoke -> C ABI guard/holders -> same public facades
native result -> shared category mapping -> language value / structured exception
```

Runtime is not a universal device manager: it exposes Core operations, while camera, keyboard,
transport and projection adapters call their respective public facades. Feature flags describe the
compiled library, not the presence of a complete one-to-one language wrapper for every Core/GEF type.

### Buffer ownership: BIND-BUFFER-02

FrameBuffer holds a shared pointer to a const byte vector. Copying that value shares immutable
storage; constructing from caller bytes copies it. There is no stride/format in this byte container;
CameraFrame and projection descriptions supply those meanings. Input `[00,7f,80,ff]` must survive
input mutation and destruction of the Runtime that made the output.

| Boundary | Input copy | Output and release rule |
| --- | --- | --- |
| Node copyFrame | Copy Buffer/view bytes before queuing async work | Fresh independent Node Buffer; garbage collection owns its storage |
| Python copy_frame | Acquire contiguous buffer, copy under GIL, release input view; native copy outside GIL | Read-only FrameBuffer/buffer-protocol export retains its Python owner |
| Java copyFrame | Copy remaining bytes of a duplicate ByteBuffer into a direct input, then into native FrameBuffer | NativeBuffer owns storage; returned read-only ByteBuffer is a borrowed view valid only while NativeBuffer remains open |
| C# CopyFrame | Array bytes copied through C ABI into FrameBuffer | SafeHandle owns native bytes; ToArray makes a separate managed copy |

Java `NativeBuffer.buffer()` does not make a new owning allocation. A previously obtained view is
not revoked when close releases native memory. Keep NativeBuffer strongly reachable and open while
using any view; copy into a Java-owned array before closing if it must survive. Read-only prevents
mutation, not a use-after-close. Automatic close, cleaner and Dispose are terminal for buffer owners.

### Core work and shutdown: BIND-ASYNC-03

| Operation | Native execution / completion | Stop boundary |
| --- | --- | --- |
| Node copyFrame | Owned input in AsyncOperation; napi_async_work; completion builds Buffer and resolves/rejects Promise | Per-environment registry participates in cleanup; no caller buffer borrow |
| Node wait | PromiseOperation owns cancellation and worker; thread-safe function delivers completion | Cleanup cancels/joins; atomic exchange prevents releasing the runtime reference twice |
| Node runAfter | CallbackOperation worker waits, queues through thread-safe function | cancel sets flag; close cancels, joins and aborts dispatch. A callback already queued at cancel can still run: dispatch itself does not recheck cancellation |
| Python Runtime.wait | Capture shared cancellation token under state mutex, release GIL, wait, reacquire GIL to raise/return | close sets closed/cancel under mutex; it does not join the caller thread doing wait |
| Java wait/timer | JNI holder plus cancellation; callbacks use JVM attachment and global references | Explicit owner close, native worker cleanup and terminal handle clearing; do not race lifecycle mutations |
| C# Wait | Synchronous P/Invoke on calling thread; optional cancellation holder | Cancel requests interruption; Dispose releases handle, not a general concurrent-wait barrier |

Native binding wait uses a monotonic deadline, sleeps at most 5 ms per slice, checks cancellation
before slices and once at the end. Negative duration is invalid; Python and Node also cap their wait
inputs at 24 hours. A slice is not a real-time scheduling guarantee. The C ABI does not impose that
same upper cap. Callers serialize native owner destruction rather than assuming all close methods
cancel and wait for every external thread.

### Camera callbacks and projection: BIND-DEVICE-04

Camera session close versus terminal owner release follows the table above. Frames delivered across
runtime boundaries own detached bytes. Callback dispatch and exception handling are adapter-specific:
Python acquires the GIL on the camera worker, builds CameraFrame/error, and invokes the registered
callable. Python callback exceptions are reported as unraisable and consumed by the adapter, so they
do not propagate to the C++ camera loop and do not imply the native loop's throwing-callback stop.
Stop/close release the GIL while joining so a callback needing the GIL can finish. Owner destruction
inside a managed callback is unsupported; retain an external owner and stop from it.

C camera callbacks transfer a frame handle to the receiver; C# wraps it as an owned CameraFrame
and retains the managed delegate while registered. Its reverse-P/Invoke bridge catches managed
adoption/application exceptions, records the first in `WebCamera.CallbackException`, suppresses later
delivery and requests Stop. Read that property after owner-side Stop/join; it remains readable after
Close/Dispose, clears on successful Start, and is restored if Start fails. A frame passed to the
application remains application-owned even if it throws: dispose it with `using` unless retaining it.
Stop-request failure does not resume delivery. This is not a fatal-CLR or whole-heap recovery guarantee.
See [CABI-ADOPT-06](CAbiContract.md) for partial-result cleanup and native handle-allocation errors.
JNI reports and clears pending Java callback exceptions, so those exceptions do
not invoke the native throwing-callback stop policy. JNI keeps callback global references and attaches
the worker; Node dispatches copied events to its environment. Native callback completion and managed
queued delivery are distinct boundaries; exhaustive close/queued-delivery races are not certified
by no-device tests. [C ABI](CAbiContract.md) defines status/TLS/token lifetimes.

Projection adapters copy layer bytes/vertices/indices into the public projection request, call
`renderProjection`, and return owned packed RGBA8 plus adapter metadata. Native Renderer/resources
exist only during that operation. See [Projection](OuiProjection.md) for scale-two rounding and
partial failure. XPT wrappers preserve HTTP responses with status errors and do not automatically
retry; their deadline limitations remain those of [XPT](XptTransport.md), not an absolute OS-call bound.

### Raw transport error values: BIND-TRANSFER-05

TCP/Serial `send` and UDP `sendTo` preserve the native completed byte count when a returned
TransportResult fails. UDP `receiveFrom` preserves bytes and sender only for `DatagramTruncated`.
Transfer errors support catching through the language's base error type.

- Python raises `WseTransferError(WseError)` with `bytes_transferred`, `received_data` and
  `source_endpoint`. The last two are independent `bytes` and a `(host, port)` tuple for truncation,
  or `None` for a failed send. The Python package and type stub export the subtype when XPT is enabled.
- JavaScript keeps its existing `Error` object and numeric category/code/nativeCode fields. The
  TypeScript `TransferError` interface adds `bytesTransferred`; truncation also supplies an owned
  `receivedData` Buffer and `sourceEndpoint`. Those two properties are absent for failed sends.
  `TransferError` is a type interface, not a runtime constructor. Counts use the same JavaScript
  number representation as successful sends; no wider integer precision is promised.
- Java raises `TransferException extends WseException`, with `bytesTransferred()`,
  `receivedData()` and `sourceEndpoint()`. The prefix is Java-owned, and its accessor returns a
  defensive copy; send failures return null for data/source. The exception has no close obligation.
- C# retains its existing `TransferException` with `BytesTransferred`, `ReceivedData` and
  `SourceEndpoint`. See [CABI-TRANSFER-07](CAbiContract.md) for C entry-point compatibility and TLS rules.

For example, receiving `[00,7f,ff,01,02]` into capacity 3 raises DatagramTruncated with count 3,
prefix `[00,7f,ff]` and sender. The suffix is discarded. A later receive or socket close cannot
change the retained prefix. Timeout/cancellation without a packet remains the ordinary base error
and fabricates no data. Current TCP/Serial receive returns its completed chunk only on success;
the bindings do not invent an error prefix. A failed send count is local progress, not proof of
peer acceptance. Neither cancellation nor partial progress authorizes automatic replay.

The private [conversion view](../../../lang/common/transfer_failure.h) borrows from the native
result only during exception construction. Each adapter copies data into its runtime before the
native result is destroyed. Python constructs under the GIL. JNI scopes local references and stops
on a pending Java exception; Node stops after failed value/property creation and retains a pending
VM exception. JNI/Node transfer-error construction contains C++ exceptions. See
[JNI local frames](https://docs.oracle.com/en/java/javase/17/docs/specs/jni/functions.html#pushlocalframe)
and [Node-API exceptions](https://nodejs.org/api/n-api.html#exceptions).
Allocation/conversion failure may replace the transport error and leave no recoverable prefix.
Pre-call validation, disposed handles, native operations that throw before returning a Result,
fatal runtime exhaustion and all other binding entry points are outside this boundary.

`wse.binding.transfer_failure_contract` covers 15 synthetic native conversion cases, including
nonzero send progress with timeout/cancellation/I/O failure and suppression of data for other
receive errors. `wse.binding.python_xpt`, `wse.binding.node_xpt` and `wse.binding.java_contract`
exercise real loopback truncation, sender/prefix retention, subsequent datagrams, close, base catch
compatibility and zero-count send failures. The synthetic maximum size_t case is native-only;
it does not establish managed integer limits. These are not whole-heap fault sweeps, physical
serial tests or proof of nonzero failed sends through every OS/runtime path.
The Python XPT contract can also run without a module-path argument to import the installed `wse`
package. It checks the exception's module identity and a pickle round trip after the socket closes.

## Detailed evidence map

| Contract | Gate / source | Limit |
| --- | --- | --- |
| BIND-STRUCT-01 / BIND-BUFFER-02 | `wse.binding.facade_contract`, language Core contracts, `wse.capi.runtime_contract` | Detached-copy vectors; no safe Java view use after native owner close |
| BIND-ASYNC-03 | Node lifecycle/stress, Python lifecycle/stress, Java contract/stress | Runtime-specific cases; queued-callback cancel races and arbitrary user execution are not universal bounds |
| BIND-DEVICE-04 | Language Tmr/OUI/IUI/XPT contracts, common projection/frame fixtures | Enabled components only; no camera hardware callback certification |
| BIND-TRANSFER-05 | Native transfer-failure contract and Python/Node XPT/Java contracts | Synthetic nonzero failed sends; real UDP truncation; no whole-heap failure certification |
| C ABI layout / outputs | [C ABI design](CAbiContract.md) | Header snapshot and loaded Core shim test are distinct evidence |

For reconstruction, first reproduce the four-byte copy and two-call C example, then explain input,
returned-owner and view lifetimes separately. Component availability alone is not proof of a bound
operation. Test registration is in [WseTests.cmake](../../../cmake/WseTests.cmake).

The automated matrix covers Windows x86-64 and Linux x86-64 shared/static runtime tests,
simultaneous JavaScript/Python/Java/C# selection, Tmr/OUI contracts, installed-package loading, one
canonical 64-byte plus FNV-1a full-frame comparison across C++, JavaScript, Python, and Java, lifecycle,
cancellation and bounded-memory stress, Java 17/21/25, and Linux ARM64 shared/static cross builds.
ARM64 runtime execution remains a separate Raspberry Pi gate and is not implied by cross-build
success.

C# has its own runtime and component contracts, including frame processing and disposal. Its OUI
contract checks output dimensions, row pitch, the 64-byte frame length, adapter reporting, and a
reference pixel; it does not perform the full-frame FNV-1a comparison above.

Runtime regressions cover repeated callback-start failure on an unopened camera, and Python/C#
HTTP 200/404/503 responses with and without the authenticated entry point. A Python responder runs
in the same process on another thread, proving that HTTP work releases the GIL. Error responses
retain their status, body, and headers, and the adapter does not retry the exchange.

The managed ownership fixture adds 53 rollback injections and 8 native-worker callback cases;
the native delivery helper has 5 failure/ownership cases. See [C ABI evidence](CAbiContract.md).

## Extension composition

Optional extension-owned camera adapters may reuse the existing frame, capability and error
conversions through compile-time adapter fragments. These hooks are disabled without an overlay.
Their registrations, managed entry points, native exports, tests and documentation belong to
that overlay and must not be installed by a public-only build. Selecting one controlled
component must not implicitly include another component's managed classes.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
