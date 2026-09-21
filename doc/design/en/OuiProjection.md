# WSE OUI Projection Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document is the normative design for portable projection drawing owned by `wse::oui`.
`ProjectionPipeline` maps ordered projection layers to the backend-independent `Renderer` contract.
Each layer combines one mesh, one color texture, an optional alpha-map texture, a sampling filter,
a normalized opacity, and an optional edge blend. The contract contains no native D3D12, Vulkan,
window, or display handle.

Projection uses `ProjectionPipeline`, `ProjectionMeshAdapter`, and the portable renderer's opaque
resources. A projection mesh's `src` uses source-pixel coordinates.

## Coordinate and mesh contract

The public headers are [ProjectionMeshAdapter.h](../../../api/oui/renderer/ProjectionMeshAdapter.h)
and [ProjectionPipeline.h](../../../api/oui/renderer/ProjectionPipeline.h). Their implementation is
[ProjectionMeshAdapter.cpp](../../../core/oui/renderer/ProjectionMeshAdapter.cpp) and
[ProjectionPipeline.cpp](../../../core/oui/renderer/ProjectionPipeline.cpp), using the private
[execution/cleanup owner](../../../core/oui/renderer/ProjectionExecution.h); native drawing remains
behind [Renderer](OuiRenderer.md). The mesh adapter is a value transformation; the pipeline borrows
the renderer and owns only temporary supersampling resources during execute.

- `sRendererVertex2D::position_x/position_y` are D3D/Vulkan-compatible normalized device coordinates.
  X runs from -1 at the left to +1 at the right. Y runs from +1 at the top to -1 at the bottom.
- `texture_u/texture_v` are normalized source coordinates. U and V run from zero at the top-left to
  one at the bottom-right.
- `ProjectionMeshAdapter::createMesh()` converts legacy pixel-space `float64_mesh2d` data into this
  coordinate system. Source and target extents must each be at least 2 x 2 pixels.
- Every legacy grid cell becomes two stable triangle-list primitives. Empty, inconsistent,
  non-finite, invalid-index, or overflowing mesh data returns a portable validation error.
- Finite positions outside NDC and UVs outside zero to one remain valid. Positions are clipped by the
  rasterizer and UVs reach the clamp sampler, allowing correction meshes to extend beyond a screen edge.
- `Renderer::updateMesh()` replaces vertex, index, and topology data together under the same opaque
  handle. The contract provides no partial update.
- The renderer builds the complete replacement before swapping it in. Failure preserves the old
  mesh, and only submissions made after a successful update observe the new mesh.

### Exact conversion and triangulation: OUI-PROJ-01

For source extent `(Ws,Hs)`, target extent `(Wt,Ht)` and a grid point with source `(sx,sy)` and
destination `(dx,dy)`, compute in double precision and store float vertex fields:

```text
position_x = 2 * dx / (Wt - 1) - 1
position_y = 1 - 2 * dy / (Ht - 1)
texture_u  = sx / (Ws - 1)
texture_v  = sy / (Hs - 1)
```

Pixel coordinates include the endpoint `extent-1`; do not divide these mesh coordinates by the full
extent. Grid vertices remain in row-major order. For cell `(r,c)` in a grid with `Vw` vertices per row:

```text
LT = r*Vw+c; RT = LT+1; LB = (r+1)*Vw+c; RB = LB+1
append indices [LT, RT, RB, LT, RB, LB]
total index count = 6 * (Vw-1) * (Vh-1)
```

Reference vector: a 2-row, 3-column grid with `src=(4*c,4*r)`, `dst=(2*c,2*r)`, source 9 x 5 and
target 5 x 3 gives top-middle NDC `(0,1)`, UV `(0.5,0)`. Its complete index sequence is
`[0,1,4,0,4,3,1,2,5,1,5,4]`. The geometry contract uses a float tolerance of `1e-6` for this vector.

## Projection pass and layer contract

`sProjectionPassDescription` identifies one color attachment, an optional render area, a normalized
clear color, one or more ordered layers, the required final texture state, and `supersample_scale`.
The pass always clears and stores the attachment. Layers are executed in vector order.

- The default `supersample_scale` of one creates no intermediate target. The current contract accepts
  only one or two.
- A value of two requires the final output extent in `render_area.extent`. An empty extent or integer
  overflow while doubling it is a validation error.
- At two, layers render into an internal RGBA8 target whose width and height are twice the final
  extent. The complete intermediate is then linearly clamp-sampled into the final render area with
  replace blending.

Each `sProjectionLayerDescription` has these rules:

- `mesh` and `source_texture` are required valid opaque handles.
- `alpha_texture` is optional. Absence is represented only by the all-zero handle.
- `sampling_filter` is `Nearest` or `Linear`; projection defaults to `Linear`.
- `opacity` is finite and between zero and one inclusive.
- `edge_blend` specifies attenuation widths from the left, right, top, and bottom texture-UV borders
  and selects a `Linear` or `Smoothstep` curve. Every width is finite and between zero and one;
  zero disables attenuation at that border.
- Source and alpha textures must have `Sampled` usage and cannot be the active color attachment.

The generic `sMeshDrawCommand` also exposes sampling, alpha, blend, opacity, and edge-blend fields so
the backend contract does not depend on the projection facade. `ProjectionPipeline` fixes projection
draws to `SourceAlpha` blending.

## Sampling, alpha, blend, and color contract

- Nearest and bilinear filtering use clamp addressing in U and V.
- Color and alpha textures are sampled with the same filter and UV.
- RGBA8/BGRA8 alpha maps use their alpha channel. R8 alpha maps use their red channel as the alpha
  factor. Unused color channels do not affect composition.
- Edge blend is evaluated from interpolated UV. For each border whose `width > 0`, divide normalized
  distance from that border by `width` and clamp the result to obtain `t`. `Linear` uses `t`, while
  `Smoothstep` uses `t²(3-2t)`.
- Active border factors multiply. Opposing horizontal or vertical widths may overlap and use the
  same multiplication rule through the overlap.
- Final source alpha is `texture alpha * alpha-map factor * edge factor * layer opacity`.
- An alpha map remains the compatibility path for arbitrary or externally calibrated masks. The
  four-border curve is a texture-free convenience for common fades; when both are present, they multiply.
- Straight-alpha composition is `source.rgb * source.alpha + destination.rgb * (1-source.alpha)`.
- Destination alpha uses `source.alpha + destination.alpha * (1-source.alpha)`.
- Layers draw from the beginning of the vector, so every later layer composites over earlier layers.
- The current D3D12 and Vulkan paths accept RGBA8/BGRA8 color and alpha textures plus non-render-target R8
  sampled textures. sRGB transfer conversion is not currently implemented.
- RGBA8/BGRA8 values are treated as linear numeric UNORM values. Applications must perform any
  required color-space conversion outside the projection pipeline.

### Sampling and composition example: OUI-PROJ-02

The CPU bilinear reference samples texel centers. For normalized `(u,v)` and a `W x H` texture:

```text
x = clamp(u*W - 0.5, 0, W-1); y = clamp(v*H - 0.5, 0, H-1)
x0 = floor(x); x1 = min(x0+1, W-1); fx = x-x0
y0 = floor(y); y1 = min(y0+1, H-1); fy = y-y0
sample = (1-fy)*((1-fx)*C[x0,y0] + fx*C[x1,y0])
       + fy*((1-fx)*C[x0,y1] + fx*C[x1,y1])
```

This sampling formula uses full texture extent, unlike mesh pixel-to-UV conversion above. A constant
red source `(1,0,0,1)`, alpha-map factor 0.5, opacity 0.5 and a left smoothstep width 0.5 sampled at
`u=0.25` give `t=0.5`, edge factor 0.5 and effective source alpha 0.125. Over opaque blue `(0,0,1,1)`,
the output is `(0.125,0,0.875,1)`, approximately RGBA8 `(32,0,223,255)`. UNORM encoding of input factors
and backend rounding are subject to the one-LSB test tolerance; this is not premultiplied-alpha input.
Quantize at each RGBA8 attachment write, including the scale-two intermediate, before sampling the
next pass. Comparing only an unquantized final formula misses that intermediate rounding.

## Resource and execution contract

- Successful projection submission returns a fence. Work may execute asynchronously; the current
  Vulkan adapter completes its internal wait before returning. See OUI-SUBMIT-03 in the Renderer design.
- At scale two, the supersample and downsample passes execute in submission order on the same
  renderer, and the facade returns the second fence. Internal RGBA8 texture and fullscreen-mesh
  handles are released after submission while backend resources remain alive through fence completion.
- Mesh, source, alpha-map, combined descriptor-heap, and attachment resources remain alive until the
  returned fence completes, even if the caller destroys the corresponding opaque handles.
- A draw submitted before a mesh update retains the old vertex/index resources through its fence.
  Later submissions resolve the same opaque handle to the new resources, so the two submissions do
  not invalidate one another.
- Validation and backend failures are returned through `RendererResult`; the facade never reports a
  dummy success or silently disables an alpha map.
- `ProjectionPipeline` does not own the caller's renderer, textures, meshes, surfaces, or fences.

### Pass construction and failure order: OUI-PROJ-03

`execute(renderer*, description)` first checks the renderer pointer and structural description.
Nonzero handle validation does not establish backend resource existence; Renderer resolves it during
execution. At scale one, the pipeline builds one clear/store render pass with SourceAlpha layers in
input order and returns that pass's result, without creating a texture or mesh.

At scale two it follows this sequence:

```text
validate pass and doubled extent
  -> allocate CPU descriptors for both passes and the downsample mesh
  -> create RGBA8 temporary target
  -> create fullscreen downsample mesh
  -> pass 1: ordered SourceAlpha layers
  -> pass 2: linear sampling / Replace blend
  -> destroy temporary mesh and texture handles
  -> return second fence if cleanup succeeds
Arrows show successful execution order; failure cleanup is listed below.
```

The temporary target is `2*render_area.extent`, with RenderTarget/Sampled usage and initial
RenderTarget state. Pass one uses origin `(0,0)`, the doubled extent, the original clear color and
layers, and final ShaderResource state. The fullscreen mesh uses positions
`(-1,1),(1,1),(1,-1),(-1,-1)`, UVs `(0,0),(1,0),(1,1),(0,1)` and indices `[0,1,2,0,2,3]`.
Pass two clears/stores the real attachment at the original render area, uses linear sampling,
Replace blend, opacity 1 and no alpha map/edge blend, then applies the requested final state.

On narrow screens, scroll this table horizontally.

<div class="wse-projection-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| Failure point | Cleanup / returned result | What may already have happened |
| --- | --- | --- |
| Validation or target creation | Return that error | No temporary resource to release |
| Mesh creation | Destroy target, return mesh error | Target was allocated; cleanup result is secondary |
| Pass one | Destroy mesh then target, return pass-one error | Native submission may have started before failure |
| Pass two | Destroy both, return pass-two error | Pass one may have completed; no transaction rollback |
| Cleanup after two successes | Attempt both destroys; return mesh error first, else target error | Both passes were submitted, but no success fence is exposed on cleanup failure |
| All stages succeed | Return pass-two fence | Internal handles gone; pending native resources retained as required |

</div></div>

All CPU pass/mesh vectors are prepared before temporary GPU handles are acquired. Each returned
temporary handle is adopted immediately by one scope owner, which attempts mesh then texture
destruction exactly once on success, returned failure and exception unwinding. One failed or throwing
destroy does not prevent the other attempt. If an operation already failed or threw, cleanup failures
are secondary and the original result/exception is preserved. After two successful passes, structured
cleanup errors prefer mesh then texture; if either destroy throws, the first cleanup exception is
re-thrown after both attempts. Low-level C++ exceptions can propagate; this is not a no-throw API.

A failed destroy may leave a resource registered in Renderer, including when Vulkan has a pending
submission. The pipeline does not retry, return private handles, or promise immediate reclamation.
Renderer remains the owner until shutdown/destruction; after unrecoverable execution/cleanup failure,
retire the renderer once the application has finished with its caller-owned resources. Shutdown's GPU
wait limitations remain those in [Renderer](OuiRenderer.md). A backend allocation that throws before
returning a handle is the backend's responsibility. A caller must not equate failed execute with an
untouched attachment: already submitted passes are not rolled back.

The handle-free `renderProjection()` binding facade supplies the requested output width/height as the
explicit render area, including scale two. Its local Renderer owns all resources through readback or
failure. `wse.oui.binding_projection_contract` checks every byte of a uniform opaque scale-two frame
on both software backends; existing non-uniform pipeline goldens still check two-stage quantization.

## Current backend and verification scope

On narrow screens, scroll this table horizontally.

<div class="wse-projection-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| Contract | Executable entry | Limit |
| --- | --- | --- |
| OUI-PROJ-01 | [Geometry vectors](../../../test/characterization/projection_geometry_contract.cpp) | Literal coordinates and full two-cell index order; not all degenerate geometry |
| OUI-PROJ-02 | `wse.oui.projection_scale_alpha_golden`, `wse.oui.projection_edge_multisource_golden` | CPU reference and one-LSB tolerance; not color calibration |
| OUI-PROJ-03 | Supersample/dynamic-mesh goldens, binding scale-two frame and `wse.oui.projection_cleanup_contract` | 117 fake-backend cases for status/exception/allocation failures and secondary cleanup; native driver allocation/submit faults are not injected |

</div></div>

Windows D3D12 and Linux Vulkan 1.2 implement source/alpha textures, nearest/linear clamp samplers,
per-draw constants, replace/straight-alpha blending, four-border edge blending, and dynamic meshes
through the same portable contract. D3D12 WARP and an Ubuntu software Vulkan adapter are the
automated reference adapters.

The deterministic projection test converts a legacy fullscreen mesh, linearly scales a 2 x 2 RGBA
source to 4 x 4, samples a non-uniform alpha map, applies layer opacity 0.75, and compares every color
channel with a CPU bilinear/straight-alpha reference within one RGBA8 LSB. Its exact WARP RGBA FNV-1a
baseline is `0xce4b43253436392e`; the Ubuntu Vulkan value is `0x277fcfc63b3185a2`. Both remain within
one LSB of the CPU reference.

The two-stage projection test draws the same source and a single-channel R8 alpha map into an 8 x 8
internal target, then linearly downsamples to 4 x 4. A CPU two-stage reference includes intermediate
RGBA8 quantization and compares every channel within one LSB. Its exact WARP RGBA FNV-1a baseline is
`0xa3ccfe45a48e2d20`; the Ubuntu Vulkan value is `0xbdb24c2a72bce0b4`. Both satisfy the one-LSB CPU tolerance.

The edge/multi-source test draws a red right-edge smoothstep fade first and a blue left-edge fade at
opacity 0.75 second into an 8 x 4 target. Its CPU reference reproduces ordered straight-alpha
composition and RGBA8 quantization after each draw, with every channel within one LSB. D3D12 WARP and
Ubuntu Vulkan both produce the exact RGBA FNV-1a `0x17b97761907effd5`.

The dynamic-mesh test updates one mesh handle from a left-half shape to a right-half shape without
waiting for the first fence. It submits the two versions to separate 8 x 8 targets, destroys the mesh
and source handles, then waits. The old left-half RGBA FNV-1a is `0xbbb816f5e80169e5`; the new
right-half value is `0xb39c749492b617e5`. D3D12 and Vulkan produce the same two exact values. This
fixes handle reuse and old/new resource retention in one deterministic test.

The shared example `example/cpp/oui/portable_projection.cpp` draws the same 2 x 2 RGBA source and
fullscreen mesh to 4 x 4 through `ProjectionPipeline`, using nearest sampling on D3D12 WARP and
Ubuntu Vulkan. It requires zero LSB difference and the same RGBA FNV-1a `0xe0f9517b5bf10dc5` and is
also executed by CTest, fixing one public API workflow for both Windows and Linux.

The Wayland presentation gate uses the same Vulkan Projection Pipeline to draw a 2 x 2 source with
linear sampling and left/right smoothstep edge blending. Drawing and presentation must complete in
windowed mode, borderless fullscreen, restored windowed mode, and after a 128 x 96 resize. The
pre-resize swapchain texture handle must fail safely after recreation.

## Explicitly unsupported scope

- Supersample factors above two, arbitrary factors, reusable intermediate targets, and quality/memory policy.
- Gamma-aware or sRGB composition, photometric correction, and color calibration.
- Arbitrary custom edge curves, projector-specific edge-blend settings, and calibration-mask generation.
- Partial vertex/index-range updates, GPU-written meshes, and caller-side concurrent updates on one renderer.
- Physical display disconnect/reconnect during projection, a representative DRM/KMS projection scene,
  and hardware acceptance of direct-display restoration.
- Long-running operation, memory-leak checks, temperature, and frame-time acceptance on Raspberry Pi 4/5.

Unsupported work must remain observable and must not be represented as certified by the single-layer
Windows WARP baseline.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
