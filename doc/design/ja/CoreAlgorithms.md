# WSE Core演算設計

> Canonical source: [English Core Algorithms](../en/CoreAlgorithms.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象と実装の対応

[Core Data Model](CoreDataModel.md)を使うCPU演算を定義します。入力・結果は所有値であり、
演算自体はDevice Session、GPU処理、Background Threadを作りません。Camera形式のDecodeと
RendererのSamplingは、各Componentの契約に従います。

| 責務 | 公開契約・実装 | 検証 |
| --- | --- | --- |
| 行列演算、共分散、逆行列 | [Matrix Header](../../../api/wse/data/wse_Matrix.h)、[実装](../../../core/wse/data/wse_Matrix.cpp) | [Matrix correctness](../../../test/characterization/wse_matrix_correctness_contract.cpp) |
| 点の対応変換 | [Homography Header](../../../api/wse/data/wse_Homography.h)、[実装](../../../core/wse/data/wse_Homography.cpp) | [Projection geometry](../../../test/characterization/projection_geometry_contract.cpp)。OUIが必要 |
| 向き変換・累積 | [ImageTransform](../../../api/wse/data/wse_ImageTransform.h) | [Core画像演算](../../../test/characterization/wse_image_algorithms_contract.cpp) |
| Bayer補間・色行列 | [ImageDemosaic](../../../api/wse/data/wse_ImageDemosaic.h) | 同Core Test。追加の[Camera Frame例](../../../test/characterization/camera_frame_ops_contract.cpp)はTmrが必要 |

以下は現行の計算規則です。浮動小数点の再現ではObjectのByte一致ではなく、例で指定する許容誤差を
使います。入力は有限値で、適切なScaleとしてください。NaN／Infinityの拒否や演算Overflow検出は
一様には実装されていません。Memory確保は例外を投げる場合があり、計算失敗をResultで返すAPIでも
すべての失敗が非例外になるわけではありません。

## 行列演算と標本共分散: CORE-ALG-01

`Matrix`は倍精度、`Matrix_f`は単精度です。Constructorの第1引数は行数です。
Aをm行k列、Bをk行n列とすると、次の計算になります。

```text
C = A * B
C[i,j] = sum(A[i,t] * B[t,j], t=0..k-1)
transpose(A)[j,i] = A[i,j]
```

積はm行n列の0行列を確保し、i、j、tの昇順に、行列のScalar型で加算します。加減算には同じShapeが
必要です。次元不一致は複合代入の変更前に`std::invalid_argument`、Scalarの厳密な0による除算は
`std::domain_error`です。等値比較はShapeと各値の厳密比較であり、許容誤差付き比較ではありません。
単位行列生成の公開名は`Identify(n)`です。

`Covariance(&mean, samples)`はN-1で割る**標本共分散**です。

```text
mean[r] = sum(samples[i][r], i=0..N-1) / N
cov[r,c] = sum((samples[i][r]-mean[r]) * (samples[i][c]-mean[c])) / (N-1)

samples = [1,2]^T, [3,6]^T
mean = [2,4]^T
cov  = [[2,4], [4,8]]
scalar samples [1,3] -> mean 2, covariance matrix [[2]]
```

出力Pointerは非nullで、入力とは別の変更可能Objectを指す必要があります。Sample数は2以上、
すべて同じ行数の列Vectorとします。不正入力は行列Meanの代入前に`std::invalid_argument`で拒否します。
その後はMeanを書いてから共分散の確保・計算を行うため、確保失敗時に両出力をまとめて元へ戻す保証は
ありません。Scalar版は各値を1×1行列へ包み、共分散成功後にScalar Meanを代入します。
例の行列比較には絶対許容誤差1e-9を使います。

## 行列式と逆行列: CORE-ALG-02

正方行列でなければ`std::invalid_argument`です。Copy上で計算し、元の行列は変更しません。

| 次元 | 行列式 | `tryInverse()` |
| --- | --- | --- |
| 1 | a | 1/a。厳密な0なら`Computation/SingularMatrix` |
| 2 | ad-bc | 下記の閾値を満たすとき`[[d,-b],[-c,a]] / (ad-bc)` |
| 3以上 | 部分Pivot選択付きGauss消去 | Aと単位行列を並行して操作するGauss-Jordan消去 |

2次では`abs(ad-bc) <= 4*epsilon*(abs(ad)+abs(bc))`を拒否します。
epsilonは`numeric_limits<T>::epsilon()`です。3次以上では、最初の行列から固定閾値
`tau = max(abs(original A[i,j])) * n * epsilon`を求めます。最大絶対値が0なら直ちに失敗します。

列rごとに行r〜n-1から同列の絶対値最大の行を選び、rへ交換します。同値では先の行を保持します。
行列式は交換ごとに符号を反転し、Pivotを掛け、下側の行を消去します。こちらの0判定は厳密比較なので、
行列式が非0でも`tryInverse`が受け付けるとは限りません。

逆行列ではAと単位行列側の両方を行交換します。`abs(pivot) <= tau`なら、Strictな`CoreResult`で
`SingularMatrix`を返します。それ以外は両行をPivotで割り、他の全行から対象列を消去します。
単位行列側が結果です。消去法の分岐は時間O(n^3)、一時Memory O(n^2)です。

```text
A = [[0,1],[1,0]] -> det(A)=-1, inverse(A)=A
A = [[1,2,3],[4,5,6],[7,8,10]] -> det(A)=-3
A * inverse(A) approximately equals Identify(3), absolute tolerance 1e-9
float A = [[1,1],[1,1.0000001]] -> SingularMatrix
```

これはScaleに依存する数値的な拒否規則であり、条件数推定や最小二乗Solverではありません。
任意の悪条件入力に対する誤差上限は保証しません。現行の空0×0行列は行列式1、逆行列は失敗です。
Applicationが必要とする非空入力検証の代わりに、この特殊値を利用しないでください。

## Homographyと座標の向き: CORE-ALG-03

列Vectorの座標を、SourceからDestinationへ写します。

```text
w  = H[2,0]*x + H[2,1]*y + H[2,2]
x' = (H[0,0]*x + H[0,1]*y + H[0,2]) / w
y' = (H[1,0]*x + H[1,1]*y + H[1,2]) / w
```

2対応点は軸平行矩形の対角頂点です。(x0,y0)、(x1,y0)、(x1,y1)、(x0,y1)の順へ展開します。
4対応点はその順の頂点を直接指定します。空、個数不一致、1点、3点は`std::invalid_argument`です。
Portable Coreで4点超は`std::logic_error`です。別Componentを有効にしても多点近似Solverへ自動では切り替わりません。

4点版はSource→単位正方形と、単位正方形→Destinationを合成します。後者は頂点p0〜p3から
次のように再構成できます。

```text
v1=p1-p0; v2=p2-p0; v3=p3-p0; cross(a,b)=a.x*b.y-a.y*b.x
a0=cross(v1,v3); a1=cross(v1,v2); a3=cross(v2,v3); a2=a1+a3-a0
g=a0-a1; h=a0-a3; k=a2
Q = [[a3*v1.x+g*p0.x, a1*v3.x+h*p0.x, k*p0.x],
     [a3*v1.y+g*p0.y, a1*v3.y+h*p0.y, k*p0.y],
     [g,              h,              k]]
H is proportional to Q(destination) * inverse(Q(source))
```

実装はSourceの4面積項を厳密な0で検査する解析的逆変換を使います。`Matrix::tryInverse`の
許容誤差規則は使いません。合成後は係数の最大絶対値で正規化し、H[2,2]を必ず1にするわけではありません。
Scaleだけが異なる係数配列ではなく、変換された点を比較してください。

Sourceの(0,0)、(100,0)、(100,100)、(0,100)をDestinationの(10,20)、(210,20)、
(210,120)、(10,120)へ写す例は、`x'=2*x+10`、`y'=y+20`です。(50,50)は(110,70)になります。
Geometry Testの座標絶対許容誤差は1e-6です。

Sourceの縮退は`std::runtime_error`となる場合があります。Destinationの縮退、非有限係数、
変換時の分母0は網羅的に検査しません。有限・非縮退の座標を与え、射影の分母が0になる位置を避けてください。
任意の入力に頑健なFitting処理ではありません。Setupは計算後に保持係数を置き換えます。
Meshの頂点順とNDC／UV変換は[Core Data Model](CoreDataModel.md)と[OUI Projection](OuiProjection.md)を参照します。

## 画像の向き変換: CORE-ALG-04

`applyOrientation`はPixel／Channel値を変えず、独立した画像を返します。空画像・未知Orientationは
`std::invalid_argument`です。Sourceの幅W、高さH、Pixel(x,y)を次の位置へ書きます。

| Orientation | Target (x,y) | Target幅、高さ |
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

補間や色変換は行いません。`orientedExtent`は寸法計算だけを行い、出力Pointerは有効である必要があります。
このHelper自体は未知Enumを拒否しません。回転後のBayer Pattern維持はCamera Metadataの処理であり、
Coreの1 Channel Imageが持つ性質ではありません。

## 画像累積と丸め: CORE-ALG-05

`ImageAccumulator<Format>`はChannelごとの和、枚数、単一Shapeを所有します。

```text
Empty --add(nonempty image)--> Accumulating(shape, count=1)
Accumulating --add(same shape)--> Accumulating(count+1)
Accumulating --average()--> independent image; accumulator unchanged
any state --reset()--> Empty(width=0, height=0, count=0)
```

空画像・2枚目以降のShape不一致は`std::invalid_argument`です。Shapeの拒否では和・枚数を変更しません。
0枚での`average()`は`std::logic_error`です。`averageImages`も順にaddしてaverageするため、
空Listを拒否します。

和はChannel格納幅16bit以下ならuint32_t、それ以外ならuint64_tです。N枚の各Channelについて
整数除算`(sum + floor(N/2)) / N`を行います。10と21の平均は16です。Alphaも同じ式であり、
Gamma、Alpha重み、画像位置合わせ、Timestamp、自動Resetは扱いません。

和と枚数のOverflow検査はありません。枚数を累積型で表せる範囲に保ち、
`N*maximum_sample + floor(N/2)`が型の範囲内になるよう呼出側で制限します。
8bit／16bit Sampleの64枚は範囲内です。初回addの確保失敗には強い例外保証を定義していません。
その場合はResetするか累積器を破棄します。同時変更は呼出側で同期します。

## Bayer補間と色行列: CORE-ALG-06

`demosaic<SourceFormat,TargetFormat>`はSourceが1 Channel、Targetが3 Channelで、
Sourceは2×2以上である必要があります。Patternの偶奇基準はSourceの(0,0)です。

| Pattern | 第1行／第2行 |
| --- | --- |
| Rggb | R G / G B |
| Bggr | B G / G R |
| Grbg | G R / B G |
| Gbrg | G B / R G |

`Block2x2`は左上から出力Tileを処理します。奇数の最終列・行では、Sampling窓を左・上へ1 Pixelずらして
2×2を確保します。窓内を行優先で走査し、各色の**最初の**Sampleを採用します。Greenも最初の値で、
2つのGreenの平均ではありません。そのRGB組をTile内の有効な全PixelへCopyします。

`Bilinear`という公開Enumの計算は、各出力位置の周囲3×3を画像内へ切り詰め、
Bayer色別の和と個数から`(sum[color] + count[color]/2) / count[color]`を求めるものです。
自位置の色も、同色の他Sampleが窓内にあれば平均されます。端では窓を縮め、鏡映・複製・0埋めはしません。

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

有効なEnumを使い、RGB／BGRの指定とTarget形式を明示的に一致させます。Order引数が実際のChannel配置を
決めます。SampleはTarget格納型へCastされ、Depth正規化やPixelの上位Bitを残す変換は行いません。
本書の基準経路は、Source／Targetを同じ8bitまたは16bit格納幅にします。広い型の和、未知Method／Order、
異なるDepth間の変換は一般的には検証されていません。

`applyColorMatrix`は3×3のdouble行列と**格納順**の3 Channel値を掛け、各結果を
[0, Channel格納型の最大値]へClampし、`value+0.5`を格納型へCastします。
RGB／BGRの自動交換、Offset、Gamma、White balanceはありません。Clamp上限は格納幅であり、
それより小さい論理Bit深度ではありません。RGB8の[10,20,30]に対角[30,-1,0.5]を掛けると[255,0,15]です。
有限係数と8／16bit Channelを基準とし、非有限値や64bit変換の一般保証はしません。

## 検証と変更

Core画像契約はTmr／OUIなしで上記の向き・平均・Bayer／色の固定値を検証します。既存MatrixとProjection
Testが数値計算の根拠です。任意入力の安定性、累積Overflow対策、画質、校正精度、全縮退ケースの認証ではありません。

再実装による受入は[Design Verification](DesignVerification.md)を参照します。丸め、特異判定閾値、
Bayer端処理、Channel配置の変更は観測可能な結果を変えるため、設計・Testのレビューが必要です。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
