# WSE XPT Transport設計

> Canonical source: [English XPT Transport Design](../en/XptTransport.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書は`WSE::Xpt`が所有するPortable通信境界の正式設計書です。Windows／Linux共通のEndpoint、Timeout、Cancellation、構造化Error、同期TCP／UDP／Serial Clientおよび
明示Retry Policy、同期HTTP Client、固定libcurl依存、ARM64 PackageおよびOffline Kit境界を定義します。
機種固有Protocol、Command、認証情報およびURIは本Moduleへ入れてはいけません。

## Support matrix

| Target | TCP | UDP | HTTP | `SerialPort` | Library形式 |
| --- | --- | --- | --- | --- | --- |
| Windows x86-64／MSVC | 対応 | 対応 | 対応 | 対応 | Shared、Static |
| Linux x86-64／GCC・Clang | 対応 | 対応 | 対応 | 対応 | Shared、Static |
| Linux ARM64／GCC Cross | Cross build済み | Cross build済み | Cross build済み | Cross build済み | Shared、Static |


Linux ARM64はShared／Static Cross buildとPackageに対応します。Pi 4のCore／XPT Runtime smokeは
[Hardware Validation](HardwareValidation.md)が記録する限定範囲で受理済みです。Cross compileもこのSmokeも、
すべての実機Network／Serial deviceを認証するものではありません。全対応TargetへXPTの全公開Headerを含めます。

## Public contract

| 型 | 責務 |
| --- | --- |
| `Endpoint` | Host名または数値AddressとPortを保持する。Remoteでは非Zero、UDP BindではZeroを自動割当として扱う |
| `Timeout` | `steady_clock`で測る有限Durationを表す。負値は不正 |
| `CancellationSource` | Thread-safeにCancellationを要求する |
| `CancellationToken` | Copy間でCancellation状態を共有し、Operation側で観測する |
| `OperationContext` | 各Blocking Operationへ明示するTimeoutとCancellationを束ねる |
| `TransportError` | 安定したCategory／Code、Message、診断用Native codeを保持する |
| `TransportResult<T>` | ResultContractに従い、成功値または失敗Errorの一方を返す |
| `TransferResult<T>` | Errorと共存して意味を持つ転送進捗・受信Prefixを返す |
| `HttpResult` | 4xx／5xxを含む受信ResponseとHTTPの成否を返す |
| `TcpClient` | Move-onlyの接続所有者としてConnect、Send、Receive、Disconnectを提供する |
| `UdpDatagram` | 受信した1 Datagramの送信元EndpointとPayloadを保持する |
| `UdpClient` | Move-onlyのSocket所有者としてBind、SendTo、ReceiveFrom、Closeを提供する |
| `SerialPort` | Move-onlyのPort所有者として8N1のOpen、Send、Receive、Closeを提供する |
| `RetryPolicy` | 初回を含む最大試行回数と固定／指数Backoffを保持し、安全側のRetry Decisionを返す |
| `RetryDecision` | Retry可否、次の試行前の待機時間またはStop理由を保持する |
| `HttpAuthentication` | Move-onlyのRuntime Username／Secretと明示Server negotiated認証Policyを保持し、破棄時に保持Textを消去する |
| `HttpRequest` | Method、URL、Header、Bodyを保持する |
| `HttpExecutionOptions` | Response Body上限、Retry Policy、操作の安全性、一時的失敗RetryのOpt-inを保持。必須OperationContextとは別の値 |
| `HttpResponse` | HTTP Status、Header、Body、試行回数を保持する |
| `HttpClient` | libcurl型を露出しない同期HTTP実行と明示Retry lifecycleを提供する |

公開HeaderはWinSock `SOCKET`、POSIX file descriptor、`sockaddr`またはPlatform Error値を主Error codeとして
露出しません。`nativeCode()`は診断用であり、分岐には`category()`と`code()`を使用します。

## 構造とOperation Context: XPT-OP-01

| Owner／値 | 実装と寿命 |
| --- | --- |
| TCP | [TcpClient.cpp](../../../platform/xpt/network/TcpClient.cpp): 一意PIMPLがSocket、Remote Endpoint、有効Local Endpointを所有 |
| UDP | [UdpClient.cpp](../../../platform/xpt/network/UdpClient.cpp): 一意PIMPLがSocket、Family、Local Endpointを所有 |
| Serial | [SerialPort.cpp](../../../platform/xpt/serial/SerialPort.cpp): 一意PIMPLがNative Portを所有。WindowsのOverlapped状態は操作ローカル |
| HTTP | [HttpClient.cpp](../../../platform/xpt/http/HttpClient.cpp): Process単位curl Runtimeと、試行単位Handle／Header List／Response蓄積領域。公開Native Handleなし |
| Cancellation | [Cancellation.cpp](../../../core/xpt/operation/Cancellation.cpp): SourceとTokenが1つのAtomic Boolを共有 |
| Retry判定 | [RetryPolicy.cpp](../../../core/xpt/retry/RetryPolicy.cpp): 純粋なPolicy計算。Workerや資源所有なし |

公開宣言の入口は[api/xpt/stew.h](../../../api/xpt/stew.h)です。Native OwnerはMove-onlyです。
Move代入は先の旧資源を解放してPIMPLを移します。Move元はNative資源を持たず、破棄または置換値の代入が
可能ですが、接続を自動再初期化する状態ではありません。同一Ownerの呼出と寿命操作はConsumerが直列化します。
背景Transport Worker、公開完了Callback、非同期Closeはありません。

呼出側は各操作へ`OperationContext(timeout, token)`を別途渡し、HTTPには`HttpExecutionOptions`も渡します。
暗黙Timeoutはありません。負の期間はValidation失敗、0は有効ですが通常I/Oの予算が残りません。
Validationや無操作経路はCancellation／Deadline確認より先に戻る場合があり、0なら全操作がTimedOutを
返すという保証ではありません。

初期Validation後に1つのSteady-clock Deadlineを定め、巨大な加算はClock最大値へ飽和させます。
Address候補、Interrupted／Would-block待機、HTTP Backoffは残予算を使い、期間を再開始しません。
Socket待機、HTTP Polling、Serial待機のSliceは最大20 msです。これは待機Sliceの上限であり、
Cancellation完了までの上限ではありません。OS Scheduling、同期名前解決、Native後処理はさらに長くなり得ます。

既定TokenはCancellationを要求しません。`CancellationSource::cancel()`は共有Atomic状態へtrueを保存し、
TokenはAcquire／Release順序で観測します。要求は冪等でReset不可です。Source破棄後もTokenは状態を保持し、
破棄だけではCancelしません。Cancellationは協調的であり、送信済みByteや相手側の動作は取り消しません。
Caller-confinedなOwnerの操作と並行使用するのはCancellation Sourceだけです。

## TCP operation semantics

- `connect`、`send`および`receive`は同期Operationで、`OperationContext`の指定が必須です。
- TimeoutはOperation開始時に`std::chrono::steady_clock`から一度だけ計算し、Retry可能なOS待機でも延長しません。
- Cancellationは協調的です。Socket待機は最大20 msのSliceを使い、要求を観測したら
  `eTransportErrorCode::Cancelled`を返します。遅延の限界はXPT-OP-01を参照してください。
- `send`は指定Buffer全体を送信するまで継続し、失敗時の`value()`は送信済みByte数です。
- `receive`は最大Sizeまでの一回分を返します。相手の正常Closeは空成功ではなく`RemoteClosed`です。
- Send／ReceiveのTimeoutとCancellationは確立済み接続を保持します。Native I/OのResetと相手Closeは
  Local状態をCloseします。再接続には下記の置換規則を適用します。
- `disconnect`とDestructorはSocketをShutdown／Closeします。`disconnect`は冪等です。
- TCP接続成功時にOSが選択した数値Local endpointを記録します。`getLocalEndpoint()`は切断までその値を返し、
  上位Protocol AdapterがNative socketを露出せずに自身のAddress fieldを構築できます。
- `checkPeerConnection()`は即時かつ非破壊の`MSG_PEEK`を実行し、Pending payloadを消費しません。OSが観測できる
  正常Closeは`RemoteClosed`、Reset／Receive失敗は構造化Connection／InputOutput Errorを返してLocal状態を閉じます。
  Would-blockは現時点でCloseを観測していないことだけを意味し、End-to-end生存証明ではありません。

`TcpClient`はCaller-confinedです。同一InstanceのMethodを複数Threadから同時実行してはいけません。
`CancellationSource::cancel()`だけはOperation実行Threadとは別のThreadから呼べます。公開Callbackはなく、
非同期Callback寿命は本契約の対象外です。

### TCPの状態と処理: XPT-TCP-02

| 操作／経路 | 前状態 | 後状態と保持結果 |
| --- | --- | --- |
| 引数不正またはCancel済みContextでConnect拒否 | ClosedまたはConnected | 元の状態を保持。置換を開始しない |
| Validation済みConnect開始 | ClosedまたはConnected | 名前解決前に旧SocketをClose。1つのDeadlineでNonblocking候補を試行 |
| 候補成功 | 内部的にConnecting | Socket／Endpointを保存してConnected |
| 置換開始後の解決失敗、Timeout、Cancellation、全候補失敗 | 内部的にConnecting | 候補をCloseしてClosed。旧SocketへRollbackしない |
| Send／ReceiveのTimeout／Cancellation | Connected | Socketを保持し、完了進捗またはErrorを返す |
| Native Receiveが0を返す | Connected | CloseしてRemoteClosed |
| Disconnect／Owner破棄 | 全状態 | 所有SocketをShutdown／CloseしEndpointをClear |

ConnectはValidation、旧SocketのClose、Host解決、Address候補の反復の順です。各候補をNonblockingで
接続し、Pendingなら書込可能になるまで待って`SO_ERROR`を確認します。失敗候補は次候補の前にCloseします。
したがって名前解決に成功する0予算の再接続は、新しい接続を確立できなくても旧接続を失います。

Sendは最大`INT_MAX`のChunkごとにNative書込の完了Offsetを進めます。Would-block／Interruptedでも
同じDeadlineを使います。失敗した`TransferResult<size_t>`は既知の完了Byte数を保持しますが、Applicationの
受領確認やBuffer再送の許可ではありません。有効な接続／Contextへの空SendはCancellation確認前に0で成功します。
Receiveの上限は正かつ`INT_MAX`以下で、Buffer確保後の最初の非空Readを返します。要求Sizeを満たす処理や
Application Messageの区切りは行いません。Native I/Oの接続ErrorはOwnerをCloseしますが、待機Helper失敗は
別経路で返すため、全Error CategoryからCloseを推測しません。復旧選択前にOwner状態を確認します。

## UDP operation semantics

- `bind`、`sendTo`および`receiveFrom`は同期Operationで、TCPと同じ`OperationContext`を使用します。
- `bind`はHostを必須とし、Port 0をOSによるEphemeral port自動割当として受理します。成功後の実Portは
  `getLocalEndpoint()`で取得します。再Bindは成功時だけ既存Socketを置換し、失敗時は元のSocketを保持します。
- 未Bindの`sendTo`はAddress familyに合うSocketを遅延生成し、OS割当のLocal Endpointを保持します。
- 1回の`sendTo`は1 Datagramです。部分送信を成功として扱わず、0 byte Datagramも実際に送信します。
- Portable Payload上限は65,507 byteです。上限超過はSocketへ到達する前に`MessageTooLarge`を返します。
- `receiveFrom`は1 Datagramと送信元を返します。指定Bufferより大きいDatagramは破棄せず、保存できた
  Prefixと送信元を`value()`へ保持して`DatagramTruncated`を返します。
- TimeoutまたはCancellation後もSocketとLocal Endpointを保持します。`close`は冪等です。

`UdpClient`もCaller-confinedです。同一Instanceを複数Threadから同時操作してはいけません。

### UDPの状態と処理: XPT-UDP-03

| 操作／経路 | 資源への影響 |
| --- | --- |
| Bind／再BindのValidation、解決、候補Bind、Endpoint照会失敗 | 候補だけCloseし、既存Socket／Endpointは保持 |
| Bind／再Bind成功 | 候補を確定し旧SocketをClose、新しい有効Endpointを公開 |
| 未Bind OwnerのSend | 解決して候補を生成し、Send成功後だけ保持 |
| Bind済みOwnerのSend | 既存SocketのAddress Familyを使用。暗黙Family置換なし |
| ReceiveのTimeout／Cancellation | SocketとEndpointを保持 |
| Receive切詰め | Datagram全体を消費し、保存Prefix／送信元とDatagramTruncatedを返す |
| Close／破棄 | SocketとEndpointを解放。Close反復は無害 |

切り捨てた末尾を後のReadで取り出すことはできません。例えば5 ByteのDatagramを容量3で受信すると、
3 ByteとErrorを返し、次のReceiveは新しいDatagramを待ちます。0 Byte Datagramも実際のMessageであり、
相手Closeではありません。UDPに接続Handshake、配送確認、自動再送はありません。

## Serial operation semantics

- `SerialPort::open`、`send`および`receive`は同期Operationで、TCP／UDPと同じ`OperationContext`を使用します。
- 初期Portable framingは8 data bits、no parity、1 stop bit、no flow controlに固定します。対応Baud rateは
  1200、2400、4800、9600、19200、38400、57600および115200です。
- WindowsのDevice名は`COM3`等、Linuxは`/dev/ttyUSB0`、`/dev/ttyACM0`等のPathを指定します。
- `open`は新DeviceのOpenと設定が成功した場合だけ既存Portを置換します。失敗時は既存Portを保持します。
- Windowsの`open`はNative受信Timeoutを初期化し、Driver既定値や前利用者の設定に依存せず、
  要求Bufferを満たさない短い応答も返します。無受信時はOperation Deadlineで戻り、Native Timeout設定失敗はOpen失敗にします。
- `send`は指定Buffer全体をOSへ渡すまで継続し、失敗時の`value()`は送信済みByte数です。
- `receive`は最大Sizeまでの一回分を返します。Timeout／Cancellation後もPortを保持します。
- 待機Sliceは最大20 msですが、下記のNative完了後処理が必要です。`close`は冪等です。

`SerialPort`もCaller-confinedです。`CancellationSource::cancel()`だけは別Threadから呼べます。Linuxの
PTY TestはBackend契約の自動検証であり、USB-UART、GPIO UARTまたは対象機器の実機認証ではありません。

XPTは明示Device pathを受け取り、Device discoveryを所有しません。Coreの
`pickupDeviceInfo::Serials()`は、存在する場合にWindows Device instance identityまたはLinux
`/dev/serial/by-id` identityをOpaqueな`stable_id`として返します。ConsumerはLogical session失敗後の
再Open前にその値から現在Pathを解決できます。Stable IDはXPT endpointではなく、OS間でPortableでもなく、別の列挙Deviceへ
Fallbackする理由にしてはいけません。

`SerialPort`は公開Serial Transport APIです。

`TcpClient`、`UdpClient`および`SerialPort`のPIMPLはRAII ownerが保持します。Move代入または
破棄時のSocket／Port解放は`Impl` destructorへ集約されます。

### Serialの状態と処理: XPT-SERIAL-04

OpenはPath／Baud／Contextを検査し、候補を開いてRaw 8N1・Flow Controlなしへ設定し、
Cancellation／Deadlineを再確認してから確定します。確定前は既存Handleを保持し、失敗後も使用できます。
成功時は旧HandleをCloseして候補を取り込みます。名前解決前に旧Socketを閉じるTCP再接続とは異なります。

LinuxはNonblocking DescriptorとOperation Deadlineに対するPollを使います。WindowsはPending転送ごとに
EventとOVERLAPPEDを所有します。Timeout／Cancellation時は`CancelIoEx`後、Native完了まで待ってから
Event、OVERLAPPED、Bufferを解放します。この最後の待機は`INFINITE`のため、取消を遅らせるDriverでは
名目Deadlineを超え得ます。使用中Memoryを残して戻ることより、Native I/OのMemory寿命を優先します。

Sendは完了Chunkを報告しますが、取消対象のPending書込も既にWireへ影響している可能性があるため、
返すByte数はRollbackや相手側受理の証明ではありません。Receiveは短い応答も含め上限までの取得Prefixを返します。
Native Read／Writeの失敗経路ではPortをCloseする場合があり、Timeout／Cancellationでは保持します。
復旧前にResultと`isOpen()`を確認します。生Transport操作はDevice再OpenやCommand再送を自動実行しません。

## Retry policy semantics

- 既定`RetryPolicy`は有効ですが最大試行回数1であり、Retryを行いません。最大試行回数は最初の試行を含みます。
- `RetryPolicy::evaluate()`は判定だけを行い、待機、Transport呼出し、再接続またはOperation再実行を行いません。
- 呼出側は試行ごとに`Idempotent`／`NonIdempotent`と`Retryable`／`DoNotRetry`を明示します。
- `Idempotent`かつ`Retryable`の明示が揃った場合だけRetry候補になります。非冪等Operationは常に停止します。
- Errorなし、Cancellation、Validation、Protocol、Securityおよび`Unsupported`は、呼出側が`Retryable`を
  指定しても停止します。Resolution、Connection、InputOutput、TimeoutおよびHTTP Categoryは、呼出側が
  一時的失敗と明示した場合だけRetry候補になります。
- Fixed Backoffは設定間隔を維持します。Exponential Backoffは最初のRetryで初期間隔を使い、以後2倍して
  最大間隔で飽和します。現在Jitterは加えません。
- 最大試行回数0、負の間隔、初期間隔未満の最大間隔および未定義Strategyは不正Policyです。不正Policyと
  完了済み試行数0はRetryせず、安全側のStop理由を返します。

TCP／UDP／Serial Operationは`RetryPolicy`を暗黙利用しません。特に部分送信後のBuffer再送、UDP Datagram再送、
Serial Command再送またはDevice Open再試行をTransport層が自動実行してはいけません。Retry lifecycle全体の
Deadline、待機中Cancellationおよび再接続Sequenceは、Policyを利用する上位Operationが明示的に管理します。

### Retry計算: XPT-RETRY-05

完了した失敗試行数`n >= 1`に対する指数Delayは
`min(maximum_delay, initial_delay * 2^(n-1))`で、飽和演算で求めます。最大4試行、初期10 ms、上限25 msなら、
失敗1／2／3回後のDelayは10／20／25 ms、4回後は停止します。固定Delayは常に初期値です。
evaluate内にJitterや隠れたSleepはありません。

判定優先順はPolicy不正、試行数0、失敗なし、Cancellation、非冪等操作、失敗分類／Category拒否、試行上限、
Retryです。複数の停止条件が重なる場合、この順序で`stop_reason()`が決まります。

## HTTP operation and dependency semantics

- `HttpClient::execute`／`executeAuthenticated`は同期Operationで、別引数の`OperationContext`を全体Deadlineとして使用します。
- 認証付き実行はCredentialを`HttpRequest` Headerへ入れず、Serverが提示した認証ChallengeだけにBackendが応答します。
  Redirectは無効のままで、Runtime Credentialを構成済みRequest originへ限定します。Move-only認証値はMove代入／破棄時に
  保持しているUsername／Secretを消去します。
- Server negotiationではServerが提示した場合にBasic認証が選ばれることがあります。Basicは平文HTTP上でCredentialを
  保護しないため、DeploymentはHTTPSまたは承認済みの隔離Device networkを使用します。WSEはAuthorization Headerや
  Credential materialをLogへ出しません。
- Response body上限を超えた場合は部分成功にせず、Structured Errorを返します。
- HTTP 4xx／5xxは受信Responseと`HttpStatusError`を返します。StatusだけでRetryは許可されません。
- Retryは`RetryPolicy`、Request冪等性および失敗分類がすべて許可した場合だけ実行します。待機中もCancellationと
  全体Deadlineを観測します。
- 公開Header、CMake targetおよびConsumerへcurl型／Headerを露出しません。

### HTTP試行の順序: XPT-HTTP-06

1. Request／Options／Contextと、指定時の認証値を検査します。GET／HEADにBodyは指定できません。
   非対応Method、壊れたHeader／URL、不正Optionsは試行前に失敗します。
2. Cancellation確認、Process単位curl Runtime取得、全体Deadline設定を行います。
3. 試行数を増やし、新しいEasy／Multi Handle、Request Header List、Response蓄積領域を作ります。
   残Timeout、Redirect無効、内部Body／Header Callbackを設定します。利用者Callbackではありません。
4. 完了、Cancellation、DeadlineまでPerform／Pollします。Body蓄積ではByte上限とAllocation失敗を確認し、
   Headerは別に蓄積します。Body上限はHeader／Memory総量の上限ではありません。蓄積時の`std::bad_alloc`をCallback内で捕捉して失敗を記録します。
5. 試行資源を解放し、Status、Header、受理したBody、試行数からHttpResponseを作ります。成否判定の優先順は
   Cancellation、Timeout、Body上限、Allocation失敗、その他curl Error、HTTP Status >= 400、成功です。
6. 失敗時は分類してRetryPolicyへ照会します。許可時は同じ全体Deadline／Tokenで待ち、新しい試行を開始します。
   複数試行のResponseは連結しません。

既定は最大1試行、NonIdempotent、一時的失敗Retry無効です。明示Opt-in、Idempotent、許可するPolicyが
全て必要です。HTTP分類器は408、425、429、500、502、503、504と、対象の解決／接続／I/O／Timeout失敗を
一時的失敗候補とし、ResponseTooLargeとResourceExhaustedを除外します。分類だけで再実行は許可しません。

失敗HttpResultも受信Responseを保持でき、上限／転送失敗時には受理済みBody Prefixが残る場合があります。
完全な成功としてParseしてはいけません。Backoff中のCancel／Timeoutでは前試行のResponseと新しい待機Errorを
返します。試行数はWSEの試行数であり、Server negotiated認証内の各通信Exchange数ではありません。

### 依存とPackage境界

- libcurl 8.21.0のSource、SHA-256、公式署名Sidecar、署名者Fingerprintおよびcurl Licenseを
  Standalone Bootstrap Manifestへ固定します。
- BootstrapのArchive処理はPython標準Libraryだけを使い、Path traversal、Symbolic／Hard link、Device entry、
  不完全TargetおよびPlatform／Architecture metadata不一致を拒否します。
- 原本Source Archiveは搬送時に共用できますが、展開TreeとDebug／Release Static Libraryは対象OS別に
  分離します。WindowsはSchannel、LinuxはOpenSSLを使用します。
- `wse-dependency.json`へSource identity、対象OS／Architecture、Linkage、License、System dependencyおよび
  Bootstrap recipe hashを記録します。
- WindowsはSchannel、Linux x86-64／ARM64はOpenSSLを使用します。ARM64 Install packageは固定OpenSSLを
  Package内部へ同梱します。

## Error contract

CategoryはValidation、Resolution、Connection、InputOutput、Timeout、Cancellation、Protocol、Security、
HTTPを区別します。TCP／UDP／Serial実装は少なくともInvalidArgument、HostNotFound、AddressUnavailable、
ConnectionRefused、ConnectionReset、NetworkUnreachable、NotConnected、RemoteClosed、TimedOut、Cancelled、
BindFailed、SendFailed、ReceiveFailed、MessageTooLarge、DatagramTruncated、ResourceExhaustedおよびUnknownを
返せます。SerialのOpen／設定失敗はOpenFailed／ConfigurationFailedです。Transport固有Codeを別Transportで
無理に返してはいけません。

OSのError番号は`nativeCode()`へ保存しますが、WindowsとLinuxで同じ値になる保証はありません。Messageは
診断用であり、機械判定や翻訳Keyとして使用してはいけません。

## Current limitations

- `getaddrinfo`によるSystem name resolutionは現在同期呼出しであり、その呼出し中だけはCancellationで
  即時中断できません。経過時間は同じDeadlineへ算入します。解決成功後は候補LoopでTimeout／Cancellationを
  確認しますが、解決失敗は直接HostNotFoundを返す場合があります。全Allocation、解決、後処理の所要時間を
  Timeoutが制限するわけではありません。
- XPTはSerial列挙とHot-plug通知を所有しません。CoreはSnapshot列挙を公開し、Consumerが観測済み
  Session失敗後に自前の上限付き再列挙を行います。Event駆動Hot-plugは未実装です。
- Custom serial framing／flow controlは未実装です。
- Transportへの暗黙Retryは実装していません。特に非冪等Operationへ自動Retryを適用してはいけません。
- TCP／UDP／HTTP LoopbackとLinux PTY Serialは契約検証済みですが、Physical LAN、Raspberry Piおよび機器接続の
  認証ではありません。

## 検証契約

| 契約 | 根拠 | 残る限界 |
| --- | --- | --- |
| XPT-OP-01 | TCP／UDP／Serial／HTTPのDeadlineとCancellation Test | Scheduling／Resolver／Driverの取消時間上限ではない |
| XPT-TCP-02 | [TCP Loopback](../../../test/characterization/xpt_tcp_loopback.cpp): 不正／Cancel済み再接続は保持、0予算の置換はClose、相手Close／Peek | 実Networkの全障害Matrixではない |
| XPT-UDP-03 | [UDP Loopback](../../../test/characterization/xpt_udp_loopback.cpp): 再Bind、Payload上限、切詰め、0 Datagram | 配送／信頼性保証なし |
| XPT-SERIAL-04 | [Serial契約](../../../test/characterization/xpt_serial_port_contract.cpp): 入力ErrorとLinux PTY再Open／I/O | Windows実DriverのPending取消はPTYで検証しない |
| XPT-RETRY-05 | [Retry契約](../../../test/characterization/xpt_retry_policy_contract.cpp): 判定優先順、試行数、飽和 | 実操作の冪等性は呼出側が判断 |
| XPT-HTTP-06 | [HTTP契約](../../../test/characterization/xpt_http_contract.cpp): Local Server、応答上限、Cancel、Retry、認証 | 一般Internet／TLS／機器認証ではない |

Windows MSVCとLinux GCC／ClangのShared／StaticでTCP／UDP LoopbackとPortable Serial契約Testを実行します。UDPでは通常、
0 byte、65,507 byte、切詰め、Timeoutおよび別ThreadからのCancellationを確認します。Install treeだけを参照する
Consumerは`WSE::Core`と`WSE::Xpt`へLinkし、Source treeまたはPlatform headerへ依存してはいけません。
Linux SerialはPTYで双方向送受信、Timeout、CancellationおよびTransactional reopenを検証します。Windowsでは
実機不要の入力／Error契約を検証します。
Retry契約Testは、既定無効、試行上限、固定／指数Backoff、飽和計算、非冪等、明示的Failure分類および
Cancellation／Validation／Protocol／Securityの安全側停止を全Platform／Library形式で確認します。
HTTP契約TestはLocal fake serverでMethod、Header、Body、status、Response上限、Timeout、Cancellation、
Retry lifecycle、Challenge駆動認証および不完全な認証値の拒否を確認します。ARM64はTest executableの
Cross compileまでをGateとし、実行はPi 4で補完します。

Python／JavaScript／Java／C#はRaw Transport失敗時のSend進捗とUDP切詰めPrefix／送信元を保持します。
例外形、所有、互換性、検証範囲は[Language Bindings](LanguageBindings.md)のBIND-TRANSFER-05を参照します。
失敗SendをBindingが自動再送することはありません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
