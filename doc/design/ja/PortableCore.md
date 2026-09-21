# WSE Portable Core設計

> Canonical source: [English Portable Core Design](../en/PortableCore.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書は`WSE::Core`の現在のPortable境界を定義する正式設計書です。Windows／Linux実装に適用します。
Optional componentの詳細、多言語Bindingおよび実機認証は本契約の対象外です。
XPTの詳細は[XPT Transport設計](XptTransport.md)、OUIは[OUI Renderer設計](OuiRenderer.md)と
[OUI Projection設計](OuiProjection.md)、Tmrは[Tmr Camera設計](TmrCamera.md)で定義します。

## Support matrix

Dataと振る舞いの詳細は[Core Data Model](CoreDataModel.md)、[Core演算設計](CoreAlgorithms.md)、
[Core共通サービス設計](CoreServices.md)で定義します。本書はPlatform境界を保持し、
Timerの周期規則とLog／Licenseの共有状態はサービス設計を参照します。

| Target | Compiler | Library形式 | 対応範囲 |
| --- | --- | --- | --- |
| Windows x86-64 | MSVC／Visual Studio 2022 | Shared、Static | CoreおよびOptional component |
| Linux x86-64 | GCC、Clang | Shared、Static | Core、GEF、IUI evdev、XPT、OUI Vulkan／Wayland／DRM-KMS、Tmr |
| Linux ARM64 | GCC Cross、GCC／Clang Native | Shared、Static | Core／GEF／IUI／XPT／OUI／Tmr。実機範囲はHardware Validationに従う |

Linuxは64-bit x86-64およびARM64だけを受理します。未対応Optional componentをLinuxで有効にした
Configureは失敗します。XPTはTCP／UDP／HTTP、Portable `SerialPort`および`RetryPolicy`境界を全対応Targetへ
提供します。未対応TargetやComponentをDummy成功として扱ってはいけません。

## OS境界

Portable CoreはData型、Timer、Log、License計算およびThread lifecycleを所有します。OS Adapterは
Device識別子、Device列挙、Thread priority要求およびPlatform Log出力を所有します。CMakeはTarget OSに
対応するAdapterを一組だけ選択します。

### Wait／Time

`Wait::sec`、`msec`、`usec`および`nsec`は、対応する`std::chrono` Durationを指定して
`std::this_thread::sleep_for`を呼びます。単位はAPI契約です。OS Schedulerによる遅延は許容し、Real-time
精度は保証しません。

### Thread lifecycle

Worker Threadは実装の関心事です。公開APIはThread・Atomic・Mutex状態を公開しません。Workerを持つ各機能（`Timer`、IUI `Keyboard`）は内部Workerを1本だけ
所有し、次を共通の保証とします。実行中の`std::thread`をMove先へ移送しない。停止は冪等で、Owner
threadからの呼出しでは終了までJoinする。Worker自身のThreadからの停止は要求のみを行い、Joinは
DestructorまたはNextのStartが引き受ける。停止要求は待機中のWorkerを即座に起こす。Worker本体の
ExceptionはThread境界を越えない。Owner Threadからの破棄はCallback完了を待ってJoinします。Callback自身からTimer／Keyboardを破棄しては
いけません。Self-stopはSelf-destructionを許可するものではありません。

### Timer／IUI Callback

`Timer`はWorkerを内部所有するfinalなClassです。Thread・Atomic・Mutex状態を公開しません。Lifecycleは
`start(std::chrono::milliseconds, TimerCallback)`／`setInterval()`／`stop()`／`isRunning()`です。
`start()`はCallbackのCaptureを所有して停止時に解放し、0以下の間隔と空のCallbackを
`std::invalid_argument`で拒否し、実行中はfalseを返します。`stop()`は冪等でCallback内からも呼べ
（joinはDestructorまたは次の`start()`が引き受ける）、復帰後に新しい呼び出しは始まりません。
CallbackのExceptionはThread境界を越えずTimerを停止させます。停止要求は待機中のWorkerを即座に
起こします。


IUIの`Keyboard`はWindows／Linuxで構築時に監視を開始します。`snapshot()`は全Key groupを同一更新時点の
所有値として返します。WindowsはVirtual-key state、Linuxはevdevを使用し、利用可否をSnapshotと分けて
報告します。`KeyCallback`は状態変更後に監視Workerで実行します。公開APIは所有Snapshotと値Getterを使用します。権限とCallback解除は[IUI Keyboard設計](IuiKeyboard.md)に
従います。`clearCallback()`は実行中CallbackをJoinしません。

### License Device識別子

Linux Adapterは、Loopback以外の`AF_PACKET` Interfaceから、非Zeroの6-byte Hardware addressを持つ
最初のInterfaceを選び、小文字Colon区切りで返します。対象がない場合は空文字列を返します。Interface
列挙順は安定したHardware identityではありません。複数Interface間の識別方針とLinux Device licenseの
認証は定義していません。

### Device列挙

Linux `pickupDeviceInfo`はNetwork interfaceとSerial candidateを列挙します。NetworkはInterface名、IPv4／IPv6、
MAC addressおよびsysfs identityを返します。SerialはttyUSB、ttyACM、ttyAMA、ttyS、ttySC、ttyXRUSBおよび
rfcomm Classの`/dev` pathを返します。接続規格や機種を推測してはいけません。Monitor、Keyboard、Mouse、
TouchおよびGamepadは`std::logic_error`を送出し、成功したかのような空Listを返しません。

### Path／Package

Core source選択とInstall package生成はWindows専用Build path前提を持ちません。Install済みConsumerは
`find_package(WonderStewEngine CONFIG REQUIRED)`を使用し、`WSE::Core`と`WSE::*` Component Targetへ
Linkします。PackageはConsumer向けの`WonderStewEngine` Targetを定義しません。WSEは独立した公開Filesystem path APIを定義しません。

Static Install packageは公開依存`Threads::Threads`をPackage設定から解決します。XPT Packageは固定libcurlを
内部Dependencyとして同梱し、Linux ARM64 Packageは固定OpenSSLも同梱します。CMake ConfigureはDependencyを
Downloadしません。

<a id="データの高速アクセス"></a>
## データの高速アクセス

検査なしの`operator[]`は、メモリへ直接アクセスするために必要不可欠なC++公開データAPIです。
検査付きの`Map::at(x, y)`／`Map::row(y)`と併存します。呼出し元が不正な添字を指定できることだけを
理由に削除したり、戻り値を`Result`・行のコピー・所有Bufferへ置き換えたりしません。

| データクラス | 式 | 非const／constの戻り値 |
| --- | --- | --- |
| `Map<T>`、`Matrix_<T>`／`Matrix`、`Homography` | `data[y]` | 行先頭の`T*`／`const T*` |
| `Image_<Format>`とImage Alias | `image[y]` | Pixel Pointer／const Pixel Pointer |
| `Pixel_<T, N, Depth>` | `pixel[channel]` | `T&`／`const T&` |
| `Mesh_<Source, Destination>` | `mesh[y]` | Vertex Pointer／const Vertex Pointer |

Overloadは整数の添字を受け付け、inlineかつ`noexcept`とします。元の領域への非所有Viewを返し、
メモリ確保・コピー・境界検査・値の丸め・同期処理を行いません。行は`width()`要素（Meshは
`vertex_width()`要素）のStrideで連続し、ChannelはPixelの元の配列を参照します。非const側の書込みは
const Viewから直ちに読めます。Matrix・Image・Homographyは継承したOverloadを公開APIとして明示します。

呼出し元は有効な添字を指定します。Storageは空でなく、行・列・Channelは各要素数未満であることが前提です。
負数・範囲外の添字は前提違反です。Pointer／Referenceは所有Objectが存続する間だけ利用でき、所有Objectが
領域を置換・再確保すると無効になります。並行読書きには通常のC++同期規則を適用します。

`wse.core.fast_indexing`とInstall済みConsumerでOverloadの利用可否、Constness、行Stride、元の領域との
同一性を固定します。任意実行の`wse.bench.indexing`はReleaseのMatrixおよびImage／Pixel走査を、同じ入力の
直接Pointer走査と測定順を交互に入れ替えて比較し、絶対時間による合否判定を設けず計測値を出力します。

## 検証契約

Linuxの各Compiler／Library形式はBootstrap、Core CharacterizationおよびCore Runtime契約を通過する
必要があります。Install packageはModern `WSE::*` Component TargetからのCompile、Link、実行と、
Consumer向けWonderStewEngine Aliasが定義されていないことを確認します。
Windows Shared／Static全Component回帰Gateも必須です。ARM64 Cross build成功をHardware認証として扱っては
いけません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
