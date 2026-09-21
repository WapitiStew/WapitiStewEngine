# WonderStewEngine 公開API 操作仕様書

> Canonical source: [English Public API Reference](../en/API_REFERENCE.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

本書はInstall Packageの`include/wse/api`配下に含まれる公開Headerを対象とした操作仕様です。実装詳細、
機種依存Extensionおよび公開Packageに含まれないComponentは扱いません。

非推奨宣言、正規Replacementおよび削除条件は[非推奨API・移行Guide](DeprecationMigration.md)を参照してください。
Platform、Ownership、Thread、Callback、ShutdownおよびErrorの共通規則は
[Architecture](../design/ja/Architecture.md)、[Thread／Ownership Model](../design/ja/ThreadOwnership.md)、
[公開API Policy](../design/ja/PublicApiPolicy.md)で定義し、以下のComponent章から詳細契約を参照します。

C ABI診断は動的確保せずUTF-8最大1023 ByteとNULを保持します。C#のNative呼出しは戻るまでSafeHandle入力を
保持しますが、Disposeで処理を取り消しません。可変操作は引き続き直列化します。配置と寿命規則は
[C ABI](../design/ja/CAbiContract.md)を参照してください。

## Coreデータの添字アクセス

`matrix[y][x]`、`homography[y][x]`、`image[y][x][channel]`、`pixel[channel]`、`mesh[y][x]`で
非const／constの領域へ直接アクセスします。有効な添字、領域の寿命、検査付きAPIは
[Portable Coreの契約](../design/ja/PortableCore.md#データの高速アクセス)を参照してください。
[Core Data Model](../design/ja/CoreDataModel.md)は型の関係、行優先配置、Channel順序、所有、外部Buffer配置を説明します。
[Core演算設計](../design/ja/CoreAlgorithms.md)は式と数値例、
[Core共通サービス設計](../design/ja/CoreServices.md)はTimer・Log・LicenseのLifecycleを説明します。

```cpp
wse::Matrix matrix(2, 3); // rows, columns
matrix[1][2] = 42.0;
const auto& view = matrix;
const auto* row = view[1];
wse::img3c8_t image(640, 480);
image[10][20][2] = 255;
```

## 前提

- 対象: C++17 / Windows x86-64、Linux x86-64、Linux ARM64（ModuleごとのSupport matrixに従う）
- インクルードパス: `<install-prefix>/include/wse/api`
- 公開API入口: `wse/xpt/tmr/iui/oui/gef` 各 `stew.h`
- OpenCV連携: Opt-inの`<cv/OpenCvAdapter.h>`（`wse::ocv`の変換関数）を使用

## 入口ヘッダ

BINのBlock配置、対応する設定CSV文法、Read／Writeの制約は
[GEF File形式](../design/ja/GefFileFormats.md)を参照してください。

GEFに`ReadLimits`、`readWithLimits`、CSVの`readReplace`、両Controllerの`writeAtomic`を追加しました。
従来CSVの`read`は追記を維持し、BIN読込は成功時だけ置換して再Writeできます。
外部入力には上限付きAPI、保存失敗時に元Fileが必要な場合はAtomic置換を使います。

| モジュール | 入口ヘッダ | 用途 |
| --- | --- | --- |
| wse | `<wse/stew.h>` | コア（データ構造、ログ、ユーティリティ、ライセンス） |
| xpt | `<xpt/stew.h>` | Portable通信（TCP、UDP、HTTP、Serial、Retry） |
| tmr | `<tmr/stew.h>` | カメラ制御 |
| iui | `<iui/stew.h>` | 入力（Keyboard など） |
| oui | `<oui/stew.h>` | 出力表示／レンダリング |
| gef | `<gef/stew.h>` | CSV／BIN設定Format |
| link | `<link/*.h>` | 外部連携Macro |

## 共通仕様 (wse)

### ログ

- `wse::registDefaultLog()` で既定ログを登録
- 出力: `WLog()` / `DLog()`、設定変更: `changeDefaultLog(...)` / `changeDevelopLog(...)`
- ログ出力先は `eLogMode`（`CONSOLE`/`LOG_FILE`/`FULL`）で制御

### 例外 / エラー

- 使い方違反: 標準Exception（`std::invalid_argument`／`std::out_of_range`／`std::domain_error`／
  `std::logic_error`）
- 計算上起こり得る失敗: `CoreResult`（`wse::CoreError`）。License操作は`LicenseResult`、
  各Componentは自身のResult型を返す
- Legacyの`wseException_`／`wse::ErrorCode`は2026-09-14にLegacy削除Programで削除済み
  （[非推奨API・移行Guide](DeprecationMigration.md#ja-legacy-removal-program)参照）

### データ構造

- 座標・範囲: `Point2_`, `Point3_`, `Point4_`, `Range1D/2D/3D`, `Size_`
- 行列: `Matrix_`（`determinant` / `transpose` / `inverse`）
- 変換: `Homography::setup(...)` / `transform(...)`
- メッシュ・セル: `Mesh`, `Cell`

### 画像

- `Image_<ePixFormat>` と別名 `img1c16_t`, `img3c08_t`, `img4c16_t` などを使用
- `ePixFormat` でチャンネル数・ビット深度を指定
- OpenCV連携: `<cv/OpenCvAdapter.h>`の`wse::ocv::fromCvMat`／`toCvMat`で`cv::Mat`と相互変換

### スレッド/タイミング

- Worker Threadは内部実装。Thread・Atomic・Mutexを公開するClassはなく、Legacyの`BMultiThread`
  Worker基底は2026-09-13に削除（[非推奨API・移行Guide](DeprecationMigration.md#ja-core-worker-timer)参照）
- タイマ: `Timer`はWorkerを内部所有するfinalなClass。`start(std::chrono::milliseconds, TimerCallback)`、
  `setInterval()`、`stop()`、`isRunning()`。旧`initialize()`等の面は2026-09-13に削除
  （[非推奨API・移行Guide](DeprecationMigration.md#ja-core-worker-timer)参照）
- 待機: `Wait::sec/msec/usec/nsec`

### 変換/取得ユーティリティ

- 画像フォーマット変換: `castData::image<>()`, `castData::to4chAlphamap<>()`
- デバイス情報取得: `pickupDeviceInfo::Serials/Networks/Monitors/Keyboards/Mouses/Touchscreens/Gamepads`

### ライセンス

- キー生成/読み込み: `Licensekey::genrate` / `Licensekey::load`
- ライセンス書き込み: `LicenseWriter::save`
- ライセンス読み込み: `License::load`
- Error: 各操作は`LicenseStatus`／`LicenseResult`（`wse::LicenseError`）を返す。二重Loadは
  `std::logic_error`、範囲外のWriter Enumは`std::invalid_argument`

## Portable通信 (xpt)

- 共通Operation: `Endpoint`、`Timeout`、`CancellationSource`、`OperationContext`
- Structured Error: `TransportError`、`TransportResult<T>`
- Network: `TcpClient`、`UdpClient`、`UdpDatagram`
- `TcpClient::checkPeerConnection()`はPending byteを消費せずOSが観測できるFIN／Resetを確認する。成功は
  Remote生存保証ではない
- HTTP: `HttpClient`、`HttpRequest`、`HttpExecutionOptions`、`HttpResponse`。Request・Options・Contextは別引数。
  libcurl型は公開しない
- Serial: Windows／Linux共通`SerialPort`が唯一のSerial面。Legacy `SerialConnector`は2026-09-13に削除
  （[非推奨API・移行Guide](DeprecationMigration.md#ja-xpt-transport)参照）
- Retry: `RetryPolicy`、`RetryDecision`。非冪等Operationを暗黙再送しない
- TCP再接続はValidation後に旧Socketを閉じる。UDP再BindとSerial再Openは成功時だけ置換を確定する

Owner／状態表、部分進捗、Deadline／後処理の限界、HTTP試行順序、Retry計算の詳細は
[`XPT Transport設計`](../design/ja/XptTransport.md)を参照してください。

PythonのWseTransferError、JavaのTransferException、JavaScriptのTransferError Field、C#のTransferExceptionは、
Send失敗の完了数とUDP切詰めのPrefix／送信元を保持します（BIND-TRANSFER-05）。既存の基底Catchも有効です。
Countを読んで復旧方法を判断し、元Payloadを自動再送しません。[Binding失敗時の値](../design/ja/LanguageBindings.md)を参照します。

## カメラ操作 (tmr)

### WebCamera

- 列挙／Capability: `WebCamera::enumerate()`、`WebCamera::capabilities()`
- Profile選択: `sCameraStreamProfile`の`native_format`と`output_formats`から
  `sCameraStreamConfiguration`を構成する
- Lifecycle: `open()`、`start()`、`readFrame()`またはCallback、`stop()`、`close()`
- Capability／Control: `currentCapabilities()`、`controlCapability()`、`getControl()`、`setControl()`
- UVC Extension Unit: `getExtensionUnit()`、`setExtensionUnit()`。広告済みSelectorと既知Schemaだけを使用する
- 状態: `isOpen()`、`isStreaming()`。C++の`close()`は冪等なSession終了で、通常の同じOwnerを再Openできる

返却`CameraResult<T>`／`CameraStatus`の成功を確認し、失敗時は`CameraError`のCategory、Code、Native codeと
Messageを使用します。FrameとXU Payloadは所有値です。Media Foundation、DirectShow、V4L2、libcamera、COM、
ioctlおよびNative handleは公開APIへ現れません。

`CameraSession`はC++の低水準Portable基盤として残りますが、通常のUSB Web Cameraと全Managed言語の正規入口は
`WebCamera`です。旧Lifecycle、Raw callback、画像処理および設定WindowのMethodは公開APIから削除済みです。
[移行Guide](DeprecationMigration.md#ja-tmr-webcamera)を参照し、Frame処理はCameraFrameOpsを使用します。
[Tmr設計](../design/ja/TmrCamera.md)で選択、Callback終了、Byte配置を定義します。Callback配信終了だけでは
Backendが停止しない場合があります。BindingのSession終了とOwner終端解放は
[言語別の所有契約](../design/ja/LanguageBindings.md)に従います。

CameraのTimeoutは待機予算でありCopy・Native後始末・Stopの実時間上限ではありません。V4L2はBuffer返却失敗で
Closeするため再Start前に`isOpen()`を確認します。MFはLock成功後、Copy例外時もUnlockを試みます。
[Native Frame所有契約](../design/ja/TmrCamera.md)を参照してください。

libcameraはRequest／Mappingの所有をトランザクションとして扱います（TMR-LIBCAMERA-06）。
Copy／Control例外や再Queue失敗後はStop結果を確認してから再開します。停止失敗時もPending Requestを保持し、
切断したPipelineではCloseが無期限に待つ場合があります。[Tmrの資源と停止制約](../design/ja/TmrCamera.md)を参照します。

Windows MFの列挙／Device名、能力照会Source、途中OpenはScopeに対応して後始末します（TMR-MF-INIT-07）。
確保例外は後始末後に伝播しえます。最終CloseはOpenしたOwner Threadで行い、Shutdownの試行だけでDriver復旧を
判定しません。[Tmr初期化所有権](../design/ja/TmrCamera.md)を参照します。

## 出力/レンダリング (oui)

### Portable Renderer（正規API）

- Owner: Move-only `Renderer`。`initialize()` → Resource／Surface生成 → Render／Present → `shutdown()`
- Resource: `createTexture`／`uploadTexture`／`createMesh`／`updateMesh`／`destroyTexture`／`destroyMesh`
- Handleは1回のRenderer寿命に属し、Shutdown時に破棄し、別Rendererへ渡しません。
  [OUIの資源／送信詳細](../design/ja/OuiRenderer.md)と[Projectionの計算式／Pass順序](../design/ja/OuiProjection.md)を参照します。
- Surface: `createSurface`／`getSurfaceTexture`／`resizeSurface`／`setSurfaceWindowMode`／`destroySurface`
- 実行: `executeRenderPass`／`waitFence`／`readTexture`／`presentSurface`
- Display: `enumerateDisplays`／`processSurfaceEvents`／`pollSurfaceEvents`
- Projection: `ProjectionPipeline`と`ProjectionMeshAdapter`を使用する

`renderProjection()`はScale 2に必要な出力Extentを設定します。低水準`ProjectionPipeline`は例外時も
両一時Handleの解放を試み、破棄失敗で資源がRenderer終了まで残る場合があります。
[Projectionの失敗順序](../design/ja/OuiProjection.md)を参照してください。

Texture、Mesh、SurfaceおよびFenceの値は世代番号付きOpaque IDであり、Native GPU handleではありません。
1つの`Renderer`への呼出しはCallerが直列化し、失敗は`RendererResult<T>`／`RendererError`で処理します。
詳細は[OUI Renderer設計](../design/ja/OuiRenderer.md)を参照してください。

### Legacy互換API

- GPU初期化: `InterfaceGPU::initialize`, `setTargetAdapter(sGPUAdapterDesc)`
- テクスチャ: `RenderTexture2D::initialize`, `uploadCPUResourse`, `downloadGPUResourse`
- `InterfaceGPU::device()`／`factory()`は解放してはいけない非所有Viewです。
- Copyされた`InterfaceGPU`／`RenderTexture2D`は内部RAII ownerを共有し、一方の`Release()`／`finalize()`で
  他方のResourceを失効させません。
- Legacy `void*` GPU aliasを新規Code、言語BindingまたはModule境界へ追加しないでください。

### ScreenRenderer

- 設定: `setupContentsDesc`, `setupScreenDesc`, `addContents`, `updateContentsCell`
- 実行: `initialize` → `uploadContentsSource` / `importRenderSource` → `render`
- 取得: `downloadScreenData`, `exportScreenTexture`
- α: `uploadAlphamapSource` / `importAlphamapSource`

### ProjectionRenderer

- 設定: `setupScreenDesc`, `setupProectorDesc`, `setupRenderingConfig`
- 実行: `initialize` → `uploadScreenSource` / `importScreenSource` → `render`
- 取得: `downloadProjectionData`, `exportProjectionData`
- 幾何補正: `updateProjectionGeometric`

### WindowRenderer / OS_Display

- WindowRenderer: `updateRendererConfig`, `initialize`, `render`, `show`
- OS_Display: `updateDisplayConfig`, `openWindow`, `renderStillImage`, `showStillImage`,
  `setStreamFrame`, `displayStreamFrame`

## 入力 (iui)

Linuxは全Key Sourceが失敗しKey照会にEACCES／EPERMがあれば`PermissionDenied`、それ以外の
全Capture失敗は`Disconnected`を返します。後のScanでOwnerを交換せず復旧できます。

- `Keyboard::getASCII()`は最小の有効ASCII添字を返す。最後の入力文字ではない
- 状態配列: `ascii_state`, `function_state`, `arrow_state`, `lock_state`, `command_state`
- 一貫した全状態: `Keyboard::snapshot()`。変更通知は所有`KeyCallback`を`setCallback()`へ設定する
- Windows／Linux共通で`accessState()`が`Starting`、`Ready`、`Unavailable`、`PermissionDenied`または
  `Disconnected`を返し、`isAvailable()`は`Ready`の場合だけtrueとなる
- 非Ready時の全Release Snapshotを物理Keyが離された証拠として扱わない
- Linuxは読取専用evdevを使用し、Device grab／Permission変更を行わない。ASCIIはUS Layout互換Key stateであり、
  Localize済みText／IME／Unicode入力ではない
- WindowsのReadyは物理Device／AccessのProbeではない。StatusとSnapshotは個別の観測である
- Copyは所有Callableを共有し、`clearCallback()`は取得済みCallableの完了を待たない。可変CaptureはApplicationで同期する

Platform別可用性、Poll順序、Callback／Copyの寿命と実機検証は[IUI Keyboard設計](../design/ja/IuiKeyboard.md)を参照してください。

## 設定/CSV/BIN (gef)

- CSV: `CSVController::read`, `readWithLimits`, `readReplace`, `write`, `writeAtomic`, `contents`, `toDataMAP2D`
- BIN: `BINController::setContents(...)`, `read`, `readWithLimits`, `write`, `writeAtomic`
- Error: File操作は`GefStatus`／`GefResult`（`wse::gef::GefError`）を返す。空Controllerの
  `last_index()`は`std::logic_error`
- 形式: `<gef/setting/SettingFormat.h>`の`SETTING_*`と`eCategory`

## 連携マクロ (link)

- OpenCV: `<cv/OpenCvAdapter.h>`（Opt-in Adapter。`link/cv_link.h`は2026-09-14に削除）
- 各Module: `<link/tmr_link.h>`

## サンプルコード

### 基本ログとデータ型

```cpp
#include <wse/stew.h>

int main()
{
    wse::registDefaultLog();
    wse::double_xy pt(10, 50);
    WLog() << pt.str();
    return 0;
}
```

### WebCamera で 1 フレーム取得

```cpp
#include <wse/stew.h>
#include <tmr/stew.h>

int main()
{
    wse::registDefaultLog();

    using namespace wse::tmr;
    const auto devices = WebCamera::enumerate();
    if (!devices.succeeded() || devices.value().empty()) return 0;
    const auto capability = WebCamera::capabilities(devices.value().front());
    if (!capability.succeeded() || capability.value().stream_profiles.empty()) return 1;
    const auto& profile = capability.value().stream_profiles.front();
    if (profile.output_formats.empty()) return 1;

    WebCamera camera;
    if (!camera.open(devices.value().front(), {
            profile.native_format, profile.output_formats.front(), true}).succeeded()) return 1;
    if (!camera.start().succeeded()) return 1;
    const auto frame = camera.readFrame(2000U);
    camera.stop();
    camera.close();
    if (!frame.succeeded()) return 1;
    return 0;
}
```

### Keyboard 入力の取得

```cpp
#include <wse/stew.h>
#include <iui/stew.h>

int main()
{
    wse::registDefaultLog();
    wse::iui::Keyboard keyboard;

    if (!keyboard.isAvailable()) {
        return 0;
    }

    bool running = true;
    while (running) {
        const int8_t key = keyboard.getASCII();
        if (key == 'q') {
            WLog() << "quit";
            running = false;
        }
    }
    return 0;
}
```

## 多言語API

| 言語 | Owner | Profile／Frame | 終了 |
| --- | --- | --- | --- |
| JavaScript | `new WebCamera()` | `streamProfiles`／所有`Buffer` | `close()` |
| Python | `WebCamera()` | `stream_profiles`／所有`FrameBuffer` | Context managerまたは`close()` |
| Java | `new WebCamera()` | `streamProfiles()`／`CameraFrame` | try-with-resourcesまたは`close()` |
| C# | `new WebCamera()` | `StreamProfiles()`／所有`CameraFrame` | `using`または`Dispose()` |

C# BindingはP/InvokeがC++ Classを解決できないため、C++ Classではなく`api/wse/capi`の平坦C ABIを呼びます。
公開Componentは他の3 Bindingと同じで、Core、XPT、IUI、OUIおよびTmrです。

いずれのBindingも現行APIのみを公開します。非推奨の`WebCamera` MethodとCallback方式のLegacy Serial
ConnectorはBindingへ含めません。[非推奨と移行Guide](DeprecationMigration.md)
を参照してください。

4言語すべてが`Keyboard` ownerでIUIを公開します。

| 言語 | Owner | Snapshot | 終了 |
| --- | --- | --- | --- |
| JavaScript | `new Keyboard()` | `snapshot()`が所有Objectを返す | `close()` |
| Python | `Keyboard()` | `snapshot()`が`list[bool]`の`dict`を返す | Context managerまたは`close()` |
| Java | `new Keyboard()` | `snapshot()`が`KeyboardState`を返す | try-with-resourcesまたは`close()` |
| C# | `new Keyboard()` | `Snapshot()`が`KeyboardState`を返す | `using`または`Dispose()` |

BindingはCallbackではなくPollingです。`snapshot()`は5つのKey groupを同一更新時点で読み取り、
読取不能時は読取可否State由来のStructured Errorで失敗するため、空Snapshotとして誤認させません。

4言語すべてがXPTを`TcpClient`、`UdpClient`、`SerialPort`およびHTTP実行入口として公開します。XPTは
Default Timeoutを持たないため全Operationが明示Deadlineを要求し、失敗時は`code`にTransportの安定Codeを
保持したまま`category`を正規化します。

詳しい型はJavaScriptの`index.d.ts`、Pythonの`__init__.pyi`、Javaの`io.wapitistew.wse` JARおよび
C#の`WapitiStew.Wse.dll`付属XML Documentationを正本とし、
[多言語Binding設計](../design/ja/LanguageBindings.md)と[How-to](HOWTO.md)を参照してください。

Install配置・依存伝播・明示Component Target検査は[Build／Packaging](../design/ja/BuildPackaging.md)、
平坦InterfaceのHandle所有、Status配置、Thread-local診断、2段階Buffer取得は
[C ABI契約](../design/ja/CAbiContract.md)を参照してください。

C#はSafeHandle受入までNative出力を一時所有し、途中の配列／結果生成失敗でも解放します。
`WebCamera.CallbackException`は最初のManaged Callback／受入例外を保持し、後続配信を抑止します。
Owner側Stop／Join後に確認します。CallbackがThrowしても受渡し済みFrameは利用側の所有です。
詳細は[C ABI受入契約](../design/ja/CAbiContract.md)を参照してください。

C#はTransferExceptionでSend失敗の進捗とUDP切詰めPrefix／送信元を公開します。既存WseExceptionのCatchは有効です。
BytesTransferred、ReceivedData、SourceEndpointはManaged値で、Disposeは不要です。既存C receive_fromは成功時のみを維持し、
追加wse_capi_udp_client_receive_from_with_progressがDatagramTruncatedでも所有Datagramを返します。失敗でも解放してください。
削除SymbolはなくABI version 1も維持します。旧Shimには新入口がないためManaged／Nativeは同じBuildの組を使います。
保証と限界は[C ABI部分転送](../design/ja/CAbiContract.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
