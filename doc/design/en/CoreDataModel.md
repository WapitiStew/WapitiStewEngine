# WSE Core Data Model

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope and type structure

This design specifies the shared CPU data representation. It does not make an in-memory C++
object a serialized file format or a compiler-neutral ABI. Core owns values; camera, renderer
and language adapters translate their external buffers at explicit boundaries.

```text
Map<T> owns vector<T>, width, height
  +-- Matrix_<T>        floating-point matrix; Matrix is the double-precision form
  |     +-- Homography 3 x 3 projective transform
  +-- Image_<Format>   Map of PixelAlias<Format>

Pixel_<Storage, Channels, Depth> owns its channel array
TiePoint_<Source, Destination> pairs source and destination coordinates
Mesh_<Source, Destination> owns a rectangular grid of TiePoint vertices
```

The [Map](../../../api/wse/data/wse_Map.h), [Matrix](../../../api/wse/data/wse_Matrix.h),
[Image](../../../api/wse/data/wse_Image.h), [Pixel](../../../api/wse/data/wse_Pixel.h),
[Mesh](../../../api/wse/data/wse_Mesh.h) and [Homography](../../../api/wse/data/wse_Homography.h)
headers are the public declarations. Template behavior is partly implemented in those headers;
the [data implementation directory](../../../core/wse/data/) contains the out-of-line parts.

## Shape and addressing: CORE-DATA-01

For an initialized Map, `size() = width() * height()` and storage is row-major:

```text
width = 3, height = 2
row y=0: [a b c]
row y=1: [d e f]
storage: [a b c d e f]
index(x,y) = y * width + x
```

`Map(width,height)` uses columns first. `Matrix(rows,columns)` uses rows first, so a 2-row,
3-column Matrix has `width()==3` and `height()==2`. Confusing the constructor order changes
both allocation and addressing. `image[y][x][channel]` first selects a pixel row, then a pixel,
then a stored channel.

| Access | Result | Invalid access |
| --- | --- | --- |
| `operator[](y)` then `[x]` | Non-owning row pointer then element reference | Caller precondition; unchecked, inline, noexcept |
| `at(x,y)` | Mutable/const element reference | `std::out_of_range` |
| `row(y)` | Mutable/const pointer to one row | `std::out_of_range` for invalid row or zero width |
| `get(index)` / `set(index,value)` | Checked linear access | `std::out_of_range` |
| `elements()` | Non-owning contiguous storage pointer | Caller supplies valid span |
| `data()` | Independent vector copy | Allocation can fail |
| `vector() const` | Pointer to const vector owned by Map | Owner lifetime required; cannot resize through it |

Default Map is 0 x 0. A zero-width Map may retain a nonzero height but has no accessible row.
Shape multiplication is checked before allocation; overflow and a supplied vector of the wrong
size throw `std::invalid_argument`. `init` value-initializes the new elements. These checks
must not be inferred for unchecked indexing.

## Ownership and mutation: CORE-DATA-02

Copying a data owner produces independent storage. Copy assignment builds a replacement before
swapping. Moving transfers the storage; no promise is made that the source's shape becomes zero.
Reset or assign the moved-from object before shape-dependent access. Reacquire all views after
owner assignment, initialization, move or other storage replacement.

Pointers and references do not retain their owner. `const` prevents mutation through that view;
it does not freeze another alias. Mutable element access cannot resize the vector, which
preserves the shape invariant. Concurrent read/write needs caller synchronization. Core data
values do not create threads or perform device I/O.

## Pixel storage and channel order: CORE-DATA-03

| Logical depth | Storage per channel | Example format |
| --- | --- | --- |
| 8 | `uint8_t`, 1 byte | `CH3D8` |
| 10, 12, 14, 16 | `uint16_t`, 2 bytes | `CH1D12`, `CH4D16` |
| 32 | `uint32_t`, 4 bytes | `CH3D32` |
| 64 | `uint64_t`, 8 bytes | `CH1D64` |

Each pixel is the channel array without extra padding: pixel bytes = channels * storage bytes.
Logical depth is not a packed-bit wire format: a 12-bit channel still occupies two bytes.
`CH3`/`CH4` color use RGB/RGBA ordering; `BGR3D8`, `BGRA4D8` and their 16-bit variants encode
the reverse color order explicitly. Alpha remains the fourth channel.
`convertChannelOrder()` is the explicit color-order conversion; copying bytes never implies it.
Depth narrowing follows the Pixel conversion contract (high bits are retained), not an
unannounced normalized-color conversion.

An Image is packed: row bytes = width * channels * storage bytes. Native data owners use typed
host storage. File formats and external byte buffers define their own byte order separately.
`memory_size()` counts element storage, not allocator overhead or the complete C++ object.

## Interleaved buffer boundary: CORE-DATA-04

[ImageInterleaved](../../../api/wse/data/wse_ImageInterleaved.h) represents external memory by
data pointer, width, height, row stride and accessible byte count. A view borrows const bytes;
a target borrows writable bytes. Neither owns the allocation.

For pixel size P, width W, height H:

1. Reject null memory, zero extent, multiplication overflow or invalid layout.
2. Packed row R = W * P. Effective stride S = supplied stride, or R when zero.
3. Require S >= R and the implementation's H * S overflow check to pass.
4. Require accessible bytes >= (H - 1) * S + R, with checked addition.
5. Copy R bytes of sample data per row; do not copy padding into the packed Image.

For a 2 x 2 RGB8 image, R=6. With S=8, fourteen accessible bytes are sufficient: bytes 6 and 7
are first-row padding and the last row needs no trailing padding. A 13-byte view is invalid.
Sixteen-bit samples at this boundary are little endian: `34 12` becomes `0x1234`.
`makeImageFromInterleaved` constructs an owned copy; writes use `writeImageToInterleaved` and
preserve target padding. No view or backend buffer is retained by the resulting Image.
Only the formats accepted by the helper are supported; a Pixel storage specialization alone
does not imply every external-buffer conversion is available.

Camera and renderer bridges additionally validate their own description and format rules:
[Tmr frames](TmrCamera.md#frames-as-core-images), [OUI frames](OuiRenderer.md#frame-and-core-image).
For example, a Bayer single-channel carrier does not itself retain the Bayer pattern, and
RGBA16 float cannot be reinterpreted as Core integer RGBA16.

## Geometry and numerical boundaries

Matrix has row-major scalar storage. Homography applies a 3 x 3 transform to homogeneous
coordinates, while a Mesh stores both source and destination positions at each grid vertex.
Mesh `operator[]` advances by `vertex_width()` rather than a pixel width. A quad has corner
order top-left, top-right, bottom-right, bottom-left; the projection adapter produces triangles.
Source pixel coordinates and destination pixel coordinates are not NDC/UV until explicitly
converted by [ProjectionMeshAdapter](../../../core/oui/renderer/ProjectionMeshAdapter.cpp).

Matrix arithmetic and inverse failures are governed by the public header and
[matrix correctness test](../../../test/characterization/wse_matrix_correctness_contract.cpp).
Projection's coordinate direction, interpolation and alpha formula are in
[OUI Projection](OuiProjection.md). Image orientation, accumulation and demosaicing live in
[ImageTransform](../../../api/wse/data/wse_ImageTransform.h) and
[ImageDemosaic](../../../api/wse/data/wse_ImageDemosaic.h), independent of camera ownership.
The formulas, rounding, singularity checks and representative values are specified by
[Core Algorithms](CoreAlgorithms.md). Calibration methods remain outside this data-layout document.

## Reconstruction checks

| ID | Evidence | Required observation |
| --- | --- | --- |
| CORE-DATA-01 | [map invariants](../../../test/characterization/wse_map_invariants_contract.cpp), [fast indexing](../../../test/characterization/wse_fast_indexing_contract.cpp) | Row offset, checked failures, overflow rejection and aliasing |
| CORE-DATA-02 | [Map owner implementation](../../../api/wse/data/wse_Map.h), source review | Independent copies; review storage replacement and moved-from restrictions |
| CORE-DATA-03 | [pixel storage](../../../test/characterization/wse_pixel_storage_contract.cpp), [channel order](../../../test/characterization/wse_image_channel_order_contract.cpp) | Storage width, channels, pixel stride and explicit reorder |
| CORE-DATA-04 | [interleaved contract](../../../test/characterization/wse_image_interleaved_contract.cpp) | Padded rows, owned copy, little-endian samples and preserved alpha |

The tests are evidence for the named observations, not proof of every lifetime interleaving.
An independent implementation can first reproduce the 3 x 2 indexing example and 2 x 2 padded
RGB8 example without any camera or GPU. Numerical algorithm reconstruction is separate work.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
