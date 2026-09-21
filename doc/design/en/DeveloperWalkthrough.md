# WSE Developer Walkthrough

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and reading route

This walkthrough connects [Architecture](Architecture.md), [Core Data Model](CoreDataModel.md),
[Tmr Camera](TmrCamera.md), [OUI Renderer](OuiRenderer.md) and [Projection](OuiProjection.md).
It follows a real offscreen example and a separate fake-camera test from input through cleanup.
The component designs define their contracts; the steps below explain how to assemble and observe
them. Running these examples is guided verification, not blind reconstruction or physical-device acceptance.

The renderer example links `WSE::Oui`, which brings Core transitively. The fake-camera test links
`WSE::Tmr`. Tmr does not start an OUI renderer: an application explicitly connects owned CPU values
between the two. The internal camera injection seam used by the test is not an installed public API.

## Offscreen image to projection and back: WALK-IMAGE-01

Use the complete [portable_projection.cpp](../../../example/cpp/oui/portable_projection.cpp),
registered as `wse.oui.backend_projection_golden`. It selects D3D12 on Windows and Vulkan on Linux,
requests a software adapter, and checks offscreen/mesh capabilities. It opens no camera or display surface.

```text
caller-owned Core RGBA image
    -> uploadTexture -> upload fence -> wait
caller-owned mesh identity + source texture identity
    -> ProjectionPipeline (borrows Renderer)
    -> backend submission -> render fence -> wait
offscreen surface owns target texture
    -> readTexture -> independent packed CPU frame -> check -> release owners
```

| Stage | Exact example input / action | Ownership and observation |
| --- | --- | --- |
| Initialize | Explicit backend, software adapter, required capabilities | Renderer owns backend; stop on an initialization/capability error |
| Create source | 2 x 2 RGBA8; Sampled and TransferDestination; initial CopyDestination | Source texture is application-owned |
| Upload | Top row red/green, bottom row blue/white; alpha 255 everywhere | Core image owns 16 logical bytes. Upload copies the input; caller data may be released after upload returns. The example waits for upload completion |
| Create mesh | Positions (-1,1), (1,1), (1,-1), (-1,-1); UV (0,0), (1,0), (1,1), (0,1); indices [0,1,2,0,2,3] | Application owns mesh identity. These directly specified vertices are distinct from the grid adapter's row-major ordering |
| Create output | Offscreen surface 4 x 4, obtain its texture | Surface owns target; do not directly destroy that texture |
| Submit | One Nearest layer, opacity 1, no alpha map/edge fade, scale 1, final CopySource | Projection builds one clear/store pass and borrows Renderer. Fence represents submitted work |
| Read/check | Wait render fence, then read target | Returned frame is packed RGBA8, exactly 64 bytes, independent of GPU storage |
| Finish | Destroy surface, mesh, source texture; shutdown renderer | Example checks rendering results before cleanup. Early return relies on Renderer destruction; its final cleanup statuses are not asserted by this example |

Every output quadrant is a 2 x 2 block: red at top-left, green at top-right, blue at bottom-left,
white at bottom-right. All alpha bytes are 255. The example compares every channel with zero-LSB
tolerance and then checks RGBA FNV-1a `0xe0f9517b5bf10dc5`. This fixture is exact; linear filtering
and alpha examples use the separate tolerance in OUI-PROJ-02.

Check each Result before accessing its value. A failed upload/submit/read does not prove that no work
started or that an attachment is unchanged. Surface ownership, pending resource retention and cleanup
failure behavior remain as specified in OUI-OWNER-01 and OUI-PROJ-03. The example is not an exhaustive
failure-injection test and does not promise bounded shutdown.

### Connecting a camera frame explicitly

A `Bgra8` camera frame maps to `img4c08_bgra_t` through
[CameraFrameOps](../../../api/tmr/camera/CameraFrameOps.h), then to a `Bgra8Unorm` renderer upload.
`toImage()` creates a packed owned image; it does not borrow a camera buffer or silently swap channels.
Use explicit channel conversion if the application chooses RGBA instead. Width, height, format and
texture usage must agree before upload. This connection is an application assembly rule, not an
extra capture path exercised by the offscreen example.

For BGRA8 3 x 2 with stride 16, Camera requires at least 32 bytes and accepts trailing storage.
The packed Core payload is 24 bytes. A Renderer frame with that same stride requires exactly
32 bytes. Do not pass a camera descriptor through by assuming the two validity rules are identical.

## Fake-camera delivery and recovery: WALK-CAMERA-02

Read TMR-CALLBACK-03, then use
[tmr_camera_backend_contract.cpp](../../../test/characterization/tmr_camera_backend_contract.cpp).
Its internal `MockCameraBackend` supplies synthetic values and deterministic failures without
enumerating or opening a device. The test injects it into a CameraSession and drives these branches:

| Event / operation | Delivery state | Backend / owner responsibility |
| --- | --- | --- |
| Open then start with callback | Worker starts; synchronous read would be ConcurrentRead | Session retains backend and copied callable |
| Two read Timeouts | No callback for either timeout; worker reads again | Backend remains streaming |
| Third read returns ReadFailed | Exactly one failure callback, then delivery ends | Mock remains streaming; `isStreaming()` alone does not prove active delivery |
| Separate branch: callback throws on a successful frame | Exception is caught, delivery ends and worker releases its capture | Backend is not implicitly stopped |
| Owner calls stop | Worker joined, CallbackState cleared | Backend stops; caller may start again |
| Callback calls stop | Stop requested; no self-join | Owner must later reap worker before destruction/restart |
| Owner stops a blocked read | Read completes before native stop; cancelled result is suppressed | Owner join is the completion barrier |
| Close | Stop and close backend, release owner references | Repeated close is harmless; no physical evidence is produced |

The successful mock frame is BGRA8 2 x 2, stride 8, with 16 owned bytes. Separate sessions maintain
their own synthetic sequence counters. A delivered callback argument borrows the loop-local result;
copy the successful frame if the application retains it beyond that callback.

To reconstruct the state model as a learning exercise, record backend streaming, worker active,
join pending and callable ownership as separate fields. Feed timeout/timeout/error and
success/throw sequences; compare the observations above before consulting the test.
The [start/stop fault test](../../../test/characterization/tmr_camera_start_stop_contract.cpp) adds
callable-copy and thread-launch failures, rollback failure, read exceptions and concurrent owner/self-stop.
On Linux, the [V4L2 lifecycle test](../../../test/characterization/tmr_v4l2_lifecycle_contract.cpp) drives
the actual adapter with synthetic native calls; no device is opened.
A fake backend's prompt stop does not establish native-driver cancellation latency.
The test's two-second polling deadline is a test harness limit, not an API timing guarantee.

## Build and run the guided paths: WALK-RUN-03

Run from the WSE root with dependencies and compilers already provisioned through the
[Build Guide](../../en/BuildGuide.md). Configure remains offline. Use fresh build directories;
these commands select no physical camera, keyboard or display-mode gates. Linux requires the
supported Vulkan development/runtime dependencies and a usable software adapter; the camera
exercise uses its mock even though Tmr is built.

Windows PowerShell:

```powershell
cmake --preset windows-msvc-shared-public -B build/design-walkthrough-win -DWSE_BUILD_TESTING=ON -DWSE_ENABLE_CAMERA_HARDWARE_TESTS=OFF -DWSE_ENABLE_IUI_HARDWARE_TESTS=OFF -DWSE_ENABLE_DISPLAY_MODE_TESTS=OFF
cmake --build build/design-walkthrough-win --config Debug --parallel 4
ctest --test-dir build/design-walkthrough-win -C Debug --output-on-failure -R "^wse[.]tmr[.]camera_(backend_contract|frame_ops)$|^wse[.]oui[.]backend_projection_golden$"
```

Linux shell:

```sh
cmake --preset linux-gcc-shared-tmr -B build/design-walkthrough-linux -DCMAKE_BUILD_TYPE=Debug -DWSE_BUILD_OUI=ON -DWSE_ENABLE_LIBCAMERA=OFF -DWSE_BUILD_TESTING=ON -DWSE_ENABLE_CAMERA_HARDWARE_TESTS=OFF -DWSE_ENABLE_IUI_HARDWARE_TESTS=OFF -DWSE_ENABLE_DISPLAY_MODE_TESTS=OFF
cmake --build build/design-walkthrough-linux --parallel 4
ctest --test-dir build/design-walkthrough-linux --output-on-failure -R "^wse[.]tmr[.]camera_(backend_contract|frame_ops)$|^wse[.]oui[.]backend_projection_golden$"
```

Expect exactly three selected tests. Zero selected tests is not success: inspect the component
options and registration before proceeding. On a failure keep the diagnostic and backend identity;
do not switch to a hardware pass or silently omit the failing example.
For C ABI and installed consumers, follow [C ABI](CAbiContract.md) and
[Build Packaging](BuildPackaging.md); this source-tree exercise does not establish installed loading.

## Input and memory boundaries: WALK-BOUND-04

Validation establishes representation and supported operations; it is not a universal workload limit.
Select application budgets before accepting external dimensions, counts or requests, and use
overflow-checked arithmetic before allocation.

| Boundary | Existing check / limitation | Application decision |
| --- | --- | --- |
| Core indexing / copies | Unchecked row/channel access requires valid indices; copied values allocate independent storage | Check dimensions/indices; cap retained copies and lifetime of borrowed views |
| GEF BIN / CSV | Tag/layout checks; no configurable total-file/count budget, no general quoted CSV parser | Bound file size, block count and decoded memory before invoking a controller; use a fresh controller after failure |
| XPT HTTP / transfers | Body limit and partial-transfer contracts; no single total header/memory budget | Set body/work budgets, bound accumulated metadata, consume partial byte counts without blind resend |
| Tmr frames / callbacks | Checked stride/format/storage; owned frames; callback executes on reader worker | Choose frame dimensions and retained-frame count; keep callbacks bounded or implement an application queue/drop policy |
| OUI resources | Extent/usage validation; submitted work retains backend resources | Limit live textures, pending submissions and readbacks; destroying a handle alone does not immediately reclaim pending GPU memory |
| Binding / C ABI | Native-owned copies, caller-owned copy destinations, operation-specific errors | Account for native and managed buffers simultaneously; release owners and catch at the language's documented callback boundary |

Quantify logical payloads separately from process/GPU peak memory:

```text
packed image bytes = width * height * channels * bytes_per_channel
camera BGRA bytes >= effective_stride * height
renderer BGRA frame bytes = effective_row_pitch * height
retained packed camera payloads = retained_count * packed_image_bytes
RGBA8 final target = 4 * W * H
scale-two intermediate = 4 * (2W) * (2H) = 16 * W * H
final + scale-two intermediate = 20 * W * H
```

At 1920 x 1080 the packed RGBA8 payload is 8,294,400 bytes. Final plus scale-two intermediate
is 41,472,000 logical texture bytes. This excludes source textures, row alignment, staging/readback,
allocation metadata, driver bookkeeping, managed copies and older in-flight submissions.
It is neither a peak-memory guarantee nor a capacity reservation.

The explicit camera-to-image and upload steps are copy boundaries. C ABI `runtime_copy_frame`
and `frame_buffer_copy_to` create separate native and caller storage. Do not infer an exact count
of implementation temporaries or a zero-copy path from this ownership diagram.

## Blocking and measurement: WALK-MEASURE-05

| Operation | Timing interpretation |
| --- | --- |
| Core Timer callback | Fixed delay after callback completion; callback time adds to the period |
| IUI callback clearing | Removing registration does not join an already copied invocation |
| XPT deadline/cancellation | Shared operation budget; resolution, scheduling and native cancellation can exceed a requested wait |
| Tmr callback/stop | Internal 100 ms read request is not a shutdown bound; user code and native stop add latency |
| OUI readback/shutdown | Vulkan readback passes the caller timeout to its native fence wait, retaining unresolved work; preparation/copy and shutdown are not universally bounded |
| Binding Runtime wait | Cancellation observed in sleep slices; scheduling prevents a strict response-time guarantee |

Use [Core Services](CoreServices.md), [Thread Ownership](ThreadOwnership.md),
[XPT](XptTransport.md), Tmr and OUI as the detailed sources. Do not substitute observed latency
for a documented upper bound.

The existing opt-in [indexing benchmark](../../../test/benchmark/indexing_benchmark.cpp) measures
Core Matrix and Image/Pixel scans against direct pointers on identical inputs, alternating order.
For an already configured Core build, configure a separate Release measurement directory with
`WSE_BUILD_TESTING=ON` and `WSE_BUILD_BENCHMARKS=ON`, build target `wse.bench.indexing`,
and run CTest with `-R "^wse[.]bench[.]indexing$" -V` (Windows also uses `-C Release`).
It checks equal sums and reports measurements; it has no absolute-time performance gate.

For an application pipeline, record monotonic timestamps around capture completion, conversion,
upload return/fence completion, submit return/fence completion, readback, callback and owner stop.
Choose warm-up/sample counts explicitly, keep input and build configuration fixed, report
median/tail/max and failures, and label cold-start separately. Record OS, architecture, compiler,
optimization, adapter, extents, formats, strides, layer count, scale and pending-frame count.
Do not subtract an unspecified camera timestamp epoch from the host clock.

Observe process memory and adapter-specific GPU/staging memory at the same workload boundaries.
After stopping submissions, wait for completion and release owners before comparing steady retained
memory. CPU/GPU timings, CTest elapsed time and device timestamp differences are different measures.
No FPS, universal shutdown deadline, maximum accepted input size or bounded peak memory is promised
by this walkthrough.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
