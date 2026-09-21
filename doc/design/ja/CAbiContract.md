# WSE 平坦C ABI設計

> Canonical source: [English Flat C ABI Design](../en/CAbiContract.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 範囲と構造: CABI-OWNER-01

Installする[集約Header](../../../api/wse/capi/stew.h)はC# P/Invokeが使うC互換境界です。
公開Core／Component facadeへ委譲し、Platform backendを公開しません。共有Shimは
`WSE_BUILD_DOTNET_BINDING`で選択し、CoreがSTATICでも共有Libraryです。HeaderのInstallはShimのBuildを
意味しません。[Build Packaging](BuildPackaging.md)を参照してください。

| 層 | Source | 責務 |
| --- | --- | --- |
| 公開表現 | [wse_capi.h](../../../api/wse/capi/wse_capi.h)、[Core関数](../../../api/wse/capi/wse_capi_core.h) | 呼出規約、Status、不透明型、版、Buffer、取消API |
| 共通Bridge | [capi_internal.h](../../../lang/cs/native/capi_internal.h)、[capi_internal.cpp](../../../lang/cs/native/capi_internal.cpp) | Holder実体、例外Guard、Thread-local診断、文字列Copy |
| Core操作 | [wse_capi_core.cpp](../../../lang/cs/native/wse_capi_core.cpp) | Runtime／FrameBuffer／Cancellation所有 |
| Component | [XPT](../../../lang/cs/native/wse_capi_xpt.cpp)、[IUI](../../../lang/cs/native/wse_capi_iui.cpp)、[Tmr](../../../lang/cs/native/wse_capi_tmr.cpp)、[OUI](../../../lang/cs/native/wse_capi_oui.cpp) | 型変換と委譲。選択したComponentとともにCompile |
| Managed Bridge | [NativeMethods.cs](../../../lang/cs/Wse/NativeMethods.cs)、[FrameBuffer.cs](../../../lang/cs/Wse/FrameBuffer.cs) | Sequential layout、P/Invoke、Error保存、SafeHandle所有 |

```text
C / P/Invoke caller -- owns --> opaque C holder -- owns --> public C++ value/owner
                            destroy exactly once
C# object -- owns --> SafeHandle -- releases --> opaque C holder
input pointer -- borrowed for call --> copy into native-owned storage
output buffer -- owned by caller <-- copy from native-owned storage
```

Opaque handleは未完成StructへのPointerであり、検査済み整数Resource IDではありません。対応APIから返った
生存中Handleだけが有効です。`destroy(NULL)`は無害ですが、同じ非NULL addressの二重破棄、解放済み／別用途Pointerの
使用は不正です。破棄後は呼出元変数をNULLにします。WrapperのDisposeの冪等性はSafeHandleによるもので、
Nativeの二重破棄検出ではありません。Native利用側はOwner操作と破棄を直列化します。
ManagedのOpaque入力はSafeHandleとしてP/Invokeへ渡し、Native呼出しが戻るまでOwnerを保持します。
省略可能なCancellation pointerとXPT context内のPointerは、同期操作の間
[NativeHandleLease](../../../lang/cs/Wse/NativeHandleLease.cs)のDangerousAddRef／Releaseで保持します。
Disposeは自身の参照を解放し、最後の呼出し／LeaseがNativeを破棄します。操作の取消や全外部呼出元の終了待機は行いません。
競合した呼出しは完了するかObjectDisposedExceptionになり、二回取得の途中でDisposeされれば二回目で失敗し得ます。
この寿命保護は可変操作を直列化しません。呼出元Threadへの制約とCamera Start／Stop／Close・Owner破棄の直列化を維持します。

## 表現と版: CABI-LAYOUT-02

Windowsは`__cdecl`とC linkage、LinuxはNative C呼出規約です。`wse_capi_bool`はint32で、0がfalse、非0がtrueです。
`size_t`はPointer幅でuint32固定ではなく、`wse_capi_user_data`はintptr_tです。1 Byte Booleanや固定幅Sizeとして
Marshalしてはいけません。

| Status field | C型 | ABI SnapshotのOffset／Size |
| --- | --- | --- |
| category | int32_t | 0／4 Byte |
| code | int32_t | 4／4 Byte |
| native_code | int64_t | 8／8 Byte |
| Status全体 | struct | 16 Byte |

以下は64-bit Windows／Linux ABIの全公開値Structです。型名には`wse_capi_`を前置します。
Field順序、個別Offset、Field sizeは[CapiLayout.def](../../../test/support/CapiLayout.def)に固定し、
CとC++のCompilerが独立に照合します。Managed testはCの実測値を`Marshal.SizeOf`、`Marshal.OffsetOf`、
Inline array長、Nested structのAlignmentと比較します。Inventory gateは公開Struct／Fieldの追加や脱落を検出します。

<div class="wse-cabi-layout" style="max-width:100%; overflow-x:auto;"><div style="min-width:34rem;">

| 値Struct | Size（Byte） | Alignment（Byte） |
| --- | --- | --- |
| <span style="white-space:nowrap;">`status`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`components`</span> | 24 | 4 |
| <span style="white-space:nowrap;">`keyboard_state`</span> | 672 | 4 |
| <span style="white-space:nowrap;">`projection_vertex`</span> | 16 | 4 |
| <span style="white-space:nowrap;">`renderer_color`</span> | 16 | 4 |
| <span style="white-space:nowrap;">`edge_blend`</span> | 20 | 4 |
| <span style="white-space:nowrap;">`projection_layer`</span> | 104 | 8 |
| <span style="white-space:nowrap;">`projection_request`</span> | 72 | 8 |
| <span style="white-space:nowrap;">`projection_frame`</span> | 272 | 8 |
| <span style="white-space:nowrap;">`operation_context`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`camera_format`</span> | 20 | 4 |
| <span style="white-space:nowrap;">`camera_stream_configuration`</span> | 28 | 4 |
| <span style="white-space:nowrap;">`camera_control_value`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`camera_control_capability`</span> | 72 | 8 |
| <span style="white-space:nowrap;">`camera_usb_identity`</span> | 12 | 4 |
| <span style="white-space:nowrap;">`camera_frame_description`</span> | 24 | 8 |

</div></div>

全16 Struct・87 Fieldで、Core-only buildでもOptional componentの配置を含みます。
数値Snapshotは64-bit対象であり、32-bitやARM64での実行証跡ではありません。
`enum wse_capi_keyboard_access_state`のTagと値は維持し、Typedef名には
`wse_capi_keyboard_access_state_value`を使います。Cの規則に従い、関数`wse_capi_keyboard_access_state`と
別名にします。[移行説明](../../ja/DeprecationMigration.md#c-abi-keyboard-type-ja)も参照してください。

Category 0～11はNone、InvalidArgument、NotFound、InvalidState、InputOutput、Timeout、Cancellation、Protocol、
Security、Unsupported、ResourceExhausted、Internalです。CodeはComponent／操作ごとに解釈し、Categoryと操作文脈を
合わせて比較します。これらはProcess内のStructで、Packed形式やEndian非依存File／Network形式ではありません。

`WSE_CAPI_ABI_VERSION == 1`とBinding ABI 1は独立した契約です。利用前に`wse_capi_abi_version()`を照会し、
CompileしたHeaderと不一致なら拒否します。ManagedのNativeAbiVersionは照会を公開するだけで、Layoutの自動交渉では
ありません。互換関数の追加では版を変えず、Layout／Signature／意味の変更には[Version gate](VersionCompatibility.md)を適用します。

## Status・診断・出力: CABI-ERROR-03

失敗し得る操作はStatusを値で返し、Category Noneが成功です。Destroy、ABI版、診断Pointer、一部の純粋な値Helperは
別の返却型を持ち、全ExportがStatusを返すわけではありません。成功Statusを返す呼出しは、そのThreadの前の診断を
消します。失敗時は全Component bridgeで共通の固定Thread-local bufferへ保存します。

`wse_capi_last_error_message()`はUTF-8／NUL終端Pointerの借用です。同じThreadで、次のStatus生成呼出しより前に
Copyします。Size照会やStatusを返すCleanupでも上書きされ得ます。独立所有Error objectではなく、別Threadは別の
診断を持ちます。分岐には保存したCategory／Codeを使い、Message／Native codeの文字列固定を前提にしません。

共通Guardはbad_allocをResourceExhausted、その他のC++例外をInternal、Code 0へ変換します。
Guardと診断Writerはnoexceptで、診断のための動的確保を行いません。Threadごとに1024 Byteを用意し、
Message最大1023 ByteとNULを保持します。長い正常UTF-8はCode pointの途中で切らず、Category／Code／Native codeを維持します。
切詰めMarkerは追加せず、通常データのString getterの二回呼出し規則も変更しません。不正な入力Encodingの修復はしません。
Catch内の二次確保を除去するもので、全Native／Managed確保点のResource巻戻しを証明するものではありません。
GuardやNULL検査は不正Addressや任意の外部例外を捕捉しません。

呼出し前に出力HandleをNULL、Scalarを既知値へ初期化します。現在の生成／失敗経路の多くは成功まで出力を
変更しません。解放を手配せずに既存の
所有Handleを出力Slotへ渡してはいけません。出力は操作ごとに異なります。

| 操作 | 失敗時に意味を持つ出力 |
| --- | --- |
| 通常の厳密ResultによるCreate／Read／Convert | 新しい所有値なし。未初期化出力を読まない |
| TCP／Serial Send、UDP SendTo | 検査前にCountをZero化。Errorと既知のNative進捗を共に返す。Peer受領確認ではない |
| UDP receive_from_with_progress | DatagramTruncatedでも所有Prefix／Sourceを返し、失敗でも解放が必要。既存receive_fromは成功時のみ |
| HttpStatusErrorとなるHTTP execute | Body／Status／Headerを含む所有Response handle。失敗でもDestroyが必要 |
| HTTP引数／Context／Transport失敗 | Responseなし。内部NULL化より先に検査する場合があり、利用側でも初期化 |
| Size照会／Copy | 下表のRequired size／Written count規則 |

HTTP ShimはTransient retryを無効にし、要求をNonIdempotentと分類します。4xx／5xxのResponseをTransport失敗と
同一視しません。部分進捗は[XPT](XptTransport.md)に従います。
進捗保持と互換UDP APIは後掲CABI-TRANSFER-07で定義します。

## 二回呼出しCopy: CABI-BUFFER-04

| API群 | Destination NULLの照会 | 非NULLで容量不足 | 成功 |
| --- | --- | --- | --- |
| copyStringを使うUTF-8文字列 | 終端NULを含む必要Byte数。Capacity無視 | InvalidArgument、必要Sizeを維持、Destination無変更 | ByteとNULをCopy |
| Core frame_buffer_copy_to | 終端なしのPayload size | InvalidArgument、Written countを0にし、Destination無変更 | PayloadだけCopy |
| copyBytesを使うXPT Byte getter | 正確なPayload size | InvalidArgument、必要Sizeを維持 | PayloadだけCopy |

Size／Count pointerは非NULLが必須です。先行するOwner検査で失敗すると変更しない場合があります。
現Version文字列`1.0.0`の照会は6を返し、容量5は切詰めず失敗、容量6は16進で`31 2e 30 2e 30 00`をCopyします。
空Payloadは0で、終端を付加しません。確保と二回目の呼出しは利用側が行います。両呼出しの間もSource ownerを保持し、
変更を直列化します。可変Objectの原子的Snapshotではありません。

入力`[0x00,0x7f,0x80,0xff]`へのruntime_copy_frameは独立Copyを所有します。入力変更やRuntime破棄後も返却
FrameBufferは変わりません。4を照会、4 Byte確保、4 Byte Copy、最後にBufferを別途Destroyします。
Runtimeがすべての返却値の寿命を所有するわけではありません。

## 取消とCallback: CABI-CALL-05

Cancellation holderはBinding用と、XPT選択時にはXPT用のSourceを所有し、Cancelで両方を要求します。
Tokenは共有Flagを保持します。Cancelは繰返し可能で解除されず、次の独立操作には新Sourceを作ります。
Native呼出し中はOwner／Handleの格納を有効に保ち、破棄を直列化します。Runtime waitは最大5 msのSleep区間ごとと
終了時に取消を確認し、0時間Waitでも最終確認を行います。OS／Scheduling遅延があるため5 msは厳密な応答上限ではありません。

Camera callbackはCamera worker上です。成功時は新たな所有Camera-frame handleを受け取り、1回Destroyするか
Ownerへ移譲します。失敗時はFrame NULLで、Status／診断はそのWorker thread上で有効です。intptr_tのUser tokenは
Libraryが解釈しません。利用側はOwner側StopがJoinするまでCallback関数とTokenの参照先を保持し、後で登録を解放します。
Callback側StopはJoin境界ではありません。Managed delegateはCamera SafeHandleがNative破棄完了まで保持し、
Start失敗では前の参照を復元、Callback側Stopでは参照を消しません。Owner側Closeまたは次のStart成功で参照を解放／置換できます。
Callback内でManaged ownerをClose／Disposeしてはいけません。配信とStreamingは
[Tmr](TmrCamera.md)、言語の例外処理は[Language Bindings](LanguageBindings.md)を参照します。

## Managed出力の所有権受入: CABI-ADOPT-06

[NativeOutput](../../../lang/cs/Wse/NativeOutput.cs)はNative生成／出力呼出しより前に確保する一時Ownerです。
PointerはZeroで開始し、失敗Statusと一緒に返されたResponseもScopeで所有します。このOwnerを保持したまま
返却WrapperとSafeHandleを確保し、`Take()`の直後に`Attach()`して移譲します。この2操作の間で確保してはいけません。
空になった一時Ownerは何も解放せず、SafeHandleだけがOwnerになります。先にWrapper確保、Statusからの例外生成、
受入処理が失敗した場合は、GCを待たずScope終了時に未公開のNative出力を破棄します。

受入後のFrame／Selector metadata読取に失敗した場合も、SafeHandleをDisposeしてから再Throwします。
Device／Selector配列は全要素が成功した場合だけ返し、途中の要素生成／登録失敗では既に生成した全要素とNative listを
解放します。成功時は各要素の所有権が呼出元へ渡ります。HTTP Status失敗のResponseはHttpStatusExceptionが保持しますが、
その例外生成に失敗した場合はResponseをDisposeします。Transport失敗ではZero初期化済み出力が変更されない場合があります。
ProjectionはAdapter名／結果生成の失敗時に受入済みFrameBufferを解放し、GCHandleの後始末Listへの登録失敗時も
直前に固定したArrayをUnpinします。

[CameraCallbackRegistration](../../../lang/cs/Wse/CameraCallbackRegistration.cs)は、一時Ownerを確保する前に
確保不要のRaw pointer退避を置きます。捕捉可能なManaged受入／Metadata／Application例外はReverse P/Invoke境界内で処理し、
最初の例外を`WebCamera.CallbackException`に保持します。その登録は以降の配信を抑止してStopを要求します。
Stop要求自体が失敗しても後続FrameはApplicationを呼ばず破棄します。再開前はOwner側StopでJoinする必要があります。
Close／Dispose後も例外は読め、Start成功で消去、Start失敗では以前の登録を復元します。Lifecycle操作は直列化します。
通常のNative errorをApplicationへ渡すだけではCallbackExceptionにならず、その処理中にManaged例外がThrowされた場合だけ
記録します。このPropertyはWorker失敗の観測に使えます。

Application callbackへ入る直前にFrame所有権が移ります。Frameを保持した後にThrowしても利用側の責任であり、Bridgeは
所有権を取り戻して破棄しません。保持する意図がなければusingを使います。CallbackからCameraをClose／Disposeしてはいけません。
CLRの致命的障害、Process終了、Heap全体の枯渇に対する回復保証ではありません。

Nativeの[配信Helper](../../../lang/cs/native/camera_callback.h)はFrame handle生成をGuardします。
bad_allocはResourceExhausted、その他のC++生成例外はInternalとしてNULL FrameとWorkerのTLS診断で1回通知し、
内部MarkerをCameraSession workerのC++ Callback例外境界で受け止めて配信を終了します。
MarkerはExportされたC ABIやManaged境界を越えません。配信終了だけではNative streamingは止まらず、OwnerのStop／Joinが必要です。
成功FrameはCallback開始時点で受信側が所有します。Callback呼出しを確保Guardの外へ置くことで、受信側のThrowを
二度目の通知へ変換しません。

## C境界の部分転送: CABI-TRANSFER-07

TCP／Serial SendとUDP SendToは、他の引数検査より先に非NULLのCount出力をZero化します。
Native操作が返った後、[publishTransferCount](../../../lang/cs/native/transfer_result.h)がError変換より前に
完了数を公開し、失敗でもCount代入を飛ばしません。既知の完了Byte数はTimedOut、Cancelled等と共存できます。
これはPeerの受領確認ではありません。取消されたPending Serial Writeは完了Chunk数を超えてWireへ影響し得ます。
TransferResultが返る前のC++例外ではZeroが残りますが、Wireに影響がなかった証明ではありません。
Count公開後に診断変換が失敗した場合、外側GuardがResourceExhausted／Internalへ変換してもCountは残ります。
Adapterは自動再送しません。

`wse_capi_udp_client_receive_from`は成功時だけOwnerを公開します。
`wse_capi_udp_client_receive_from_with_progress`は検査前に非NULLの出力をNULL化し、成功または
DatagramTruncatedで所有Datagramを返します。切詰め時も格納済みPrefixと元の送信元Endpointを持ち、消費済みSuffixは
次回取得できません。Status失敗でもDatagramをDestroyします。それ以外のErrorはNULLです。
Handle生成／診断変換の確保失敗もNULLで、消費済みDatagramは再送・回復しません。
内部unique_ptrが生成とStatus変換の完了まで所有します。失敗時TLS診断はPayload／Source Getterを呼ぶ前にCopyします。
Getter成功はTLSを消去します。両入口は文書化した公開値配置とABI version 1を使い、Managed／Nativeは同じBuildの組を使います。

C#のTcpClient.Send、SerialPort.Send、UdpClient.SendToはNative失敗時にTransferExceptionをThrowし、
BytesTransferredをulongで保持します。失敗経路でNative size_tを狭めません。WseException派生なので既存の基底Catchは有効です。
UDP ReceiveFromは新C APIを使用し、DatagramTruncatedでは独立したManagedのReceivedDataとSourceEndpointを
TransferExceptionへ持たせます。BytesTransferredはPrefix長です。Throwが完了する前にNative DatagramをDisposeするため、
例外自体にNative OwnerやDispose責任はありません。Send失敗のReceivedData／SourceEndpointはnullです。
通常のUDP Timeout／引数失敗はWseExceptionで、Payloadを生成しません。呼出し前の引数・破棄済み検査や確保例外を
TransferExceptionへ一律変換しません。PrefixのCopy／受入／例外生成に失敗した場合もNative所有を解放してその例外を伝えますが、
Byte列を回復可能とする保証ではありません。

現C++のTCP／Serial Receiveは最初の完了Chunkを成功で返し、失敗は空の値です。Windows Pending Readの失敗／取消も含み、
Native Resultが公開しない受信PrefixをAdapterは生成しません。Python／JavaScript／Javaも個別Runtime Adapterで進捗／Prefixを公開します。
[Language Bindings](LanguageBindings.md)のBIND-TRANSFER-05を参照します。以下のC ABI Gateは、
その変換／所有経路だけの証拠として扱います。

再現Gateはwse.capi.transfer_contract（本番Helper 27ケース）、wse.capi.udp_progress_contract（実C ABIとUDP Loopback）、
wse.binding.dotnet_transfer_contract（Managed人工25ケース、うち巻戻し5境界）、wse.binding.dotnet_contract
（実ShimのUDPとTCP送信／取消）です。人工HandleでLeak／二重解放を検知し、Getterが診断を上書きしても元のErrorが残ることを確認します。
1回のSend失敗と非零進捗の共存は決定的なFixtureで検証し、OSのSocket Buffer容量からShort Writeを強制できるとは仮定しません。
Serial実機、Heap全体枯渇、Wireの送達、全Native I/O故障を認証するMatrixではありません。

## 根拠と未検証範囲

- `wse.capi.abi_snapshot`と`wse.capi.c_layout_snapshot`は全値配置をC++／CでCompileします。
  C FixtureはManaged照合用の実測値も出力します。この2 TestはShimをLoadしません。
- `wse.capi.runtime_contract`はBuild済みShimを公開Core C関数で呼び、文字列／Bufferの二回呼出し、独立所有、TLS診断、Sticky取消を検証します。
- `wse.capi.allocation_contract`は本番の共通／Core bridgeとFacade sourceを実行FileへCompileし、Runtime生成、Cancellation生成、Frame copyの
  捕捉可能な`operator new`確保点へ順に失敗を注入します。失敗時に出力を公開しないこと、次の確保位置での回復、
  確保が持続的に失敗しても診断を返せること、長いUTF-8、未知例外、TLS分離を確認します。
  MSVCの独立FixtureだけはDebug iterator管理を無効にします。STLがnoexcept Move内でContainer proxyを確保すると、
  失敗時はC ABI guardへ届く前にTerminateするためです。本番Build flagは変更しません。MSVC Debug STLの確保枯渇、
  Aligned／Native allocator、全ComponentのResource巻戻しは注入範囲外です。
- `wse.binding.dotnet_contract`はC／C#の16配置・87 Field、最後のLeaseの解放順序と冪等性、Core／Frame／Cancellationの
  64回のDispose競合を検証します。Source inventory gateはSafeHandleを使う直接Opaque入力96引数を検査します。
  Component／HTTP検証は選択時に実行します。
- `wse.binding.dotnet_ownership_contract`は人工C ABI Providerで所有Handle数と二重解放を検査します。
  Constructor、返却Owner、途中の配列、Metadata、HTTP例外生成、Projection結果／Pin登録の53故障注入と、
  Native workerからのReverse P/Invoke 8ケースを実行します。保持Frame、後続配信の抑止、Stop失敗、登録のResetも検査します。
  実CameraやCLR Heap枯渇を起こすTestではなく、名前を付けたManaged境界への故障注入です。
- `wse.capi.camera_delivery_contract`は本番の配信Helper／Guardを使い、所有Frameの受渡し、Native確保失敗、
  生成例外2種類、受信側Throwの5ケースで単一Error通知と内部停止Markerを確認します。
  CameraSession worker側の例外封じ込めは既存のCamera Start／Stop契約で別途検証します。
- 全Native I/OのStress、Native device確保、Device workerの全Shutdown競合は別作業として残します。
  無機材Testで実Cameraや全競合・全OOMを認証しません。

再現時はBootstrap済みWindows／Linux presetでHost .NET SDKと`WSE_BUILD_DOTNET_BINDING=ON`を指定してBuildし、
`ctest --test-dir <build-directory> -C Debug --output-on-failure -R "wse.capi.|wse.binding.dotnet.*contract"`を実行します。
CoreがSTATICでもShimは共有Libraryです。通常の全SuiteでHeader inventoryと文書も確認し、実機Opt-inは無効にします。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
