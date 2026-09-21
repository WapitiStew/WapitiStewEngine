# WSE非推奨API・移行Guide

> Canonical source: [English Deprecation and Migration Guide](../en/DeprecationMigration.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

本書は公開C++ APIの非推奨面、正規Replacementおよび削除判定を定義します。機械可読な全件台帳は
[`DeprecationLedger.tsv`](../deprecation/DeprecationLedger.tsv)です。台帳の件数、移行先、日英Anchorおよび
削除Gateは`wse.deprecation_ledger`で公開Headerと照合されます。

## Application所有のLogとKit Metadata

Applicationが所有する`Logger<Tag>`別名、または5引数の汎用`writeLog`を使用します。
TagはApplication側で外部Linkage付き宣言と1か所の定義を持ち、依存するC++ Binaryを
すべて再Buildします。SDKからApplication TagをImportする旧Binaryとは互換性がありません。
現在の契約は[Core共通サービス](../design/ja/CoreServices.md#ja-application-owned-tags)を参照してください。
Offline Candidateはformat v2、受入Manifestはschema version 2です。SDKの識別情報は
`wse`から読み、ApplicationのRevisionは利用側の配布記録で管理します。既存v1成果物は
対応するv1 Verifier／Schemaと保存するか、元のCommit済みSourceから再生成します。
v1の版番号だけをv2へ書き換える手順は使用できません。

<a id="c-abi-keyboard-type-ja"></a>
## C ABI Keyboard状態型

列挙型には`wse_capi_keyboard_access_state_value`または`enum wse_capi_keyboard_access_state`を使います。
照会関数は`wse_capi_keyboard_access_state(handle, &state)`のままで、出力はint32です。
従来の同名Typedefと関数はCの通常識別子の名前空間で衝突し、準拠CompilerでCompileできませんでした。
Enum tag、定数値、関数Symbol／Signature、全Struct配置を維持してHeaderを修正するため、ABI 1とWSE 1.0.0は変更しません。
有効なC Typedef／関数の組を削除する変更ではなく、Compile障害の修正です。

## Constructor初期化

Project所有C++のclass／structは、非staticメンバをConstructor初期化子リストで初期化する。
設定Recordには従来のメンバ順の既定引数付きConstructorがあり、空・部分・位置指定の波括弧構築を維持する。
これらのRecordがAggregateまたは自明なDefault Constructorを持つことへ依存せず、Constructorを使用する。
Copy／Moveは所有権の意味を維持する。C ABIのstructは変更しない。
公開標準ログ整形関数`formatLogMessage()`はSinkへ配送せず文字列を返す。
Header更新後は、依存するC++ CodeとLibraryをまとめて再Buildする。

## 削除Policy

WSE 1.0.0で、本台帳が追跡していた非推奨宣言とLegacy Opaque Aliasをすべて削除しました。Entryの追加は
偶然ではなく審査を経た決定です。現在の台帳は空であり、削除済みEntryは
[`RemovedApiLedger.tsv`](../deprecation/RemovedApiLedger.tsv)へ履歴として残しています。

- 台帳登録は削除予告ではありません。
- 削除はOwnerが承認したSemVer majorだけで行います。
- ReplacementとCompile warningを含むReleaseを少なくとも1 Minor期間維持し、Release noteと移行例を提供します。
- 対象APIを使う公開Consumerを0件にし、Shared／StaticのInstalled package consumerを非推奨警告Error設定で通します。
- Source／Binary互換、対象Platform、RollbackおよびLicenseへの影響をRelease審査で記録します。

<a id="ja-legacy-removal-program"></a>
### 公開前Legacy削除Program

2026-09-13、Ownerは残存するすべての互換面を初公開前に削除すると決定しました。根拠は後述の変更と同じ
未公開Waiver、すなわちWSEが未公開である間は審査済みのIn-place削除がDeprecation cycleを代替し、
Versionは公開まで1.0.0のままという規則です。最初に公開する1.0.0は未処理Entryを持ちません。
非推奨宣言も、互換Classも、Legacy Error面も残さず、恒久的な互換Option
（`WSE_ENABLE_LEGACY_API`のようなSwitch）は導入しません。

審査済みの作業一覧は[`LegacyRemovalInventory.tsv`](../deprecation/LegacyRemovalInventory.tsv)です。
各行は対象面、Replacement、先に移行すべきConsumer、ABI影響、削除Phaseを記録し、
`wse.legacy_removal_inventory`が本Fileを検証して`DeprecationLedger.tsv`の全idを網羅していることを
要求します。行が`pending`を離れるのは作業を実施するPull Requestを通じてのみで、`removed`行は履歴と
して残します。

削除に先立ち、管理下Consumerを次のとおり分類しました。

| Consumer | 分類 |
| --- | --- |
| EngineのTest／Example／Installed-package consumer test | 各削除と同一Changeで移行 |
| Application Host（Vendored EngineとExtension checkoutを含む） | 対象面の削除前に移行 |
| Access-controlled Extension overlay | 影響する削除のMerge前に専用環境で確認 |
| EngineDevelopper | 2025-06から休眠中のHistorical開発Harness。Pinned Engine snapshotとともにArchiveし、移行しない |
| Language BindingとFlat C ABI | Legacy Core Error面に未結合。既存のABI契約Testで再確認 |

Programの進行中は3つのSource ratchetが後退を防ぎます。`wse.public_header_boundary`のRaw void-pointer
とThread-state baseline、および`wse.public_api_policy`のLegacy error-surface baselineです。いずれも
減少のみが許され、削除済みのDebtが静かに戻ることはありません。

Programは2026-09-14に完了しました。全Inventory行が`removed`、全Ratchet Baselineが空、Deprecation Ledgerが空です。`wse.legacy_free` Gateがこの終状態を固定し、Legacy面の再導入はInventoryへの明示的なOwner決定なしにSuiteを通過しません。

### 1.0.0で削除したもの

| Component | 削除 | Replacement |
| --- | --- | --- |
| Core | 可変`Map::vector()` Overloadと参照を返す`Map::begin()`（2026-09-08非推奨化、2026-09-13にLegacy削除Programで削除） | `Map::elements()`／`Map::at()`／`Map::row()`と`Map::front()` |
| Core | `BMultiThread::setCallback(function, void*)` | `setCallback(EventCallback)` |
| Core | `Matrix_::inverse()`とLegacyデータ面のthrow ― Map／Matrix／Image／Pixel／Point／Range／Mesh／Homography／Pixel Format Helperの`wseException(ErrorCode)`（2026-09-14にLegacy削除ProgramでIn-place置換） | 使い方違反は標準Exception、計算上起こり得る失敗は`CoreResult`（`Matrix_::tryInverse()`） |
| Core | `wse::Timeout`のAtomic Flag Spin-wait Helper（2026-09-14にLegacy削除Programで削除）とDevice PickupのLegacy `TIMEOUT`／`UNIMPLEMENTED` throw | 待機が必要な箇所は`OperationContext`型のDeadline。Device PickupのPlatform失敗は`std::runtime_error`、未実装列挙は`std::logic_error` |
| GEF | `BINController`／`CSVController` File I/OのLegacy `wseException(ErrorCode)` throw（2026-09-14にLegacy削除ProgramでIn-place置換） | 運用上の失敗は`wse::gef::GefError`を運ぶ`GefStatus`／`GefResult`。空Controllerの`last_index()`は`std::logic_error` |
| Core | License面 ― `License::load`・`Licensekey::genrate`／`load`・`LicenseWriter::save`のLegacy `wseException(ErrorCode)` throw（2026-09-14にLegacy削除ProgramでIn-place置換） | 運用・検証上の失敗は`wse::LicenseError`を運ぶ`LicenseStatus`／`LicenseResult`。二重Loadは`std::logic_error`、範囲外のWriter Enumは`std::invalid_argument` |
| Core | `wse::ErrorCode` Enum・`wseException_`・`wseException`／`wseThrowException` Macroと`wse/utility/wse_Exception.h`（2026-09-14にLegacy削除Programで削除、対Extension Migration完了後） | 使い方違反は標準Exception、運用上の失敗は各ComponentのResult契約（`CoreResult`・`LicenseResult`・`GefResult`など） |
| Core | OpenCV結合 ― 7 Data Headerの`cv::`変換コンストラクタ・代入・`cv()`アクセサと`link/cv_link.h`有効化マクロHeader（2026-09-14にLegacy削除Programで削除） | Opt-inの`<cv/OpenCvAdapter.h>`：公開Interleaved契約の上に実装した`wse::ocv::fromCv*`／`toCv*`自由関数 |
| Core | 残るLegacy名 ― Spell違いの`memoey_size()`、汎用名の`LicenseWriter::eType`／`eVer`、Spell違いの`wse_Practiser.h`（2026-09-14にLegacy削除Programで改名。他の列挙済みSpell違いは`ErrorCode` Enumとともに削除済み） | `memory_size()`・`LicenseWriter::eLicenseType`／`eLicenseVersion`・`wse_PixelFormat.h`。`snake_case`の単純GetterはCoding Ruleの正規命名として維持 |
| Core | `BMultiThread` Public Worker基底そのもの ― Thread／Atomic／Mutex Pointer Accessor、Raw `void*` Callback Context、Task状態機械、`setThreadPriority_High()`（2026-09-13にLegacy削除Programで削除） | `Timer`と`Keyboard`がCompositionする内部Worker実装 |
| Core | TimerのContext pointer版`initialize()`／Constructor | `initialize(interval, TimerCallback, EventCallback)` |
| Core | `Timer::updateTimerCofig()` | `updateTimerConfig()` |
| Core | `Timer`のPublic `BMultiThread`継承（継承Worker API込み）と`initialize()`／`updateTimerConfig()`／`setTimerCallback()`／`clearTimerCallback()`（2026-09-13にLegacy削除Programで削除） | `start(interval, callback)`／`setInterval()`／`stop()`／`isRunning()` |
| XPT | `SerialConnector`のContext pointer版`open()`／`setCallback()` | `setCallback(EventCallback)`と`open(port, baudRate)` |
| XPT | `SerialConnector` Class本体（2026-09-13にLegacy削除Programで削除） | 所有Thread 1本の上の`SerialPort` |
| XPT | `TransportError::native_code()` | `nativeCode()` |
| OUI | `RendererError::native_code()` | `nativeCode()` |
| OUI | `ProjectionRenderer`／`ScreenRenderer`／`WindowRenderer`／`InterfaceGPU`／`RenderTexture2D`／`OS_Display`／`OS_MultiSourceDisplay` | `Renderer`と`ProjectionPipeline`およびSurface |
| OUI | 上記Headerが宣言していた`void*` Alias 9件 | `Renderer`が所有するOpaque Handle |
| IUI | `Keyboard::ref_*_state()` | `snapshot()`または所有配列Getter |
| IUI | `Keyboard`のPublic `runProcess()` Worker overrideと`BMultiThread`基底（2026-09-13にLegacy削除Programで削除。Snapshot／Callbackの公開APIは不変） | 内部所有の監視Worker |
| Tmr | 旧`WebCamera` Method 42件 | `enumerate`／`open`／`start`／`stop`／`close`、Control API、`CameraFrameOps` |
| Package | Install済み`WonderStewEngine`互換Alias Target（2026-09-13にLegacy削除Programで削除。Package名と`find_package(WonderStewEngine ...)`は不変） | `WSE::Core`／`WSE::*` Component Target、またはConsumer側Local Alias |

本削除にあたり、Ownerが`major-owner-private-consumers`と`major-owner-hardware-consumers`のGateを免除
しました。これらは別環境のPrivate Consumer審査と、Replacementに対するCamera実機認証を要求するもので、
いずれも実施していません。したがって削除面をまだ呼び出しているPrivate ConsumerやHardware統合は、
非推奨警告ではなく本表を頼りに移行する必要があります。

`WebCamera(index)`は削除対象ではなく、正規APIを通じて当該IndexのEnumerated Deviceを選択するよう
実装し直しました。Constructorは状態を返せないため、失敗は`lastError()`で報告します。

<a id="ja-parameter-convention"></a>
### 関数引数規約

[公開API Policy](../design/ja/PublicApiPolicy.md)に、Sourceが部分的にしか従っていなかった引数規約を
明文化しました。これを適用した結果、出力を持つすべての関数のSignatureが変わります。out引数はPointerに
なり、引数列の先頭へ移動します。

これは互換Overloadを持たないIn-placeなSource破壊的変更であり、WSEが未公開である間のOwner判断として
行いました。根拠は上記のtmrリネームと同じです。Versionは公開までは1.0.0のままです。移行の手掛かりに
なる非推奨警告は出ないため、次の形の違いを頼りに移行します。

```cpp
// 変更前
wse::img3c08_t image;
wse::tmr::toImage( frame, image );
wse::tmr::readImage( camera, 1000U, image );
renderer.readTexture( texture, 1000U, image );

// 変更後 ― 書き込まれる引数が先頭に来て、Addressで渡す
wse::img3c08_t image;
wse::tmr::toImage( &image, frame );
wse::tmr::readImage( &camera, &image, 1000U );
renderer.readTexture( &image, texture, 1000U );
```

すべてを生んだ規則は1つです。outとin-outはPointerで受け取り、順序はin-out、out、inとします。
これにより、呼び出し側はどの引数が書き換えられるのかをその場で見て取れます。

<a id="ja-pixel-32bit-storage"></a>
### 32bit Pixel Storageの是正

`CH1D32`／`CH2D32`／`CH3D32`／`CH4D32`の`PixelTraits`は、Header文書が`uint32_t`と約束しながら
Storage型を`uint64_t`と宣言しており、32bit Formatが1 Channelあたり8 byteを消費してByte layoutが
文書化された契約から乖離していました。Storage型を`uint32_t`へ是正しました。

これは互換Aliasを置かないその場でのBinary layout変更であり、WSE未公開の間のOwner決定
(2026-09-08、Issue CORE-006) によるものです。根拠は上記の変更と同じで、版数は公開まで1.0.0の
ままです。Source修正は不要です。`UINT32_MAX`を超える値はもともと文書化された32bit契約の外であり、
縮小・拡大CastはBit深度基準で規則も不変です。旧8 byte strideで生の32bit画像Bufferを永続化していた
Consumerは再Serializeが必要です。`wse.core.pixel_storage`契約が全Pixel FormatのStorage型・
Channel数・Bit深度・strideをCompile時に固定します。

<a id="ja-inventory"></a>
## 全件Inventory

公開Headerに`[[deprecated("...")]]`宣言はなく、Legacy `void*` Aliasも存在しません。この件数を
構成していた`wse::Map`のAccessor 2件は2026-09-13に削除し、
[`RemovedApiLedger.tsv`](../deprecation/RemovedApiLedger.tsv)へ記録しました。後続の節は、
削除面が警告付きでまだCompileできた1.0.0より前のReleaseに対して書かれたCodeのための移行Guideとして残しています。

<a id="ja-map-legacy-accessors"></a>
## Core Map Accessor

2026-09-13削除（2026-09-08非推奨化）。形状を破壊できる`Map`のAccessorから、検査付きの面へ移行します。

```cpp
// Before
std::vector<float>* raw = map.vector();  // ここでのresizeはwidth x heightを静かに破壊する
float& first = map.begin();              // 名前に反してIteratorではなく参照を返す

// After
float* values = map.elements();          // 値の読み書きのみ。形状は変更できない
float& checked = map.at(0, 0);           // 境界検査付き要素アクセス
float* rowValues = map.row(0);           // 境界検査付き行Pointer (width()要素分)
float& firstChecked = map.front();       // 空のMapはstd::out_of_rangeで拒否する
```

`vector()`は返したPointer経由のresize／clearを許し、保持要素数と`width() * height()`の対応を
崩せました。`elements()`は値アクセスだけを提供し、形状には触れられません。`begin()`はIteratorではなく
参照を返し、`front()`と同様に空のMapで未定義動作でした。両者は空のMapを`std::out_of_range`で拒否し、
`begin()`は`front()`へ置き換え済みです。`get()`／`set()`の範囲外は`std::out_of_range`、確保系
Constructorと`init()`は`width x height`が`size_t`で表現できることを検証してOverflow時は
`std::invalid_argument`を投げます（[Coreデータ型のError契約](#ja-core-error-contract)参照）。

<a id="ja-core-worker-timer"></a>
## Core Worker／Timer

2026-09-13（Legacy削除Program）に`Timer`は`BMultiThread`の継承をやめ、Workerを内部所有する
finalなClassになりました。継承していたGeneric Worker API（`bootThread()`／`stopThread()`／
`notifyTask()`／`waitTask()`／`getThread()`／`getStateFlag()`／`state()`／
`setThreadPriority_High()`／基底Constructor／Worker `EventCallback`）と旧Timer面
（`initialize()`／`updateTimerConfig()`／`setTimerCallback()`／`clearTimerCallback()`）は、
Deprecation期間なしで`Timer`から除去しました。

```cpp
// 変更前 ― 継承したWorker Lifecycleと分離した設定手順
timer.initialize(100U, []() { /* periodic work */ },
    [](wse::BMultiThread::eEvent event) { /* lifecycle event */ });
timer.bootThread();
timer.updateTimerConfig(200U);
timer.stopThread();

// 変更後 ― Composition化した単一のLifecycle。間隔はstd::chrono値
timer.start(std::chrono::milliseconds(100), []() { /* periodic work */ });
timer.setInterval(std::chrono::milliseconds(200));
timer.stop();
```

`start()`はCallbackを束ね（CaptureはTimerが所有し、停止で解放）、0以下の間隔と空のCallbackを
`std::invalid_argument`で拒否し、実行中はfalseを返します。`stop()`は冪等でCallback内からも
呼べ、復帰後に新しい呼び出しは始まりません。CallbackのExceptionはThread境界を越えずTimerを
停止させます。Worker eventのLifecycle Callbackは`isRunning()`で置き換えます。

`BMultiThread`自体も同じProgramで2026-09-13に削除しました。Thread／Atomic／Mutex Pointer
Accessor、Raw `void*` Callback Context、Task状態機械、Best-effortの`setThreadPriority_High()`を
持つPublic Worker基底はもう存在しません。これを派生していたCodeは`std::thread`（またはその
Workerが駆動していた機能）を直接所有します。WSE内の利用者だった`Timer`と`Keyboard`は内部Worker上で
動作し、自身のAPIだけを公開します。

<a id="ja-core-error-contract"></a>
## Coreデータ型のError契約

2026-09-14（Legacy削除Program）に、Coreのデータ面 ― `Map`、`Matrix_`、`Image_`とその
Demosaic／Interleave／Transform Helper、`Pixel_`、`Point2_/3_/4_`、`Range1D/2D/3D`、`Mesh`、
`Homography`、Pixel Format Helper ― はLegacy `ErrorCode`を載せた`wseException_`のthrowを
やめました（In-place、Deprecation期間なし）。

- **使い方違反**は対応する標準Exceptionを投げます。不正な引数・サイズ・Formatは
  `std::invalid_argument`、形状外Indexは`std::out_of_range`、0除算や数学的定義域外のData
  （特異な設定済みColor Matrix、退化した分布）は`std::domain_error`、あり得ない状態での
  操作は`std::logic_error`です。
- **計算上起こり得る失敗**はthrowせず失敗`wse::CoreResult`を返します。`Matrix_::inverse()`は
  `tryInverse()`へ置き換え、特異行列は`eCoreErrorCategory::Computation`／
  `eCoreErrorCode::SingularMatrix`として届きます。

```cpp
// 変更前 ― すべての失敗がLegacy ErrorCodeを載せたwseExceptionだった
try { const wse::Matrix inverted = matrix.inverse(); }
catch (const wse::wseException_& e) { /* e.code()で分岐 */ }

// 変更後 ― 使い方違反は標準Exception、特異入力はResult
const wse::CoreResult<wse::Matrix> inverted = matrix.tryInverse();
if (!inverted.succeeded()) { /* inverted.error().code() == SingularMatrix */ }
```

`wse::ErrorCode`・`wseException_`・`wseException`／`wseThrowException` Macroは、アクセス制御
Extensionの対Migration完了を受けて2026-09-14に`wse/utility/wse_Exception.h`とともに削除しました。
使い方違反は標準Exception、運用上の失敗は各ComponentのResult契約で報告され、移行元となる
Legacy Error面は残っていません。

<a id="ja-xpt-transport"></a>
## XPT Transport

Windows専用でCallback方式の`SerialConnector`は2026-09-13に削除しました（Legacy削除Program。
互換面であり`[[deprecated]]`ではありませんでした）。Serial面は`SerialPort`だけです。
Caller-confinedでMove-onlyの同期Ownerであり、明示`OperationContext`のTimeout／Cancellationと
部分転送Resultを持ちます。

```cpp
// 変更前 ― 非同期Callback、Timeoutなし、send()は失敗できない
wse::xpt::SerialConnector connector;                     // 削除済み
connector.setCallback([](wse::xpt::SerialConnector::Event, const std::vector<unsigned char>&) {});
const bool started = connector.open("COM3", 115200);     // OPENED／TMR_FAILED_OPEN待ち

// 変更後 ― 所有Thread 1本が同期Portを駆動する
wse::xpt::SerialPort port;
const wse::xpt::OperationContext context(wse::xpt::Timeout::milliseconds(5000));
const auto opened = port.open("COM3", 115200, context);  // TransportStatus。Event待ちなし
const auto sent = port.send(frame, context);             // 送信byte数 == Frame長を検証する
const auto received = port.receive(256U, context);       // Poll。Timeout Categoryは「受信なし」
```

Event駆動のConsumerは、`SerialPort`を所有するSerial I/O Worker Threadを1本だけ持ち、`receive`の
結果を自身のEventへ変換します。他Threadから許される呼出しは`CancellationSource::cancel()`だけです。
Protocol固有処理をXPTへ持ち込みません。

<a id="ja-oui-renderer"></a>
## OUI Renderer

Error accessorは`nativeCode()`へ置換します。Legacy GPU／Texture／Windowの`void*` aliasを新規Codeへ追加せず、
世代番号付きOpaque IDを返す`Renderer`へ移行します。

```cpp
wse::oui::Renderer renderer;
wse::oui::sRendererConfiguration configuration;
const auto initialized = renderer.initialize(configuration);
if (initialized.succeeded()) {
    const auto capabilities = renderer.getCapabilities();
    // createTexture/createMesh/createSurface等はOpaque IDを返す
}
renderer.shutdown();

const std::int64_t diagnostic = rendererError.nativeCode();
```

`InterfaceGPU`、`RenderTexture2D`、`ProjectionRenderer`、`ScreenRenderer`および`WindowRenderer`は、
は削除しました。削除前に、正規`Renderer`との同等性を主張ではなく実行で示しています。Contractが同一
Source画像と同一Meshを両経路へ通し、CPUへ読み戻したPixelを比較したところ、両者はBit単位で一致しました。

| Legacy | Portable置換 |
| --- | --- |
| `InterfaceGPU::initialize()`／`isInitialized()`／`finalize()` | `Renderer::initialize()`／`isInitialized()`／`shutdown()` |
| `InterfaceGPU::target_desc()` | `getCapabilities()`。`adapter_name`も報告する |
| `InterfaceGPU::setTargetAdapter()` | `sRendererConfiguration::adapter_name` |
| `RenderTexture2D::initialize()`／`finalize()` | `createTexture()`／`destroyTexture()` |
| `RenderTexture2D::uploadCPUResourse()`／`downloadGPUResourse()` | `uploadTexture()`／`readTexture()` |
| `ProjectionRenderer`／`ScreenRenderer`のSetupと`render()` | `sProjectionPassDescription`と`ProjectionPipeline::execute()` |
| `updateProjectionGeometric()`／`updateContentsCell()` | `ProjectionMeshAdapter::createMesh()`と`updateMesh()` |
| `downloadProjectionData()`／`downloadContentsData()` | `readTexture()` |
| `WindowRenderer`のWindow設定／`show()`／`render()` | `createSurface()`／`setSurfaceWindowMode()`／`presentSurface()` |
| `render(const wse::img4c16_t&)` | Sampled Sourceとしての`eRendererPixelFormat::Rgba16Unorm` |

移行時に時間を要した点を3つ記録します。

- Projection Meshの`src`は**Source Pixel座標**であり、正規化座標ではありません。0.0〜1.0を渡すと、
  大きなSourceの左上2×2 Texelだけをサンプルします。
- Legacy Upsampling Pixel ShaderはAlpha mapを乗算するため、`uploadAlphamapSource()`を呼ぶまで
  Projectionは黒く描画されます。Portable経路にこの暗黙の前提はありません。
- `readTexture()`は自身の`row_pitch`を報告し、Backendがこれを整列します。Packedとみなすと2行目以降が
  壊れます。Legacyの`downloadProjectionData()`は`Image_`を返すためこれが表に出ませんでした。

次は欠落ではなく意図的な非目標です。Portable APIはこれらを渡さないために存在します。

- 生のDevice／Factory／Texture Resource／Heap／Command List Pointer（`GPUDevicePtr`、`GPUFactoryPtr`、
  `Ptr_2DTexResource`、`Ptr_2DTexHeap`、`Ptr_GPUCommander`）と生のTarget Pointer
  （`Ptr_ProjectionTarget`、`PtrScreenTarget`、`PtrWindowClass`、`PtrWindowHandle`）。これらを必要とする
  Consumerは、WSEを使うのではなくBackend Codeを書いています。
- `WindowRenderer::initialize(vs_path, ps_path)`が受け付けていた、File PathによるPipeline Shaderの差し替え。
  Portable PipelineはShaderを自身で所有します。

`OS_Display`と`OS_MultiSourceDisplay`も同時に削除しました。呼び出していたのはどのBuildもCompileしない
Sourceだけであり、`OS_MultiSourceDisplay`に至っては実装がどこにも存在しないままInstall規則がHeaderを
配布していました。

<a id="ja-iui-keyboard"></a>
## IUI Keyboard

監視Thread中に内部配列Referenceを保持せず、同一更新時点のSnapshotまたは所有配列を取得します。

```cpp
const wse::iui::KeyboardState state = keyboard.snapshot();
const auto ascii = keyboard.ascii_state();
```

<a id="ja-tmr-webcamera"></a>
## Tmr WebCamera

通常のUSB/UVC Cameraでは、Device列挙、広告済みProfile、所有Frameおよび型付きControlを使用します。

```cpp
const auto devices = wse::tmr::WebCamera::enumerate();
const auto capability = wse::tmr::WebCamera::capabilities(devices.value().front());
const auto& profile = capability.value().stream_profiles.front();

wse::tmr::sCameraStreamConfiguration configuration{
    profile.native_format, profile.output_formats.front(), true};
wse::tmr::WebCamera camera;
camera.open(devices.value().front(), configuration);
camera.start([](const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& frame) {
    // 成功時のFrameはByte列を所有する
});
camera.stop();
camera.close();
```

Control、Format、OpenCV変換および設定Windowの詳細対応は
[WebCamera移行Guide](WebCameraMigration.md)を参照してください。物理Camera Gate、Generic XUおよびPi系保留を
Software test成功へ読み替えません。

## SDK Package Layout

JavaScript BindingのInstall先を`lang/javascript`から`lang/js`へ変更しました。Source treeと
SDKの双方でJavaScript系Directory名を`js`へ統一するためです。

| 変更前 | 変更後 |
| --- | --- |
| `<sdk>/lang/javascript/index.js` | `<sdk>/lang/js/index.js` |
| `<sdk>/lang/javascript/index.d.ts` | `<sdk>/lang/js/index.d.ts` |
| `<sdk>/lang/javascript/package.json` | `<sdk>/lang/js/package.json` |

```js
// 変更前
const wse = require('C:/wse-sdk/lang/javascript');
// 変更後
const wse = require('C:/wse-sdk/lang/js');
```

本変更はPackage Layoutのみの変更です。Node-API Module名、公開API面および`bindingAbiVersion`は
変更していないため、`require`のPath以外にSource修正は不要です。CTest名
`wse.documentation.quickstart_javascript`と環境変数`WSE_JAVASCRIPT_PACKAGE`はDirectoryではなく
言語名を示す識別子のため、変更していません。

## 台帳対象外

- `WonderStewEngine` CMake Targetは互換Targetですが、削除予定の非推奨Targetではありません。
- C++ `CameraSession`は低水準Portable Camera基盤であり、非推奨ではありません。
- Access-controlled Extension Headerは公開台帳へ含めず、別環境のPrivate台帳で管理します。
- 名前が古いだけのAPIやLegacy classを、`[[deprecated]]`または正式な移行判定なしに削除対象へしません。

C#はTransferExceptionでSend失敗の進捗とUDP切詰めPrefix／送信元を公開します。既存WseExceptionのCatchは有効です。
BytesTransferred、ReceivedData、SourceEndpointはManaged値で、Disposeは不要です。既存C receive_fromは成功時のみを維持し、
追加wse_capi_udp_client_receive_from_with_progressがDatagramTruncatedでも所有Datagramを返します。失敗でも解放してください。
削除SymbolはなくABI version 1も維持します。旧Shimには新入口がないためManaged／Nativeは同じBuildの組を使います。
保証と限界は[C ABI部分転送](../design/ja/CAbiContract.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
