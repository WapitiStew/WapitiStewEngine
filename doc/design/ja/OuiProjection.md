# WSE OUI Projection設計

> Canonical source: [English OUI Projection Design](../en/OuiProjection.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書は`wse::oui`が所有するPortable Projection描画の正式設計書である。`ProjectionPipeline`は、
順序付きProjection LayerをBackend非依存`Renderer` Contractへ変換する。各LayerはMesh、Color Texture、
任意Alpha-map Texture、Sampling filter、正規化Opacityおよび任意Edge blendを組み合わせる。Contractへ
Native D3D12、Vulkan、WindowまたはDisplay handleを露出しない。

Projectionは`ProjectionPipeline`、`ProjectionMeshAdapter`およびPortable Rendererの
Opaque Resourceで行う。Projection Meshの`src`はSource Pixel座標である。

## 座標とMesh契約

公開Headerは[ProjectionMeshAdapter.h](../../../api/oui/renderer/ProjectionMeshAdapter.h)と
[ProjectionPipeline.h](../../../api/oui/renderer/ProjectionPipeline.h)、実装は
[ProjectionMeshAdapter.cpp](../../../core/oui/renderer/ProjectionMeshAdapter.cpp)と
[ProjectionPipeline.cpp](../../../core/oui/renderer/ProjectionPipeline.cpp)と内部の
[実行／解放Owner](../../../core/oui/renderer/ProjectionExecution.h)である。Native描画は
[Renderer](OuiRenderer.md)の背後に置く。Mesh adapterは値の変換であり、PipelineはRendererを借用し、
Execute中の一時Supersampling resourceだけを所有する。

- `sRendererVertex2D::position_x/position_y`はD3D／Vulkan共通のNormalized Device Coordinateとする。
  Xは左端-1から右端+1、Yは上端+1から下端-1とする。
- `texture_u/texture_v`は正規化Source座標とする。U／Vは左上0から右下1へ進む。
- `ProjectionMeshAdapter::createMesh()`はLegacy Pixel座標の`float64_mesh2d`を上記座標へ変換する。
  Source／Target Extentはそれぞれ2 x 2 Pixel以上を必要とする。
- 各Legacy Grid cellを一定WindingのTriangle-list 2個へ変換する。空、不整合、非有限、不正Indexまたは
  OverflowするMesh dataはPortable Validation Errorを返す。
- 有限なNDC範囲外のPositionと0～1範囲外のUVは拒否しない。PositionはRasterizerでClipされ、UVは
  Clamp samplerへ渡される。これにより画面端を越える補正Meshを表現できる。
- `Renderer::updateMesh()`は同一Opaque handleのVertex、IndexおよびTopologyを一括更新する。部分更新は行わない。
- 更新用Resourceを完全に生成してから交換する。生成失敗時は旧Meshを維持し、成功後の新しい送信だけが
  更新後Meshを参照する。

### 正確な変換式と三角形分割: OUI-PROJ-01

Source extent `(Ws,Hs)`、Target extent `(Wt,Ht)`、格子点のSource `(sx,sy)`とDestination `(dx,dy)`から、
Doubleで計算してFloatのVertex fieldへ格納する。

```text
position_x = 2 * dx / (Wt - 1) - 1
position_y = 1 - 2 * dy / (Ht - 1)
texture_u  = sx / (Ws - 1)
texture_v  = sy / (Hs - 1)
```

Pixel座標は終点`extent-1`を含む。このMesh座標をExtentそのもので割ってはいけない。VertexはRow-major順を維持する。
1行が`Vw` Vertexの格子で、Cell `(r,c)`を次のように変換する。

```text
LT = r*Vw+c; RT = LT+1; LB = (r+1)*Vw+c; RB = LB+1
append indices [LT, RT, RB, LT, RB, LB]
total index count = 6 * (Vw-1) * (Vh-1)
```

参照Vector: 2行3列、`src=(4*c,4*r)`、`dst=(2*c,2*r)`、Source 9 x 5、Target 5 x 3なら、上中央は
NDC `(0,1)`、UV `(0.5,0)`となる。全Index列は`[0,1,4,0,4,3,1,2,5,1,5,4]`である。
Geometry contractはこのVectorのFloat許容誤差を`1e-6`とする。

## Projection Pass／Layer契約

`sProjectionPassDescription`はColor attachment 1個、任意Render area、正規化Clear color、1個以上の
順序付きLayer、最終Texture stateおよび`supersample_scale`を指定する。PassはAttachmentを常にClear／Storeし、
LayerをVector順に実行する。

- `supersample_scale`の既定値1は中間Targetを使用しない。現在受理する値は1または2だけとする。
- 2を指定する場合、最終出力Extentを`render_area.extent`へ明示する。空Extentと2倍時の整数Overflowは
  Validation Errorとする。
- 2倍時は最終Extentの幅／高さをそれぞれ2倍した内部RGBA8 TargetへLayerを描画し、その全域を
  Linear clamp sampling／Replace blendで最終Render areaへ縮小する。

`sProjectionLayerDescription`には次の規則を適用する。

- `mesh`と`source_texture`は有効なOpaque handleを必須とする。
- `alpha_texture`は任意であり、未指定は全要素0のHandleだけで表す。
- `sampling_filter`は`Nearest`または`Linear`とし、Projectionの既定値は`Linear`とする。
- `opacity`は有限かつ0以上1以下とする。
- `edge_blend`はTexture UVの左／右／上／下からの減衰幅と、`Linear`または`Smoothstep`曲線を指定する。
  各幅は有限かつ0以上1以下とし、0はその辺の減衰を無効にする。
- Source／Alpha Textureは`Sampled` Usageを持ち、現在のColor attachmentと同一にできない。

Generic `sMeshDrawCommand`もSampling、Alpha、Blend、OpacityおよびEdge blendを公開し、Backend Contractを
Projection Facadeへ依存させない。`ProjectionPipeline`はProjection drawを`SourceAlpha` Blendへ固定する。

## Sampling／Alpha／Blend／Color契約

- Nearest／Bilinear filterはU／V方向にClamp addressingを使用する。
- Color／Alpha Textureは同じFilterとUVでSamplingする。
- RGBA8／BGRA8 Alpha mapはAlpha channel、R8 Alpha mapはRed channelをAlpha係数として使用する。
  使用しないColor channelは合成へ影響しない。
- Edge blendは補間後UVで計算する。各辺について`width > 0`のとき、辺からの正規化距離を`width`で割り、
  0～1へClampした値`t`を得る。`Linear`は`t`、`Smoothstep`は`t²(3-2t)`を係数とする。
- 有効な四辺係数は乗算する。左右または上下の幅が重なることを許可し、重複領域でも同じ乗算則を使う。
- 最終Source Alphaは`texture alpha * alpha-map factor * edge factor * layer opacity`とする。
- Alpha mapは任意形状や外部Calibration済みMaskを保持する互換経路であり、四辺曲線は共通的なFadeを
  Textureなしで表す補助契約である。両方を指定した場合は乗算する。
- Straight-alpha Color合成は`source.rgb * source.alpha + destination.rgb * (1-source.alpha)`とする。
- Destination Alphaは`source.alpha + destination.alpha * (1-source.alpha)`とする。
- LayerはVectorの先頭から描画し、後のLayerを先に描画したLayerの上へ合成する。
- 現D3D12／Vulkan経路はRGBA8／BGRA8 Color／Alpha Textureと、非RenderTarget用途のR8 Sampled Textureを受理する。
  sRGB Transfer変換は未実装である。
- RGBA8／BGRA8値をLinear numeric UNORMとして扱う。必要なColor-space変換はこのPipelineの外で行う。

### Samplingと合成の数値例: OUI-PROJ-02

CPU Bilinear参照はTexel中心をSampleする。正規化 `(u,v)` と `W x H` Textureに対して次を用いる。

```text
x = clamp(u*W - 0.5, 0, W-1); y = clamp(v*H - 0.5, 0, H-1)
x0 = floor(x); x1 = min(x0+1, W-1); fx = x-x0
y0 = floor(y); y1 = min(y0+1, H-1); fy = y-y0
sample = (1-fy)*((1-fx)*C[x0,y0] + fx*C[x1,y0])
       + fy*((1-fx)*C[x0,y1] + fx*C[x1,y1])
```

このSampling式は、前述のMesh Pixel→UV変換と異なりTexture extent全体を使う。一様な赤Source `(1,0,0,1)`、
Alpha-map係数0.5、Opacity 0.5、左Smoothstep幅0.5を`u=0.25`でSampleすると、`t=0.5`、Edge係数0.5、
有効Source alpha 0.125となる。不透明な青 `(0,0,1,1)` への合成は `(0.125,0,0.875,1)`、RGBA8で約
`(32,0,223,255)`である。入力係数のUNORM符号化とBackendの丸めは1 LSBのTest許容差に従い、
Premultiplied-alpha入力ではない。RGBA8 Attachmentへの各書込みで量子化し、Scale 2の中間Targetも量子化後に
次PassがSampleする。未量子化の最終式だけで比較すると中間の丸めを再現できない。

## Resource／実行契約

- 成功したProjection送信はFenceを返す。非同期実行を許容するが、現Vulkan adapterは内部待機の完了後に返す。
  Renderer設計のOUI-SUBMIT-03を参照する。
- 2倍時はSupersample passとDownsample passを同一RendererのSubmission順で実行し、後段Fenceを返す。
  内部RGBA8 TextureとFullscreen MeshのOpaque handleは送信後に解放するが、Backend ResourceはFence完了まで保持する。
- Callerが対応Opaque handleを破棄した場合も、Mesh、Source、Alpha map、結合Descriptor heapおよび
  Attachment Resourceを返却Fence完了まで保持する。
- Mesh更新前に送信済みの描画は旧Vertex／Index ResourceをFence完了まで保持する。更新後の送信は同じ
  Opaque handleから新Resourceを取得するため、更新前後の描画が相互に破壊されない。
- Validation／Backend失敗は`RendererResult`で返す。Dummy成功やAlpha mapの暗黙無効化を行わない。
- `ProjectionPipeline`はCallerのRenderer、Texture、Mesh、SurfaceまたはFenceを所有しない。

### Pass構築と失敗順序: OUI-PROJ-03

`execute(renderer*, description)`は先にRenderer pointerとDescriptionの構造を検査する。非零Handle検査は
Backend resourceの存在を保証せず、実行時にRendererが解決する。Scale 1では、入力順のSourceAlpha layerを
持つClear／Store passを1つ構築し、その結果を返す。TextureやMeshは作成しない。

Scale 2は次の順序で処理する。

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

中間Targetは`2*render_area.extent`、RenderTarget／Sampled usage、初期StateはRenderTargetである。
Pass 1は原点 `(0,0)`、2倍Extent、元のClear colorとLayer、最終ShaderResource stateを使う。
Fullscreen meshのPositionは `(-1,1),(1,1),(1,-1),(-1,-1)`、UVは `(0,0),(1,0),(1,1),(0,1)`、
Indexは`[0,1,2,0,2,3]`である。Pass 2は実Attachmentを元のRender areaでClear／Storeし、Linear sampling、
Replace blend、Opacity 1、Alpha map／Edge blendなしで描画し、要求された最終Stateへ遷移する。

狭い画面では表を横Scrollして各列を確認してください。

<div class="wse-projection-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| 失敗箇所 | 解放／返却結果 | 既に起き得ること |
| --- | --- | --- |
| 検査またはTarget作成 | 当該Errorを返す | 解放対象の一時Resourceなし |
| Mesh作成 | Targetを破棄し、Mesh errorを返す | Targetは確保済み。解放結果より元Errorを優先 |
| Pass 1 | Mesh、Targetの順で破棄し、Pass 1 errorを返す | 失敗前にNative送信を開始している可能性 |
| Pass 2 | 両方を破棄し、Pass 2 errorを返す | Pass 1完了の可能性。Transaction巻戻しはない |
| 2つの成功後の解放 | 両方の破棄を試行。Mesh errorを優先し、なければTarget error | 両Pass送信済みでも、解放失敗時は成功Fenceを公開しない |
| 全段階成功 | Pass 2のFenceを返す | 一時Handleは消去済み。未完了Native resourceは必要な間保持 |

</div></div>

両Pass／MeshのCPU Vectorは一時GPU Handle取得前に構築する。返された一時Handleは直ちに1つのScope ownerが
引き取り、成功・Result失敗・例外巻戻しの全経路でMesh、Textureの順に1度ずつ破棄を試行する。一方の解放失敗や
例外でも他方を試行する。元の操作が失敗または例外なら、解放の失敗を副次として元のResult／例外を保持する。
両Pass成功後は、Structured cleanup errorならMesh、Textureの順に優先する。解放中に例外があれば両方を試行後に
最初の解放例外を再送出する。低水準C++ APIの例外は伝播し得るため、No-throw APIではない。

破棄失敗では、VulkanのPending submissionなどにより資源がRendererへ登録されたまま残る場合がある。Pipelineは
再試行や内部Handleの返却をせず、即時解放を保証しない。資源OwnerはShutdown／破棄までRendererである。復旧不能な
実行／解放失敗の後は、Applicationが自身の資源を使い終えてからRendererを終了する。ShutdownのGPU待機制約は
[Renderer設計](OuiRenderer.md)に従う。Handle返却前に起きるBackend内部の確保例外はBackendが解放責任を持つ。
送信済みPassの巻戻しはないため、Execute失敗をAttachment無変更と解釈しない。

Handleを公開しないBinding Facadeの`renderProjection()`は、Scale 2を含めて要求出力Width／Heightを明示Render areaへ
設定する。ローカルRendererがReadbackまたは失敗まで全資源を所有する。`wse.oui.binding_projection_contract`は
一様・不透明なScale 2 Frameの全Byteを両Software backendで確認する。既存の非一様Pipeline goldenは引き続き
2段階の量子化を検証する。

## 現Backend／検証範囲

狭い画面では表を横Scrollして各列を確認してください。

<div class="wse-projection-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| 契約 | 実行可能な入口 | 限界 |
| --- | --- | --- |
| OUI-PROJ-01 | [Geometry vector](../../../test/characterization/projection_geometry_contract.cpp) | 固定座標と2 Cellの全Index順序。全退化形状を網羅しない |
| OUI-PROJ-02 | `wse.oui.projection_scale_alpha_golden`、`wse.oui.projection_edge_multisource_golden` | CPU参照と1 LSB許容差。Color calibrationではない |
| OUI-PROJ-03 | Supersample／Dynamic-mesh golden、Binding Scale 2 Frame、`wse.oui.projection_cleanup_contract` | Fake backendの117条件でStatus／例外／確保失敗と副次解放を検査。Native driverの確保／送信故障は未注入 |

</div></div>

Windows D3D12とLinux Vulkan 1.2は、Source／Alpha Texture、Nearest／Linear clamp sampler、Draw単位Constant、
Replace／Straight-alpha Blend、四辺Edge blendおよびDynamic Meshを同じPortable Contractで実装する。
自動検証の基準AdapterはD3D12 WARPとUbuntuのSoftware Vulkan adapterとする。

決定論的Projection TestはLegacy Fullscreen meshを変換し、2 x 2 RGBA Sourceを4 x 4へLinear拡大し、
不均一Alpha mapとLayer opacity 0.75を適用する。全Color channelをCPU Bilinear／Straight-alpha参照と
RGBA8 1 LSB以内で比較する。WARPのRGBA FNV-1a完全一致値は`0xce4b43253436392e`、Ubuntu Vulkan値は
`0x277fcfc63b3185a2`であり、両方ともCPU参照との差は最大1 LSBである。

2段Projection Testは同じSourceと単Channel R8 Alpha mapを8 x 8内部Targetへ描画し、4 x 4へLinear縮小する。
CPUでRGBA8中間量子化を含む2段参照を生成し、全Channelを1 LSB以内で比較する。WARPのRGBA FNV-1a
完全一致値は`0xa3ccfe45a48e2d20`、Ubuntu Vulkan値は`0xbdb24c2a72bce0b4`であり、両方ともCPU参照との
1 LSB許容差を満たす。

Edge／Multi-source Testは8 x 4 TargetへRedの右Smoothstep Fadeを先に、Opacity 0.75のBlue左Smoothstep
Fadeを後に描画する。CPUは各Draw後のRGBA8量子化と順序付きStraight-alpha合成を再現し、全Channelを
1 LSB以内で比較する。WARPとUbuntu VulkanのRGBA FNV-1a完全一致値はともに
`0x17b97761907effd5`である。

Dynamic Mesh Testは同一Mesh handleを左半分形状から右半分形状へ、最初のFenceを待たずに更新する。
更新前後を別の8 x 8 Targetへ送信し、送信直後にMesh／Source handleを破棄してからFenceを待つ。
旧形状の左半分RGBA FNV-1aは`0xbbb816f5e80169e5`、新形状の右半分は
`0xb39c749492b617e5`である。D3D12とVulkanが同じ2値へ完全一致し、更新前後のResource保持と
Handle再利用を同時に固定する。

Backend共通Example `example/cpp/oui/portable_projection.cpp`は同じ2 x 2 RGBA SourceとFullscreen meshを
D3D12 WARPおよびUbuntu Vulkanで
4 x 4へNearest描画し、0 LSB差とRGBA FNV-1a `0xe0f9517b5bf10dc5`の一致を要求する。これは
CTestからも実行し、Windows／Linuxで同じ公開`ProjectionPipeline`利用手順を固定する。

Wayland表示Gateは同じVulkan Projection Pipelineから2 x 2 Source、Linear samplingおよび左右Smoothstep
Edge blendをWindow Surfaceへ描画する。Windowed、Borderless Fullscreen、Windowed復元、128 x 96 Resize後の
各状態で描画とPresentを完了し、Resize前のSwapchain Texture handleが安全に無効化されることを要求する。

## 明示的な未対応範囲

- 2より大きいSupersample倍率、任意倍率、再利用可能な中間Targetおよび品質／Memory Policy。
- Gamma-aware／sRGB合成、Photometric補正およびColor Calibration。
- 任意のCustom Edge curve、Projector固有Edge blend設定およびCalibration Mask生成。
- Vertex／Index範囲だけの部分更新、GPU書込みMeshおよびCaller側からの同一Renderer同時更新。
- 物理Display切断中のProjection継続／再接続、DRM-KMS上の代表Projection sceneおよびDirect Display復元の実機受入。
- 長時間連続動作、Memory leak、温度およびFrame時間のRaspberry Pi 4／5実機受入。

未対応機能をSingle-layer Windows WARP baselineで認証済みとして扱ってはいけない。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
