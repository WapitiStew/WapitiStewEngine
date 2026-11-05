# WSE非同期Log

> Canonical source: [English Asynchronous Logging](../en/AsyncLogging.md)
> Documentation version: WSE 1.1.0
> Last synchronized: 2026-11-05

## 対象と公開API: CORE-LOG-01

C++17のCoreサービスとして、明示的なOwnerを持つ非同期出力経路を提供します。Applicationは
[AsyncLogRuntime](../../../api/wse/utility/AsyncLogRuntime.h)を生成し、設定を渡し、すべての
Producer停止までOwnerを保持します。構築だけではWorkerを起動しません。最初の`configure()`成功で
Writerを1本起動し、再設定では同じWriterを使います。`stop()`は排出とJoinを行い、後のConfigureで
同じOwnerを再開できます。

同期`Logger<Tag>`、`writeLog()`、登録済み`LogSink`とGlobal最低Levelは独立しています。
既存APIは[Core共通サービス](CoreServices.md)の同期配送・Profile・解除規則を維持します。
非同期Runtimeの生成によって、既存LibraryのLogを迂回配送したりProcess共通Threadを暗黙起動したりしません。

Tag名、Verbosityの意味、文面Catalog、表示、起動・終了順はApplicationが所有します。
WSEは有限配送、Writer実行、出力ResourceとPortableな診断を所有します。
Application固有Enum、Error ID、文面IDを公開SDKへ追加しません。

| 公開型・操作 | 責務 |
| --- | --- |
| `sAsyncLogRecord`、`sLogPart` | Timestamp・重大度・Tag・Source・呼出元・Verbosity・本文Part・属性の所有値 |
| [LogMessageBuilder](../../../api/wse/utility/LogMessageBuilder.h) | 値変換・Separator・有限の型付き本文構築 |
| `AsyncLogLine` | Runtimeを指定するRAII Stream入口。破棄時に完成Recordを1件投入 |
| `sAsyncLogSettings` | Runtime別Filter・上限・出力・Formatter・Console Callback・Custom Sink |
| `AsyncLogRuntime::tryAppend()` | 完成RecordのFilter・検証と有限Queueへの所有Copy |
| `AsyncLogSink` | Writer上での`write(record, text)`と`flush()` |
| [LogError／LogStatus](../../../api/wse/error/LogError.h) | PortableなCategory／Codeと診断用Message／Native code |
| `sAsyncLogStatus` | 稼働・受付状態、Queue件数、累積件数と最後のError |

## 型付き本文と値変換: CORE-LOG-02

`LogMessageBuilder`はProducerに閉じて使用します。通常のAppendは、先頭値と空文字列を含む
各値の前へ、選択したWSE Separatorを付けます。値の間だけに付ける規則ではありません。
文字列、UTF-8へ変換したWide文字列、算術型、Bool、EnumはWSEの変換規則に従います。
算術型は`std::to_string`、Boolは`true`／`false`、通常Enumは幅に応じた16進表現です。

専用の非Template `append(eLogControl)`と`operator<<`は通常Enum Templateより優先して
`eLogControl::NewLine`を受け、Separatorを付けず型付き`NewLine` Partを追加します。
次の通常値には従来どおり前置Separatorを付けます。改行はRecordの分割・投入・Sink Flushを行わず、
Separatorの規則もResetしません。

```cpp
wse::LogMessageBuilder body( wse::eLogSeparate::SPACE );
body << "first" << wse::eLogControl::NewLine << "second";
// 前置Space + first + Platform改行 + 前置Space + secondとして本文を表示する。
```

本文Partの型はQueueからFormatter Callbackまで保持します。`renderLogParts()`は`Text`内の
CR／LF／NULを可視文字列`\r`・`\n`・`\0`にし、型付きNewLineだけを`END_LINE()`へ変換します。
Prefix／Suffixは付加しません。特殊な文字列Delimiterは使わないため、任意の入力文字列が
明示的な表示制御へ化けません。既定Formatterは設定Suffixを表示行ごとではなくRecord全体へ1回だけ適用し、
Record取得時点のTimestampと標準のTag／File／行番号配置を使います。
Custom Formatterは完全な出力文字列を返し、Prefix／Suffixと型付き本文の表示を自身で担当します。
制御を先に単なる文字列へ潰さず、`renderLogParts()`を利用できます。

既存同期Stream Loggerは通常文字列の扱いを維持します。別の場所で非同期Runtimeを使うことだけを理由に、
同期APIの値変換・改行処理を変更しません。

`AsyncLogLine`はRuntime・Tag・Verbosity・呼出元・重大度を結び、破棄時に1件投入します。
`setLevel()`でそのRecordの重大度を変えます。RuntimeはLineの破棄完了まで生存させます。
専用NewLine Overloadは制御Enumを数値化せず、型付き制御として保持します。
Lineは構築時にSeparatorとBuilder上限をSnapshotします。破棄前に再設定された場合、最終投入には
現在のRuntime Filter・Record上限を適用し、受理した設定がFormatter・Sink・Suffixを決めます。
Line構築で、その寿命全体の旧設定を予約するわけではありません。

## 受付・Filter・容量: CORE-LOG-03

Recordは文字列、Part、属性を所有し、受付時に所有値をCopyします。WriterはProducerのStack、
入力Buffer、機能Frame、一時文字列を借用しません。同じ入力Recordを`tryAppend()`と並行変更しません。

FilterはRuntimeごとです。重大度は`minimum_level`以上を必要とし、Verbosity 0と
`LogLevel::Off`は無効です。Tag別閾値は`default_verbosity`を上書きします。
VerbosityはApplicationが意味を定める符号なし尺度で、重大度とは独立です。
このRuntimeの設定変更は同期WSEのGlobal Filterや別Runtimeへ作用しません。

`queue_capacity`、`record_byte_limit`、`record_part_limit`、`metadata_byte_limit`で
受付Queue、本文、Metadataを制限します。既定はQueue 1024件、本文16KiB、Part 1024個、
Metadata 16KiBです。本文ByteはTextの生Byte数と、NewLineごとの`END_LINE().size()`を合計します。
属性数も`record_part_limit`以下に制限します。過大入力は全体を拒否し、
黙って切り詰めたり複数Queue Recordへ分割したりしません。

`tryAppend()`は`noexcept`で、Producer上でFormatterやSink I/Oを実行しません。
結果は`Accepted`・`Filtered`・`NotRunning`・`QueueFull`・`RecordTooLarge`・`Failed`を区別します。
AcceptedはQueueへの所有移管であり、出力成功ではありません。同期とCopyを使うため、
Lock-freeやReal-timeの保証ではありません。

## Writer・Callback・失敗: CORE-LOG-04

```text
producer -> runtime filter and limits -> owned finite queue -> one writer
                                                      -> formatter(record)
                                                      -> console/file output
                                                      -> custom sink write(record, text)
owner -> flush barrier / configure drain / stop and join
```

WriterはQueue Mutexの外で整形と出力を行います。任意のFormatter、Console Writer、Custom Sinkは
有効設定が保持し、そのWriterで呼びます。Captureが借用する外部ResourceはStop完了まで生存させます。
同じSinkを複数Runtimeで共有する場合、その内部同期はSinkが所有します。
1つのRuntimeのWriterは、別RuntimeのCallbackを直列化しません。

ConsoleにはWSE出力AdapterまたはApplicationの`console_writer`を使えます。
Promptも表示するFrontendは、Callback内で自身のMutexを取り、両方を直列化できます。
Writer完了を待つRuntime制御を呼ぶ間は、そのFrontend Mutexを保持しません。
WSEはApplicationのUI Lockを所有しません。
Console Callbackには別のFlush Hookがないため、Applicationが必要とする場合、借用BufferのFlushを自身で行います。
Runtime BarrierはCallbackの書込み完了まで待ちます。

FileはUTF-8の`file_path`を使い、再設定確定前に候補ResourceをOpenし、Open／Write／Flush失敗を返します。
Application固定のFile名は要求しません。Custom Sinkは`LogStatus`を返します。
Formatter／Callback失敗は封じ、Producer Appendからの例外ではなくLog診断にします。
複数出力先の書込みはAtomic Transactionではなく、別出力先へ既に書いたByteは取り消せません。

`getStatus()`はQueue・受付状態、Accepted／Completed／Dropped／Filtered件数と
`last_error`のSnapshotを返します。Category／Codeは分岐契約で、Message／Native codeは診断用です。
Log障害とApplicationの操作結果を分けます。明示再設定の成功で保持Errorを解除し復旧できます。

`LogError`はLog専用Category `None`・`Configuration`・`Capacity`・`Io`・`Callback`・`Lifecycle`・
`Internal`を使います。Codeは不正設定・停止中受付・Queue／Record上限・Open／Write／Flush失敗・
Callback／再入失敗・確保／Thread失敗を区別します。Application、Transport、DeviceのError Enumとは独立です。

Callbackから`isEnabled()`と`getStatus()`は照会できますが、自身のRuntimeをConfigure／Flush／Stopしたり、
Recordを再投入したり、破棄したりしません。
制御の再入は自己排出・自己Joinを待たず`ReentrantControl`で拒否します。
Appendの再入も循環配送を防ぐため`Failed`で拒否します。
戻らないCallbackは外部からのFlush／Stopも妨げます。Applicationコードを強制中断せず、
I/O完了の有限時間も保証しません。

## 再設定と完了: CORE-LOG-05

`configure()`は受付停止・有効設定置換の前に候補を検証・準備します。候補準備失敗では旧設定・出力Resourceを
保持します。成功時は旧設定で受け付けたRecordを排出後、Formatter・Sink・出力・Filterを切り替えます。
Writerは1本を再使用し、Recordや再設定ごとに生成しません。

`output=NONE`でCustom Sinkもない場合は出力をFilterします。Custom SinkがあればSinkだけの配送が可能です。
Native出力を無効にしたことだけでSink用Recordを捨てません。

`flush()`は呼出し前に受け付けたRecordのBarrierです。後から投入する並行Producerによって、そのBarrierを
無期限に先送りしません。対象出力ResourceとCustom SinkもFlushします。
Producer停止、永続化／fsync、独立した同期WSE LogのBarrierを意味しません。
`flush_each_record`の既定はfalseです。trueなら各Recordの完了計上前に出力ResourceとSinkをFlushします。
明示BarrierとStopの契約も維持します。

通常終了はApplicationが制御します。Producer停止・Join、最終Record保持、`stop()`、借用依存Resourceと
所有Moduleの解放の順にします。`stop()`は受付を閉じ、受理済みRecordを排出・FlushしWriterをJoinします。
復帰後はそのRuntimeからCallbackを呼ばず、Formatter／Console／SinkのCaptureも解放済みです。
再設定の旧CaptureとStopのCaptureはQueue／Control Lockの外で破棄します。
反復Stopは安全です。DestructorもOwner側の後始末を行いますが、
破棄前に操作失敗を確認する場合は明示Stopを使います。

## 検証と互換性

Runtime・並行契約は所有Record、Runtime別Filter、Writer上の出力、容量、型付き改行、候補準備失敗、
出力／Callback失敗、継続Producer中Barrier、並行Producer内の順序、Capture解放、終了を検証します。
Software契約であり、実機やStorage永続化の認証ではありません。
公開Header、Package、外部Consumer GateでC++17のSHARED／STATIC利用を検証します。
実行入口は[設計と検証](DesignVerification.md)を参照してください。

同期APIのSource互換を維持します。新公開Headerを使って再BuildするApplicationは、対応するInstall済みWSEを使います。
独立してBuildしたSDK／Application間の任意のBinary Layout交換互換を保証する契約ではありません。

## 変更履歴

| 日付 | 変更内容 |
| --- | --- |
| 2026-11-05 | 明示Ownerの非同期配送、型付き改行本文、独立Filter、診断、完了Barrierを定義。 |
