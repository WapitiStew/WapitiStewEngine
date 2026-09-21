# WSE Tmr Camera設計

> Canonical source: [English Tmr Camera Design](../en/TmrCamera.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書は`WSE::Tmr`が提供する汎用Camera APIの正式設計です。Camera列挙、Capability、
Stream lifecycle、Frame ownership、Control、Callback、ErrorおよびCalibration値をWindowsと
Linuxで共通化します。USB Video Class（UVC）Cameraは、共通基盤だけに機能を縮退させず、UVCの
Stream profile、標準ControlおよびExtension UnitをCapabilityとして公開します。機器固有の
非公開Extensionは本契約の外に置き、公開Packageへ含めません。

公開入口は`<tmr/stew.h>`です。個別Headerは`<tmr/camera/Camera.h>`、
`<tmr/camera/CameraTypes.h>`、`<tmr/camera/CameraError.h>`および
`<tmr/calibration/CameraCalibration.h>`です。Web Camera向け高水準Facadeは`WebCamera`であり、
正規Headerは`<tmr/device/WebCamera.h>`です。`camera/`はPortable session／Contract、`device/`は
利用者が操作するDevice classという責務で統一し、`<tmr/camera/WebCamera.h>`は設けません。

## 公開APIの階層

Camera APIは次の責務を分離します。

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| 層 | 公開型 | 責務 |
| --- | --- | --- |
| Portable session | `CameraSession` | Backend非依存の列挙、Open、Stream、所有Frame、Errorおよび低水準Capability操作 |
| Web Camera facade | `WebCamera` | Web／UVC Deviceの選択、Native stream profile、標準Control、Auto modeおよびExtension Unitの高水準操作 |
| Calibration | `sCameraCalibration` | Device I/Oから独立した補正値 |
| Private device | 公開Package外 | 機種固有Protocol、Registerおよび秘密情報 |

</div></div>


`WebCamera`は`CameraSession`を内部所有するFacadeとし、継承でBackendを公開しません。通常のUSB Web Camera
利用者と多言語Bindingは`WebCamera`を第一入口とし、CSI Camera、Virtual CameraまたはBackend選択が必要な
高度な利用者は`CameraSession`を使用します。どちらもMedia Foundation、DirectShow、V4L2、libcamera、
COM型、File descriptorまたはNative handleを公開しません。


`WebCamera`のLifecycleは`CameraSession`と同じ`Closed -> Open -> Streaming -> Open -> Closed`です。
公開APIは`open(device[, configuration])`、`start`、`stop`、`close`を使用します。

### `WebCamera` Facade契約

`WebCamera::enumerate()`は`UsbUvc`と、OSがTransportを確定できなかった`Unknown`をWeb Camera候補として返します。
`Csi`、`Virtual`および`Network`は明示的に除外します。`Unknown`を残すのはNotebook内蔵Camera等を、推測だけで
利用不能にしないためです。利用側は表示名やID文字列でTransportを推測してはいけません。


利用側は`open(device)`または`open(device, configuration)`、`start()`／`stop()`、`readFrame()`、
`currentCapabilities()`、`getControl()`／`setControl()`、Generic XU APIを使用します。Exposure／Gain／Focusの
Convenience methodは同じCapability・Range・Step・Mode・Access検証を通ります。操作失敗は
`CameraStatus`／`CameraResult`で返し、`lastError()`は保持するErrorを公開します。

FrameからCore ImageへはCamera Methodではなく`CameraFrameOps.h`を使用します。
[FrameをCoreのImageとして扱う](#ja-frames-as-core-images)を参照してください。現行FacadeにはOS設定Windowや
One-shot自動調整のConvenience APIはありません。現行APIで公開する未対応操作は構造化Errorを返します。
機種固有Metadataは公開Packageの対象外です。

## 実装と所有者: TMR-OWNER-01

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| 層 | Source | 所有／委譲 |
| --- | --- | --- |
| Web facade | [WebCamera.cpp](../../../core/tmr/device/WebCamera.cpp) | Unique ImplとSession adapter、選択Device、Capability／Configuration cache、最終Error。列挙Runtimeは共有 |
| Session | [Camera.cpp](../../../core/tmr/camera/Camera.cpp) | Unique Impl、共有Backend、任意のCallback threadと共有CallbackState |
| 内部境界 | [CameraBackend.h](../../../core/tmr/camera/CameraBackend.h)、[WebCameraInternal.h](../../../core/tmr/device/WebCameraInternal.h) | 内部Interface／Test注入。InstallするConsumer APIではない |
| Windows取得 | [MediaFoundationCameraBackend.cpp](../../../platform/tmr/win/camera/MediaFoundationCameraBackend.cpp) | Source reader、Source、CallbackのCOM owner。選択されたNative packed profileには内部DirectShow経路 |
| Linux選択／取得 | [LinuxCameraBackend.cpp](../../../platform/tmr/linux/camera/LinuxCameraBackend.cpp)、[V4L2](../../../platform/tmr/linux/camera/V4L2CameraBackend.cpp)、[libcamera](../../../platform/tmr/linux/camera/LibcameraCameraBackend.cpp) | RouterがAdapterを選択。各AdapterがDescriptor／BufferまたはCamera manager／Requestを所有 |
| Frame値／処理 | [CameraTypes.cpp](../../../core/tmr/camera/CameraTypes.cpp)、[CameraFrameOps.cpp](../../../core/tmr/camera/CameraFrameOps.cpp) | 所有Byte値とCoreへのFormat変換。Deviceを所有しない |

</div></div>


```text
WebCamera unique Impl
  +-- owns --> unique session adapter --> CameraSession unique Impl
                                           +-- retains --> shared backend --> native resources
                                           +-- owns --> callback thread
                                                          +-- retains --> CallbackState
                                                                           +-- retains --> backend
backend -- copies --> owned frame bytes
thread -- borrows loop-local result during invocation --> user callback
```

WorkerはCameraSession本体ではなくCallbackStateとCallableをCaptureします。Backendの共有保持はCallback停止時の
寿命を保護するためで、Owner操作の並行実行を許可しません。CameraSessionはCopyもMoveもできません。
WebCameraはMove-onlyで、ImplとSessionを移譲し、Move代入は先に代入先SessionをCloseします。
Move元にSessionを自動再生成しません。通常のFacadeでは`close()`後も再Openできます。

## 対応Backend

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Target | Backend | 現在の実装範囲 |
| --- | --- | --- |
| Windows x86-64 | Media Foundation | 列挙、Capability、BGRA8 Frame、同期／Callback取得、停止／再開、共通Camera Control |
| Linux x86-64／ARM64 | V4L2 | `/dev/video*`列挙、Format／Control照会、MMAP Stream、同期／Callback取得 |
| Linux x86-64 | libcamera 0.7.2 | 列挙、Capability、Stream設定、Request／Buffer、所有Frame、停止／再開、切断通知および共通Camera Control |
| Linux ARM64 | libcamera | 同じAdapter sourceを利用可能。Pi 4 Native／実機Gateで受理するまで固定ARM64 Dependencyを既定提供せず、明示RootがないBuildは`UnsupportedBackend` |

</div></div>


`Automatic`は対象OSの既定Backendを選びます。利用できないBackendを要求した場合、架空のDeviceや
空の成功値へFallbackせず、構造化Errorを返します。Camera IDはBackendが返すOpaque値であり、利用側は
PathやSymbolic linkの形式を解釈してはいけません。

LinuxのBackend選択はV4L2 Device I/Oから分離した内部Routerが担当します。V4L2とlibcameraは個別の
Adapter境界を持ち、新しい機種やCamera stackを追加しても公開`CameraSession`を変更しません。`Automatic`は
実libcamera AdapterがDeviceを列挙できた場合にlibcameraを選び、DeviceがなければV4L2へ移ります。明示指定した
BackendがBuildされていない場合は`UnsupportedBackend`を返し、架空Deviceを生成しません。

libcameraは0.7.2、Git Commit `191e202178f02430b5942397c70d215cdd2056fa`、
LGPL-2.1-or-laterへ固定します。BootstrapがSource、Licenseおよび生成物を`vendor/libcamera`へ準備し、
CMake Configure中はNetwork取得しません。libcamera 0.7.2自体がC++20を要求するため、その内部Adapter sourceだけを
C++20でCompileします。WSE公開Header、公開TargetおよびConsumerの要求はC++17のままです。

## DeviceとCapability

`CameraSession::enumerate()`は`backend`、Opaque `id`、表示名および型付きTransport情報を持つ
`sCameraDeviceInfo`を返します。Transportは少なくとも`UsbUvc`、`Csi`、`Virtual`、`Network`および
`Unknown`を区別します。UVC Deviceでは取得可能な場合にUSB Vendor ID、Product ID、Serial numberおよび
UVC versionを任意情報として返します。取得不能な識別情報を推測または空文字の成功で補いません。

`capabilities()`は選択DeviceのNative stream profile、変換可能なOutput formatおよびControl Capabilityを
返します。Native stream profileはWidth、Height、Frame rate分子／分母およびDeviceが送出するPixel formatの
組です。Open要求はNative profile、利用側へ渡すOutput pixel formatおよび変換許可方針を分離します。
BackendがMJPEG／YUYVをBGRA8へ変換する場合でもNative profileをBGRA8として広告してはいけません。
変換を許可しない要求は完全一致しない限り`UnsupportedFormat`を返します。

`CameraSession::open(sCameraOpenDescription)`は互換経路であり、Stream configurationへ明示変換します。
現行APIはNative profileとOutput formatを分離したOpen overloadを使用します。Backend固有の
差はCapabilityまたは`UnsupportedFormat`／`UnsupportedControl`で表し、Silent変換を契約にしません。

libcameraは`FrameDurationLimits`から最短／最長Frame durationと範囲内の30 fps候補をCapabilityへ変換します。
要求FPSを同Controlで正確に指定できないCameraは、推測値を広告せず`UnsupportedFormat`とします。

8bit形式に加えて、Frameは`Gray16`、`Rgb16`、`Bgr16`および16bit Bayer 4種を運べます。Bayerの名前は
左上2x2の並びを表します。絵ではなくMosaicを送るSensorにはこれらが要り、解釈ではなくSensorのSample
そのものを扱いたい場合にも要ります。将来のPixel Format追加で既存の列挙値を変更してはいけません。

`FrameRate`は正規のControlの1つです。Frame rateがRateではなくRegisterであるDeviceも、他のControl
と同じく`unit`と`physical_scale`で表します。

Backendは変換せずにFrameを渡すこともできます。Deviceが元から出す形式を出力として要求し、変換を無効に
すれば、Deviceが送ったままのSampleが届きます。Mosaicが往復を生き延びるのはこの経路だけです。変換すると
Mosaicの絵になり、そこから色を起こすことはできません。

## LifecycleとThread契約

Lifecycleは`Closed -> Open -> Streaming -> Open -> Closed`です。

- `open()`は有効なDeviceとFormatを必要とし、二重Openは`AlreadyOpen`です。
- `start()`は同期読出し可能なStreamを開始します。`start(callback)`は同じStream上に専用Workerを作ります。
- `readFrame(timeout_ms)`のTimeoutは1 ms以上です。Callback中の同期Readは`ConcurrentRead`です。
- `stop()`と`close()`は未Open／停止済み状態でも安全に呼べます。停止後は同じOpen Sessionを再開できます。
- DestructorはCallback workerとBackend resourceを終了します。
- CallbackはSession所有Worker上で逐次実行され、同一Sessionで並列呼出しされません。Callback例外は
  Library境界を越えず、そのCallback Streamを終了します。
- Callback内から`stop()`を呼ぶと停止を要求し、自己Joinも通常経路のdetachも行いません。所有Threadからの
  次の`stop()`、`start()`または`close()`が終了済みWorkerをJoinして回収します。Callback内でのSession破棄は
  互換上Memory safeに退避しますが、所有側で回収できないため非推奨です。利用側は同一`CameraSession`への
  他Threadからの同時操作を外部同期し、Callbackが参照する利用側ObjectをCallback終了まで保持します。

`CameraSession`と`WebCamera`のPIMPLは`std::unique_ptr`が単独所有します。Windows Media Foundation Backendの
COM objectは`ComPtr`で所有し、独自CallbackのCOM規約上の`Release()`実装を除いて手動参照解放を行いません。
Linux BackendのFile descriptor、MMAPおよびlibcamera objectも各BackendのRAII ownerから公開境界へ出しません。

### Openと選択の順序: TMR-OPEN-02

明示的な`WebCamera::open(device, configuration)`はSessionの有無、AlreadyOpen、Configurationの妥当性、
許可されたTransport、Capability照会、広告されたNative profile／Output formatとの完全一致を順に確認して
Sessionを呼びます。Width、Height、Format、Frame rateの分子と分母が一致する必要があり、等価な分数への
正規化は行いません。Sessionは候補Backendを作成し、Open成功時だけ保持します。失敗時は候補を破棄します。

`open(device)`は優先Outputを持つ最初の有効Profileを選びます。現行の優先順位はBGRA8、BGR8、RGB8、
YUYV422、NV12、Gray8、MJPEGです。最大解像度／最大FPSの選択ではなく、この一覧外の形式だけに対応する
Profileには明示Configurationが必要です。Native／Outputの区別と`allow_conversion`も要求の一部です。

CapabilityはCacheであり、観測のたびにDeviceを照会しません。`currentCapabilities()`が確認するのは選択情報で、
Deviceの生存ではありません。Index constructorも選択だけを行い、Openしません。既定Openは明示Openへの委譲前に
選択を更新するため、Open失敗やAlreadyOpenの後に新規Sessionと一致しない選択情報が残る場合があります。
明示Configurationと操作結果、`isOpen()`を使い、Cacheだけで所有状態を判断しません。`close()`は選択、
Capability／Configuration cacheおよび最終Errorを消去します。

### CallbackとSessionの状態: TMR-CALLBACK-03

BackendのStreamingとCallback配信は別の状態です。

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| 呼出し／Event | Session／Backendへの効果 | Worker／結果への効果 |
| --- | --- | --- |
| ClosedからStart | NotOpen | Workerなし |
| OpenからStart | Backend startへ委譲 | 同期StartにはCallback workerなし |
| 空CallbackでStart | Backend start前にInvalidArgument | Workerなし |
| Callable付きStart | 所有State／Callableを準備後にBackend start | Thread所有権を公開後、WorkerのRead／利用側Callbackを許可 |
| State／Callable準備が例外 | Backendは開始しない | 元のC++例外を送出。Workerなし |
| Backend開始後のThread生成／Copyが例外 | Backend stop、停止失敗／例外ならClose | Callback stateを消去し元の生成例外を送出。Openなら再Start、Closedなら再Open |
| Callback active中の同期Read | ConcurrentRead | Frameを競合取得しない |
| Worker readがTimedOut | Streamingを継続 | Callbackを呼ばず再Read |
| その他のWorker read失敗 | Backend状態はAdapterに従う | 失敗を1回配信して配信終了 |
| Worker readが例外 | Backendは自動停止しない | 事前確保済みResultでbad_allocはResourceExhausted、他はBackendFailureを1回通知して配信終了 |
| 利用側Callbackが例外 | Backendは自動停止しない | 例外を捕捉して配信終了 |
| Owner threadからStop | Stop flag設定、進行中Readを待ちBackend stopを直列化 | Backend mutexを解放後にJoin、CallbackStateを解放しBackend stop結果を返す |
| Callback threadからStop | 同じ停止要求／Backend stop | Joinは延期し、後でOwnerが回収 |
| Close | Stop、Backend close、Backend参照解放 | Owner側では先にJoin。繰返しCloseは安全 |

</div></div>


Loopは共有Backend mutexの下で`readFrame(100 ms)`を要求し、Lockを解放後にStopを再確認し、
TimedOutを抑制して、全状態Lockの外でCallbackを呼びます。Timeout以外のRead失敗またはCallback例外で配信を終了します。
Cancel後にReadが完了しても勝手に終端Errorを配信しません。確認後のStopは呼出しと重なり得るため、
Owner側Joinが完了境界です。引数はLoop内Resultの借用なので、Frameを保持するならCopyします。

Worker終了でfalseになるのは`active`であり、Backendは必ずしも停止しません。`isStreaming()`はBackendを
照会するため、Callback例外や終端Read error後もtrueになり得ます。再開前にOwner側の`stop()`でBackend停止と
Thread回収を行います。Callback内から自分のWorkerを再開できません。WebCameraは現行Callbackをそのまま渡し、
そのErrorを`lastError()`へ転記しないため、配信Resultを確認します。成功操作が最終Errorを消す場合があり、
Cache観測も常に最終Errorを更新するわけではありません。

100 msはBackendへのRead要求であり、Shutdown時間の保証ではありません。利用者CodeとNative stopの時間も加わります。
V4L2は単一期限に対して待機を再試行しますが、CameraSessionはRead／後始末全体の期限やCancellation tokenを持ちません。
OwnerのLifecycle操作は、記載したCallback stop例外を除いて外部で直列化します。

OwnerはBackend開始前にCallbackState、終端Error格納領域、CallableのCopyを準備します。
Publication conditionにより、Ownerが`std::thread`を格納するまで新Workerは待機します。
Callable／Capture破棄まで保持するThread-localなCallback識別により、自己StopはOwnerがJoin中かもしれないThread objectへ触れず回収を延期します。
両Stop呼出しはCallbackStateのBackend mutexでNative stopを直列化し、Join中は保持しません。
Native stopが例外ならBackendをCloseし、Owner側ではWorkerを回収してから再送出します。
`close()`はその例外を捕捉して後始末を完了します。Native startの例外でも不確実な途中状態をCloseして再送出します。
公開C++の確保／Thread例外は送出され得ます。これは巻戻し契約であり、全操作の無例外／無確保Result保証ではありません。

### Backend処理と失敗時の限界

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Adapter | 取得／Driverへの返却順序 | 停止／途中失敗 |
| --- | --- | --- |
| Media Foundation | Sample要求、内部Callback通知待機、所有PayloadへCopy／検査 | SessionがCallback read／stopを直列化。Adapter stopで公開を閉じ非同期Flush完了を待機。CloseでSource shutdownとCOM解放 |
| V4L2 | MMAP bufferをQueue、STREAMON。期限付きpoll、DQBUF、返却Owner／Copy／検査、QBUF | QBUF／STREAMON途中失敗はSTREAMOFFで復元。復元／Stop失敗ならDescriptorをClose後にUnmap。復元成功なら再Start可能 |
| libcamera | Request作成、Camera start、Queue。完了QueueでReaderを起こし、FrameをCopy、Requestを再利用／再Queue | Start／Readを巻戻し、完了Ringを事前確保。Stop／返却BarrierまでPending所有を保持 |

</div></div>


V4L2はQueue所有状態をStreamingとは別に保持します。STREAMON失敗でもQueue済みBufferはDriver所有のため、
最初のQBUFで失敗した場合も含め、再試行前にSTREAMOFFでQueueを復元します。元の失敗操作のerrnoを後始末後も保持します。
後始末は確保不要のNative呼出しを使用し、復元不能ならDeviceをCloseして各Mappingを1回解放します。
これはKernelの[STREAMON／STREAMOFF契約](https://docs.kernel.org/userspace-api/media/v4l/vidioc-streamon.html)に従います。

### Native Frameの返却義務と待機予算: TMR-NATIVE-05

V4L2 OpenはDescriptor取得直後に巻戻しOwnerを置きます。Format設定、Buffer vector確保、全Mappingが
成功するまで、途中Return／C++例外でDescriptorをCloseし、成功済みMappingを全て解放します。
失敗後はOpenを再試行できます。Error生成中の例外も対象ですが、Error生成自体は無確保ではありません。

V4L2 Readは正のmillisecond予算から`steady_clock`の期限を1回計算します。各poll直前に残時間を
millisecondへ切上げ、`INT_MAX`で制限します。長い区間のpoll Timeout、EINTR、DQBUFのEAGAIN／EINTRでも
同じ期限へ戻ります。予算切れならDequeueせず`TimedOut`を返します。10 msで3 ms後に割込みなら残り7 msです。
`UINT32_MAX` msは2147483647、2147483647、1 msに分け、負数による無期限待機にはしません。

DQBUF成功後、有効IndexにはScope内で1回の返却義務を持たせます。所有ByteへCopyしてFrameを検査し、
Type／Memory／Indexのみを入れた零初期化QBUFで返します。Payload超過、Driver error flag、格納量不足、
Copy例外でも返却します。QBUF失敗ならCameraをClose後にUnmapします。Index不正や終端Dequeue失敗も、
返却先を推測せずCloseします。Copy／検査が先に失敗した場合は、後始末失敗より元のResult／例外を優先します。
正常FrameのQBUF失敗なら保存したNative errorを返し、Frameは公開しません。返却失敗後は`isOpen()`を確認し、
再Start前にOpenし直します。[KernelのBuffer交換契約](https://docs.kernel.org/userspace-api/media/v4l/vidioc-qbuf.html)では
失敗したDequeueが有効IndexなしにBufferを消費する場合があり、このClose方針はそれに対応します。

Media FoundationはSampleとContiguous bufferをCOM ownerで保持します。内部の
[Frame copy helper](../../../platform/tmr/win/camera/MediaFoundationFrame.h)はLock成功後だけUnlock義務を作ります。
Pointer、Length／Capacity、Timestamp倍率の範囲を検査し、所有ByteへCopy、格納量検査、Unlock 1回の順で成功します。
Metadata不正、Frame不足、確保例外でもUnlockを1回試みます。Lock失敗ではUnlockしません。正常FrameのUnlock失敗は
`Backend/ReadFailed`ですが、先行する検査Error／例外があればそちらを優先します。このSample copy失敗だけでは
Adapterを自動停止しません。試行後にBuffer ownerを解放しますが、Native Unlock失敗をDriver復旧の証明とは扱いません。
[Media FoundationのLock寿命](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock)に従います。

これらはFrame待機予算であり、Copy、Native call、後始末、Stopの実時間上限ではありません。
V4L2の後始末ioctlは引き続き期限なしでEINTRを再試行します。MF／DirectShowは個別の絶対待機期限、
libcameraは述語付き待機を使用します。Session共通の処理全体DeadlineやCancellation tokenはありません。
待機成功でも、Read全体が予算内に返るとは保証しません。

`wse.tmr.v4l2_read_contract`は実Adapterに27件の人工Open／Read故障を注入します。Vector確保失敗、
各Query／Map位置、Error生成失敗、Payload確保失敗、Buffer不正、返却とStopの複合失敗、仮想Clockによる
EINTR／EAGAIN／巨大Timeoutを対象とします。Close／Unmap／返却の一意性、再Open、元Error、所有Byteも検査します。
`wse.tmr.media_foundation_frame_contract`は実Helperに16件のFake buffer検証を行います。Lock失敗
（E_OUTOFMEMORYを含む）、Payload確保失敗、Metadata／格納量不正、Timestamp overflow、Unlock失敗、
所有Copyの寿命が対象です。Windowsの確保故障はDebug STL管理領域ではなくPayloadに限定します。実Driverは使いません。

MFのActivation／Reader factory内部やlibcamera内部の故障、実抜去／復旧、全Allocator枯渇は
残る検証課題です。公開C++では後始末後も確保例外が伝播し、全操作の無例外Resultは保証しません。

### libcameraのRequest／Mappingトランザクション: TMR-LIBCAMERA-06

内部の[資源Helper](../../../platform/tmr/linux/camera/LibcameraResources.h)で所有権と診断生成を分離します。
Plane Copyは使用量、長さ0、OffsetとLengthの加算Overflow、出力容量をMap前に検査します。
Scoped ownerはCopy確保例外時も`munmap`を1回試み、Map失敗時にはUnmapしません。明示Unmapの失敗は
`ReadFailed`ですが、1回の解放試行はNative Unmap失敗からの復旧保証ではありません。
Copy済みByteはNative Planeと別所有になり、途中までCopyしたFrameは公開しません。

StartはRequest全体と完了格納域の準備、初期Control、Native start、各RequestのControl設定とQueueを
一つのトランザクションにします。失敗Resultや例外では、WSEの確保不要な後始末を経て元の失敗を返します。
libcamera内部のStop自体は確保や待機を行いえます。Native stop成功をRequest破棄のBarrierとし、
後始末Errorで元のErrorを上書きしません。Open中のSignal接続失敗も巻き戻し、Camera参照はManager停止前に解放します。

同じ資源Helperの共有Manager Leaseを、1つのWSE Runtime内の列挙・能力照会・Open Sessionで共有します。
libcameraはCameraManagerを1つしか許可しません。Lease取得と最後のManager破棄を同じRegistry Mutexで直列化し、
新規生成と最終解放の競合を防ぎます。生成／Start失敗ではLeaseを残さず、Camera参照を解放してからLeaseを返します。
`wse.tmr.libcamera_manager_contract`は失敗後の再試行、Leaseの重複保持、並行1,000回と、実際の固定版Managerの
2つのLeaseを検査します。実Cameraは開きません。WSEと独立したlibcamera Managerの同時生成、独立ロードされた
複数WSE Runtimeの混用は避けます。この共有化は、下記の上流Libraryの切断時Pending問題を修復するものではありません。

完了Callbackはlibcamera Threadで動作します。Mutex下で事前確保RingとPending台帳を更新しReaderを起こすだけで、
Container拡張やControl Metadata解釈をしません。容量は所有Request数と同じです。Cancelled RequestはPending所有を
解除しますがFrameを公開しません。不明／重複Completionや内部Callback失敗は終端状態として保持し、Readerが
`BackendFailure`で観測します。MetadataはReader側で更新します。Callback外周で例外を捕捉しますが、libcamera自体の
Signal配信やNative Allocatorを無例外にする保証ではありません。

Readerは完了Requestを取得し、Metadata更新、Frame Copy、Reuse、Control設定、1回の再Queueを行います。
通常のCopy検証Errorでも再Queueを試みます。Copy／Control／Reuse例外または再Queue失敗ではStreamを停止し、
先行するCopy Errorがあれば再Queue Errorより優先します。失敗後の再開は先に`stop()`の成功を確認します。
後始末成功時はOpenを維持しますが、`isStreaming()==false`だけでは全Native Requestの返却を証明しません。

Stop失敗時もRequestとAllocatorの所有を保持し、Native動作またはPending所有が残る間は再Startを拒否します。
CloseはStopを再試行し、Pending返却を待ってからSignalを切断し、Request、Allocator、Configuration、Camera、
Managerの順に解放します。この待機にDeadlineはありません。固定libcamera 0.7.2の通常の`Camera::stop()`は
Pending完了を同期的に待ちますが、`Camera::disconnect()`にはPending処理の未解決事項が残っています。
切断時はCloseが無期限に待つ可能性があり、人工Testから抜去復旧の実機認証を推定しません。
本書のBackend節の固定Commitと[上流Camera API](https://docs.libcamera.org/master/public-api/classlibcamera_1_1Camera.html)を参照します。

`wse.tmr.libcamera_resources_contract`は本番Helperに人工Mapping／Request操作を与え、確保巻戻し、Overflow、
独立Byte、解放失敗、Start／Readのトランザクション、Pending保持、取消、確保なしのRing 1,000周を検証します。
libcamera内部そのものへの故障注入ではありません。有効Linux構成では実Adapterの列挙／無効ID検査を必須にし、
Stubの成功で代用しません。いずれも実Camera撮影やDriver停止時間を認証しません。

### Media Foundation初期化の所有権: TMR-MF-INIT-07

内部の[資源Owner](../../../platform/tmr/win/camera/MediaFoundationResources.h)で列挙、能力照会、Openを扱います。
Task allocatorが返したActivation配列を直ちに所有し、全参照を保持したまま出力Vectorを確保します。
参照を`ComPtr`へ移す際は、次の操作が例外を出す前にRaw配列のSlotを空にします。未移譲参照と配列は
どの終了経路でも解放します。取得したDevice名もUTF-8変換前にTask memory ownerへ渡します。
Microsoftの[列挙結果の所有権](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-mfenumdevicesources)と
[確保済み文字列の契約](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getallocatedstring)に対応します。

Source ownerは`ComPtr`と、参照解放前にWSEが1回試みる`Shutdown`を組み合わせます。能力照会ではReader／Controlより
先にSource ownerを宣言し、Profile Copyや診断生成が例外を出してもReader／Controlの参照を先に解放します。
両Reader生成経路で`MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN=TRUE`を設定し、
[Reader所有権属性](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-source-reader-disconnect-mediasource-on-shutdown)に従ってShutdownの責務をWSEへ残します。

Runtime ownerはCOM初期化成功（`S_FALSE`を含む）ごとに`CoUninitialize`を1回対応させます。
`RPC_E_CHANGED_MODE`では既存Apartmentを借り、COM終了の責務を取得しません。MF startup失敗時は直ちにCOMの責務を
解放します。Startup成功時はMF shutdown後にCOMを終了します。後始末の繰返しは無害で、同じRuntime ownerを再初期化できます。
Microsoftの[COM初期化の対応関係](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex)を参照します。

Openは入力／Lifecycle検査後、Runtime初期化前に巻戻しOwnerを設定します。その後の失敗ResultやC++例外は
DirectShow代替経路も含め途中のBackendをCloseし、設定完了時だけCommitします。先行するNative失敗後はCallbackを
確保しません。診断生成が成功すれば元のHRESULTを保持しますが、診断生成自体の確保失敗は後始末後に例外として
伝播しえます。`AlreadyOpen`で拒否した再Openは既存Sessionを閉じません。

専用Native ThreadがRuntime初期化と最終解放の所有を守ります。下記TMR-MF-STOP-08を参照します。
CallbackのSelf-stopでCOM所有は移りません。Shutdown失敗後のDriver復旧、Native呼出しの無期限待機は完了保証の範囲外です。

`wse.tmr.media_foundation_resources_contract`は29ケースを実行します。本番Ownerへ人工COM object／Runtime結果を与え、
実Vector／文字列の確保失敗、部分移譲、Source解放順序、巻戻し／Commit、再試行を検証し、実MF属性StoreでReader設定も
確認します。Cameraは開きません。実Activation／Reader factory内部や全Adapter確保箇所への故障注入、全Heap枯渇、
実機認証を行うTestではありません。

### Media Foundationの停止と所有Thread: TMR-MF-STOP-08

内部の[Native所有Thread](../../../core/tmr/camera/CameraOwnerThread.h)がWindows Backendを生成し、全操作と破棄を
同じ専用Threadで実行します。DirectShow代替経路とCOM／MFの初期化・終了も含みます。静的な列挙・能力照会は、
呼出しThread内で初期化から破棄まで完結する一時Runtimeを使います。公開Camera Callback Workerは別Threadです。
動作中の呼出しがないSessionは別ThreadからCloseできますが、操作と破棄の並行実行は禁止のままです。
Native操作から同じDispatcherへ同期再入してはいけません。Dispatcherは完了までStack上のTaskと引数を借り、
配送のためのHeap確保をしません。返却値やNative操作自体の確保は別です。Factory失敗時はThreadをJoinしてから
例外を伝播し、操作例外は待機している呼出し側へ戻します。

[Reader状態](../../../platform/tmr/win/camera/MediaFoundationReadState.h)は非同期`Flush`前にFrame公開を閉じます。
未完了Readがない場合もNative側の待ちFrameを捨てるためFlushします。Flushの戻りだけでは再開せず、
Native呼出しが戻った後、`OnFlush`通知を最大2,000 ms待ちます。
Microsoftの[Flush完了契約](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-flush)に従います。
Flush中のRead CallbackはFrameを公開せず、Flush待機も解除しません。完了成功後のStartだけが公開を再設定します。
FlushのHRESULT失敗はNative code付きBackendFailure、通知TimeoutはTimedOutを返します。両方ともReaderを終端状態にし、
Close／ReopenまでStartを拒否します。CloseはCallback状態を閉じてからReader／Control／Source参照を解放し、専用Threadで
MF／COMを終了します。遅れて届くCallbackは自身のCOM状態を保持し、破棄されたBackendを借りません。Stop／Closeの繰返しは無害です。

2,000 msは通知待機の予算であり、Flush／Release／Shutdown、Driver呼出し、利用側Callback実行の期限ではありません。
所有Thread契約はFactory失敗、例外、Move-only／Void返却、同時配送1,000回、配送時の確保禁止、別Threadからの破棄を検査します。
MF Read状態契約は本番Helperと実MF Sampleで、成功、HRESULT失敗、通知欠落・遅延、再開拒否、Sample保持、公開境界を確認します。
任意のNative factory内部故障、Process全体のOOM、物理抜去の認証ではありません。

## FrameとOwnership

`sCameraFrame`はCopy可能な所有値で、Frame byte列はBackend bufferから分離されています。利用側は次の
Frame取得後およびSession終了後も保持できます。FrameはDescription、Sequenceおよび単調増加Clockの
nanosecond Timestampを持ちます。Timestamp epochは公開せず、同一Stream内の順序／差分に使用します。

### Byte配置と検査: TMR-FRAME-04

`row_stride == 0`は最小Packed strideを選び、明示Strideは最小値以上が必要です。Packed形式の必要量は
`effective_stride * height`です。Camera Frameは`data.size() >= memorySize()`を許容し、余分な末尾Byteを
拒否しません。Renderer Frameの完全一致条件とは異なります。例としてBGRA8の3 x 2、Stride 16は32 Byte必要で、
画素(x=2,y=1)はOffset `1*16 + 2*4 = 24`、31 Byteは無効、33 Byteは有効です。

NV12はWidthとHeightの両方が偶数で、YとUV Planeは同じStrideで連続します。必要量は
`stride*height + stride*(height/2)`です。4 x 2、Stride 6ならYが12 Byte、UVが6 Byte、合計18 Byteです。
YUYV／UYVYは偶数Widthが必要ですが、奇数Heightは許容します。MJPEGは非空の圧縮Payloadで、非零Extent、
Stride 0、記述上の`memorySize() == 0`を使用し、非圧縮のbytes-per-pixelを当てはめません。

Size計算ではOverflowとStride不足を拒否し、16bit SampleはLittle Endianです。
`valid()`は格納構造の検査であり、MJPEGの復号可否や画像品質を保証しません。Sequence生成とTimestamp取得は
Adapter依存です。停止／再開をまたぐSequenceの単調性や、異なるDeviceのTimestamp同期を仮定しません。

## Error契約

失敗は`CameraResult<T>`／`CameraStatus`で返します。`CameraError`は安定したCategory、Code、診断Message
および任意のNative codeを持ちます。利用側の分岐はCategory／Codeを使い、MessageやNative codeは診断に
限定します。Validation、Lifecycle、Device、I/O、Timeout、UnsupportedおよびBackend failureを区別します。
Device抜去は可能な場合`DeviceDisconnected`へ分類し、HangやProcess終了として扱いません。

## Control

共通Control catalogは、少なくともExposure、Gain、Focus、Brightness、Contrast、Saturation、White Balance、
Zoom、Iris、Hue、Sharpness、Gamma、Color Enable、Backlight Compensation、Pan、Tilt、Rollおよび
Power Line Frequencyを識別します。Deviceが未対応のControlはCapabilityへ含めず、すべてのCameraが同じ
Controlを持つと仮定しません。Capabilityは安定したControl ID、表示名、Range、Step、Default、
Manual／Automatic対応、Read／Write可否および単位を示します。将来の標準Control追加で既存Control IDを
変更してはいけません。

V4L2 Backendは対応する標準Controlを照会・設定します。Windows BackendはMedia Foundation Sourceから取得した
Windows Camera Controlを内部Adapterで同じ型へ変換します。これらのOS interface、Property IDおよびFlagは
公開Headerへ露出しません。利用側はBackendを判定せず、Capability列挙、`getControl()`および`setControl()`を
使用します。`WebCamera`はExposure、Gain、Focus等の型付きConvenience methodも提供しますが、内部では同じ
Capability検証経路を通します。

Manual時の`value`はCapabilityのRange／Stepに従います。`valueFromNormalized(0.0～1.0)`と
`normalizedFromValue()`を使えば、OSごとのNative単位を知らずにSlider等の相対制御を実装できます。
Automatic時の`value`は無視します。Range外、Step不一致、非対応ModeはDeviceへ送らず構造化Errorを返します。
読取り失敗と書込み失敗は`ControlReadFailed`／`ControlWriteFailed`で区別します。未対応Controlを成功扱いして
はいけません。

Capabilityは`unit`と`physical_scale`も持ち、`physicalFromValue()`／`valueFromPhysical()`で物理量と
整数`value`を相互変換します。公開単位はExposureのMicroseconds、White BalanceのKelvin、FocusのDiopters、
GainのMultiplierおよびBrightness／Contrast／SaturationのRelativeです。Backendが換算を保証できないControlは
`DeviceNative`を返します。利用側は特定OSの単位を推測せず、`unit`を確認して表示・保存します。

### UVC Extension Unit

標準Controlで表現できないUVC Extension Unit（XU）は、通常Controlとは分離した任意の高度APIで扱います。
公開識別子はUVC Unit GUID／Unit ID、Selector、方向、許容Payload長およびRead／Write可否を持ちます。
Payloadは所有Byte列として受け渡し、公開APIへ`IKsControl`、Windows Property set、Linux ioctl、Pointerまたは
File descriptorを露出しません。

XUの書込みは、Deviceが広告したSelectorとLengthの一致、最大長、Session状態およびAccess modeをDevice I/O前に
検証します。未知Selectorの自動書込み、Vendor commandの推測および成功扱いのFallbackは禁止します。公開WSEは
汎用XU transportだけを提供し、機種固有のSelector名、Schema、Commandおよび秘密情報はPrivate
Extensionに置きます。

### Stream後処理との境界

Rotate、Flip、Frame averaging、Color変換およびOpenCV変換はUVC Device Controlではありません。
これらは所有Frameに対するPortable image処理または利用側Adapterの責務とし、Camera Backendへ混在させません。
OS標準のCamera設定WindowもPortable APIの必須機能にせず、必要な場合だけPlatform optional serviceとして
提供します。

その画像処理は`CameraFrameOps.h`が担います。Cameraのメソッドではなく所有Frameに対する自由関数として
提供し、Device操作と画素の加工を分けています。向き補正、同一形状のFrameを平均する累積器、および
Bayer配列からColorへの変換を持ちます。演算そのものはCoreの`wse_ImageTransform.h`と
`wse_ImageDemosaic.h`にあり、Cameraを一切知りません。本HeaderはCamera Pixel FormatをCoreの形式へ
写し、写せない形式を明示的に拒否するというCamera側の責務だけを持ちます。

向き補正と平均化はBayer Frameに対して答えが分かれるため、問いを分けています。Bayerの画素を動かすと
別の色のSiteへ移るため向き補正は拒否し、`isFrameOperationSupported()`がそれを報告します。平均化は
Frame間で同じSite同士を足すだけで配列を壊さないため受け入れ、`isFrameAveragingSupported()`が
そちらを報告します。Colorへ起こす前に平均するほうがNoiseも落ちます。

加算はSample単位で行うため、16bit形式でも上位byteと下位byteは分かれません。変換方式は、2x2の4 Sampleを
4画素へ複製して値を変えないBlock複製と、近傍を平均して端を滑らかにするBilinearから選べます。
いずれも最終行・最終列を含む全画素を書き込みます。

<a id="ja-frames-as-core-images"></a>
### FrameをCoreのImageとして扱う

同じHeaderが、所有FrameとCoreの`wse::Image_`を相互に変換します。`toImage()`はRow Strideを畳んで
詰まったImageを返し、`toCameraFrame()`は詰まったFrameを返します。`readImage()`と
`readAveragedImage()`は読み出しと変換を1回の呼び出しで行います。いずれも自由関数のままです。
画素を取得することはDeviceの仕事であり、画素を整えることはそうではないからです。

宛先の型がFormatを指名します。これがChannel順序を境界の向こうへ運ぶ仕組みです。Camera Formatは
次のように写ります。

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| Camera Format | Core Format | Image型 |
| --- | --- | --- |
| `Gray8` / `Gray16` | `CH1D8` / `CH1D16` | `img1c08_t` / `img1c16_t` |
| `Rgb8` / `Rgb16` | `CH3D8` / `CH3D16` | `img3c08_t` / `img3c16_t` |
| `Bgr8` / `Bgr16` | `BGR3D8` / `BGR3D16` | `img3c08_bgr_t` / `img3c16_bgr_t` |
| `Bgra8` | `BGRA4D8` | `img4c08_bgra_t` |
| `Bayer16*` | `CH1D16` | `img1c16_t` |
| `Yuyv422` / `Uyvy422` / `Nv12` | 変換後に`CH3D8` | `img3c08_t`のみ |
| `Mjpeg` | なし | なし |

</div></div>


この表の3点は意図的なものです。`Bgr8` FrameはBGRのままのImageになり、途中で並べ替えられません。
そこから`img3c08_t`を要求すると黙って入れ替えるのではなく`UnsupportedFormat`になります。
明示的な経路は`wse::convertChannelOrder()`です。`Bgra8` Frameは4 ChannelのImageになりAlphaを保ちます。
この経路でChannelが捨てられることはありません。そしてBayer FrameはSampleの入れ物としてだけ写ります。
配列はPixel Formatに乗らないため、`bayerPatternOf()`と組にするか、先に`demosaicFrame()`で変換します。

`Yuyv422`、`Uyvy422`、`Nv12`はSubsamplingされており固有のCore Formatを持たないため、BT.601でRGBへ変換し、
`img3c08_t`へのみ写ります。`Mjpeg`は圧縮されておりEngineは復号器を持たないため、
`Unsupported`／`UnsupportedFormat`を返します。16bit SampleはLittle Endianで運ばれます。

これらはすべて`CameraStatus`または`CameraResult`を返します。CoreのImage型は確保失敗を例外で報告するため、
各変換はそれを捕らえて`Backend`／`ResourceExhausted`として返します。例外はTmrの境界を越えません。


## 公開API境界

`CameraSession`はC++の低水準Sessionです。4言語Bindingはいずれも`WebCamera`をCamera Ownerとし、
`CameraSession`を公開しません。`Parameter<T>`は独立したData型です。機種固有XU SchemaやPrivate device headerは公開しません。

### Backendを指定しない利用例

```cpp
using namespace wse::tmr;

const auto devices = CameraSession::enumerate();
if (!devices.succeeded() || devices.value().empty())
    return;

const auto capability = CameraSession::capabilities(devices.value().front());
if (!capability.succeeded() || capability.value().formats.empty())
    return;
CameraSession camera;
if (!camera.open({ capability.value().device, capability.value().formats.front() }).succeeded())
    return;

for (const auto& control : capability.value().controls)
{
    if (control.control == eCameraControl::Brightness && control.supports_manual)
    {
        if (!camera.setControl({ control.control, eCameraControlMode::Manual,
                                 control.valueFromNormalized(0.5) }).succeeded())
            return;
    }
}
```

## Calibration境界

`sCameraCalibration`はDevice I/Oから独立した所有値です。Camera ID、Intrinsics、Brown-Conrady
Distortion、3 x 3 Color matrixおよびLens shading mapを個別の`std::optional` Domainとして保持します。
`valid()`はFinite値、Dimension、正のFocal length、Map要素数などの構造条件を検査しますが、Calibration
精度や対象機器との一致を認証しません。EEPROM等の読書き、補正実行および永続化は別Adapterの責務です。

## Packageと検証境界

<div class="wse-camera-wide" style="max-width:100%;overflow-x:auto"><div style="min-width:48rem">

| 契約 | 実行可能な根拠 | 限界 |
| --- | --- | --- |
| TMR-OWNER-01 / TMR-OPEN-02 | [WebCamera contract](../../../test/characterization/tmr_webcamera_contract.cpp)、[Backend contract](../../../test/characterization/tmr_camera_backend_contract.cpp) | 偽Adapterによる選択／所有。すべてのCache変化や確保失敗を注入していない |
| TMR-CALLBACK-03 | Backend契約と[開始／停止故障注入](../../../test/characterization/tmr_camera_start_stop_contract.cpp): Callable copy／Thread生成失敗、巻戻し失敗、Read例外、Publication、Read／Stop排他、Owner／自己Stop競合 | 確保／Thread失敗経路の注入であり全Heap確保点の網羅ではない。任意Callback／Native呼出しの時間保証なし |
| Native start巻戻し | [V4L2 lifecycle故障注入](../../../test/characterization/tmr_v4l2_lifecycle_contract.cpp): 各QBUF位置、STREAMON／STREAMOFF失敗、元errno、再開、後始末順序 | 実Adapterと人工Native呼出し。物理CameraやMF／libcameraのNative故障認証ではない |
| TMR-FRAME-04 | [Camera contract](../../../test/characterization/tmr_camera_contract.cpp)、[Frame ops](../../../test/characterization/camera_frame_ops_contract.cpp) | Padding、奇数NV12 Height拒否、所有Byte／形式変換。Decoder／画質認証ではない |

</div></div>


Owner図とAdapterの資源順序はSource照合も根拠にします。Mockで実Adapterの全巻戻しを検証したと解釈しません。
画像演算は[Core Algorithms](CoreAlgorithms.md)、Ownerの待機境界は[Thread Ownership](ThreadOwnership.md)、
根拠の範囲は[Design Verification](DesignVerification.md)を参照してください。

Tmr-only Packageは`WSE::Core`と`WSE::Tmr`、公開Camera／Calibration HeaderだけをInstallします。
Private legacy extensionは明示した内部Buildだけで追加され、公開Packageへ混入してはいけません。

Linux x86-64 Tmr Packageは固定libcamera Runtime、IPA module、LicenseおよびDependency metadataを同梱します。
libcameraを含むPackageはLinuxのlibudev、OpenSSLおよびYAML Runtimeへ依存します。Tmrを無効化したPackage、
または`WSE_ENABLE_LIBCAMERA=OFF`のPackageへlibcameraを混入させません。

自動GateはWindows Shared／Static、Linux x86-64 GCC／Clang Shared／Static、Install Consumer、
Linux ARM64 GCC Cross buildを対象とします。Mock BackendはNative／Output変換、Generic XU境界、
1000回のStream中Control、停止／再開、複数Session、所有Frame、切断Errorを実機なしで検証します。
公開Header／Ownership GateはOS固有Camera型の公開、PIMPLの手動確保／解放、通常Callback停止時のdetach、
Windows公開Backendの手動COM解放を拒否します。

実libcamera Adapterの検査範囲はCompile、DeviceなしのManager列挙、無効ID、Mock、Package Consumer、
構造化Errorです。Windows Camera Smokeは選択Profileの列挙、Frame出力、Control、再開、Callbackを対象とします。
Pi 4 V4L2 SmokeはDebug／Release × Shared／Staticで列挙、Capability、読取可能Control、所有Frame、Stop／Closeを
対象とし、Control値は書きません。実機の正確な結果と構成は非公開の受入Recordに置き、Statusの適用範囲は
[Hardware Validation](HardwareValidation.md)に従います。高解像度／全Format、Control書込み／復元、物理抜去／再接続、
長時間運転には個別の証跡が必要です。Pi 5、libcamera実機、Pi CSIは未認証です。
MockやCross build成功を実機認証として扱ってはいけません。

<a id="ja-packed-uyvy-frames"></a>
## Packed UYVY Frame

`Uyvy422`のPixel format値は15です。2画素を
U0 Y0 V0 Y1順の4byteで表します。幅は偶数が必要ですが、奇数の高さとPadding付きStrideに対応します。
Media Foundation、V4L2、libcameraでは変換無効時にNative形式を保持します。
Windowsでは、奇数高さのNative UYVY/YUYV形式をMF Sourceが取得できない場合に対応するため、
内部でDirectShow Graphを使用します。選択済みInterfaceまたは一意なWindows Device Instanceで照合し、
候補が曖昧なら失敗します。Pinは直接接続だけを許可します。GraphはSampleを所有Bufferへコピーして
最新の1枚を保持し、停止前に待機中の読取処理を起こします。Camera／Video処理Controlは選択したSourceの
COM Interfaceを使用し、公開APIへPlatform型を出しません。

奇数高さのPacked Native ProfileでBGRA8を要求し、変換を許可した場合も同じ取得経路を使用します。
Outputだけを指定した互換Descriptionは、同じDeviceの広告済みProfileから、寸法と有理数Frame rateが
完全一致するNative形式を解決します。別Deviceへ切り替えません。変換無効時はNative Byteを保持するか、明示的に失敗します。

[DirectShowCaptureFormat.h](../../../platform/tmr/win/camera/DirectShowCaptureFormat.h)は100 ns単位で
`10000000 * denominator / numerator`を要求Intervalとします。広告された最小／最大Interval内だけで
既定Intervalを変更します。`SetFormat`後の`GetFormat`と接続済みSample型の両方でSubtype、寸法、
16-bit Packed配置、1 tick以内のInterval一致を確認し、Driverが選んだ近いRateへの置換は拒否します。
[WindowsのFormat選択契約](https://learn.microsoft.com/en-us/windows/win32/directshow/configure-the-video-output-format)に従い、
Interval範囲だけでその間の全Rate対応を保証しません。

[CameraYuvConversion.h](../../../core/tmr/camera/CameraYuvConversion.h)は所有Sourceを検査し、
中間RGB画像を作らず各Pairを変換します。`toImage()`と同じBT.601 Limited-rangeの整数係数を使い、
`C=max(0,Y-16)`、`D=U-128`、`E=V-128`から
`R=clamp((298*C+409*E+128)>>8)`、`G=clamp((298*C-100*D-208*E+128)>>8)`、
`B=clamp((298*C+516*D+128)>>8)`を求め、各Channelを0..255へ制限します。
出力はStride `width*4`のPacked B,G,R,255です。SourceのPaddingを読み飛ばし、Sequence／Timestampと行順序を保持します。
奇数高さは許可し、奇数幅は拒否します。不正なStorageは`ReadFailed`、出力確保失敗は`ResourceExhausted`です
（Error構築自体の確保は失敗し得ます）。固定の色変換であり、Deviceの色特性推定や光学校正は行いません。

MF Sample失敗とEnd-of-streamはNative Statusを保持した`Backend/ReadFailed`とし、明示的なDevice無効化または
`ERROR_DEVICE_NOT_CONNECTED`だけを`Device/DeviceDisconnected`とします。Readerの終端失敗後はStreamingを停止し、
Read／Startを拒否します。CloseはReaderの`Flush`を呼ばずに解放し、SourceをShutdownします。復旧にはClose／Reopenが必要です。
これは[Source readerのError境界](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/ne-mfreadwrite-mf_source_reader_flag)に従います。
Windowsの`wse.tmr.directshow_format_contract`と`wse.tmr.media_foundation_error_contract`は人工入力でInterval／Readbackと
Error分類を確認します。Portableな`wse.tmr.camera_frame_ops`は固定BGRA Byte、Padding、Metadata、奇数高さ、不正入力を確認します。
これらはDriver、実抜去、全Native確保失敗の認証ではありません。

`toImage()`はBT.601でRGB8へ明示変換できます。向き補正と平均化の前には変換が必要です。
各言語Bindingでも同じ列挙値を公開します。

C ABIはCallback用Frame handle確保をGuardし、生成失敗時はNULL／Errorを1回通知して配信を終了します。
C#はManaged Callback／受入例外をBridge内で捕捉し、`WebCamera.CallbackException`へ公開します。
受渡し済みFrameはApplication所有のままです。Owner側Stop／Joinは引続き必要です。所有権移譲、Reset、
人工Testの範囲は[CABI-ADOPT-06](CAbiContract.md)を参照します。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
