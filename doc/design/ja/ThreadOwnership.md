# WSE Thread／Ownership Model

> Canonical source: [English Thread and Ownership Model](../en/ThreadOwnership.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Ownership

- 各Resourceは1つのRAII Ownerを持ちます。公開Handle／Frame bufferは所有値または明示的な非所有IDです。
- PIMPLはOS Handle、COM Object、File descriptor、ThreadおよびCallback stateを所有します。
- 返却Frame／Binding BufferはByte列を所有し、Call／Callback終了後のBackend capture bufferを参照しません。
- Raw pointerは暗黙のOwnership移譲ではありません。公開APIはRaw `void*`を持ちません。

## Thread safety

- Value型／Immutable descriptionはThread間でCopyできます。
- StatefulなDevice、Transport、Renderer、Runtime Ownerは、Component契約に明記しない限り同時変更できません。
- 同期OperationはComponent ContextでTimeout／Cancellationを定義し、Cancelを成功扱いにしません。
- Stop／Closeは冪等で、Destructorは停止要求と所有WorkのJoinを行います。有限のPoll待機はUser Callbackや
  すべてのOS呼出しを時間制限しません。Callbackが戻らなければOwner ThreadのJoinも完了できません。

## Callback

Callback APIは実行Thread、保持State、再入可能操作、解除動作および例外境界を記載します。WSEは借用Stack
Callbackではなく所有Callableを保持します。解除・停止・破棄の同期保証は次のように異なり、個別契約を優先します。
例外をC・OS・言語Runtimeの境界外へ出してはいけません。Native操作は対応例外を変換しますが、Callbackの
捕捉はAdapterごとに異なり、C# Camera bridgeはCallback例外を`CallbackException`へ保持します。
限界は[多言語Binding](LanguageBindings.md)と[C ABI](CAbiContract.md)を参照してください。

| 操作 | 取得済み・実行中Callback | 復帰・再入契約 |
| --- | --- | --- |
| TimerのOwner Thread `stop()` | Joinして待つ | 以降の起動なし。Ownerは再Startできる |
| TimerのCallback内`stop()` | 現在の呼出しは継続 | 停止要求のみ。Ownerからの破棄または次のStartでJoin |
| Keyboard `clearCallback()` | 取得済みCallableは実行されうる | 以後の取得を止めるが完了待機ではない。Callback内のSnapshot参照・Clearは可能 |
| KeyboardのOwner Threadからの破棄 | Worker／Callbackを待つ | Callback内で同一Keyboardを破棄・代入してはいけない |
| Log Sink解除 | 取得済みRegistry Snapshotから呼ばれうる | 完了待機ではない。共有SnapshotがSinkを保持し、Producer Threadからの並行呼出がありうる |
| CameraSessionのOwner Thread `stop()`／`close()` | 進行中Readを待ち、Native stopを直列化後、Backend lock外でJoin | 正常StopはOpenを維持。Native後始末失敗ならCloseされる場合があり、CloseはSessionを終了 |
| CameraのCallback内`stop()` | 現在の呼出しは継続し、OwnerとNative stopを直列化 | Thread-local識別で自己Joinを延期。次のOwner Thread Stop／Start／CloseでJoin |
| Managed終端解放 | Runtimeによる | LanguageBindingsに従う。Session Closeと終端解放を区別 |

Timer Callbackは自身のTimerを破棄してはいけません。CameraSessionには互換性のため安全なSelf-destruction
経路がありますが、外部Ownerを保持しOwner Threadで回収する利用を推奨します。
Self-stopが可能なことから一般的なSelf-destruction保証を導いてはいけません。

CameraSessionはNative start前にCallback stateを準備し、Thread所有権を公開後にWorkerを進めます。
生成失敗ではBackendを停止し、巻戻し不能ならCloseして元のC++例外を送出します。Read例外は終端Callback resultを
1回通知します。状態と例外の詳細は[Tmr Camera](TmrCamera.md)のTMR-CALLBACK-03に従います。

## Worker実装とLock

内部[WorkerController](../../../core/wse/utility/wse_WorkerController.h)はThread、Mutex、Condition variable、
Running・Stop要求Flagを所有し、公開Task Queueは持ちません。Timer／Keyboardはこれを組み込み、
CameraSessionは固有のCallback Worker状態を持ちます。

```text
owner start -> reap previously finished thread outside lock -> mark running -> launch body
owner stop  -> set stop flag -> notify wait -> take thread under lock -> join outside lock
self stop   -> set stop flag -> notify wait -> return to callback -> body exits
next owner stop/start/destructor -> join finished thread
```

Worker本体の例外はThread終了前に捕捉します。Workerが終了に必要とするMutexを保持してJoinしません。
Component状態のLockはSnapshotやCallback取得を保護し、KeyboardのUser Callbackは状態Mutexの外で実行します。
すべてのOwnerの同時変更を許可するものではありません。XPT／RendererはCaller-confinedであり、
別ThreadからのCancellationは明示的な別機能です。

[Worker契約](../../../test/characterization/wse_worker_controller_contract.cpp)は反復Start／Stop、Self-stop、
Wakeup、例外捕捉を検証します。[Keyboard lifecycle](../../../test/characterization/iui_keyboard_lifecycle.cpp)は
Callback Captureの所有とOwner置換を検証し、物理Key入力や全取得Raceの検証ではありません。

## Binding Runtime

Runtime Closeの動作は言語ごとです。NodeはWorker回収を所有し、Python Core Runtime Closeは実行中Callを
Joinせず取消要求し、C# Runtimeは同期Handle Ownerです。取消でQueue済みCallbackが一律に消えるわけでは
ありません。Adapterに明示保証がない限り、Owner破棄と操作を直列化します。GC／FinalizerはFallbackであり、
Exampleは明示Close、Context managerまたは`AutoCloseable`を使用します。

C#は平坦C ABIを通じて同期Runtime操作を公開します。Ownerは単一の`SafeHandle`と`IDisposable`を使用し、
Exampleは`using`または明示的な`Dispose()`で解放します。Disposeは冪等かつ終端で、以降の操作は
`ObjectDisposedException`とします。`WebCamera`の`Close()`はDevice Sessionだけを終了し、同じObjectを
再Openできます。`Dispose()`はOwnerを解放します。各言語のSession終了と終端解放の対応は
[多言語Binding](LanguageBindings.md)契約を参照してください。

Timer周期とLog Sink再入の詳細は[Core共通サービス設計](CoreServices.md)で定義します。
CameraのCallback配信とBackend streamingは別状態です。終端Read errorやCallback例外で配信終了しても
`isStreaming()`はtrueになり得るため、再開前にOwner側でStop／回収します。Renderer HandleはBackend寿命の世代を持ち、
Moveで移譲します。Shutdown／再初期化後と別Instanceでは別世代を使います。世代予約はAtomicですが、
同じRendererの操作は利用側の直列化が必要です。現Vulkanは送信内部で完了を待ち、Shutdown待機も有限時間保証ではありません。
停止Workerや返却Fenceを共通の完了保証と解釈せず、Tmr／OUI設計の限界に従います。
KeyboardのCopyは独立Worker間でCallableを共有し、Access状態とSnapshotは別の観測です。
公開順序とPlatform差は[IUI Keyboard](IuiKeyboard.md)を参照してください。
Component固有Ruleは[XPT](XptTransport.md)、[OUI Renderer](OuiRenderer.md)、[Tmr Camera](TmrCamera.md)、
[多言語Binding](LanguageBindings.md)で正式に定義します。

libcamera完了Callbackは事前確保したRequest Ringだけを更新し、Metadata処理はReaderへ移します。
停止失敗時もPending所有を保持し、切断したPipelineのCloseには有限待機保証がありません。
[Tmr Camera](TmrCamera.md)のTMR-LIBCAMERA-06を参照します。

Windows MFの列挙／Device名はTask memory owner、Sourceは参照解放前にShutdownを試みるOwnerで管理します。
専用Native ThreadがBackendの生成・操作・破棄を担当し、公開CallbackのSelf-stopや後の別ThreadからのCloseでも、
COM終了を呼出し側へ移しません。公開Callback Workerは別Threadです。[Tmr Camera](TmrCamera.md)のTMR-MF-STOP-08で
同期Stack配送、非同期Flush完了、通知待機2,000 ms、失敗後のClose／Reopenを定義します。Native呼出し時間自体は有界ではありません。
libcameraの列挙とSessionは1つのWSE Runtime内でManager Leaseを共有し、生成と最後のLease解放を直列化します。
切断時Pending Requestの制約は残ります。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
