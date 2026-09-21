# WSE Core Data Model

> Canonical source: [English Core Data Model](../en/CoreDataModel.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象と型構造

共通CPU Data表現を定義します。C++ Objectのメモリ表現をFile形式やCompiler非依存ABIとするものではありません。
Coreは値を所有し、Camera／Renderer／言語Adapterが明示的な境界で外部Bufferを変換します。

```text
Map<T> owns vector<T>, width, height
  +-- Matrix_<T>        floating-point matrix; Matrix is the double-precision form
  |     +-- Homography 3 x 3 projective transform
  +-- Image_<Format>   Map of PixelAlias<Format>

Pixel_<Storage, Channels, Depth> owns its channel array
TiePoint_<Source, Destination> pairs source and destination coordinates
Mesh_<Source, Destination> owns a rectangular grid of TiePoint vertices
```

公開宣言は[Map](../../../api/wse/data/wse_Map.h)、[Matrix](../../../api/wse/data/wse_Matrix.h)、
[Image](../../../api/wse/data/wse_Image.h)、[Pixel](../../../api/wse/data/wse_Pixel.h)、
[Mesh](../../../api/wse/data/wse_Mesh.h)、[Homography](../../../api/wse/data/wse_Homography.h)にあります。
Template実装の一部はHeader内、外部定義は[data実装](../../../core/wse/data/)にあります。

## ShapeとAddress: CORE-DATA-01

初期化済みMapは`size() = width() * height()`を満たし、行優先です。

```text
width = 3, height = 2
row y=0: [a b c]
row y=1: [d e f]
storage: [a b c d e f]
index(x,y) = y * width + x
```

`Map(width,height)`は列数が先、`Matrix(rows,columns)`は行数が先です。2行3列のMatrixは
`width()==3`、`height()==2`です。`image[y][x][channel]`はPixel行、Pixel、格納Channelの順に選びます。

| Access | 結果 | 不正Access |
| --- | --- | --- |
| `operator[](y)`の後に`[x]` | 非所有Row Pointerから要素参照 | 呼出側の前提。検査なし、inline、noexcept |
| `at(x,y)` | 非const／const要素参照 | `std::out_of_range` |
| `row(y)` | 非const／constの1行Pointer | 範囲外行・幅0は`std::out_of_range` |
| `get(index)`／`set(index,value)` | 検査付き線形Access | `std::out_of_range` |
| `elements()` | 非所有の連続領域Pointer | 有効範囲は呼出側が守る |
| `data()` | 独立したVector Copy | 確保失敗がありうる |
| `vector() const` | Map所有のConst VectorへのPointer | Ownerの寿命が必要。この経路でResizeできない |

既定Mapは0×0です。幅0・高さ非0も可能ですが有効な行はありません。Shape積のOverflowと入力Vectorの
要素数不一致は`std::invalid_argument`で拒否します。`init`は新しい要素を値初期化します。
これらの検査を、検査なし添字Accessにもあると解釈してはいけません。

## 所有と変更: CORE-DATA-02

Data OwnerのCopyは独立した領域を作ります。Copy代入は置換値を作ってからSwapします。
Moveは領域を移し、移動元Shapeが0になる保証はありません。移動元をShape依存操作に再利用する前に
Resetまたは代入してください。代入・初期化・Move等で領域を置換した後はViewを取得し直します。

Pointer／ReferenceはOwnerを保持しません。Const Viewは別Aliasの変更を禁止しません。
可変要素AccessはVectorをResizeできず、Shape不変条件を維持します。同時読書きの同期は呼出側が管理します。
Data値はThreadを作らずDevice I/Oを行いません。

## Pixel格納とChannel順序: CORE-DATA-03

| 論理Depth | 1 Channelの格納 | Format例 |
| --- | --- | --- |
| 8 | `uint8_t`、1 byte | `CH3D8` |
| 10、12、14、16 | `uint16_t`、2 byte | `CH1D12`、`CH4D16` |
| 32 | `uint32_t`、4 byte | `CH3D32` |
| 64 | `uint64_t`、8 byte | `CH1D64` |

PixelはPaddingなしのChannel配列であり、Pixel byte数はChannel数×格納byte数です。
12bit Channelも2 byteを使い、Packed-bit通信形式ではありません。Colorの`CH3`／`CH4`はRGB／RGBA、
`BGR3D8`／`BGRA4D8`と16bit版は逆順Colorを明示します。Alphaは4番目に残ります。
`convertChannelOrder()`が明示的な順序変換です。Byte Copyは順序変換を意味しません。
Depth縮小はPixel変換契約に従い上位bitを保持し、暗黙の正規化Color変換ではありません。

ImageはPackedで、行byte数は幅×Channel数×格納byte数です。Native Ownerは型付きHost表現を使い、
File・外部byte bufferのByte順は別途定義します。`memory_size()`は要素領域の大きさであり、
AllocatorのOverheadやC++ Object全体の大きさではありません。

## Interleaved Buffer境界: CORE-DATA-04

[ImageInterleaved](../../../api/wse/data/wse_ImageInterleaved.h)は外部メモリをData Pointer、幅、高さ、
Row stride、Access可能byte数で表します。ViewはConst byte、Targetは書込み可能byteを借用し、所有しません。

Pixel byte数P、幅W、高さHに対し、次の順序です。

1. Null、空Extent、積Overflow、不正配置を拒否する。
2. Packed行R = W×P。実効Stride Sは指定値、0指定ならR。
3. S >= Rと、実装のH×S Overflow検査を満たす。
4. Access可能byte数 >= (H−1)×S+Rを検査付き加算で確認する。
5. 各行R byteのSampleをCopyし、PaddingはImageへCopyしない。

2×2 RGB8はR=6です。S=8なら14 byteで足り、先頭行のbyte 6・7はPadding、最終行の末尾Paddingは不要です。
13 byteは不正です。境界の16bit SampleはLittle Endianで、`34 12`は`0x1234`になります。
`makeImageFromInterleaved`は所有Copyを作り、`writeImageToInterleaved`は宛先Paddingを保存します。
Imageは元ViewやBackend Bufferを保持しません。Helperが受理するFormatだけが対応範囲であり、
Pixel格納型があるだけで全外部Buffer変換が可能になるわけではありません。

Camera／Rendererは独自のDescription・Format検査も行います。
[Tmr Frame](TmrCamera.md#ja-frames-as-core-images)、[OUI Frame](OuiRenderer.md)を参照してください。
Bayerの単Channel表現だけでは配列Patternを保持せず、RGBA16 FloatをCoreの整数RGBA16として再解釈しません。

## 幾何・数値境界

Matrixは行優先のScalar配列です。Homographyは同次座標へ3×3変換を適用し、Meshは各格子Vertexに
Source／Destination座標を持ちます。MeshのRow strideはPixel幅ではなく`vertex_width()`です。
QuadのCorner順序は左上・右上・右下・左下で、Projection AdapterがTriangle化します。
Source／Destination Pixel座標は[ProjectionMeshAdapter](../../../core/oui/renderer/ProjectionMeshAdapter.cpp)で
明示変換するまでNDC／UVではありません。

Matrix演算と逆行列失敗は公開Headerと[Matrix契約](../../../test/characterization/wse_matrix_correctness_contract.cpp)、
Projectionの座標方向・補間・Alpha式は[OUI Projection](OuiProjection.md)に従います。
方向変換・平均化・DemosaicはCamera所有から独立した
[ImageTransform](../../../api/wse/data/wse_ImageTransform.h)と
[ImageDemosaic](../../../api/wse/data/wse_ImageDemosaic.h)にあります。式、丸め、特異判定、代表値は
[Core演算設計](CoreAlgorithms.md)で定義します。Calibration方式は本データ配置設計の対象外です。

## 再現の検証

| ID | 根拠 | 確認する性質 |
| --- | --- | --- |
| CORE-DATA-01 | [Map不変条件](../../../test/characterization/wse_map_invariants_contract.cpp)、[高速添字](../../../test/characterization/wse_fast_indexing_contract.cpp) | 行Offset、検査付き失敗、Overflow拒否、Aliasing |
| CORE-DATA-02 | [Map Owner実装](../../../api/wse/data/wse_Map.h)のSource Review | 独立Copy。領域置換と移動元制約はReview |
| CORE-DATA-03 | [Pixel格納](../../../test/characterization/wse_pixel_storage_contract.cpp)、[Channel順序](../../../test/characterization/wse_image_channel_order_contract.cpp) | 格納幅、Channel数、Pixel stride、明示変換 |
| CORE-DATA-04 | [Interleaved契約](../../../test/characterization/wse_image_interleaved_contract.cpp) | Padding、所有Copy、Little Endian、Alpha保持 |

各Testは上記性質の根拠であり、全Lifetime競合の証明ではありません。独立実装はまず3×2添字例と
2×2 Padding付きRGB8例をCamera／GPUなしで再現できます。数値Algorithm全体の再現は別の作業です。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
