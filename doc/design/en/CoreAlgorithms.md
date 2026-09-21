# WSE Core Algorithms

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope and implementation map

This document specifies the CPU operations built on [Core Data Model](CoreDataModel.md).
Inputs and results are owned values; these operations create no device session, GPU work or
background thread. Camera format decoding and renderer sampling remain component contracts.

| Responsibility | Public contract / implementation | Evidence |
| --- | --- | --- |
| Matrix arithmetic, covariance, inverse | [Matrix header](../../../api/wse/data/wse_Matrix.h), [implementation](../../../core/wse/data/wse_Matrix.cpp) | [matrix correctness](../../../test/characterization/wse_matrix_correctness_contract.cpp) |
| Point correspondence | [Homography header](../../../api/wse/data/wse_Homography.h), [implementation](../../../core/wse/data/wse_Homography.cpp) | [projection geometry](../../../test/characterization/projection_geometry_contract.cpp); requires OUI |
| Orientation and accumulation | [ImageTransform](../../../api/wse/data/wse_ImageTransform.h) | [Core image algorithms](../../../test/characterization/wse_image_algorithms_contract.cpp) |
| Bayer interpolation and color matrix | [ImageDemosaic](../../../api/wse/data/wse_ImageDemosaic.h) | Same Core test; additional [camera frame cases](../../../test/characterization/camera_frame_ops_contract.cpp) require Tmr |

The formulae below describe current calculations. Floating-point reconstruction compares values
within the stated example tolerances, not object bytes. Inputs must be finite and reasonably
scaled: the implementation does not uniformly reject NaN/infinity or detect arithmetic overflow.
Allocation failures may throw. A checked result for a computational failure is not a no-throw API.

## Matrix operations and sample covariance: CORE-ALG-01

`Matrix` is double precision; `Matrix_f` is float precision. Rows are the first constructor
argument. For A of shape m by k and B of shape k by n:

```text
C = A * B
C[i,j] = sum(A[i,t] * B[t,j], t=0..k-1)
transpose(A)[j,i] = A[i,j]
```

Multiplication allocates an m by n zero matrix and accumulates in ascending i, j, t order using
the matrix scalar type. Addition/subtraction requires equal shapes. A dimension mismatch throws
`std::invalid_argument` before changing a compound-assignment target. Scalar division by exact
zero throws `std::domain_error`. Equality compares shape and exact element values; it is not a
numerical-tolerance predicate. `Identify(n)` is the public spelling of the identity constructor.

`Covariance(&mean, samples)` computes **sample** covariance, dividing by N-1:

```text
mean[r] = sum(samples[i][r], i=0..N-1) / N
cov[r,c] = sum((samples[i][r]-mean[r]) * (samples[i][c]-mean[c])) / (N-1)

samples = [1,2]^T, [3,6]^T
mean = [2,4]^T
cov  = [[2,4], [4,8]]
scalar samples [1,3] -> mean 2, covariance matrix [[2]]
```

The output pointer must be non-null and refer to a separate mutable object. At least two
samples are required; all must be column vectors of the same row count. Invalid input throws
`std::invalid_argument` before the matrix mean is assigned. The implementation then writes
the mean before allocating/computing covariance, so allocation failure is not a transaction
covering both outputs. The scalar overload wraps samples as 1 by 1 matrices and assigns its
mean after covariance succeeds. Example matrix comparisons use absolute tolerance 1e-9.

## Determinant and inverse: CORE-ALG-02

Both operations require a square matrix; otherwise they throw `std::invalid_argument`.
They compute on copies and do not alter the source.

| Size | Determinant | `tryInverse()` |
| --- | --- | --- |
| 1 | a | 1/a; exact zero returns `Computation/SingularMatrix` |
| 2 | ad-bc | `[[d,-b],[-c,a]] / (ad-bc)` if the threshold below permits it |
| 3 or more | Gaussian elimination with partial pivoting | Gauss-Jordan elimination on A and an identity companion |

For size 2, reject when `abs(ad-bc) <= 4*epsilon*(abs(ad)+abs(bc))`, where epsilon is
`numeric_limits<T>::epsilon()`. For larger matrices, set a fixed threshold
`tau = max(abs(original A[i,j])) * n * epsilon`. An all-zero maximum fails immediately.

At each column r, choose the row with the largest absolute entry in that column among rows
r..n-1; ties retain the earlier row. Swap it into r. Determinant elimination changes sign
for each swap, multiplies the pivots, and eliminates rows below. Its zero-pivot check is
exact, so a nonzero determinant does **not** prove that `tryInverse` will accept the matrix.

For inverse, swap both A and the identity companion. A pivot with `abs(pivot) <= tau` returns
a failed strict `CoreResult` with `SingularMatrix`. Otherwise divide both rows by the pivot,
then eliminate the column from every other row. The companion is the result. Complexity is
O(n^3) time and O(n^2) temporary storage for these elimination branches.

```text
A = [[0,1],[1,0]] -> det(A)=-1, inverse(A)=A
A = [[1,2,3],[4,5,6],[7,8,10]] -> det(A)=-3
A * inverse(A) approximately equals Identify(3), absolute tolerance 1e-9
float A = [[1,1],[1,1.0000001]] -> SingularMatrix
```

This is a scale-dependent numerical rejection policy, not condition-number estimation or a
least-squares solver. No error bound is promised for arbitrary ill-conditioned input.
The current empty 0 by 0 case yields determinant 1 and a failed inverse; do not use that
edge case as a replacement for validating an application's nonempty input.

## Homography and coordinate direction: CORE-ALG-03

Coordinates use column vectors. A Homography maps source points to destination points:

```text
w  = H[2,0]*x + H[2,1]*y + H[2,2]
x' = (H[0,0]*x + H[0,1]*y + H[0,2]) / w
y' = (H[1,0]*x + H[1,1]*y + H[1,2]) / w
```

Two correspondences are opposite corners of axis-aligned rectangles, expanded in order
(x0,y0), (x1,y0), (x1,y1), (x0,y1). Four correspondences supply those ordered corners directly.
Empty, mismatched, one-point and three-point lists throw `std::invalid_argument`.
More than four points throws `std::logic_error` in portable Core; no fitted multi-point
solver is silently selected by enabling another component.

The four-point construction composes source-to-unit-square and unit-square-to-destination
matrices. To reconstruct the latter for corners p0..p3, define:

```text
v1=p1-p0; v2=p2-p0; v3=p3-p0; cross(a,b)=a.x*b.y-a.y*b.x
a0=cross(v1,v3); a1=cross(v1,v2); a3=cross(v2,v3); a2=a1+a3-a0
g=a0-a1; h=a0-a3; k=a2
Q = [[a3*v1.x+g*p0.x, a1*v3.x+h*p0.x, k*p0.x],
     [a3*v1.y+g*p0.y, a1*v3.y+h*p0.y, k*p0.y],
     [g,              h,              k]]
H is proportional to Q(destination) * inverse(Q(source))
```

The implementation uses an analytic source inverse, with exact-zero checks on its four
area terms, rather than `Matrix::tryInverse` and its tolerance policy. It normalizes the
composed coefficients by their maximum absolute value, not necessarily H[2,2].
Compare transformed points, not scale-equivalent coefficient arrays.

Source corners (0,0), (100,0), (100,100), (0,100) mapped to (10,20), (210,20),
(210,120), (10,120) give `x'=2*x+10`, `y'=y+20`; (50,50) becomes (110,70).
The geometry test uses absolute coordinate tolerance 1e-6.

Source degeneracy can throw `std::runtime_error`. Destination degeneracy, nonfinite
coefficients and a zero transform denominator are not comprehensively validated. Callers
must supply nondegenerate finite geometry and avoid the projective horizon; the current
code is not a general robust fitting routine. Setup computes before replacing stored coefficients.
Mesh cell ordering and NDC/UV conversion remain in [Core Data Model](CoreDataModel.md) and
[OUI Projection](OuiProjection.md).

## Image orientation: CORE-ALG-04

`applyOrientation` returns an independent image with unchanged pixel/channel values.
An empty source or unknown orientation throws `std::invalid_argument`. For source
width W, height H and pixel (x,y), write it to:

| Orientation | Target (x,y) | Target width, height |
| --- | --- | --- |
| None | (x,y) | W,H |
| Rotate90CW | (H-1-y,x) | H,W |
| Rotate180 | (W-1-x,H-1-y) | W,H |
| Rotate90CCW | (y,W-1-x) | H,W |
| FlipHorizontal | (W-1-x,y) | W,H |
| FlipVertical | (x,H-1-y) | W,H |

```text
source 3x2:    Rotate90CW 2x3:    Rotate90CCW 2x3:
1 2 3         4 1                3 6
4 5 6         5 2                2 5
              6 3                1 4
```

No interpolation or color conversion occurs. `orientedExtent` only computes dimensions;
its output pointers must be valid, and it does not itself reject unknown enum values.
Preserving a Bayer pattern across orientation is a camera metadata operation, not a
property of a one-channel Core Image.

## Image accumulation and rounding: CORE-ALG-05

`ImageAccumulator<Format>` owns per-channel sums, frame count and a single shape:

```text
Empty --add(nonempty image)--> Accumulating(shape, count=1)
Accumulating --add(same shape)--> Accumulating(count+1)
Accumulating --average()--> independent image; accumulator unchanged
any state --reset()--> Empty(width=0, height=0, count=0)
```

Empty input or a later shape mismatch throws `std::invalid_argument`; a rejected shape
does not change sums/count. `average()` with no frames throws `std::logic_error`.
`averageImages` is add-each-then-average and therefore also rejects an empty list.

The sum type is uint32_t for channel storage up to 16 bits and uint64_t otherwise.
For N frames each channel returns `(sum + floor(N/2)) / N` using integer division:
10 and 21 produce 16. All channels, including alpha, follow this formula. No gamma,
alpha weighting, registration, timestamps or automatic reset are involved.

The implementation does not check sum/count overflow. Callers must keep the count
representable in the accumulator type and ensure `N*maximum_sample + floor(N/2)` fits.
For 8-bit/16-bit samples, 64 frames are within that bound. Allocation failure during the
first add is not a documented strong exception guarantee; reset or discard that accumulator.
Concurrent mutation requires caller synchronization.

## Bayer interpolation and color transform: CORE-ALG-06

`demosaic<SourceFormat,TargetFormat>` requires one source channel, three target channels
and at least a 2 by 2 image. Pattern parity is anchored at source (0,0):

| Pattern | First row / second row |
| --- | --- |
| Rggb | R G / G B |
| Bggr | B G / G R |
| Grbg | G R / B G |
| Gbrg | G B / R G |

`Block2x2` visits output tiles from top-left. For an odd trailing column/row, its sampling
window shifts one pixel left/up to remain 2 by 2. Scan the window row-major and take the
**first** sample of each color, including the first green; copy that RGB triple to every
valid pixel of the output tile. The two greens are not averaged.

`Bilinear`, the public enum name, computes each output pixel from its clipped 3 by 3
neighborhood. Sum samples separately by Bayer color and use
`(sum[color] + count[color]/2) / count[color]`. This also averages the site's own color
when other samples of that color occur in the neighborhood. Borders shrink the window;
they do not mirror, duplicate or zero-fill samples.

```text
RGGB input:
10 20 30
40 50 60
70 80 90

Bilinear RGB at (0,0) = [10,30,50]
Bilinear RGB at (1,0) = [20,40,50]
Bilinear RGB at (1,1) = [50,50,50]
Block2x2 RGB at (0,0) = [10,20,50]
Block2x2 RGB at (2,2) = [90,60,50]
```

Use valid enum values and explicitly match the requested RGB/BGR order to the target format.
The order argument controls physical channel placement. The helper casts samples to target
storage; it does not normalize depth or perform Pixel's high-bit narrowing conversion.
Use matching 8-bit or 16-bit source/target storage for these reference paths. Wide sums,
invalid method/order values and mismatched-depth conversion are not generally validated.

`applyColorMatrix` multiplies a 3 by 3 double matrix by the three **stored** channel values,
clamps each result to [0, max(channel storage type)], then casts `value+0.5` to that type.
There is no automatic RGB/BGR reorder, offset, gamma or white balance. The clamp uses storage
width, not a smaller logical bit depth. For an RGB8 pixel [10,20,30] and diagonal [30,-1,0.5],
the output is [255,0,15]. Supply finite coefficients and keep this reference path to 8/16-bit
channels; no general nonfinite or 64-bit conversion guarantee is made.

## Verification and evolution

The Core image contract checks the literal orientation, average and Bayer/color vectors
above without enabling Tmr or OUI. Existing matrix and projection tests supply the numerical
evidence listed earlier. They do not certify arbitrary input stability, sum overflow handling,
image quality, calibration accuracy or every degeneracy case.

See [Design Verification](DesignVerification.md) for reconstruction acceptance. Changes to
rounding, singularity thresholds, Bayer edge handling or channel placement change observable
results and require corresponding design/test review, not merely a source-map update.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
