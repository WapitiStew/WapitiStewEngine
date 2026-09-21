# WSE 開発者向け実習

> Canonical source: [English Developer Walkthrough](../en/DeveloperWalkthrough.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と読む順序

[Architecture](Architecture.md)、[Core Data Model](CoreDataModel.md)、[Tmr Camera](TmrCamera.md)、
[OUI Renderer](OuiRenderer.md)、[Projection](OuiProjection.md)をつなぎ、既存のOffscreen Exampleと
別のFake Camera Testを、入力から解放まで追います。各Component設計を契約の正本とし、本書は組立てと
観測の手順を示します。既存Exampleの実行は手順付き検証であり、初見の独立再現や実機受入とは区別します。

Renderer ExampleはCoreを推移的に含む `WSE::Oui`、Fake Camera Testは `WSE::Tmr` をLinkします。
TmrがOUIを起動することはありません。Applicationが所有CPU値を明示的に接続します。
Testで使うCameraの内部注入箇所はInstall対象の公開APIではありません。

## 画像入力からProjection・読戻しまで: WALK-IMAGE-01

完成した [portable_projection.cpp](../../../example/cpp/oui/portable_projection.cpp) を使います。
CTest名は `wse.oui.backend_projection_golden` です。WindowsはD3D12、LinuxはVulkanを選び、
Software Adapterを要求してOffscreen／Mesh能力を検査します。CameraやDisplay Surfaceは開きません。

```text
caller-owned Core RGBA image
    -> uploadTexture -> upload fence -> wait
caller-owned mesh identity + source texture identity
    -> ProjectionPipeline (borrows Renderer)
    -> backend submission -> render fence -> wait
offscreen surface owns target texture
    -> readTexture -> independent packed CPU frame -> check -> release owners
```

| 段階 | Exampleの入力・操作 | 所有と観測 |
| --- | --- | --- |
| 初期化 | BackendとSoftware Adapterを明示し、必要能力を検査 | RendererがBackendを所有。初期化／能力のErrorで中断 |
| 入力Texture | 2 x 2 RGBA8、SampledとTransferDestination、初期CopyDestination | ApplicationがSource Textureを所有 |
| Upload | 上段は赤／緑、下段は青／白、全Alpha 255 | Core Imageは論理16 Byteを所有。Uploadで複製し、呼出終了後は入力を解放可能。ExampleはUpload完了を待つ |
| Mesh | Position (-1,1)、(1,1)、(1,-1)、(-1,-1)、UV (0,0)、(1,0)、(1,1)、(0,1)、Index [0,1,2,0,2,3] | ApplicationがMesh IDを所有。直接指定した頂点順であり、Grid AdapterのRow-major順とは区別 |
| 出力 | 4 x 4のOffscreen Surfaceを作り、Textureを取得 | SurfaceがTargetを所有。Target Textureだけを直接Destroyしない |
| Submit | Nearestの1 Layer、Opacity 1、Alpha Map／Edgeなし、Scale 1、最終CopySource | Projectionは1回のClear／Store Passを組み、Rendererを借用。Fenceが送信済み処理を示す |
| 読戻し・照合 | Render Fenceを待ち、Targetを読む | Packed RGBA8で正確に64 Byte、GPU格納から独立したCPU Frameを返す |
| 終了 | Surface、Mesh、Source TextureをDestroyし、RendererをShutdown | Exampleは描画結果を照合後に解放。途中ReturnはRendererの破棄に依存し、末尾の解放StatusはこのExampleで検査していない |

出力は各2 x 2 Pixelの区画になり、左上が赤、右上が緑、左下が青、右下が白、全Alphaが255です。
Exampleは全Channelを許容差0 LSBで照合し、RGBA FNV-1a `0xe0f9517b5bf10dc5` も確認します。
このFixtureは完全一致ですが、Linear／Alphaの例にはOUI-PROJ-02の別の許容差を使います。

Resultの成功を確認してからValueへ進みます。Upload／Submit／Readの失敗は「処理未開始」や「出力未変更」を
意味しません。Surface所有、Pending Resource保持、解放失敗はOUI-OWNER-01とOUI-PROJ-03に従います。
Exampleは全失敗注入Testではなく、Shutdown時間の上限も保証しません。

### Camera Frameを明示的に接続する

`Bgra8` Camera Frameは [CameraFrameOps](../../../api/tmr/camera/CameraFrameOps.h) で
`img4c08_bgra_t` へ変換し、Rendererの `Bgra8Unorm` Uploadへ接続できます。`toImage()` は
Packedな所有Imageを作り、Camera Bufferを借用し続けたり暗黙にChannelを入れ替えたりしません。
RGBAを選ぶなら明示的にChannel変換します。Upload前にWidth／Height／Format／Usageを一致させます。
これはApplicationの組立て規則であり、上記Offscreen ExampleがCamera取得も検証する意味ではありません。

BGRA8 3 x 2、Stride 16ではCameraは32 Byte以上を要求し、余分な末尾領域を許容します。
Packed Core Payloadは24 Byteです。同じStrideのRenderer Frameは正確に32 Byteを要求します。
両者のValid規則が同じと仮定してDescriptorを流用してはいけません。

## Fake Cameraの配信と復旧: WALK-CAMERA-02

TMR-CALLBACK-03の後に [tmr_camera_backend_contract.cpp](../../../test/characterization/tmr_camera_backend_contract.cpp)
を確認します。内部の `MockCameraBackend` がDevice列挙／Openなしで人工Frameと決定的な失敗を供給します。
CameraSessionへ注入し、次の分岐を駆動します。

| Event／操作 | 配信状態 | Backend／Ownerの責務 |
| --- | --- | --- |
| Open後にCallback付きStart | Worker開始。同期Readを重ねるとConcurrentRead | SessionがBackendと複製したCallableを保持 |
| Read Timeoutが2回 | どちらもCallbackへ通知せず、次のReadへ進む | BackendはStreamingを維持 |
| 3回目にReadFailed | 失敗Callbackを1回だけ送り、配信終了 | MockはStreamingを維持。`isStreaming()` だけでは配信中と判定できない |
| 別分岐: 正常FrameのCallbackが例外 | 例外を捕捉して配信終了、WorkerのCaptureを解放 | Backendは暗黙停止しない |
| Owner側Stop | WorkerをJoinしCallbackStateを解放 | Backend停止。再Start可能 |
| Callback側Stop | Stop要求、自己Joinをしない | 破棄／再開前にOwnerがWorkerを回収する |
| Read待機中にOwnerがStop | Read完了後にNative stop。取消されたResultは通知しない | OwnerのJoinが完了境界 |
| Close | Stop、Backend Close、所有参照を解放 | 繰返しCloseは無害。実機証拠は生じない |

Mockの正常FrameはBGRA8 2 x 2、Stride 8、所有16 Byteです。複数Sessionは人工の連番を個別に持ちます。
Callback引数はLoop内のResultを借用します。Callback終了後にFrameを保持するなら、成功FrameをCopyします。

学習用に状態モデルを再構成するときは、Backend Streaming、Worker Active、Join待ち、Callable所有を
別Fieldで記録します。Timeout／Timeout／Errorと、Success／Throwを入力し、Testを読む前に上表と比べます。
[開始／停止故障注入](../../../test/characterization/tmr_camera_start_stop_contract.cpp)では、Callable copy／Thread生成失敗、
巻戻し失敗、Read例外、Owner／自己Stop競合も確認します。Linuxの[V4L2 lifecycle test](../../../test/characterization/tmr_v4l2_lifecycle_contract.cpp)は
実Adapterに人工Native呼出しを注入し、Deviceを開きません。
Fake Backendの速いStopはNative Driverの取消遅延を証明しません。
Test内の2秒Polling期限もTest実行上の制限であり、API時間保証ではありません。

## 手順付き実習のBuildと実行: WALK-RUN-03

[Build Guide](../../ja/BuildGuide.md) に従って依存とCompilerを準備済みにし、WSE Rootから実行します。
ConfigureはOfflineです。新規Build Directoryを使い、物理Camera／Keyboard／Display ModeのGateを選びません。
Linuxには対応Vulkan開発／Runtime依存と利用可能なSoftware Adapterが必要です。
TmrをBuildしても、ここでのCamera実習はMockを使います。

Windows PowerShell:

```powershell
cmake --preset windows-msvc-shared-public -B build/design-walkthrough-win -DWSE_BUILD_TESTING=ON -DWSE_ENABLE_CAMERA_HARDWARE_TESTS=OFF -DWSE_ENABLE_IUI_HARDWARE_TESTS=OFF -DWSE_ENABLE_DISPLAY_MODE_TESTS=OFF
cmake --build build/design-walkthrough-win --config Debug --parallel 4
ctest --test-dir build/design-walkthrough-win -C Debug --output-on-failure -R "^wse[.]tmr[.]camera_(backend_contract|frame_ops)$|^wse[.]oui[.]backend_projection_golden$"
```

Linux Shell:

```sh
cmake --preset linux-gcc-shared-tmr -B build/design-walkthrough-linux -DCMAKE_BUILD_TYPE=Debug -DWSE_BUILD_OUI=ON -DWSE_ENABLE_LIBCAMERA=OFF -DWSE_BUILD_TESTING=ON -DWSE_ENABLE_CAMERA_HARDWARE_TESTS=OFF -DWSE_ENABLE_IUI_HARDWARE_TESTS=OFF -DWSE_ENABLE_DISPLAY_MODE_TESTS=OFF
cmake --build build/design-walkthrough-linux --parallel 4
ctest --test-dir build/design-walkthrough-linux --output-on-failure -R "^wse[.]tmr[.]camera_(backend_contract|frame_ops)$|^wse[.]oui[.]backend_projection_golden$"
```

選択されるTestは正確に3件です。0件は成功扱いにせず、Component Optionと登録を確認します。
失敗時は診断とBackend識別情報を保存し、実機成功への読替えや失敗Exampleの黙示除外をしません。
C ABIとInstall後のConsumerは [C ABI](CAbiContract.md) と [Build Packaging](BuildPackaging.md) に従います。
このSource Tree実習だけではInstall後のLoadを検証したことになりません。

## 入力とメモリの境界: WALK-BOUND-04

Validationは表現と対応操作を検査するもので、全処理の負荷上限ではありません。
外部の寸法・件数・Requestを受ける前にApplicationの予算を決め、確保前の積算でOverflowを検査します。

| 境界 | 既存の検査／限界 | Application側の判断 |
| --- | --- | --- |
| Core添字／Copy | 未検査の行／Channelアクセスは正しい添字が前提。値のCopyは独立領域を確保 | 寸法と添字を検査し、保持Copy数と借用Viewの寿命を制限 |
| GEF BIN／CSV | Tag／Layout検査。総File／Count予算の設定なし。一般的な引用CSV Parserではない | Controllerへ渡す前にFile量・Block数・復号Memoryを制限し、失敗後は新しいControllerを使う |
| XPT HTTP／転送 | Body制限と部分転送契約。Headerを含む単一の総Memory予算はない | Body／処理量と蓄積Metadataを制限し、部分Byte数を消費して無条件再送を避ける |
| Tmr Frame／Callback | Stride／Format／格納を検査。Frameは所有値、Callbackは読取Worker上 | 寸法と保持Frame数を決め、Callbackを短くするかApplicationにQueue／Drop方針を設ける |
| OUI Resource | Extent／Usage検査。送信済み処理がBackend資源を保持 | Texture数、未完了Submit数、Readback数を制限。Handle破棄だけでPending GPU Memoryは直ちに消えない |
| Binding／C ABI | Native所有Copyと利用側の出力領域。Errorは操作別 | Native／Managed Bufferが同時に存在する量を数え、Ownerを解放し各言語のCallback境界で例外処理 |

論理PayloadとProcess／GPUのPeak Memoryは分けて見積もります。

```text
packed image bytes = width * height * channels * bytes_per_channel
camera BGRA bytes >= effective_stride * height
renderer BGRA frame bytes = effective_row_pitch * height
retained packed camera payloads = retained_count * packed_image_bytes
RGBA8 final target = 4 * W * H
scale-two intermediate = 4 * (2W) * (2H) = 16 * W * H
final + scale-two intermediate = 20 * W * H
```

1920 x 1080のPacked RGBA8は8,294,400 Byte、最終TargetとScale 2中間Targetの合計は
41,472,000論理Texture Byteです。Source Texture、Row Alignment、Staging／Readback、確保Metadata、
Driver管理領域、Managed Copy、以前の未完了Submitは含めません。Peak Memory保証や容量予約ではありません。

Camera→ImageとUploadは明示的なCopy境界です。C ABIの `runtime_copy_frame` と
`frame_buffer_copy_to` ではNativeと利用側に別領域が生じます。この所有図から実装の一時Copy総数や
Zero-copy経路を推定してはいけません。

## 待機と計測: WALK-MEASURE-05

| 操作 | 時間の解釈 |
| --- | --- |
| Core Timer Callback | Callback完了後の固定Delay。Callback時間が周期へ加わる |
| IUI Callback解除 | 登録解除は取得済みのInvocationをJoinしない |
| XPT Deadline／Cancellation | 操作共通の予算。名前解決、Scheduling、Native取消で指定待機時間を超え得る |
| Tmr Callback／Stop | 内部100 ms Read要求はShutdown上限ではない。利用側CodeとNative Stopの時間が加わる |
| OUI Readback／Shutdown | Vulkan Readbackは利用側TimeoutをNative Fence待機へ渡し、未解決処理を保持する。準備・Copy・Shutdownの共通時間上限はない |
| Binding Runtime Wait | Sleep区間ごとに取消を観測するが、Schedulingのため厳密な応答時間保証ではない |

詳細の正本は [Core Services](CoreServices.md)、[Thread Ownership](ThreadOwnership.md)、
[XPT](XptTransport.md)、Tmr、OUIです。実測遅延を契約上の上限へ読み替えません。

既存のOpt-in [添字Benchmark](../../../test/benchmark/indexing_benchmark.cpp) は、同じ入力と交互の
測定順でCore Matrix／Image・Pixel走査と直接Pointerを比較します。
準備済みCore構成から別のRelease計測Directoryを作り、`WSE_BUILD_TESTING=ON` と
`WSE_BUILD_BENCHMARKS=ON` を指定し、Target `wse.bench.indexing` をBuildします。
CTestは `-R "^wse[.]bench[.]indexing$" -V`、Windowsでは `-C Release` も指定します。
和の一致を検査して計測値を出し、絶対時間の合否Gateは設けません。

ApplicationのPipelineでは、Capture完了、変換、Upload戻り／Fence完了、Submit戻り／Fence完了、
Readback、Callback、Owner Stopの前後へ単調時刻を記録します。
Warm-up／Sample数を明示し、入力とBuild構成を固定して中央値・裾の分位・最大値・失敗数を報告し、
初回起動は分けます。OS、Architecture、Compiler、最適化、Adapter、Extent、Format、Stride、
Layer数、Scale、未完了Frame数も残します。Epoch未指定のCamera時刻をHost時計から直接減算しません。

同じ処理境界でProcess MemoryとAdapter別のGPU／Staging Memoryを観測します。定常的な保持量を比べる前に、
送信を止め、完了を待ってOwnerを解放します。CPU／GPU時間、CTest実行時間、Device時刻差は別の測定です。
本書はFPS、共通Shutdown期限、最大入力量、Peak Memory上限を保証しません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
