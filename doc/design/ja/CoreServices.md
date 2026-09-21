# WSE Core共通サービス設計

> Canonical source: [English Core Services](../en/CoreServices.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象と所有関係

Core共通サービスは待機、周期実行、Log、License読込、Platform照会を提供します。
全体を自動制御するEngine Schedulerはありません。Applicationが起動順を決め、
Callbackの依存先を保持し、所有処理を停止してから依存先を解放します。

```text
application thread -> Wait -> standard-library sleep
application owns Timer -> Impl -> WorkerController -> one worker
                            +-> interval + owned callback, protected by settings mutex
caller's logging thread -> shared registry snapshot -> output adapter -> LogSink callbacks
application startup -> License::load -> file/decode/verify -> one shared license state
application query -> pickupDeviceInfo -> selected OS adapter -> owned result values
```

矢印は記載した呼出または包含であり、ComponentのLink依存を追加するものではありません。
Log／Licenseの共有状態はLinkされたWSE Library Instanceに属します。Static Libraryを別Moduleへ
それぞれ組み込んだ場合、ModuleをまたぐSingletonになる保証はありません。

| Service | 公開宣言 | 実装・Test |
| --- | --- | --- |
| Wait | [Wait](../../../api/wse/utility/wse_Wait.h) | [Runtime契約](../../../test/characterization/core_runtime_contract.cpp) |
| Timer | [Timer](../../../api/wse/utility/wse_Timer.h) | [Timer実装](../../../core/wse/utility/wse_Timer.cpp)、[Worker](../../../core/wse/utility/wse_WorkerController.cpp)、Runtime契約 |
| Log | [Log](../../../api/wse/utility/wse_Log.h) | [Log実装](../../../core/wse/utility/wse_Log.cpp)、Runtime契約 |
| License | [License](../../../api/wse/license/wse_License.h)、[Key](../../../api/wse/license/wse_LicenseKey.h)、[Writer](../../../api/wse/license/wse_LicenseWriter.h) | [Loader](../../../core/wse/license/wse_License.cpp)、[State](../../../core/wse/license/wse_LicenceAdmin.cpp)、[License契約](../../../test/characterization/license_contract.cpp) |
| Device照会 | [Portable Core](PortableCore.md) | Platform AdapterとRuntime契約のLinux分岐 |

## 待機と周期の決め方: CORE-SVC-01

`Wait::sec/msec/usec/nsec`は対応するchrono Durationと`sleep_for`で呼出Threadを待機させます。
GUI Loopを処理せず、Cancellationも提供しません。OSのSchedulingで待機が延びることがあります。
Runtime Testは単位のScaleを確認し、Real-timeの上限を保証するものではありません。

Timerは停止状態、保持Interval 1 msから始まります。成功した`start(interval, callback)`はCallableを
所有し、1つのWorkerを起動します。各反復は次の順です。

```text
lock settings -> snapshot interval -> unlock
waitFor(interval): stop request wakes this wait
if stopped: exit
lock settings -> acquire shared callback -> unlock
invoke callback on worker; catch any exception and exit
repeat from interval snapshot
on exit: release stored callback; worker records not-running
```

最初のCallbackは最初の待機後です。IntervalはCallback終了から次回配送までの待ち時間であり、
前回の開始時刻を基準にした固定周期ではありません。Interval 10 ms、Callback 7 msなら開始間隔は
約17 msにScheduling分が加わります。未実行TickのQueue、Callbackの並列実行、遅れの追い付き実行はありません。

`setInterval`は次のInterval取得に反映され、すでに旧Intervalを取得した待機は起こさず短縮しません。
非正Durationは`std::invalid_argument`です。`start`は空Callableも拒否し、引数検査は実行中判定より先です。
Memory確保やThread起動は例外を投げる場合があります。TimerStatusや、それらの失敗全般を元へ戻す保証はありません。

## Timerの状態と停止: CORE-SVC-02

以下は説明用の状態であり、新しい公開Enumではありません。

| 状態・呼出 | 結果と次状態 |
| --- | --- |
| Stopped＋有効start | `true`。Captureを所有してRunning |
| Running＋有効start | `false`。既存Interval／Callbackを維持 |
| 任意＋不正start／setInterval | `std::invalid_argument`。拒否した引数で設定を置換しない |
| Running＋Owner Threadからstop | 停止要求、待機を起こす、Worker Lock外でjoin。Stopped |
| Running＋Callback内stop | 停止要求後、現在のCallbackへ戻る。Stopping |
| 自己停止後にCallbackが戻る | Capture解放、Worker終了。StoppedだがThread回収が残る |
| Callback例外 | 例外を封じ、Capture解放、終了。後でOwnerが回収 |
| Stopped＋stop | 繰返し可能。未回収の終了済みWorkerがあればjoin |
| 自己停止／例外後のStopped＋start | 前Workerを回収してから再起動 |
| Owner Threadで破棄 | 停止要求とjoinの後にImplを解放 |
| Move構築／代入 | 移動元と、代入なら移動先も停止。保持IntervalをCopyし、移動先は停止状態 |

Owner側stopは取得済みCallbackを待つ場合があります。User Codeを強制中断しません。
joinしたstopが戻った後、そのWorkerはCallbackを開始できません。自己停止では現在の呼出が
まだCaptureを保持しています。`isRunning`はWorker状態のSnapshotであり、Callbackと後処理が終わるまでは
trueの場合があります。

Callback内から`stop`やInterval変更は可能です。自身のTimerの破棄・Move・代入はできません。
外部Ownerを生存させ、別Threadで回収します。呼出側のLifecycle操作は直列化してください。
内部Mutexは任意のstart／Move／破棄の同時実行を保証しません。戻らないCallbackはOwnerの停止も妨げます。
共通WorkerとComponent固有例外は[Thread Ownership](ThreadOwnership.md)を参照します。

## Logの構築と配送: CORE-SVC-03

Logは書込元Threadで同期実行します。Registry／設定はState Mutex、Console／File出力は別のOutput Mutexで
守り、Custom Sinkは両方の外で呼びます。暗黙のLog Worker、配送Queue、滞留Bufferはありません。

```text
writeLog(level, tag, source, message, details)
  -> copy profile and construct record (UTC timestamp, severity, strings)
  -> under state lock: filter level and snapshot shared_ptr sinks
  -> under output lock: configured console/file output
  -> without either lock: call each snapshotted sink, contain sink exceptions
  -> return to writer
```

既定の最低LevelはInfoです。Offまたは閾値未満のRecordは配送しません。ただし整形・確保が一切ない保証では
ありません。Profile不在ならConsole／Fileは出力せず、登録Sinkへの配送は可能です。Sink順は
Unordered Registryに依存し、規定しません。

| 入口 | 利用Profile | Recordの扱い |
| --- | --- | --- |
| Sourceを含む5引数`writeLog` | 指定Tag。空ならWSE | 空Sourceは有効Tagになる |
| Sourceなしの短い`writeLog` | WSE Profile | 指定Tag、空ならWSE。SourceはRecord Tagと同じ |
| `formatLogMessage` | 引数の設定。登録不要 | 整形Stringだけを返し、Sink／出力へ配送しない |
| `Logger<Tag>`／ILogger経路 | 登録Tag Profile | 呼出元File名・行番号を整形して配送。Profile不在ならこの経路を抑止 |

標準Formatterは渡されたFile名からDirectoryを除きます。任意のSource／Message／Detailsは呼出側の
Dataであり、自動匿名化されません。整形PrefixのLocal日付・時刻とRecordのUTC Timestampは用途が異なります。

<a id="ja-application-owned-tags"></a>
### Applicationが所有するTag

SDKの組込み定義は`WSE_TAG`／`WLog`と`WSE_DEV_TAG`／`DLog`です。
それ以外のTagと`Logger<Tag>`の別名はApplicationが所有します。外部Linkageを持つ文字配列を
ApplicationのHeaderで宣言し、C++の実装File 1つで定義してTemplate引数へ渡します。
C++17のinline文字配列も、翻訳単位間で同一のTagを持つ定義として使えます。
Stream LoggerはTagのProfileを登録してから使用します。構造化Logの汎用入口は
`writeLog(level, tag, source, message, details)`です。Application固有型はSDKからExportしません。
Install済みPackageのConsumer Testで、独自Tagを複数翻訳単位から使用し、SHARED／STATICの
両方でSinkへの配送を確認します。

## Sinkの寿命・再入・Flush: CORE-SVC-04

`registerLogSink(shared_ptr)`はSinkを保持して非0 Handleを返し、nullなら0です。
`unregisterLogSink`はHandleを削除します。0や存在しないHandleは無害です。解除は将来のRegistry Snapshotから
除くもので、**取得済みSnapshotの呼出を待ちません**。Snapshotのshared_ptrが配送終了までSinkを生存させます。

複数の書込Threadが同じSinkを同時に呼び得るため、Sink内状態は自身で同期します。Sinkは自分を解除できます。
同じThreadから再度Logを書くと、Console／Fileへは出力し得ますが、Thread-localの再入抑止により、
すべてのCustom SinkへのNested配送を止めます。1つのSinkの例外は捕捉し、他Sinkへの配送を続けます。
これは確保やPlatform出力まで常に非例外にする契約ではありません。

`flushLog`はOutput Lock内でstdout／stderrをFlushします。Custom Sinkの排出、Callback待機、
Fileの永続化保証ではありません。最終配送が必要なら、Producerを停止・joinし、Sinkと完了を同期してから
Sinkが借用する外部資源を解放します。

Runtime契約はFilter、配送しない整形、Record項目、通常の解除、自己解除、Nested配送抑止、Sink例外封じを確認します。
すべてのThread間Registry競合やStorage障害方針の検証ではありません。

## License読込と状態: CORE-SVC-05

License読込はApplication起動時に直列化し、並行する機能利用より前に実行します。
共有状態には並行Reload Protocolがありません。Key LoaderとLicense LoaderではFile不在の扱いが異なります。

| 操作 | 結果 |
| --- | --- |
| `Licensekey::load`でKeyを開けない | `LicenseResult`失敗、`Io/FileOpenFailed` |
| Key読取失敗 | `Io/ReadFailed` |
| `License::load`でLicense Byteなし | 成功し、Free／Alpha、Version 0.0.0を確定 |
| 期限付きLicenseが期間外 | `Verification/LicenseExpired`、状態未確定 |
| Device Identity不在・不一致 | `Verification/UnlicensedDevice`、状態未確定 |
| 有効なLoad | Type／Versionを一度だけ確定 |
| 後続Loadが再度の状態確定へ到達 | `std::logic_error`、置換しない |
| WriterのLicense種別Enum不正 | `std::invalid_argument` |

Device Identity取得→File読取→指定KeyでDecode→一時値を解析→状態確定の順です。
期限付きLicenseはLocal Calendar日付を`year*10000 + month*100 + day`として、開始・終了日を
**両端含み**で比較します。Monotonic時間のLeaseではなく、時計変更を防ぎません。FreeへのFallback成功も
一度のLoad枠を消費します。再起動／再読込はApplication側で判断します。

現行File Helperは、不在・読めないLicense Byteと空入力を同じ扱いにします。非空の不正License Dataと
空Decode Keyを網羅的には検査しません。有効な非空Keyと正しく構成したFileを使い、LicenseStatusから
任意入力の頑健な検証や暗号学的な真正性保証を推定しないでください。Writer成功はAtomic置換や遅いStorage障害の
検出保証でもありません。これらは現在の契約に含まれません。

既存License Testは独立したScratch Fileと、Process内1回の成功Loadを使い、Key往復、不正Writer種別、
期限、正常Load、再度の状態確定を確認します。Free Fallbackは公開HeaderとSource Reviewが根拠です。
不正File全般・Device Identityの受入はこのTestでは確認できません。
利用条件は[License Guide](../../ja/LICENSE.md)、Device Identityの制約は[Portable Core](PortableCore.md)を参照します。

## Device照会とPlatform境界: CORE-SVC-06

Device列挙は同期的に所有値を返し、呼出後のHotplugを監視しません。権限付与やDeviceをOpenできる保証も
ありません。LinuxのNetwork／Serial列挙と明示的な非対応種別は[Portable Core](PortableCore.md)に従います。
WindowsはBuildで選ぶAdapterを使います。順序、件数、安定した機器識別子がPlatform間で同じとは限りません。

照会のLifecycle状態はTimerやCamera Sessionと共有しません。Metadata列挙Testは物理入力、Capture、
Display操作の認証ではありません。

## 検証と保守

`wse.core.runtime_contract`、`wse.core.worker_controller`、`wse.core.license_contract`を使います。
Licenseは共有状態を確定するため、専用Processが必要です。Timer Testは周期呼出、Capture寿命、
自己停止とOwner停止、再起動、例外を検証します。Test TimeoutはHang検出用で、Real-time保証ではありません。

[Design Verification](DesignVerification.md)が根拠を対応付けます。Queue追加、解除の同期規則、
日付規則、Callback Schedulingの変更は観測可能な動作を変えるため、API・設計・Testのレビューが必要です。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
