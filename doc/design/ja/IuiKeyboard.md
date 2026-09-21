# WSE IUI Keyboard設計

> Canonical source: [English IUI Keyboard Design](../en/IuiKeyboard.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象と構造

`wse::iui::Keyboard`は`WSE::Iui`から固定幅のKey状態監視を提供します。Unicode Text入力、IME、
CompositorのEvent routing、Shortcut登録、排他的入力のAPIではありません。公開Headerは
[Keyboard.h](../../../api/iui/device/Keyboard.h)、Adapterは
[Windows](../../../platform/iui/win/device/Keyboard.cpp)と
[Linux](../../../platform/iui/linux/device/Keyboard.cpp)です。共通のWorker停止処理は
[Thread Ownership](ThreadOwnership.md)を参照してください。本書は現行APIと実装上の制約を説明し、
利用者へ内部Worker型やPlatform型の使用を要求しません。

```text
Caller -- calls --> Keyboard owner
                     +-- owns --> state arrays / mutex
                     +-- owns --> atomic access state
                     +-- retains --> shared callable
                     +-- owns --> monitor worker
                                    +-- uses --> private platform adapter
                                                  +-- uses --> Linux descriptors / Win32 polling
worker -- publishes under mutex --> state arrays
worker -- invokes outside mutex --> shared callable
```

監視中のOwnerごとにWorkerは1つです。Snapshot配列、Access状態、Callback登録、Scan完了Counterは
別の状態です。Native資源はAdapterの外へ公開しません。

## 状態と観測: IUI-STATE-01

`Keyboard::accessState()`は次のEnumを返します。Platformごとの遷移の違いに注意します。

狭い画面では表を横Scrollして各列を確認してください。

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| 状態 | Linuxでの意味と次の遷移 | Windowsの動作 |
| --- | --- | --- |
| <span style="white-space:nowrap;">`Starting`</span> | 初回Probe／Captureの結果公開までの初期値。直後のCapture失敗ではDisconnectedにもなる | Constructorで直接Readyへ初期化 |
| <span style="white-space:nowrap;">`Ready`</span> | 1つ以上のSourceを読めた状態。全Sourceの読取失敗でDisconnectedになり得る | 非同期Key Pollingを有効化。物理DeviceやAccess可否のProbeは行わない |
| <span style="white-space:nowrap;">`Unavailable`</span> | 読取可能Sourceがなく権限拒否も未観測。後のScanで復旧可能 | このAdapterでは生成しない |
| <span style="white-space:nowrap;">`PermissionDenied`</span> | 読取可能SourceがなくScanまたはKey CaptureでEACCES／EPERMを観測。後のScanで復旧可能 | このAdapterでは生成しない |
| <span style="white-space:nowrap;">`Disconnected`</span> | 読めていたSourceが全てCapture失敗し、権限拒否は未観測。次の定期Scanで再評価 | このAdapterでは生成しない |

</div></div>

Linuxでは読めるKeyboardが優先され、権限のない候補が混在しても使用可能SourceがあればReadyです。
拒否された候補のCapabilityは確認できない場合があるため、PermissionDeniedはAccess診断であり、
特定の物理Keyboardの存在証明ではありません。HDMI-CEC Sourceは除外します。
WindowsのReadyも物理Keyboardの存在や入力Access成功の証明として扱ってはいけません。
公開`isAvailable()`コメントにもPlatformごとの意味を明記します。Signature、Enum値、Windowsの動作は
変えません。ApplicationはReadyを物理接続確認として扱いません。

`isAvailable()`は自身が読んだAccess状態とReadyを比較します。`isAvailable()`、`accessState()`、
`snapshot()`の個別呼出は、まとめて原子的な観測にはなりません。LinuxはAccess状態を先に公開し、
その後Lockして配列を置換するため、新しい状態と古いSnapshotが一時的に見える場合があります。
非ReadyのScanが公開し終えたSnapshotは全Releaseですが、物理的なReleaseの証拠ではありません。
配列が変わらないAccess状態だけの変化ではKey Callbackを呼びません。

## Snapshot表現とMapping: IUI-DATA-02

| Group | 幅 | 解釈 |
| --- | --- | --- |
| `ascii` | 128（`ASCII_NUM`） | ASCII互換位置。Text Queueではない |
| `function` | 24（`FANCTION_NUM`、綴りを維持） | F1～F24 |
| `arrow` | 4 | 矢印Key |
| `lock` | 3 | Lock表示。瞬間的なKey押下ではない |
| `command` | 9 | 公開Headerで定義するCommand Key位置 |

`snapshot()`は1つのMutex内で5配列全てをCopyし、所有値を返します。Worker Memoryを借用する
配列Viewは返しません。`getASCII()`は有効な最小ASCII添字、なければ0を返し、最後に入力した文字を
返すものではありません。Poll間に押して離したKeyは観測できない場合があります。

WindowsはVirtual-key状態を使用します。Linuxはevdev位置をUS ASCII互換Layoutへ変換し、英字は
Caps Lock XOR Shift、Shift付き数字Keyは記号、Keypad数字はNum Lockに従います。複数Sourceの
読取状態と取得できたLock LEDはOR結合します。Localized Text、Dead Key、Compose、IME、Unicode、
Focus-aware Wayland入力にはWindow Toolkitを使い、この配列からTextを推定しません。

`isReleasedAllKey()`はScan完了Counterの変化を最大100 ms待ち、公開済みASCII／Function／Arrow／
Command配列を確認します。Lock表示は除外します。TimeoutでもSnapshotに基づくBoolを返すため、
Backend健全性確認や物理Release確認にはなりません。Callback内から呼ぶと同じWorkerが待ちの期限まで
停止します。

## Poll順序とBackend資源: IUI-POLL-03

両Adapterとも5 msの割込可能なWorker待機後にPollとCallback処理を行います。これは反復間の待機であり、
実時間のSampling周期ではありません。Linuxの再Scan Countdownは200 Tickで、追加処理がなければ約1秒、
CallbackやOSの遅延があれば延びます。

1. Worker停止を確認しながら待機。Linuxは初回とCountdown終了時にScanします。
2. LinuxはReady時だけ、State Mutex外でゼロ初期化したローカル`KeyboardState`へCaptureします。
   全Source失敗でKey照会にEACCES／EPERMがあればPermissionDenied、それ以外はDisconnectedとして
   次の再Scanを予定します。WindowsはState Mutex内で前回配列を保存し、
   Key GroupをResetしてSamplingします。
3. LinuxはAccess状態を公開後、State Mutex内で前回配列を保存して現在配列を設定します。Windowsは
   Sampling後の配列をローカル現在値へCopyします。両者ともMutex内で登録Callableの共有参照を取得し、WindowsのAccess状態はReadyのままです。
4. Mutex解放後に配列を比較。変更、Callable存在、停止未観測の条件でローカル現在値を渡して呼び出します。
   例外はCatchしてCallback登録をClearします。
5. Mutex内でScan完了Counterを増やし、待機Observerへ通知して反復します。

Callback引数は呼出中だけ有効なConst参照です。保持する場合はAddressではなく値をCopyします。
長いCallbackは次のPoll、再Scan、Scan完了通知を遅らせます。停止は利用者Codeを中断せず、最後の
呼出前停止確認の直後にも競合し得ます。

| Platform | 資源とCapture手順 |
| --- | --- |
| Windows | Win32非同期状態読取。Hook、Injection、Device Handle所有なし |
| Linux | WorkerローカルのRAII Descriptor集合。`/dev/input/event*`を読取専用・NonblockingでOpenし、Capability選別後に`EVIOCGKEY`／`EVIOCGLED`で状態照会 |

LinuxのScanは前のDescriptor集合をCloseして再構築します。候補にはA、Z、Enter、Spaceを含む
Keyboard Capabilityが必要で、HDMI-CEC（`BUS_CEC`）は除外します。Captureはioctlによる現在状態照会であり、
evdev Event Stream Queueの読取ではありません。ioctlのEINTRは再試行してから分類します。それ以外の
Key照会に失敗したDescriptorはClose／除去し、成功分を
OR結合します。LED照会失敗分はLED Bitへ寄与しません。他に読めるSourceがあれば、1つの切断だけでは
Disconnectedになりません。

内部の[Device集合](../../../platform/iui/linux/device/LinuxKeyboardDevices.h)は、Directory streamと候補Descriptorを
取得直後から所有し、Path／Vectorの確保例外から保護します。Identity／Capability照会とDirectory列挙の権限拒否も
診断に含めます。Directoryを開けず権限拒否ならPermissionDenied、それ以外ならUnavailableです。列挙が途中で
失敗しても読取可能候補があればReadyを優先します。確保失敗では途中の集合を解放してUnavailableを返し、Workerは
後で再Scanできます。古い押下状態を公開しません。Closeは1度だけ試行し、EINTRでも再試行しません
（Descriptorが既に閉じている可能性があるため）。ioctlの割込みが続く場合、実時間の上限は保証しません。

実機を使わない`wse.iui.keyboard_devices_contract`は、Directory／Open／Identity／Capability／Key照会の拒否、
Device選別、読取可能／拒否の混在、OR結合、一部／全切断、Ready後の権限拒否、除去／再接続、LED失敗、EINTR再試行、
確保失敗の巻戻し／復旧を検査します。受理・拒否・例外巻戻しの全Descriptorと開いたDirectoryは1度だけCloseされる
必要があります。実装と同じ資源OwnerへFake操作を渡す検証であり、KernelのHotplug時刻、ACL変更、物理Keyの検証ではありません。

AdapterはKernel UAPIとC++ Runtimeを使い、libinput、X11、Wayland、udevには依存しません。rootは
不要ですが、OS Device PolicyによりProcessへ読取権限が与えられている必要があります。WSEはGroup、ACL、
Device所有権、権限を変更せず、`EVIOCGRAB`も使用しません。Descriptorは置換、拒否、切断、または
Workerローカル集合の破棄時にCloseします。

## OwnerとCallback寿命: IUI-LIFE-04

狭い画面では表を横Scrollして各列を確認してください。

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| 操作 | Workerと資源への影響 | SnapshotとCallbackへの影響 |
| --- | --- | --- |
| 生成 | 新Workerを1つ開始 | 配列とPlatformごとの初期Access状態を設定 |
| Copy生成 | 元は継続、先は独立Worker／Backendを開始 | 元のLock内で配列をCopy。同じCallableを共有所有 |
| Copy代入 | 先を停止／Join後、置換Workerを開始 | 元から状態／Callbackを置換。元は継続 |
| Move生成 | 元を停止／Join後、先で新Workerを開始 | Callback所有をMoveし状態を転記。Native Worker／Descriptorは移送しない |
| Move代入 | 両Workerを停止／Join後、先を開始 | 先の状態／Callbackを置換。元は監視しない |
| 破棄 | 停止要求、待機解除、Join、Backend資源解放 | Worker完了後に登録と状態を解放 |

</div></div>

LinuxのCopy／Move先はAccess状態をStartingから再開し、Windowsは元のAccess値を保持します。
これはAccess状態とCopy配列の原子的な同時観測を意味しません。自己代入は何もしません。
Move元は破棄または再代入できます。残ったSnapshot／Access値を継続監視の証拠にはできません。
公開Stop／Startはなく、Owner寿命と代入で監視を制御します。破棄は実行中Callbackの完了を待つため、
任意の利用者Codeに対する有界停止時間は保証できません。

`setCallback()`は`shared_ptr<const KeyCallback>`でCallableを所有し、空Callableなら登録を解除します。
KeyboardをCopyするとCapture状態を複製せず同じCallableを共有します。独立Workerから同じCapture状態へ
同時呼出が起き得るため、可変Captureの同期はApplicationが行います。

`clearCallback()`はMutex内でそのOwnerの登録を除去します。以降の取得は止めますが、Workerが既に取得した
Callableや別のKeyboard Copyが保持するCallableは取り消さず、完了も待ちません。Callback完了のBarrierではなく、
全共有参照がなくなった時点でCapture対象を解放します。

CallbackはMonitor Worker上のState Mutex外で動作し、Snapshot／値Observerと`clearCallback()`を呼べます。
同じOwnerを破棄、Move代入、Copy代入してはいけません。例外はWorker境界でCatchし登録を解除しますが、
監視は継続します。この解除は競合して登録された置換Callableも消し得るため、世代を区別した例外復旧は
保証しません。通常のOwner寿命操作は外部で同期し、Callbackから安全なObserverの存在を並行破棄の許可と
解釈しません。

## 検証と残る制約

狭い画面では表を横Scrollして各列を確認してください。

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| 契約 | 根拠 | 限界 |
| --- | --- | --- |
| IUI-STATE-01 | Lifecycle起動と`wse.iui.keyboard_devices_contract`の権限／Hotplug注入Matrix | Kernel／物理遷移の注入ではない。個別Observerは一括で原子的ではない |
| IUI-DATA-02 | LifecycleのCompile時幅とSnapshot形状 | Key MappingはOpt-in物理Gateが必要 |
| IUI-POLL-03 | Device集合の故障Matrix、確保失敗Sweep、Adapter／Worker確認 | 実時間遅延やWorker／Callback全競合の保証なし |
| IUI-LIFE-04 | [Lifecycle Test](../../../test/characterization/iui_keyboard_lifecycle.cpp): Copyを通したCapture保持、解除後の最終解放、Copy／Move／代入／破棄 | 物理Key押下や実行中Callbackの強制なし。一般的なLeak証明ではない |

</div></div>

Shared／StaticとInstall済みIUI-only Consumerは対応Build／Target境界を確認し、Linux Cross Buildは
実行時入力を証明しません。物理Keyboardは[Hardware Validation](HardwareValidation.md)の別Opt-in Gateです。
Sourceが使えない状態やLifecycle Test成功を実機認証へ昇格しません。[Design Verification](DesignVerification.md)も参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
