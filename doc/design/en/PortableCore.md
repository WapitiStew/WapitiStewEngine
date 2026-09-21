# WSE Portable Core Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document defines the current portable boundary of `WSE::Core`. It is normative for the
Windows and Linux implementations. Optional-component details, language bindings, and hardware
certification are outside this contract. The [XPT Transport design](XptTransport.md),
[OUI Renderer design](OuiRenderer.md), [OUI Projection design](OuiProjection.md), and
[Tmr Camera design](TmrCamera.md) define those component boundaries.

## Support matrix

Detailed data and behavior are specified by [Core Data Model](CoreDataModel.md),
[Core Algorithms](CoreAlgorithms.md) and [Core Services](CoreServices.md). This document
keeps the platform boundary; the services design gives Timer scheduling and log/License state.

| Target | Compiler | Library forms | Supported scope |
| --- | --- | --- | --- |
| Windows x86-64 | MSVC / Visual Studio 2022 | Shared, static | Core and optional components |
| Linux x86-64 | GCC, Clang | Shared, static | Core, GEF, IUI evdev, XPT, OUI Vulkan/Wayland/DRM-KMS, and Tmr |
| Linux ARM64 | GCC cross, GCC/Clang native | Shared, static | Core/GEF/IUI/XPT/OUI/Tmr; hardware scope follows Hardware Validation |

Only 64-bit x86-64 and ARM64 Linux targets are accepted. Linux configuration fails when an
unsupported optional component is enabled. XPT exposes TCP, UDP, HTTP, the portable `SerialPort`,
and `RetryPolicy` on every supported target. An unsupported target or component must not
produce a dummy success result.

## OS boundary

Portable Core code owns data types, timers, logging, license calculations, and thread lifecycle.
OS adapters own device identity, device enumeration, thread-priority requests, and platform log
output. CMake selects exactly one adapter set for the target OS.

### Wait and time

`Wait::sec`, `msec`, `usec`, and `nsec` use `std::this_thread::sleep_for` with the corresponding
`std::chrono` duration. The unit is part of the API contract. The operating system scheduler may
resume the thread later than requested; the API does not promise real-time precision.

### Thread lifecycle

Worker threads are an implementation concern. The public API exposes no thread, atomic, or mutex
state. Each worker-backed feature (`Timer`,
IUI `Keyboard`) owns exactly one internal worker with these shared guarantees: a running
`std::thread` is never transferred during a move; stop is idempotent and joins when called by an
owner thread; a stop from the worker's own thread only requests the stop and the join is taken
over by the destructor or the next start; a stop request wakes a waiting worker immediately; and a
worker-body exception never crosses the thread boundary. Owner-thread destruction waits for the
callback before joining. A callback must not destroy
its own Timer or Keyboard; self-stop does not authorize self-destruction.

### Timer and IUI callbacks

`Timer` is a final class owning its worker internally. It exposes no thread, atomic, or mutex state. The lifecycle is
`start(std::chrono::milliseconds, TimerCallback)` / `setInterval()` / `stop()` / `isRunning()`.
`start()` owns the callback captures and releases them on stop, rejects a non-positive interval or
an empty callback with `std::invalid_argument`, and returns `false` while running. `stop()` is
idempotent, callable from the callback (the join defers to the destructor or the next `start()`),
and no new invocation begins after it returns. A callback exception never crosses the thread
boundary and stops the timer. A stop request wakes a waiting worker immediately.

IUI `Keyboard` starts monitoring on construction on Windows and Linux. `snapshot()` returns
all key groups as one owned update. The Windows adapter polls virtual-key state; the Linux
adapter uses evdev and reports availability separately from the snapshot. `KeyCallback` runs
on the monitor worker after a state change. The public API uses owned snapshots and value getters. Callback removal and permissions follow
the [IUI Keyboard design](IuiKeyboard.md); `clearCallback()` does not join an in-flight callback.

### License device identity

The Linux adapter selects the first non-loopback `AF_PACKET` interface with a non-zero six-byte
hardware address and returns a lower-case colon-separated value. It returns an empty string when
no suitable interface exists. Interface enumeration order is not a stable hardware identity;
no multi-interface identity policy or Linux device-licensing certification is defined.

### Device enumeration

Linux `pickupDeviceInfo` enumerates network interfaces and serial candidates. Network results carry
the interface name, IPv4/IPv6 addresses, MAC address, and sysfs identity. Serial results carry `/dev`
paths for ttyUSB, ttyACM, ttyAMA, ttyS, ttySC, ttyXRUSB, and rfcomm classes. The implementation does
not guess a connection generation or device model. Monitor, keyboard, mouse, touch, and gamepad
entry points throw `std::logic_error` rather than returning a false empty success.

### Paths and packages

Core source selection and installed package generation carry no Windows-only build-path assumptions.
Installed consumers use `find_package(WonderStewEngine CONFIG REQUIRED)` and link the `WSE::Core`
and `WSE::*` component targets. The package does not define a consumer-facing `WonderStewEngine` target. WSE does not define a separate public
filesystem-path API.

Static installed packages resolve their public `Threads::Threads` dependency through the package
configuration. XPT packages bundle pinned libcurl as an implementation dependency; Linux ARM64
packages also bundle pinned OpenSSL. CMake configuration never downloads dependencies.

<a id="fast-data-access"></a>
## Fast data access

Unchecked `operator[]` is a supported, essential C++ data API for direct memory access. It remains
available alongside checked `Map::at(x, y)` and `Map::row(y)`. Do not remove it or replace its return
type with `Result`, a copied row, or an owning buffer merely because callers can supply invalid indices.

| Data class | Expression | Mutable / const result |
| --- | --- | --- |
| `Map<T>`, `Matrix_<T>` / `Matrix`, `Homography` | `data[y]` | `T*` / `const T*` to the row |
| `Image_<Format>` and image aliases | `image[y]` | Pixel pointer / const pixel pointer |
| `Pixel_<T, N, Depth>` | `pixel[channel]` | `T&` / `const T&` |
| `Mesh_<Source, Destination>` | `mesh[y]` | Vertex pointer / const vertex pointer |

The overloads accept integral indices and are inline and `noexcept`. They return non-owning views
into the original storage without allocation, copying, bounds checks, clamping, or synchronization.
Rows are contiguous with a stride of `width()` elements (`vertex_width()` for Mesh); channels refer
to the pixel's original channel array. Mutable writes are immediately visible through a const view.
Matrix, Image, and Homography explicitly expose the inherited overloads as public API.

Callers provide valid indices: nonempty storage, a row below the height, a column below the width,
and a channel below the channel count. Negative or out-of-range indices violate the precondition.
Pointers/references require a live owner and are invalidated when the owner replaces or reallocates
its storage. Concurrent reads/writes follow ordinary C++ synchronization requirements.

`wse.core.fast_indexing` and the installed consumer pin overload availability, constness, row stride,
and storage aliasing. The opt-in `wse.bench.indexing` compares Release Matrix and Image/Pixel scans
against direct pointers on identical inputs with alternating measurement order; it reports measurements
without an absolute time gate.

## Verification contract

Each supported Linux compiler/library-form pair must pass bootstrap, Core characterization, and
Core runtime contracts. Installed packages must compile, link, and run consumers through the
modern `WSE::*` component targets, and must not define a consumer-facing WonderStewEngine alias. Windows
Shared and Static all-component regression gates are required. Hardware certification is recorded separately and cannot be inferred from ARM64 cross compilation.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
