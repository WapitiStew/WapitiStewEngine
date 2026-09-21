# WSE Design Verification

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and interpretation

This is the navigation and acceptance contract for reconstructing WSE behavior from its design.
It maps a claim to executable evidence and states where that evidence ends. A listed test is
not a recorded pass on the reader's machine. Hardware status belongs to
[Hardware Validation](HardwareValidation.md), and historical evidence is kept outside this document.

Use the [Architecture](Architecture.md) for responsibility and source maps, then read the
component contract before its tests. The English design and public headers define behavior.
A disagreement with implementation is investigated rather than automatically resolved by copying
the implementation into the specification.

## Contract-to-evidence map

CTest names are registered in [WseTests.cmake](../../../cmake/WseTests.cmake).
Component options and available runtimes determine which tests are present.
On narrow screens, scroll the table horizontally to read all four columns.

<div class="wse-contract-matrix" style="max-width:100%; overflow-x:auto;">
<div style="min-width:48rem;">

| Contract / claim | Design | Test entry | Coverage limit |
| --- | --- | --- | --- |
| CORE-DATA-01: shape, indexing | [Core Data Model](CoreDataModel.md) | `wse.core.map_invariants`, `wse.core.fast_indexing` | No invalid-index guarantee for unchecked access |
| CORE-DATA-02: value ownership | [Core Data Model](CoreDataModel.md) | [Map source review](../../../api/wse/data/wse_Map.h) | Not every move/view lifetime is tested |
| CORE-DATA-03: pixel layout | [Core Data Model](CoreDataModel.md) | `wse.core.pixel_storage`, `wse.core.image_channel_order` | Host value layout is not a general wire format |
| CORE-DATA-04: external rows | [Core Data Model](CoreDataModel.md) | `wse.core.image_interleaved` | Only supported interleaved formats |
| CORE-ALG-01/02: covariance, determinant, inverse | [Core Algorithms](CoreAlgorithms.md) | `wse.core.matrix_correctness` | Known values, pivoting and near-singular inputs; not arbitrary conditioning |
| CORE-ALG-03: point correspondence | [Core Algorithms](CoreAlgorithms.md) | [projection geometry](../../../test/characterization/projection_geometry_contract.cpp) | OUI-enabled gate; does not establish all degeneracy handling |
| CORE-ALG-04/05: orientation, accumulation | [Core Algorithms](CoreAlgorithms.md) | `wse.core.image_algorithms` | Literal rectangular vectors, rounding and rejected-shape state; no sum-overflow guarantee |
| CORE-ALG-06: Bayer/color formulas | [Core Algorithms](CoreAlgorithms.md) | `wse.core.image_algorithms`, `wse.tmr.camera_frame_ops` | Core vectors without Tmr; camera format coverage only with Tmr; not image-quality certification |
| CORE-SVC-01/02: waits and Timer | [Core Services](CoreServices.md) | `wse.core.runtime_contract`, `wse.core.worker_controller` | Interval-update timing and fixed-delay sequencing also require source review; not real-time bounds |
| CORE-SVC-03/04: Log dispatch and sinks | [Core Services](CoreServices.md) | `wse.core.runtime_contract` | Filter, self-removal, re-entry and exceptions; no exhaustive concurrent-unregister coverage |
| CORE-SVC-05: license state | [Core Services](CoreServices.md) | `wse.core.license_contract` | Free fallback from header/source review; not arbitrary-file or device-identity validation |
| CORE-SVC-06: device queries | [Core Services](CoreServices.md) | Linux branch of `wse.core.runtime_contract` | Metadata enumeration, not physical-device acceptance |
| GEF-BIN-01: wire bytes | [GEF](GefFileFormats.md) | `wse.gef.bin_contract` | Literal bytes, truncation and unknown tag; no big-endian certification |
| GEF-BIN-02: blocks and append | [GEF](GefFileFormats.md) | `wse.gef.bin_contract` | Read/rewrite and recovery covered by GEF-IO-04 |
| GEF-CSV-01/02: cells and settings | [GEF](GefFileFormats.md) | `wse.gef.csv_contract` | Simple LF/comma fixtures, not general quoted CSV |
| GEF-IO-04: bounded read and save recovery | [GEF](GefFileFormats.md) | `wse.gef.recovery_contract` | Exact/over budgets, rollback, short headers, rewrite/move, staged write/rename/flush failure and retry; no power-loss/OOM certification |
| GEF-RECON-03: independent codec | [GEF](GefFileFormats.md) | `wse.gef.reconstruction` | GEF plus Python interpreter; seven tags, empty/append, malformed inputs, CSV mapping; not blind third-party review |
| Worker lifecycle | [Thread Ownership](ThreadOwnership.md) | `wse.core.worker_controller`, `wse.core.runtime_contract` | Does not bound an arbitrary user callback |
| Strict and partial results | [Result](ResultContract.md), [XPT](XptTransport.md) | `wse.core.result_contract`, XPT transport contracts | Success/error access preconditions differ for partial results |
| XPT-OP-01 through XPT-HTTP-06: context, transport states and retry | [XPT](XptTransport.md) | [TCP](../../../test/characterization/xpt_tcp_loopback.cpp), [UDP](../../../test/characterization/xpt_udp_loopback.cpp), [Serial](../../../test/characterization/xpt_serial_port_contract.cpp), [HTTP](../../../test/characterization/xpt_http_contract.cpp), [Retry](../../../test/characterization/xpt_retry_policy_contract.cpp) | Loopback/PTY is not physical-device acceptance; deadline limitations also require source review |
| IUI-STATE-01 through IUI-LIFE-04: observations, polling and owner | [IUI](IuiKeyboard.md) | Lifecycle plus `wse.iui.keyboard_devices_contract` | Injected denial, reconnect, aggregation and allocation rollback; no kernel timing or complete callback race proof |
| TMR-OWNER-01 / TMR-OPEN-02: camera ownership and selection | [Tmr](TmrCamera.md) | `wse.tmr.webcamera_contract` and backend/source review | Fake backend; not every selection-cache or partial-open failure |
| TMR-CALLBACK-03: delivery versus streaming | [Tmr](TmrCamera.md) | `wse.tmr.camera_backend_contract`, `wse.tmr.camera_start_stop_contract` | Callable/launch/read faults, rollback, publication and read/stop/owner/self-stop races; not an exhaustive allocator or native fault sweep |
| V4L2 queue lifecycle | [Tmr](TmrCamera.md) | `wse.tmr.v4l2_lifecycle_contract` | Actual adapter with synthetic QBUF/STREAMON/STREAMOFF faults, retry and cleanup order; no device access |
| TMR-NATIVE-05: frame leases / wait budget | [Tmr](TmrCamera.md) | `wse.tmr.v4l2_read_contract`, `wse.tmr.media_foundation_frame_contract` | 27 V4L2 and 16 MF synthetic cases; not all allocations, libcamera or physical recovery |
| TMR-LIBCAMERA-06: request/mapping transactions | [Tmr](TmrCamera.md) | `wse.tmr.libcamera_resources_contract`, `wse.tmr.libcamera_manager_contract`, enabled `wse.tmr.camera_contract` | Production helpers with synthetic faults and real-adapter build/enumeration; no physical capture or libcamera-internal fault injection |
| TMR-MF-INIT-07: initialization ownership | [Tmr](TmrCamera.md) | `wse.tmr.media_foundation_resources_contract` | 29 owner/fault/attribute cases; no physical camera, native factory fault injection or cross-thread teardown certification |
| TMR-MF-STOP-08: native owner / flush completion | [Tmr](TmrCamera.md) | `wse.tmr.camera_owner_thread_contract`, `wse.tmr.media_foundation_read_state_contract`, opt-in camera smoke | Production dispatch/flush state, failure/timeout, restart and owner lifetime; native calls and physical unplug are not time-bounded/certified |
| TMR-FRAME-04: owned frame bytes | [Tmr](TmrCamera.md) | `wse.tmr.camera_contract` | Padded minimum-size storage and even NV12 extents; not decode/image-quality certification |
| Frame processing | [Tmr](TmrCamera.md), [Core](CoreDataModel.md) | `wse.tmr.camera_frame_ops` | Tested numerical inputs, not calibration accuracy |
| Windows packed capture and errors | [Tmr](TmrCamera.md) | `wse.tmr.directshow_format_contract`, `wse.tmr.media_foundation_error_contract` | Synthetic interval/readback and error decisions; no driver-rate or physical-removal certification |
| Spatial display RGB | [Hardware Validation](HardwareValidation.md) | `wse.oui.frame_contract`, opt-in `wse.oui.d3d12_display_mode_restoration` | 36 pixels, RGB tolerance two, alpha ignored; contract rejects black/channel swaps/reversal; physical result is recorded separately |
| OUI-OWNER-01 / OUI-RESOURCE-02: resource identity and lifetime | [OUI](OuiRenderer.md) | `wse.oui.renderer_identity`, `wse.oui.renderer_generation`, lifetime/source ownership checks | Foreign/stale rejection, live-resource preservation, move transfer, all configuration fields, parallel/saturating generation allocation; not a forged-handle or native allocation-failure proof |
| OUI-SUBMIT-03 / OUI-SURFACE-04: submit and presentation | [OUI](OuiRenderer.md) | Dynamic-mesh golden, window/resize gates and source review | Failed submit/wait cleanup and physical display modes require separate evidence |
| OUI-PROJ-01/02/03: coordinates, pixels and pass sequence | [Projection](OuiProjection.md) | Geometry/goldens, binding scale two, `wse.oui.projection_cleanup_contract` | 117 cleanup fault cases; native driver faults and arbitrary GPU hangs remain outside this fixture |
| BIND-STRUCT-01 through BIND-DEVICE-04 | [Language Bindings](LanguageBindings.md) | `wse.binding.*`, language lifecycle/stress | Enabled languages only; queued callback, managed exceptions and concurrent close limits require adapter review |
| BIND-TRANSFER-05: transport error values | [Bindings](LanguageBindings.md), [XPT](XptTransport.md) | `wse.binding.transfer_failure_contract`, Python/Node XPT and Java contract | Native synthetic counts and real UDP truncation; no whole-heap or physical serial certification |
| CABI-OWNER-01 through CABI-CALL-05 | [C ABI](CAbiContract.md) | `wse.capi.abi_snapshot`, `wse.capi.c_layout_snapshot`, `wse.capi.allocation_contract`, `wse.capi.runtime_contract`, `wse.binding.dotnet_contract` | 16 structs / 87 fields, Core allocation sweeps, lease release ordering and 64 Dispose races; excludes all-device rollback |
| CABI-ADOPT-06 / BIND-DEVICE-04 | [C ABI](CAbiContract.md), [Bindings](LanguageBindings.md) | `wse.binding.dotnet_ownership_contract`, `wse.capi.camera_delivery_contract` | 53 managed rollback injections, 8 native-worker callback cases, 5 native delivery cases; synthetic provider, no CLR heap exhaustion or physical camera |
| CABI-TRANSFER-07 | [C ABI](CAbiContract.md), [XPT](XptTransport.md) | `wse.capi.transfer_contract`, `wse.capi.udp_progress_contract`, `wse.binding.dotnet_transfer_contract`, `wse.binding.dotnet_contract` | 27 native conversion cases, 25 managed cases, real UDP/TCP loopback; positive failed-send progress is synthetic, no physical serial or heap-exhaustion proof |
| PKG-GRAPH-01 through PKG-BINDLOAD-06 | [Build Packaging](BuildPackaging.md) | [package verifier](../../../test/package/verify_public_package.py), [external consumer](../../../test/consumer/CMakeLists.txt), `wse.binding.installed_libcamera` | Must run against the selected installed artifact; manifest HEAD tree identity excludes uncommitted bytes; JNI/C ABI load checks do not exercise managed methods |
| WALK-IMAGE-01 through WALK-MEASURE-05 | [Developer Walkthrough](DeveloperWalkthrough.md) | `wse.oui.backend_projection_golden`, `wse.tmr.camera_backend_contract`, `wse.tmr.camera_frame_ops`; opt-in `wse.bench.indexing` | Guided examples and workload accounting; no blind acceptance, peak-memory/FPS guarantee or physical-device evidence |

</div>
</div>

These mappings cover the listed requirements; they do not claim complete requirement coverage. For each new
contract, add a stable ID or an unambiguous section, link its public symbols, and identify
the assertions that verify it. Mark missing tests or unresolved behavior explicitly.

## Reconstruction acceptance

For a bounded change, a developer who has not inspected its implementation should:

1. Derive the data representation, owner graph and successful call sequence from the design.
2. Reproduce a small reference input with its expected output and numerical tolerance.
3. Explain invalid input, partial failure, cancellation and shutdown where applicable.
4. Build a minimal independent reader, pure transformation or fake backend for that scope.
5. Compare against the specified vectors and relevant contract tests.

Questions that require guessing an observable behavior are documentation defects. An independent
implementation need not copy private class names or performance choices. No claim that this
acceptance has been completed is made merely by adding this checklist.

The bounded GEF exercise is implemented by [the independent reference codec](../../../test/reconstruction/gef_reference.py)
and its public-API bridge, registered as `wse.gef.reconstruction`. It verifies two-way BIN/CSV
interoperability beyond WSE's own round trip. The author has inspected WSE sources, so blind
first-time-developer acceptance remains separate; passing this gate must not be reported as that review.
For Core, reproduce row indexing and padded RGB8 copy, then the covariance, rectangular
orientation and nonuniform Bayer vectors in CoreAlgorithms. Running the library's contract
tests is not independent reconstruction; numerical, camera and renderer reconstruction
remains a separate acceptance exercise. For Tmr, reconstruct the timeout/error/callback-exception
state table and padded-frame sizes. For OUI, reproduce the two-cell index sequence, the blend
example and scale-two quantization, and explain each cleanup failure before implementing an adapter.

### Execution order and acceptance record

Schedule HTML inspection in the same batch as a detailed-design change, after paired edits and
automated document checks. Generate the complete Developer Public HTML in both languages, then
inspect navigation, code/text diagrams, long tables and numerical examples in a browser. A successful
`--check` or warning-free generation alone is not display acceptance.

Before declaring a bounded design effort complete, perform first-time-developer reconstruction
unless its owner explicitly waives that gate for the stated scope. Freeze the permitted designs,
public headers, input vectors and prepared packages with
hashes. Assign a reviewer who has not inspected the relevant implementation; provide tasks and
observable acceptance criteria without implementation/tests/solutions. Record reviewer identity,
prior exposure, environment, artifacts, questions, guesses, results and unresolved issues. Author
self-review and an AI exercise are separate evidence and do not replace human first-reader acceptance.
An unassigned reviewer remains pending unless explicitly waived, never passed. Record an owner's
waiver with its date, scope, reason, retained evidence and unverified limits in the acceptance record.
Mark that gate skipped by owner decision; do not relabel an AI result as a human pass. The waiver
does not waive document/example gates, unresolved defects or separately authorized hardware gates.

Use bounded GEF/Core/Projection tasks above, a C ABI two-call/TLS/ownership client, and an installed
Core consumer. Questions revealing missing observable behavior block that scope's acceptance.
Amend both languages, regenerate/reinspect HTML and repeat affected tasks with an unexposed reviewer
when the solution has been revealed. Physical-device work stays a separately authorized gate.

## Automated document and example validation

`wse.documentation.contract` checks paired metadata, required documents, local file links,
options/presets and the presence of five quick-start sources. It does not prove translation
equivalence, anchor validity, numerical correctness or the absence of contradictory prose.
`wse.documentation.quickstart_*` builds/runs the applicable examples when each runtime is enabled.
Design changes also require the relevant contract tests and `git diff --check`.

Use the existing [Build Guide](../../en/BuildGuide.md) and
[Doxygen Guide](../../../doxy/README.md) commands. Configuration remains offline; prerequisite
provisioning is separate. Windows and Linux validation is required for portable test/code or
documented command changes. Header, ownership, packaging or platform changes require SHARED
and STATIC gates. Physical devices remain opt-in; a skip never becomes a hardware pass.

## Maintenance

Update the English design, Japanese translation, Manifest, index and change history together.
Keep a single detailed source for shared result, ownership and hardware rules; link from the
component and state any exception. Do not duplicate historical test counts in normative clauses.
The authoring template is in [Documentation Generation](DocumentationGeneration.md#detailed-design-authoring).

Use this change-impact checklist when maintaining a detailed design:

| Changed contract | Review together | Relevant evidence |
| --- | --- | --- |
| Layout, format or algorithm | Public declarations, Core/component design, literal vectors, language conversions | Layout/numerical/format contracts; tolerance and invalid-input cases |
| Ownership, callbacks or shutdown | Component state table, Thread Ownership, managed owners and error boundary | Lifecycle/concurrency contracts; keep untested failure paths explicit |
| Public ABI or package use | C ABI/Language Bindings, Build Packaging, user guide, installed consumer | ABI snapshot/runtime, SHARED/STATIC and package/consumer gates |
| Example, navigation or rendering | Both languages, Manifest, index and generated HTML | Documentation/quick-start gates and browser inspection |

If a behavior change is deferred, retain its current limitation, affected contract, proposed next
action and required future evidence in the tracking record. Documentation completion is not proof
that every implementation limitation has been repaired.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
