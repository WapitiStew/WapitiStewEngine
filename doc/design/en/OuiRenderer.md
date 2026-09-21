# WSE OUI Renderer Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document is the normative design for the renderer and display boundary owned by `wse::oui`.
The backend-independent move-only `Renderer` facade exposes no native GPU handles and connects
Windows Direct3D 12 and Linux Vulkan 1.2. The same portable contract covers offscreen, window,
direct-display, display/mode enumeration, resize, fullscreen, hotplug, exit restoration, texture
upload, indexed-mesh drawing, and readback. `Renderer` is the only renderer, and no API exposes a
native display or DXGI handle.

## Current support matrix

| Target | Renderer status | Surface status | Library forms |
| --- | --- | --- | --- |
| Windows x86-64 / MSVC | D3D12 clear, upload, indexed textured mesh, readback, and WARP Golden verified | Offscreen, Win32 window, interactive resize, borderless/display-mode fullscreen, hotplug, direct display, and restoration implemented; physical results are scoped to the recorded equipment and modes | Shared, static |
| Linux x86-64 / GCC | Vulkan 1.2 offscreen/projection Golden verified with Mesa llvmpipe under WSL2 | WSLg Wayland window/fullscreen/resize/present verified; DRM/KMS implemented but not run on a real display | Shared, static |
| Linux ARM64 / Raspberry Pi | Pi 4 native Vulkan offscreen evidence accepted | Physical presentation remains unrun; see the scoped hardware record below | Shared, static |

Pi 4 has accepted native shared/static builds and GPU-backed offscreen Vulkan projection results.
Its HDMI, window-presentation, and DRM/KMS gates remain `HARDWARE_NOT_RUN`; Pi 5 is also
`HARDWARE_NOT_RUN`. See [Hardware Validation](HardwareValidation.md) for the accepted scope.
Windows WARP evidence must not be represented as physical-GPU, Linux, or Raspberry Pi renderer certification.

## Package and public-header boundary

- `WSE::Oui` is optional and depends on `WSE::Core`.
- Windows and Linux provide OUI-only shared and static install packages.
- `Renderer` uses PIMPL and exposes no Windows, DXGI, D3D12, WRL, COM, or `platform/oui/win` type.
- Texture, mesh, surface, and fence identities are generation-bearing opaque handles, not native handles.
- Calls to one `Renderer` instance must currently be serialized by the caller; thread safety is not guaranteed.
- The current public surface uses typed opaque handles; native `void*` GPU handles are not its API.

## Lifecycle contract

- A default-constructed renderer is uninitialized.
- Reinitialization compares all five requested fields: backend, software-adapter,
  prefer-display-adapter, validation, and the exact `adapter_name` string. An identical request
  is idempotent; a different valid request returns `AlreadyInitialized` and preserves the backend
  and its resources. No adapter-name normalization or `Automatic`/explicit-backend equivalence
  is applied. Invalid arguments retain validation precedence. Shut down before changing selection.
  The facade copies allocating configuration data before initializing the backend and moves it
  into the retained configuration only after success.
- `shutdown()` is safe and idempotent while uninitialized, destroys resources, and permits reinitialization.
- Copy is forbidden. Move transfers backend ownership and leaves the source safely uninitialized.
- Operations with otherwise valid arguments before initialization return `NotInitialized`.
  Facade argument validation can take precedence; an empty handle can return `InvalidArgument` first.
- `Automatic` selects D3D12 on Windows and Vulkan 1.2 on Linux. Requesting `Vulkan12` on Windows returns
  `UnsupportedBackend`; it does not silently fall back to D3D12.

`Renderer` is not copyable. It is initialized, used, and shut down; `shutdown()` is idempotent and
a shut-down renderer can be initialized again.

### Implementation and owners: OUI-OWNER-01

| Layer | Source | Responsibility |
| --- | --- | --- |
| Facade | [Renderer.cpp](../../../core/oui/renderer/Renderer.cpp) | Unique Impl and backend; argument/lifecycle checks, platform selection and delegation |
| Values | [RendererTypes.cpp](../../../core/oui/renderer/RendererTypes.cpp) | Pure description validation and frame sizing; no GPU ownership |
| Internal seam | [RendererBackend.h](../../../core/oui/renderer/RendererBackend.h) | Backend interface, not an installed extension interface |
| Windows | [D3D12RendererBackend.cpp](../../../platform/oui/win/renderer/D3D12RendererBackend.cpp) | COM owners, resource maps, queue/fence, pending submissions and Win32 surfaces |
| Linux | [VulkanRendererBackend.cpp](../../../platform/oui/linux/renderer/VulkanRendererBackend.cpp) | Vulkan allocations, resource maps, queue/commands, Wayland/DRM surface adapters |
| Linux submission owner | [VulkanSubmission.h](../../../platform/oui/linux/renderer/VulkanSubmission.h) | Internal native-call seam and RAII owner for command/fence and transient resources; not installed |
| Frame bridge | [RendererFrameOps.cpp](../../../core/oui/renderer/RendererFrameOps.cpp) | Owned Core image conversion; no device lifetime |

```text
Renderer unique Impl -- owns --> backend
                                  +-- owns --> texture / mesh maps
                                  +-- owns --> surface map -- owns --> surface textures
                                  +-- owns --> queue / completion tracking
                                                 +-- retains --> submitted resources
caller opaque handle -- non-owning lookup --> resource / surface maps
```

Moving the facade transfers this graph. A moved-from Renderer can initialize a new Impl; destruction
of an existing destination releases its former backend. Callers serialize all operations on one
instance. ComPtr owns Windows objects; Vulkan native allocations remain inside the adapter.
The Windows ownership scan rejects manual COM Release/Detach as a resource-release mechanism.

### Handle identity and lifetime: OUI-RESOURCE-02

Handle `valid()` only checks nonzero value and generation. It does not prove that a resource exists
or belongs to this renderer. Within one active backend, lookup checks the resource map and generation;
destroyed or unknown identities return `ResourceNotFound`, and IDs advance as resources are created.
Surface-owned textures reject direct destruction with `ResourceInUse`.

Each backend initialization reserves a nonzero 32-bit lifetime generation from the internal
[RendererIdentity.h](../../../core/oui/renderer/RendererIdentity.h) source. It is shared by renderer
instances in one loaded WSE runtime and never reset by shutdown. Reservation uses a relaxed atomic
compare/exchange loop for uniqueness, not for publishing renderer state. Failed initialization may
consume a generation. After `UINT32_MAX` reservations the source remains exhausted: initialization
returns `Resource/ResourceExhausted` before native allocation rather than wrapping to an old value.

Every texture, mesh, surface, surface attachment and fence carries this generation. All backend
lookups compare it before accepting the local value. Thus `{value=1, generation=A}` from another
renderer or a prior initialization cannot address `{value=1, generation=B}`. On an initialized,
ready backend, a foreign/stale identity returns `ResourceNotFound`; existing argument validation,
lifecycle and pending-submission error precedence remains unchanged. Rejected identities do not
modify the corresponding live resources. Surface textures retain the surface owner's generation.

Move construction and assignment transfer the backend generation with its resources: source handles
remain usable on the destination. Move assignment retires the destination's former resources;
their handles are rejected. Reinitializing the moved-from object reserves another generation.
These are opaque identities, not unforgeable security tokens. Do not synthesize fields, persist
handles, share them across independently loaded WSE copies, or use them after runtime unload.
Discard handles at shutdown even though later accidental reuse is detected. The existing 64-bit
resource/fence-counter exhaustion paths are not certified by the 32-bit generation boundary test.

Shutdown releases surfaces/swapchains, resource maps, pipelines/descriptors and native device/window
objects in adapter-specific dependency order. D3D12 signals/waits with its internal timeout but
shutdown does not report a failed wait. Vulkan retains pending work until `vkDeviceWaitIdle` returns
success or DeviceLost; other errors are retried with a 1 ms pause, without releasing resources.
That native wait has no finite timeout argument, and persistent errors can prevent shutdown from
returning. A void shutdown is not GPU-completion certification or a universal bounded stop.
Repeated shutdown is safe.

## Data and resource contract

### Frame

- `sRendererFrameDescription` contains an extent, portable pixel format, and byte row pitch.
- A zero row pitch selects the minimum packed pitch; an explicit pitch must be at least that value.
- Unknown formats, empty extents, short pitches, and size overflow are validation errors.
- Frame data size must equal `effectiveRowPitch() * height`, including final-row padding.
  R8 uses one byte/pixel, RGBA8/BGRA8 four, and RGBA16 eight. BGRA8 3 x 2 with pitch 16 requires
  exactly 32 bytes: 31 and 33 both fail, unlike Camera's minimum-size rule. Valid storage does
  not imply that every backend supports the format as a render target.
- `readTexture()` removes backend padding and returns a packed CPU frame.

<a id="frame-and-core-image"></a>
### Frame and Core image

`RendererFrameOps.h` converts a frame to a Core `wse::Image_` and back, and `Renderer` carries the
same conversion on `uploadTexture()` and `readTexture()` so a caller can work in image types
throughout. The destination type names the format, which is how a channel order survives the
boundary.

| Renderer format | Core format | Image type |
| --- | --- | --- |
| `R8Unorm` | `CH1D8` | `img1c08_t` |
| `Rgba8Unorm` | `CH4D8` | `img4c08_t` |
| `Bgra8Unorm` | `BGRA4D8` | `img4c08_bgra_t` |
| `Rgba16Unorm` | `CH4D16` | `img4c16_t` |
| `Rgba16Float` | none | none |

Every colour format here keeps four channels, so an alpha channel is never dropped on the way
through, and a `Bgra8Unorm` frame becomes a BGRA-ordered image rather than being reordered
silently; `wse::convertChannelOrder()` is the explicit route between the two orders.

`Rgba16Float` has no image type because Core's `Pixel_` is limited to an integer channel, and a
half-precision value is not one. It is not reinterpreted as `Rgba16Unorm`, since the bit patterns
denote different numbers; such a texture reports `Unsupported`/`UnsupportedFormat` and is read with
the `sRendererFrame` overload instead.

Reading folds any row pitch away. Writing sets a zero row pitch, which leaves the packed row to the
backend's own alignment rule. A sixteen-bit sample travels little endian. Every conversion returns
`RendererStatus` or `RendererResult`: the Core image types report an allocation failure by throwing,
and each conversion catches that and reports `Resource`/`ResourceExhausted`, so no exception crosses
the OUI boundary.

### Texture

- A descriptor contains extent, format, usage, initial state, and mip count.
- State must agree with usage. Destroyed or unknown handles return `ResourceNotFound`.
- The current D3D12 adapter creates one-mip RGBA8/BGRA8 render-target and/or sampled textures.
  R8 is available for non-render-target use, and projection alpha maps read its red channel.
- `uploadTexture()` requires `Sampled` and `TransferDestination` usage. The frame extent and format
  must match exactly, and its byte count must match the validated row pitch. Upload completion is
  represented by a fence; the source CPU data may be released after the call returns.
- An offscreen surface owns its texture. Direct destruction returns `ResourceInUse`; surface
  destruction releases both objects.

### Mesh

- `sMeshDescription` defines triangle-list/triangle-strip topology, 2D position-plus-UV vertices,
  and an index array.
- Vertex values must be finite, indices must be in range, and topology count rules must hold.
- The D3D12 adapter creates indexed meshes and draws them with its internal position/UV
  shader, nearest/linear clamp samplers, and RGBA8/BGRA8 replace/straight-alpha pipeline states.
  Position is normalized device coordinates; UV is normalized texture space.
- `updateMesh()` replaces vertex, index, and topology data together under the same valid opaque
  handle. It builds the complete replacement before swapping, preserves the old mesh on failure,
  and returns `ResourceNotFound` for destroyed or unknown handles.
- Submitted mesh and texture resources remain alive until the returned fence completes, including
  when the caller destroys their opaque handles immediately after submission.
- A submission made before a mesh update retains its old resources through completion; a later
  submission resolves the same handle to the replacement resources.

### Render pass and fence

- The current contract has one color attachment and supports whole-attachment or rectangular
  load/clear operations, indexed textured-mesh draw commands, and a final state.
- Clear components must be finite and between zero and one; render areas must fit the attachment.
- A draw command requires valid mesh and sampled-texture handles. It can also bind an optional alpha
  texture, select nearest/linear sampling and replace/source-alpha blending, set normalized opacity,
  and apply four-border linear/smoothstep edge blend. Sampling the active color attachment is rejected.
  Discard remains unsupported.
- Projection-specific semantics are defined by the [OUI Projection design](OuiProjection.md).
- `Present` is valid only as the final state of a current backbuffer owned by a presentable
  Window or DirectDisplay swap chain. Offscreen and standalone textures are rejected.
- Successful submission returns a fence. The backend retains command objects and referenced
  resources until completion.
- Timeout zero polls. `UINT32_MAX` is not an infinite-wait sentinel and is rejected by the finite-timeout contract.

### Submission sequence and backend differences: OUI-SUBMIT-03

The facade validates descriptions, handles and finite timeout arguments before delegation. The
backend resolves identities and checks usage/extent/format before encoding work. Upload requires
matching frame/texture descriptions, packs CPU rows into staging storage, copies to the GPU and
leaves the texture shader-readable. Readback requires `TransferSource`, copies to staging/readback
storage and returns packed CPU rows. Vulkan restores the source layout; D3D12 leaves it in
CopySource state and later operations transition as needed. A render pass resolves its
attachment and draw resources, clears/loads, draws in order and applies the requested final state.

| Stage | D3D12 | Current Vulkan |
| --- | --- | --- |
| Encode | Allocator/list plus temporary upload/readback/descriptor objects | Command buffer plus staging allocations |
| Submit | Execute command list, then signal an increasing fence value | End commands, create VkFence, queue submit, wait using readback caller timeout or internal 30,000 ms for other submissions |
| Successful return | Work may still be pending | `submitAndWait` has already observed completion and returns a recorded completed fence |
| Retention | Pending record holds command objects, descriptors and referenced ComPtr resources until completion | A single pending RAII owner retains unresolved command/fence/transients; resource maps cannot be mutated until completion |
| Wait | Wait/poll the native queue fence | Check the completed-fence set |

The portable fence contract permits asynchronous execution; it does not promise a nonblocking
submission. Vulkan `readTexture(timeout_ms)` passes the finite caller value to the native fence wait
as 64-bit nanoseconds (`uint64_t(timeout_ms) * 1,000,000`). Zero performs one completion poll after
submission; it can return `Timeout/TimedOut` without a CPU frame. `UINT32_MAX` remains rejected by
the facade. Allocation, command recording and CPU copying are outside that wait budget, so it is
not an end-to-end deadline. Other Vulkan submissions retain the internal 30,000 ms wait.

Destroying a handle after successful submission removes the lookup identity, not resources needed
by pending D3D12 work. Updating a mesh creates replacement resources before swapping the map entry;
old pending submissions retain the previous version. Successful Vulkan work is already complete.

The Vulkan failure state machine is:

```text
owned recording -> end/create fence -> queue submit -> wait
  pre-submit failure: release local owner                 |
  queue OOM: no enqueue, release local owner              +-- success: release local owner after CPU use
  queue DeviceLost/unknown: retain owner, require shutdown +-- timeout/other error: move owner to pending
pending -> next mutating API polls fence with timeout 0
  timeout: ResourceInUse, no mutation -> later success: release once, execute the requested operation
  other wait error: retain, report native error; DeviceLost remains terminal until shutdown
shutdown -> device idle success/DeviceLost -> release pending -> surfaces/maps -> device
```

`sVulkanSubmission` is allocated before native work and moved into a pre-existing `unique_ptr` slot
without allocation. It owns the command buffer, fence and the operation's upload/readback buffer,
memory, descriptor pool/framebuffer, or unpublished texture image/view/memory. Referenced textures,
meshes, pipelines and acquisition semaphores stay in their backend maps. While unresolved, **all**
resource creation, upload, draw, readback, destruction, mesh update, surface resize/mode change,
presentation and event processing first poll the pending fence. A timeout returns
`Lifecycle/ResourceInUse` without invalidating handles or changing resources; retry that rejected
operation later. Getters and waits on previously returned completed fences remain available and do
not reap pending work. A failed operation itself returns no fence, and its eventual readback frame
is discarded; after recovery a new readback performs a new copy.

On successful queue submission, layout/state changes and acquisition-semaphore consumption are
recorded before constructing portable results, even if the wait fails or result allocation throws.
Pre-enqueue errors leave those states unchanged. End-command/fence-creation failures and
QueueSubmit host/device OOM clean up locally. Unknown QueueSubmit failures and DeviceLost require
shutdown/reinitialization; wait host/device OOM and other nonterminal wait errors retain the owner
and may recover on a later successful poll. This follows Vulkan's
[QueueSubmit failure rules](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html)
and [lost-device lifetime rules](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-lost-device).

Submission failure is not an atomic rollback: D3D12 Signal may fail after ExecuteCommandLists, and
Vulkan wait may fail after queue submission. Do not infer that failure means no GPU execution or
automatically replay a drawing transaction. The Result factories enforce value/error exclusivity;
they do not establish complete exception or allocation-failure rollback for every adapter operation.
The pending owner remains safe even if constructing an error throws `std::bad_alloc`.

### Surface

The normal presentation sequence, OUI-SURFACE-04, is:

```text
createSurface -> processSurfaceEvents -> getSurfaceTexture
  -> executeRenderPass(final_state=Present) -> presentSurface
  -> processSurfaceEvents -> reacquire current texture -> next frame
```

The surface owns its buffers. Reacquire the current texture after presentation and after any
extent-changing resize/mode transition; retain application-owned source textures separately.

- The contract distinguishes `Offscreen`, `Window`, and `DirectDisplay`.
- The D3D12 adapter implements offscreen and internally owned Win32 window surfaces. A window surface
  has an initial extent, a UTF-8 title, visibility and vertical-sync settings, and two or three
  flip-discard backbuffers. The caller pumps events with `processSurfaceEvents()` and presents the
  current backbuffer with `presentSurface()` after transitioning it to `Present`.
- `resizeSurface()` resizes a windowed surface's owned window and swap chain together.
  `setSurfaceWindowMode()` targets an ID from `enumerateDisplays()` for borderless fullscreen and
  restores the saved style, placement, visibility, and render extent on return to windowed mode.
  `getSurfaceState()` exposes the current extent, mode, and target display ID.
- On D3D12, before resize or mode transition, the backend completes the GPU queue with a finite timeout and
  rebuilds swap-chain buffers when the extent changes. A successful extent-changing transition
  invalidates old surface-texture handles, so callers reacquire with `getSurfaceTexture()`. Repeating
  the same extent/mode/display is idempotent and preserves the current backbuffer handle.
- Borderless fullscreen changes only window style and placement. It does not change OS display mode,
  refresh, rotation, or power. Native-transition or buffer-rebuild failures trigger rollback, and a
  rollback failure is exposed as `BackendFailure`.
- Windows reports interactive frame resize, close, and display-topology changes through
  `pollSurfaceEvents()`. `DisplayModeFullscreen` snapshots the native mode, tests before applying it,
  and restores it on windowed return, surface destruction, renderer shutdown, or transition failure.
  `DirectDisplay` owns the same restoration transaction.
- Linux window surfaces use Wayland/xdg-shell, synchronize swapchain extent with configure events, and
  connect borderless fullscreen/windowed return, close, output hotplug, and presentation to the same API.
- Linux direct display snapshots the DRM connector/CRTC and uses `VK_EXT_acquire_drm_display` plus
  `VK_KHR_display`. Destruction and failure release the Vulkan display and restore the original CRTC/mode.
  Missing DRM nodes, permissions, or extensions produce explicit errors or a hardware-test skip.
- External/native-window injection and concurrent ownership of one display by multiple renderers remain unsupported.
- Windows shaders are compiled at runtime through the Windows SDK `d3dcompiler` system component.
  This is an exported static-link requirement, not a vendored third-party dependency.

### Display enumeration

- `enumerateDisplays()` returns a read-only snapshot of active desktop-attached outputs across all
  hardware adapters.
- `sDisplayDescription::id` combines a backend-qualified adapter ID and OS display name. It is stable
  for repeated enumeration of the same topology, but is not an EDID, serial number, or persistent
  hardware identity across reboot or hotplug.
- Callers discard the old snapshot and enumerate again after hotplug, adapter reconfiguration, or a
  display-setting change.
- A descriptor contains adapter identity/name, display name, signed desktop position, output extent
  in desktop coordinates, current/supported modes, rotation, primary status, and compatibility with
  the active renderer adapter.
- Refresh is a numerator/denominator rational. The denominator is nonzero; numerator zero means the
  backend could not determine refresh.
- The mode list always contains the current mode. If DXGI cannot provide the complete supported list,
  only current mode is retained and `modes_complete=false`; no guessed list is reported as complete.
- Displays sort by ID. Modes sort and deduplicate by extent, refresh, scan method, and format.
- A WARP renderer may enumerate system hardware outputs, but all have `renderer_compatible=false`.
- Wayland compositor outputs use `wayland:` IDs; connected DRM/KMS connectors use `drm:` IDs. Both are
  topology-scoped rather than persistent hardware identifiers.
- Enumeration changes no mode, fullscreen state, window position, or display power state.

## Error contract

Portable control flow uses `eRendererErrorCategory` and `eRendererErrorCode`. Categories distinguish
validation, lifecycle, resource, execution, timeout, unsupported, and backend failures. `native_code`
and messages are diagnostic only and must not drive cross-backend behavior. Unsupported operations
must not report success through an empty renderer, dummy frame, or silent fallback.

## Deterministic renderer baselines

The Windows reference test selects D3D12 WARP through the public `Renderer` API and creates a 10 x 10
RGBA8 offscreen surface. Five render passes clear a black border and red, green, blue, and white 4 x 4
quadrants. After fence waits, CPU readback must match `test/golden/oui_d3d12_quadrants.ppm` exactly in
RGB, with alpha 255 for every pixel and RGBA FNV-1a `0xde58c7e12a2fca45`.

The mesh Golden uploads a 2 x 2 red/green/blue/white RGBA texture, draws an indexed fullscreen quad
to an 8 x 8 offscreen target with point sampling, and requires exact quadrant pixels and RGBA FNV-1a
`0x5854021a152e5ba5`. It also destroys submitted source/mesh handles before fence completion to verify
backend retention.

The projection Golden converts a legacy mesh, linearly scales a 2 x 2 RGBA texture to 4 x 4, samples
a non-uniform alpha map, applies opacity 0.75 and straight-alpha composition, and compares with a CPU
reference within one RGBA8 LSB. Its exact WARP RGBA FNV-1a is `0xce4b43253436392e`.

The supersample Golden applies a single-channel R8 alpha map while drawing the same projection into
an 8 x 8 intermediate RGBA8 target, then linearly downsamples to 4 x 4. Every channel is within one
LSB of a CPU two-stage reference including intermediate quantization. Its exact WARP RGBA FNV-1a is
`0xa3ccfe45a48e2d20`.

The edge/multi-source Golden draws a red right-edge fade followed by a blue left-edge fade into an
8 x 4 target. Every channel is within one LSB of a CPU reference covering smoothstep, opacity, draw
order, and RGBA8 quantization. Its exact WARP RGBA FNV-1a is `0x17b97761907effd5`.

The dynamic-mesh Golden updates one handle from a left-half shape to a right-half shape before
waiting for the first fence. It submits each version to a separate 8 x 8 target and waits only after
destroying the handles. The old left shape exactly matches RGBA FNV-1a `0xbbb816f5e80169e5`; the new
right shape matches `0xb39c749492b617e5`.

The hidden-window contract creates a 16 x 12 swap chain, clears and presents it, waits for completion,
and verifies backbuffer advancement and lifecycle behavior. These tests cover WARP only. They do not
certify color conversion, exclusive fullscreen/direct display, a physical monitor, or a physical GPU.

The display-enumeration contract queries system hardware adapters through a WARP renderer and checks
two same-topology snapshots, unique IDs, deterministic order, mode invariants, and WARP incompatibility.
The number of active displays a host reports is not a Golden, and a headless zero-display snapshot is
valid.

The resize/borderless contract changes a hidden WARP window from 160 x 120 to 320 x 180 and checks old
backbuffer invalidation, clear/readback at the new extent, same-request idempotence, and an unchanged
failure for an invalid display ID. On hosts with a display, it enters borderless mode at the primary
desktop extent, restores the 320 x 180 windowed state, and verifies that display mode, placement, and
desktop extent are unchanged. A headless host skips only the positive borderless path.

The shared backend projection Golden passes the same 2 x 2 RGBA source, fullscreen mesh, nearest
sampling, and 4 x 4 target to D3D12 WARP and Ubuntu Vulkan. Every channel must match at zero LSB
difference, with RGBA FNV-1a `0xe0f9517b5bf10dc5` on both backends.

## Verification contract

| Contract | Test / review entry | Limit |
| --- | --- | --- |
| OUI-OWNER-01 / OUI-RESOURCE-02 | [Identity](../../../test/characterization/oui_renderer_identity_contract.cpp), [generation boundary](../../../test/characterization/oui_renderer_generation_contract.cpp), [lifetime](../../../test/characterization/oui_renderer_lifetime_contract.cpp), `wse.oui.resource_ownership` | D3D12/Vulkan offscreen: foreign/stale textures, meshes, surfaces, attachments and fences; move, unchanged live pixels/extent, all configuration fields. Parallel generation reservation and terminal saturation; no forged-handle security or exhaustive native allocation failure proof |
| Description/frame validation | [Renderer values](../../../test/characterization/oui_renderer_contract.cpp), [frame images](../../../test/characterization/oui_renderer_frame_image_contract.cpp) | Storage/format vectors; not every native allocation failure |
| OUI-SUBMIT-03 | `wse.oui.projection_dynamic_mesh_golden`, [Vulkan submission faults](../../../test/characterization/oui_vulkan_submission_contract.cpp) (`wse.oui.vulkan_submission_contract`) | Old/new mesh pixels, native timeout arguments, pre/post-submit failures, retained owners, guarded mutation, recovery, allocation failure and shutdown; injected DeviceLost is not physical GPU-failure certification |
| OUI-SURFACE-04 | D3D12 window/resize tests, `wse.oui.vulkan_wayland_surface` | Environment-dependent presentation; physical display modes/DRM remain separately opt-in |

Use SHARED and STATIC gates, plus installed consumers when packaging changes. Test counts are run
evidence, not normative constants. [Design Verification](DesignVerification.md) maps cross-component
acceptance; [Hardware Validation](HardwareValidation.md) is authoritative for Pi 4 and physical gates.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
