# WSE Error Handling Cookbook

> Canonical source: [English WSE Error Handling Cookbook](../en/ErrorHandlingCookbook.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

失敗しうる公開Operationはすべて[Result／Status契約](../design/ja/ResultContract.md)に従います。
厳密な`Result`／`Status`は値かErrorのどちらか一方だけを運び、部分進捗契約（`TransferResult`／
`HttpResult`）だけがErrorと並んで意味を持つ値を運びます。分岐は`category()`と`code()`で行い、
`message()`と`nativeCode()`は診断専用です。本ページは状況別のRecipe集です。

## 基本Pattern

```cpp
const wse::xpt::TransportStatus connected = client.connect( endpoint, context );
if ( !connected.succeeded() )
{
    switch ( connected.error().category() )
    {
        case wse::xpt::eTransportErrorCategory::Timeout:      /* 再試行または通知 */ break;
        case wse::xpt::eTransportErrorCategory::Cancellation: /* 静かに停止 */       break;
        default:                                              /* 通知 */             break;
    }
}
```

失敗した厳密Resultの`value()`や成功したResultの`error()`の読み出しは`wse::ResultAccessError`
（`std::logic_error`）を投げます。これは処理すべきRuntime条件ではなく呼出側の契約違反です。
先に`succeeded()`を確認するか、代替値付きの読み出しには`valueOr()`を使います。

## Coreデータ型

Coreのデータ面（`Map`・`Matrix_`・`Image_`・`Pixel_`・Point・Range・Mesh・`Homography`）は
使い方違反に標準Exception ― `std::invalid_argument`／`std::out_of_range`／`std::domain_error`／
`std::logic_error` ― を投げ、起こり得る結末には投げません。現在唯一の「計算上起こり得る失敗」は
特異行列で、`Matrix_::tryInverse()`が`wse::CoreResult<Matrix_>`を返し、失敗は
`eCoreErrorCategory::Computation`／`eCoreErrorCode::SingularMatrix`を運びます。

```cpp
const wse::CoreResult< wse::Matrix > inverted = matrix.tryInverse();
if ( !inverted.succeeded() )
{
    // 特異入力はBugではなくData. Fallback・正則化・通知のいずれかで扱う.
}
```

## GEF File I/O

`BINController`と`CSVController`はI/O・形式・上限の失敗を`GefStatus`／`GefResult<T>`で返します。
`Io`・`Format`・`Resource`と安定Codeで分岐します。確保・長さの失敗は標準例外のままで、Error生成も
確保しえます。空BINの`last_index()`は使用条件違反（`std::logic_error`）です。

設定表の再読込には上限付き置換を使います。CSVの`read`・`readWithLimits`はHeaderを含む全入力行を追記し、
`readReplace`は全入力の成功後だけ置換します。BIN読込も成功時だけ置換し、失敗時は前の状態を利用できます。

```cpp
wse::gef::CSVController csv;
wse::gef::ReadLimits limits;
limits.max_input_bytes = 1024 * 1024;
limits.max_rows = 1000;
const auto loaded = csv.readReplace(path, limits);
if (!loaded.succeeded()) {
    // LimitExceeded: 入力または許容上限を見直す。以前の表は保持される。
    // FileOpenFailed / ReadFailed: 入力の問題を解消して再試行する。
} else {
    const auto saved = csv.writeAtomic(output_path);
    if (!saved.succeeded()) {
        // 元の宛先は保持される。原因を解消して同じControllerで再試行する。
    }
}
```

`writeAtomic`は一時保存とFlush／Closeを確認後、信頼できるLocal Directory内の通常Fileを置換します。
直接の`write`も遅い失敗を`Io/WriteFailed`で返しますが、BIN追記を含め部分出力が残りえます。
Atomic置換は電源断永続性・Metadata保持・Atomic追記を保証しません。
正確な上限と復旧範囲は[GEF File形式](../design/ja/GefFileFormats.md)を参照してください。

## License操作

`License::load`・`Licensekey::genrate`／`load`・`LicenseWriter::save`は、運用・検証上の失敗
― Key File不在、期限外License、非対応Device ― を`wse::LicenseStatus`／
`wse::LicenseResult<T>`で報告し、安定分類`eLicenseErrorCategory`（`Io`、`Verification`）と
`eLicenseErrorCode`で分岐します。同一Processでの二重Loadは使い方違反（`std::logic_error`）、
範囲外のWriter Enumは`std::invalid_argument`です。

```cpp
const wse::LicenseStatus loaded = wse::License::load( key );
if ( !loaded.succeeded() &&
     loaded.error().code() == wse::eLicenseErrorCode::LicenseExpired )
{
    // 期限切れLicenseは想定すべき配布状態. Free tierで動作させるか通知する.
}
```

## Timeout

Category `Timeout`（XPT・OUI）は、`OperationContext`の明示Deadlineまたは Frame待機の超過を
意味します。Operationは停止しましたが、後続呼出しが別のErrorを返さない限り接続やDeviceは
使用可能とみなします。同じまたは長いDeadlineでの再試行か、ユーザーへの通知が正しい反応です。
Tmrは`readFrame( timeout_ms )`で同じ分類を返します。

```cpp
const auto frame = camera.readFrame( 100U );
if ( !frame.succeeded() &&
     frame.error().category() == wse::tmr::eCameraErrorCategory::Timeout )
{
    // 100ms内にFrameなし. Streamは動いているので再Poll.
}
```

## Cancellation

Category `Cancellation`は自分の`CancellationSource`が発火したことを意味します。協調的で
期待済みの結末です - 失敗としてLogせず、再試行もしません。Cancel済みContextでのOperationは
Timeoutを待たずに速やかに諦めます。

```cpp
if ( !sent.succeeded() &&
     sent.error().category() == wse::xpt::eTransportErrorCategory::Cancellation )
{
    return; // 呼出側自身の要求である
}
```

## Camera開始とCallbackの失敗

Windows MFの列挙／Device名、能力照会Source、途中OpenはScopeに対応して後始末します（TMR-MF-INIT-07）。
確保例外は後始末後に伝播しえます。最終CloseはOpenしたOwner Threadで行い、Shutdownの試行だけでDriver復旧を
判定しません。[Tmr初期化所有権](../design/ja/TmrCamera.md)を参照します。

CameraSessionの`start(callback)`では、Callable copyやThread生成のC++例外が送出され得ます。
準備はNative開始前に行い、開始後のThread生成失敗はCaptureを巻き戻します。例外を捕捉したら
`isOpen()`を確認し、Openなら再Start、Closedなら明示的に再Openします。巻戻し失敗時はBackendをCloseします。
WorkerのRead例外は、bad_allocなら`ResourceExhausted`、他は`BackendFailure`として1回通知します。
再Start前にOwner側でStopしてください。BackendのStreamingとCallback配信は別状態です。
終了とNative Adapterの限界は[TmrのCallback復旧](../design/ja/TmrCamera.md)を参照してください。

libcameraはRequest／Mappingの所有をトランザクションとして扱います（TMR-LIBCAMERA-06）。
Copy／Control例外や再Queue失敗後はStop結果を確認してから再開します。停止失敗時もPending Requestを保持し、
切断したPipelineではCloseが無期限に待つ場合があります。[Tmrの資源と停止制約](../design/ja/TmrCamera.md)を参照します。

## Device切断

TmrはDeviceの取り外しをCategory `Device`・Code `DeviceDisconnected`で報告します。Sessionは
終了です: Streamを止め、Cameraを解放し、再Openの前に`enumerate()`をやり直します - 同じ機体が
新しいDescriptionで戻ることがあります。Constructor境界やCallback境界で戻り値と分離される
呼出側のために、`WebCamera::lastError()`が直近の失敗を保持します。

```cpp
if ( !result.succeeded() &&
     result.error().category() == wse::tmr::eCameraErrorCategory::Device )
{
    (void)camera.stop();
    (void)camera.close();
    // 再Openの前にenumerate()をやり直す.
}
```

OUIのGPU版はCategory `Backend`のCode `DeviceLost`です: Rendererをshutdownして作り直します。
既存Handleは失われています。

## Backend／Operation未対応

Category `Unsupported`はPlatform・Backend・Operationの組合せが成立しないことを意味する
構成上の結末で、一時的な失敗ではありません。再試行せず、別Backend（例: Software adapter）の
選択、要求の縮小、または制限の通知を行います。何がどのPlatformで使えるかはSupport matrixが
記録します。

## Lifecycle違反

Category `Lifecycle`（OUI・Tmr）と`Validation`は、現在の状態や引数が最初から許していない
呼出しを示します: 未OpenのCameraのstart、破棄済みTexture handleの使用、逆転したRange。
これらは呼出側のBugであり、Runtimeで処理せず呼出順序を直します。Rendererの再`initialize`契約が
手本です: 同一設定は冪等に成功し、異なる設定は`AlreadyInitialized`で拒否されます。

比較には`adapter_name`の文字列完全一致を含み、結果として同じ物理Adapterを選ぶ要求でも同一視しません。
旧資源の使用を終えて`shutdown()`し、新しい設定で初期化します。MoveはBackendとそのHandleをMove先へ移譲し、
新しい資源は作りません。別Rendererまたは再初期化前のHandleは操作可能なBackendで`ResourceNotFound`となります。
Descriptionから資源を作り直し、Value／Generationを書き換えたり同じ不正識別子を再試行したりしません。
詳細は[Handle寿命](../design/ja/OuiRenderer.md)を参照してください。

OUI Vulkanには復帰可能なLifecycle Errorがある。送信待機がTimeoutした後は、GPUが保持資源を
使用中であるため、後続の変更操作が`Lifecycle/ResourceInUse`を返す場合がある。Handleを保持し、
その拒否された操作を後で再試行する。各試行は待たずに完了をPollする。TimeoutしたReadbackは
Frameを返さず、完了確認後に新たなReadbackを実行できる。失敗したDrawは既に実行された可能性が
あるため、自動再実行しない。DeviceLostにはShutdownと再初期化が必要である。
詳細は[Rendererの失敗順序](../design/ja/OuiRenderer.md)を参照する。

## 部分転送

`TransferResult`と`HttpResult`だけが、Errorと並んで値が意味を持つ形です。失敗した`send`も
届いたByte数を伝え、4xx/5xxのHTTP交換もResponseを保持します。両方を読みます。

```cpp
const wse::xpt::TransferResult< std::size_t > sent = client.send( payload, context );
if ( !sent.succeeded() )
{
    const std::size_t delivered = sent.value(); // 相手に届いた可能性のあるByte数
    // Offsetからの再開か放棄かを決める. 届いたByteは取り消せない.
}
```

## Binding例外

PythonのWseTransferError、JavaのTransferException、JavaScriptのTransferError Field、C#のTransferExceptionは、
Send失敗の完了数とUDP切詰めのPrefix／送信元を保持します（BIND-TRANSFER-05）。既存の基底Catchも有効です。
Countを読んで復旧方法を判断し、元Payloadを自動再送しません。[Binding失敗時の値](../design/ja/LanguageBindings.md)を参照します。

Prefixを確認する実行例: [Python](../../example/python/xpt/udp_loopback.py)、
[JavaScript](../../example/js/xpt/udp_loopback.js)、[Java](../../example/java/xpt/UdpLoopbackExample.java)。

各言語Bindingは全Errorを1つの正規化（`binding::Error`）で変換し、言語慣用の例外を投げます -
Pythonは`WseError(RuntimeError)`、Java／C#は`WseException`、JavaScriptはField付き`Error`。
いずれもC++呼出側が分岐に使うのと同じ数値の`category`／`code`／`nativeCode`を運びます。
HTTP失敗はResponseを添えたSubclassで、`HttpResult`と対応します。C ABIは`wse_capi_status`を
値で返し、Messageは`wse_capi_last_error_message()`が呼出Threadの次のC ABI呼出まで保持します。

C#で完了数が必要なSendではWseExceptionより先にTransferExceptionをCatchします。
UDP切詰め時は独立CopyしたPrefixと送信元も取得できます。Countや取消からRemote受領や自動再送の可否は判断しません。

```csharp
try
{
    using UdpDatagram packet = receiver.ReceiveFrom(3, context);
    byte[] complete = packet.Payload();
}
catch (TransferException error) when (error.Code == (int)TransportErrorCode.DatagramTruncated)
{
    byte[] prefix = error.ReceivedData!;
    Endpoint source = error.SourceEndpoint!.Value;
    // Detached managed bytes; no Dispose. The discarded suffix is gone.
}
```

[実行できるUDP例](../../example/cs/xpt/UdpLoopbackExample.cs)で確認できます。確保／受入失敗時は解放を行ってその例外を伝え、
元の操作ErrorやPayloadの回復を保証しません。[C ABI](../design/ja/CAbiContract.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
