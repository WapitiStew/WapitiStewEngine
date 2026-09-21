# WSE Deprecation and Migration Guide

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This guide defines deprecated public C++ surfaces, canonical replacements, and removal decisions.
The complete machine-readable inventory is
[`DeprecationLedger.tsv`](../deprecation/DeprecationLedger.tsv). The `wse.deprecation_ledger` test
matches its counts, replacements, English/Japanese anchors, and removal gates against public headers.

## Application-owned logging and kit metadata

Use an application-owned `Logger<Tag>` alias or the generic five-argument `writeLog` entry.
Declare the tag with external linkage in the application, define it once, and rebuild all
dependent C++ binaries. An old binary importing an application tag from the SDK is incompatible.
See [Core Services](../design/en/CoreServices.md#application-owned-tags) for the current contract.
Offline candidates use format v2 and accepted manifests use schema version 2. Consumers read the
SDK identity from `wse`; application distributions keep their own revision record outside it.
Retain existing v1 artifacts together with their matching v1 verifier/schema, or regenerate
from the original committed sources. Relabelling a v1 artifact as v2 is not a valid migration.

<a id="c-abi-keyboard-type"></a>
## C ABI keyboard state type

Use `wse_capi_keyboard_access_state_value` or `enum wse_capi_keyboard_access_state` for the enum type.
The query function remains `wse_capi_keyboard_access_state(handle, &state)`, and its output is int32.
The previous same-named typedef and function collided in C's ordinary identifier namespace and
prevented conforming C compilation. The repair preserves the enum tag, constants, function symbol,
signatures and all struct layouts; ABI version 1 and WSE 1.0.0 remain unchanged. This is a header
compilation repair, not removal of a valid C typedef/function combination.

## Constructor initialization

Project-owned C++ classes and structs initialize non-static members in constructor initializer
lists. Configuration records provide defaulted constructor parameters in the former field order,
so empty, partial, and positional brace construction remain supported. Consumers must not rely
on these records being aggregates or trivially default-constructible; use their constructors.
Copy and move operations keep their ownership semantics. C ABI structs are unchanged.
The public standard log formatter, `formatLogMessage()`, formats without dispatching to a sink.
Rebuild dependent C++ code and libraries together after updating the headers.

## Removal policy

WSE 1.0.0 removed every deprecated declaration and every legacy opaque alias that this ledger
tracked. An entry appearing here is a reviewed decision rather than an accident; the ledger is
currently empty, and removed entries live on in
[`RemovedApiLedger.tsv`](../deprecation/RemovedApiLedger.tsv).

- A ledger entry is not a scheduled removal.
- Removal occurs only in an owner-approved SemVer major.
- A release containing both the replacement and the compiler warning remains available for at least
  one minor-release interval, with release notes and migration examples.
- Public consumers reach zero deprecated uses, and shared/static installed-package consumers compile
  with deprecation warnings treated as errors.
- Release review records source/binary compatibility, target platforms, rollback, and licensing
  impact.

<a id="legacy-removal-program"></a>
### The pre-publication legacy removal program

On 2026-09-13 the owner decided to remove every remaining compatibility surface before the first
published release, under the same unpublished-waiver rule as the changes recorded below: WSE is
unpublished, so a reviewed in-place removal replaces the deprecation cycle, and the version stays
1.0.0 until publication. The first published 1.0.0 ships with no pending entry — no deprecated
declaration, no compatibility class, no legacy error surface, and no permanent compatibility
option (an `WSE_ENABLE_LEGACY_API` style switch is explicitly not introduced).

The reviewed work list is
[`LegacyRemovalInventory.tsv`](../deprecation/LegacyRemovalInventory.tsv). Each row records the
surface, its replacement, the consumers that must migrate first, the ABI impact, and the removal
phase; the `wse.legacy_removal_inventory` test validates the file and requires it to cover every
`DeprecationLedger.tsv` id. A row leaves `pending` only through the pull request that performs the
work, and `removed` rows remain as history.

The managed consumers were classified as follows before any removal:

| Consumer | Classification |
| --- | --- |
| Engine tests, examples, and installed-package consumer tests | Migrate in the same change as each removal |
| Application host (including its vendored engine and extension checkouts) | Migrate before the corresponding surface is removed |
| Access-controlled extension overlay | Verify in its own environment before each affecting removal merges |
| EngineDevelopper | Historical development harness, dormant since 2025-06; archived against its pinned engine snapshot, no migration |
| Language bindings and the flat C ABI | Not coupled to the legacy Core error surface; re-verified through the existing ABI contract tests |

Three source ratchets hold the program's ground while it runs: the raw void-pointer and
thread-state baselines under `wse.public_header_boundary`, and the legacy error-surface baseline
under `wse.public_api_policy`. Each may only shrink, so removed debt cannot silently return.

The program completed on 2026-09-14: every inventory row is `removed`, every ratchet baseline is
empty, and the deprecation ledger carries no entry. The `wse.legacy_free` gate pins that end
state, so reintroducing a legacy surface fails the suite until an explicit owner decision is
recorded in the inventory.

### What 1.0.0 removed

| Component | Removed | Replacement |
| --- | --- | --- |
| Core | The mutable `Map::vector()` overload and reference-returning `Map::begin()` (deprecated 2026-09-08, removed 2026-09-13 under the legacy removal program) | `Map::elements()` / `Map::at()` / `Map::row()` and `Map::front()` |
| Core | `BMultiThread::setCallback(function, void*)` | `setCallback(EventCallback)` |
| Core | `Matrix_::inverse()` and the legacy data-surface throws — `wseException(ErrorCode)` across Map/Matrix/Image/Pixel/Point/Range/Mesh/Homography and the pixel-format helper (replaced in place 2026-09-14 under the legacy removal program) | Standard exceptions for usage violations; `CoreResult` (`Matrix_::tryInverse()`) for expectable computation failures |
| Core | The `wse::Timeout` atomic-flag spin-wait helpers (removed 2026-09-14 under the legacy removal program) and the legacy `TIMEOUT`/`UNIMPLEMENTED` device-pickup throws | `OperationContext`-style deadlines where waiting is needed; device pickup reports platform failures as `std::runtime_error` and unimplemented enumerations as `std::logic_error` |
| GEF | The legacy `wseException(ErrorCode)` throws in `BINController`/`CSVController` file I/O (replaced in place 2026-09-14 under the legacy removal program) | `GefStatus`/`GefResult` carrying `wse::gef::GefError` for operational failures; the empty-controller `last_index()` throws `std::logic_error` |
| Core | The legacy `wseException(ErrorCode)` throws in the license surfaces — `License::load`, `Licensekey::genrate`/`load`, `LicenseWriter::save` (replaced in place 2026-09-14 under the legacy removal program) | `LicenseStatus`/`LicenseResult` carrying `wse::LicenseError` for operational and verification failures; a double load throws `std::logic_error` and an out-of-range writer enum throws `std::invalid_argument` |
| Core | The `wse::ErrorCode` enum, the `wseException_` class, and the `wseException`/`wseThrowException` macros with `wse/utility/wse_Exception.h` (removed 2026-09-14 under the legacy removal program, after the paired extension migration) | Standard exceptions for usage violations; the component Result contracts (`CoreResult`, `LicenseResult`, `GefResult`, ...) for operational failures |
| Core | The OpenCV coupling — `cv::` conversion constructors, assignments, and `cv()` accessors in the seven data headers plus the `link/cv_link.h` enable-macro header (removed 2026-09-14 under the legacy removal program) | The opt-in adapter `<cv/OpenCvAdapter.h>`: `wse::ocv::fromCv*`/`toCv*` free functions built on the public interleaved contract |
| Core | The remaining legacy names — the misspelled `memoey_size()` getter, the generic `LicenseWriter::eType`/`eVer` enum names, and the misspelled `wse_Practiser.h` header (renamed 2026-09-14 under the legacy removal program; the other listed misspellings had already left with the `ErrorCode` enum) | `memory_size()`, `LicenseWriter::eLicenseType`/`eLicenseVersion`, and `wse_PixelFormat.h`; `snake_case` simple getters stay canonical per the coding rule |
| Core | The `BMultiThread` public worker base itself — thread/atomic/mutex pointer accessors, the raw `void*` callback context, the task state machine, `setThreadPriority_High()` (removed 2026-09-13 under the legacy removal program) | An internal worker implementation composed by `Timer` and `Keyboard` |
| Core | Timer context-pointer `initialize()` and constructor | `initialize(interval, TimerCallback, EventCallback)` |
| Core | `Timer::updateTimerCofig()` | `updateTimerConfig()` |
| Core | The `Timer` public `BMultiThread` inheritance with its inherited worker API, and `initialize()` / `updateTimerConfig()` / `setTimerCallback()` / `clearTimerCallback()` (removed 2026-09-13 under the legacy removal program) | `start(interval, callback)` / `setInterval()` / `stop()` / `isRunning()` |
| XPT | Context-pointer `SerialConnector::open()` and `setCallback()` | `setCallback(EventCallback)` and `open(port, baudRate)` |
| XPT | The `SerialConnector` class (removed 2026-09-13 under the legacy removal program) | `SerialPort` on a single owning thread |
| XPT | `TransportError::native_code()` | `nativeCode()` |
| OUI | `RendererError::native_code()` | `nativeCode()` |
| OUI | `ProjectionRenderer`, `ScreenRenderer`, `WindowRenderer`, `InterfaceGPU`, `RenderTexture2D`, `OS_Display`, `OS_MultiSourceDisplay` | `Renderer` with `ProjectionPipeline` and surfaces |
| OUI | The nine `void*` aliases those headers declared | Opaque handles owned by `Renderer` |
| IUI | `Keyboard::ref_*_state()` | `snapshot()` or the owned-array getters |
| IUI | The `Keyboard` public `runProcess()` worker override and its `BMultiThread` base (removed 2026-09-13 under the legacy removal program; the public snapshot/callback API is unchanged) | An internally owned monitor worker |
| Tmr | The 42 historical `WebCamera` methods | `enumerate`/`open`/`start`/`stop`/`close`, the control API, and `CameraFrameOps` |
| Package | The installed `WonderStewEngine` compatibility alias target (removed 2026-09-13 under the legacy removal program; the package name and `find_package(WonderStewEngine ...)` are unchanged) | The `WSE::Core` / `WSE::*` component targets, or a consumer-local alias |

The owner waived the `major-owner-private-consumers` and `major-owner-hardware-consumers` gates for
this removal. Those gates required a separate-environment private-consumer review and camera
hardware certification of the replacements; neither was performed, so a private consumer or a
hardware integration that still called the removed surface has to migrate against this table rather
than against a deprecation warning.

`WebCamera(index)` survives the removal and now selects the enumerated device at that index through
the canonical API. It reports a failure through `lastError()`, because a constructor cannot return a
status.

<a id="parameter-convention"></a>
### The function-parameter convention

The [public API policy](../design/en/PublicApiPolicy.md) now states the parameter convention the
source had been following only in part. Applying it changes the signature of every function that
takes an output: an out-parameter became a pointer and moved to the front of the parameter list.

This is a source-breaking change made in place, with no compatibility overload, by owner decision
while WSE remains unpublished — the same basis as the tmr rename above. The version stays 1.0.0
until publication. There is no deprecation warning to migrate against, so migrate against the shape:

```cpp
// before
wse::img3c08_t image;
wse::tmr::toImage( frame, image );
wse::tmr::readImage( camera, 1000U, image );
wse::oui::sRendererFrame frame;
renderer.readTexture( texture, 1000U, image );

// after — the written-to argument comes first and is passed by address
wse::img3c08_t image;
wse::tmr::toImage( &image, frame );
wse::tmr::readImage( &camera, &image, 1000U );
renderer.readTexture( &image, texture, 1000U );
```

The rule that produced every one of these: an out or in-out parameter is a pointer, in-out comes
first, then out, then in. A call site therefore shows at a glance which arguments it is handing over
to be written.

<a id="pixel-32bit-storage"></a>
### The 32-bit pixel storage correction

`PixelTraits` for `CH1D32`/`CH2D32`/`CH3D32`/`CH4D32` declared `uint64_t` storage while the header
documentation promised `uint32_t`, so every 32-bit format consumed 8 bytes per channel and the
byte layout diverged from the documented contract. The storage type is now `uint32_t`.

This is a binary-layout change made in place, with no compatibility alias, by owner decision
(2026-09-08, issue CORE-006) while WSE remains unpublished - the same basis as the changes above.
The version stays 1.0.0 until publication. No source edit is required: values above `UINT32_MAX`
were already outside the documented 32-bit contract, and the reduction/enlargement cast rules are
bit-depth based and unchanged. A consumer that serialized raw 32-bit image buffers under the old
8-byte stride has to re-serialize. The `wse.core.pixel_storage` contract now pins the storage
type, channel count, bit depth, and stride of every pixel format at compile time.

<a id="inventory"></a>
## Complete inventory

Public headers contain no `[[deprecated("...")]]` declaration and no legacy `void*` alias. The two
`wse::Map` accessors that held that count were removed on 2026-09-13 and are recorded in
[`RemovedApiLedger.tsv`](../deprecation/RemovedApiLedger.tsv). The later sections stay as migration
guidance for code written against a pre-1.0.0 release, where the removed surface still compiled
with a warning.

<a id="map-legacy-accessors"></a>
## Core Map accessors

Removed 2026-09-13 (deprecated 2026-09-08). Replace the shape-unsafe `Map` accessors with the
checked surface.

```cpp
// Before
std::vector<float>* raw = map.vector();  // a resize here silently breaks width x height
float& first = map.begin();              // returns a reference despite the name

// After
float* values = map.elements();          // value access only; the shape cannot change
float& checked = map.at(0, 0);           // bounds-checked element access
float* rowValues = map.row(0);           // bounds-checked row pointer, width() elements
float& firstChecked = map.front();       // rejects an empty Map with std::out_of_range
```

`vector()` allowed a resize or clear through the returned pointer, desynchronizing the stored
element count from `width() * height()`; `elements()` grants value access without shape access.
`begin()` returned a reference rather than an iterator and, like `front()`, was undefined behaviour
on an empty Map. Both now reject an empty Map with `std::out_of_range`, and `begin()` was removed in
favour of `front()`. `get()`/`set()` report an out-of-range index as `std::out_of_range`, and every
allocating constructor and `init()` validate that `width x height` is representable in `size_t`,
throwing `std::invalid_argument` on overflow (see the
[Core data-type error contract](#core-error-contract)).

<a id="core-worker-timer"></a>
## Core worker and timer

On 2026-09-13 (legacy removal program) `Timer` stopped inheriting `BMultiThread` and became a
final class owning its worker internally. The inherited generic worker API (`bootThread()`,
`stopThread()`, `notifyTask()`, `waitTask()`, `getThread()`, `getStateFlag()`, `state()`,
`setThreadPriority_High()`, the base constructors, and the worker `EventCallback`) and the old
timer surface (`initialize()`, `updateTimerConfig()`, `setTimerCallback()`,
`clearTimerCallback()`) left `Timer` in place, without a deprecation cycle:

```cpp
// Before — inherited worker lifecycle plus a separate configuration step
timer.initialize(100U, []() { /* periodic work */ },
    [](wse::BMultiThread::eEvent event) { /* lifecycle event */ });
timer.bootThread();
timer.updateTimerConfig(200U);
timer.stopThread();

// After — one composition-based lifecycle; intervals are std::chrono values
timer.start(std::chrono::milliseconds(100), []() { /* periodic work */ });
timer.setInterval(std::chrono::milliseconds(200));
timer.stop();
```

`start()` binds the callback (captures owned by the timer, released on stop), rejects a
non-positive interval or an empty callback with `std::invalid_argument`, and returns `false`
while already running. `stop()` is idempotent and callable from inside the callback; after it
returns no new invocation begins. A callback exception never crosses the thread boundary and
stops the timer; `isRunning()` replaces the worker-event lifecycle callback.

`BMultiThread` itself was removed on 2026-09-13 in the same program: the public worker base with
its thread/atomic/mutex pointer accessors, raw `void*` callback context, task state machine, and
best-effort `setThreadPriority_High()` no longer exists. Code that derived a worker from it owns a
`std::thread` (or the feature it drove) directly; the WSE features that used it — `Timer` and
`Keyboard` — run on an internal worker and expose only their own APIs.

<a id="core-error-contract"></a>
## Core data-type error contract

On 2026-09-14 (legacy removal program) the Core data surfaces — `Map`, `Matrix_`, `Image_` and
its demosaic/interleave/transform helpers, `Pixel_`, `Point2_/3_/4_`, `Range1D/2D/3D`, `Mesh`,
`Homography`, and the pixel-format helper — stopped throwing `wseException_` with legacy
`ErrorCode` values, in place and without a deprecation cycle:

- A **usage violation** throws the matching standard exception: `std::invalid_argument` for bad
  arguments, sizes, and formats; `std::out_of_range` for indices outside a shape;
  `std::domain_error` for division by zero and for an operation on data outside its mathematical
  domain (a singular configured color matrix, a degenerate distribution); `std::logic_error` for
  an operation in an impossible state.
- An **expectable computation failure** returns a failed `wse::CoreResult` instead of throwing:
  `Matrix_::inverse()` was replaced by `tryInverse()`, whose singular-matrix outcome arrives as
  `eCoreErrorCategory::Computation` / `eCoreErrorCode::SingularMatrix`.

```cpp
// Before — every failure was a wseException carrying a legacy ErrorCode
try { const wse::Matrix inverted = matrix.inverse(); }
catch (const wse::wseException_& e) { /* branch on e.code() */ }

// After — usage errors are standard exceptions, singular input is a Result
const wse::CoreResult<wse::Matrix> inverted = matrix.tryInverse();
if (!inverted.succeeded()) { /* inverted.error().code() == SingularMatrix */ }
```

`wse::ErrorCode`, `wseException_`, and the `wseException`/`wseThrowException` macros were removed
on 2026-09-14 together with `wse/utility/wse_Exception.h`, after the access-controlled extension
completed its paired migration. Usage violations throw the standard exceptions and operational
failures return the component Result contracts; there is no legacy error surface left to migrate
from.

<a id="xpt-transport"></a>
## XPT transport

The Windows-only callback-based `SerialConnector` was removed on 2026-09-13 (legacy removal
program; it was a compatibility surface, never `[[deprecated]]`). `SerialPort` is the only serial
surface: a caller-confined, move-only, synchronous owner with explicit `OperationContext`
timeout/cancellation and partial-transfer results.

```cpp
// Before — asynchronous callbacks, no timeout, send() could not fail
wse::xpt::SerialConnector connector;                     // removed
connector.setCallback([](wse::xpt::SerialConnector::Event, const std::vector<unsigned char>&) {});
const bool started = connector.open("COM3", 115200);     // wait for OPENED / TMR_FAILED_OPEN

// After — one owning thread drives the synchronous port
wse::xpt::SerialPort port;
const wse::xpt::OperationContext context(wse::xpt::Timeout::milliseconds(5000));
const auto opened = port.open("COM3", 115200, context);  // TransportStatus, no event wait
const auto sent = port.send(frame, context);             // verify sent bytes == frame bytes
const auto received = port.receive(256U, context);       // poll; Timeout category = no data
```

An event-driven consumer runs one serial I/O worker thread that owns the `SerialPort` and turns
`receive` results into its own events; `CancellationSource::cancel()` is the one call allowed from
another thread. Protocol-specific logic stays out of XPT.

<a id="oui-renderer"></a>
## OUI renderer

Replace the error accessor with `nativeCode()`. Do not add legacy GPU/texture/window `void*` aliases;
migrate to generation-bearing opaque IDs owned by `Renderer`.

```cpp
wse::oui::Renderer renderer;
wse::oui::sRendererConfiguration configuration;
const auto initialized = renderer.initialize(configuration);
if (initialized.succeeded()) {
    const auto capabilities = renderer.getCapabilities();
    // createTexture/createMesh/createSurface return opaque IDs.
}
renderer.shutdown();

const std::int64_t diagnostic = rendererError.nativeCode();
```

The `InterfaceGPU`, `RenderTexture2D`, `ProjectionRenderer`, `ScreenRenderer`, and `WindowRenderer`
classes are removed. Before removal their parity with the portable `Renderer` was demonstrated
rather than asserted: a contract fed one source image and one mesh through both paths and compared
the pixels read back to the CPU, and the two agreed bit for bit.

| Legacy | Portable replacement |
| --- | --- |
| `InterfaceGPU::initialize()`, `isInitialized()`, `finalize()` | `Renderer::initialize()`, `isInitialized()`, `shutdown()` |
| `InterfaceGPU::target_desc()` | `getCapabilities()`, which now also reports `adapter_name` |
| `InterfaceGPU::setTargetAdapter()` | `sRendererConfiguration::adapter_name` |
| `RenderTexture2D::initialize()`, `finalize()` | `createTexture()`, `destroyTexture()` |
| `RenderTexture2D::uploadCPUResourse()`, `downloadGPUResourse()` | `uploadTexture()`, `readTexture()` |
| `ProjectionRenderer` / `ScreenRenderer` setup and `render()` | `sProjectionPassDescription` and `ProjectionPipeline::execute()` |
| `updateProjectionGeometric()`, `updateContentsCell()` | `ProjectionMeshAdapter::createMesh()` and `updateMesh()` |
| `downloadProjectionData()`, `downloadContentsData()` | `readTexture()` |
| `WindowRenderer` window setup, `show()`, `render()` | `createSurface()`, `setSurfaceWindowMode()`, `presentSurface()` |
| `render(const wse::img4c16_t&)` | `eRendererPixelFormat::Rgba16Unorm` as a sampled source |

Three points cost time when migrating, so they are recorded here:

- A projection mesh carries `src` in **source-pixel** coordinates, not normalized ones. Passing 0.0
  to 1.0 samples only the top-left two-by-two texels of a larger source.
- The legacy upsampling pixel shader multiplies by the alpha map, so a projection renders black
  until `uploadAlphamapSource()` is called. The portable path has no such implicit requirement.
- `readTexture()` reports its own `row_pitch`, which the backend aligns. Treating the readback as
  packed corrupts every row after the first. The legacy `downloadProjectionData()` returned an
  `Image_` and hid this.

These remain deliberate non-goals rather than gaps. They are what the portable API exists to prevent:

- The raw device, factory, texture-resource, heap, and command-list pointers (`GPUDevicePtr`,
  `GPUFactoryPtr`, `Ptr_2DTexResource`, `Ptr_2DTexHeap`, `Ptr_GPUCommander`) and the raw target
  pointers (`Ptr_ProjectionTarget`, `PtrScreenTarget`, `PtrWindowClass`, `PtrWindowHandle`). A
  consumer that needs these is writing backend code, not using WSE.
- Replacing the pipeline shaders by file path, which `WindowRenderer::initialize(vs_path, ps_path)`
  accepted. The portable pipeline owns its shaders.

`OS_Display` and `OS_MultiSourceDisplay` are removed with them. Their only callers were sources that
no build compiled, and `OS_MultiSourceDisplay` had no implementation anywhere while the install rules
shipped its header.

<a id="iui-keyboard"></a>
## IUI keyboard

Do not retain references to worker-owned arrays while monitoring. Read one synchronized snapshot or
an owned array.

```cpp
const wse::iui::KeyboardState state = keyboard.snapshot();
const auto ascii = keyboard.ascii_state();
```

<a id="tmr-webcamera"></a>
## Tmr WebCamera

Ordinary USB/UVC consumers use device discovery, advertised profiles, owned frames, and typed controls.

```cpp
const auto devices = wse::tmr::WebCamera::enumerate();
const auto capability = wse::tmr::WebCamera::capabilities(devices.value().front());
const auto& profile = capability.value().stream_profiles.front();

wse::tmr::sCameraStreamConfiguration configuration{
    profile.native_format, profile.output_formats.front(), true};
wse::tmr::WebCamera camera;
camera.open(devices.value().front(), configuration);
camera.start([](const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& frame) {
    // A successful frame owns its bytes.
});
camera.stop();
camera.close();
```

See the [WebCamera migration guide](WebCameraMigration.md) for control, format, OpenCV conversion,
and settings-window mappings. Physical-camera gates, generic XU, and held Raspberry Pi work are not
implied by software-test success.

## SDK package layout

The installed JavaScript binding moved from `lang/javascript` to `lang/js` so that every
JavaScript directory in the source tree and in the SDK uses the same `js` name.

| Removed layout | Replacement layout |
| --- | --- |
| `<sdk>/lang/javascript/index.js` | `<sdk>/lang/js/index.js` |
| `<sdk>/lang/javascript/index.d.ts` | `<sdk>/lang/js/index.d.ts` |
| `<sdk>/lang/javascript/package.json` | `<sdk>/lang/js/package.json` |

```js
// Before
const wse = require('C:/wse-sdk/lang/javascript');
// After
const wse = require('C:/wse-sdk/lang/js');
```

This is a package-layout change only. The Node-API module name, the exported surface, and
`bindingAbiVersion` are unchanged, so no source edit is required beyond the require path.
The CTest name `wse.documentation.quickstart_javascript` and the `WSE_JAVASCRIPT_PACKAGE`
environment variable name the language rather than a directory and are retained.

## Outside this ledger

- `WonderStewEngine` is a compatibility CMake target, but it is not scheduled for removal.
- C++ `CameraSession` remains the supported low-level portable camera foundation.
- Access-controlled extension headers use a separate private ledger and never enter this public ledger.
- An old name or legacy class is not a removal candidate without a formal deprecation or migration decision.

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
