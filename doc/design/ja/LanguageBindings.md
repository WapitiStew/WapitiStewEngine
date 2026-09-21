# WSE多言語Binding設計

> Canonical source: [English Language Binding Design](../en/LanguageBindings.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書はPython、JavaScript、JavaおよびC# Bindingが共有するNative facadeの正式設計です。Facadeは
`WSE::Core`の一部とし、各言語AdapterはOptionalです。pybind11によるPython、Node-APIによるJavaScript、
JNIによるJava、P/InvokeによるC# Adapterがこの契約を共有し、Platform Backendを直接Wrapしません。

C++の公開入口は`<wse/binding/stew.h>`、Pythonの入口はInstall Package内の`lang/python`、JavaScriptの入口は
`lang/js`、Javaの入口は`lang/java/wse.jar`、C#の入口は`lang/cs/WapitiStew.Wse.dll`です。
OS Native handle、Graphics handle、Backend objectおよび所有Raw pointerを
通常の公開APIへ露出しません。

## Native facade

`wse::binding::Runtime`は次の小さい安定境界を提供します。

- `info()`はWSE Version、Binding ABI VersionおよびBuild済みXPT／Tmr／OUI Capabilityを返します。
- `copyFrame()`は呼出元Byte列を不変かつ参照Count付きの`FrameBuffer`へCopyします。
- `wait()`は有限時間だけ待機し、`CancellationSource`が所有するCopy可能な
  `CancellationToken`を観測します。
- `Error`は安定Category、安定Operation code、診断Messageおよび任意のNative codeを持ちます。
  分岐にはCategory／Codeを使い、Message／Native codeへ依存しません。
- `fromXptError()`、`fromOuiError()`および`fromTmrError()`はComponentの安定Codeを保持しながら
  共通Binding Categoryへ変換します。言語AdapterはCategory mappingを複製しません。

`BINDING_ABI_VERSION`は1から開始します。既存Representationを変えないOperation追加ではABIを
更新しません。Field、数値Error categoryまたはOwnership規則の削除／再解釈には新しいABI Versionと
Migration経路が必要です。

共通Categoryは`None`、`InvalidArgument`、`NotFound`、`InvalidState`、`InputOutput`、`Timeout`、
`Cancellation`、`Protocol`、`Security`、`Unsupported`、`ResourceExhausted`および`Internal`です。
各言語Adapterは言語標準のExceptionへ変換しつつ、Cross-language Testと診断用に数値`category`、
`code`、`nativeCode`を保持します。

## Ownership、Asyncおよび終了処理

Native Adapterは[Result／Status契約](ResultContract.md)に従い、通常のResultを`success()`／`failure()`で
構築し、失敗時だけ`error()`を読みます。XPT転送は`TransferResult<T>`、HTTPは`HttpResult`を保持し、
通常の`TransportResult<T>`へ変換しません。PythonはGIL解放Scope内でNative結果を取得し、Python値の
生成やException送出の前にGILを再取得します。

入力BufferはAsync処理開始前にCopyします。出力BufferはByte列を所有し、呼出元Memory、Camera buffer、
Renderer bufferを参照しません。将来Zero-copyを追加する場合は明示的Lifetime ownerを持つ別APIとし、
現在のCopy契約を変更しません。

実行ThreadとCloseの完了境界は操作ごとに異なります。Node CoreのCopy／Wait／TimerはNative workerとRuntime
Dispatchを使いますが、XPT Wrapperは同期です。PythonのGIL解放も呼出しを非同期にはしません。取消要求は
Joinや既にQueueされたCallbackの除去を必ずしも意味しません。以下の処理順序と[Thread Ownership](ThreadOwnership.md)を
参照し、全Runtimeに共通の待機境界を推測しません。

## Node-API Adapter

JavaScript Adapterは`NAPI_VERSION=8`のC Node-APIだけを使い、V8 C++型へLinkしません。Packageは
Node.js 18以降を要求し、次を公開します。

| JavaScript API | 契約 |
| --- | --- |
| `runtimeInfo()` | Version、Binding ABI、Build済みComponentを同期取得 |
| `copyFrame(Buffer or ArrayBufferView)` | 独立した`Buffer` CopyをPromiseで返す |
| `wait(milliseconds)` | 0以上の有限待機をPromiseで返す |
| `runAfter(milliseconds, callback)` | Thread-safe Callbackと`cancel()`／`close()` Handle |

無効Argumentは同期的にThrowします。Native Async failureはPortable Error property付きExceptionでPromiseを
Rejectします。`cancel()`は複数回呼出し可能です。`close()`は未送信CallbackをCancelして所有Timer threadを
Joinし、Callback実行後も安全に呼べます。利用側がHandleをCloseしなかった場合もAddon cleanupが同じWorker回収を
実行します。任意のApplication処理の実行時間を制限する保証ではありません。

AddonはContext-awareであり、Node Workerごとに独立したInstance stateを持ちます。TestではBuffer分離、
Error値、Callback cancellation、Workerの繰返しLoad／Unloadおよび長時間Timerを残したWorker終了を検証します。

## Python Adapter

Python Adapterはpybind11 3.1.0とCPython 3.11～3.14を対象とし、次を公開します。

Tmrを有効にすると、`import wse`から`ImageOrientation`、`BayerPattern`、`DemosaicMethod`、
`CameraFrameAccumulator`、`is_frame_operation_supported`、`is_frame_averaging_supported`、
`is_bayer_format`、`bayer_pattern_of`、`apply_orientation`、`demosaic_frame`を利用できます。
撮像と同じ所有された`CameraFrame`を処理し、内部Native ModuleのImportは不要です。
型定義にはFrame Constructor、16bit／Bayer Format、Frame rate Controlも含みます。
Python Tmr契約TestはこのPackage入口を検証します。

| Python API | 契約 |
| --- | --- |
| `runtime_info()` | Version、Binding ABI、Build済みComponentを`dict`で同期取得 |
| `copy_frame(buffer)` | Contiguous buffer protocol入力から独立した読み取り専用`FrameBuffer`を返す |
| `wait(milliseconds)` | 0以上24時間以下の有限待機。Native待機中はGILを解放する |
| `Runtime` | `with`／`close()`に対応し、Close時に同Instanceの未完了`wait()`をCancelする |

`FrameBuffer`は`bytes()`、`tobytes()`、`len()`および読み取り専用buffer protocolを提供し、入力Memoryを
参照しません。`WseError`は`RuntimeError`を継承し、数値`category`、`code`、`native_code`を保持します。
`close()`は冪等です。Close済み`Runtime`の操作は`InvalidState`、別ThreadからCloseされた待機は
`Cancellation`になります。Python objectへ触れないNative区間だけGILを解放し、ブロックする区間は
すべて解放します。Copy／wait、Camera取得、Projection描画、およびDeadlineまで待つXPTの各操作が
対象です。したがって、あるThreadがHTTP通信やSocket読み取り、Serial読み取りで待っている間も
他のPython Threadは動き続け、同一Process内のResponderが同一Processの発した要求へ応答できます。

Python AdapterはMain interpreterとProcess単位のLoad／Unloadを対象とします。
Subinterpreterとfree-threaded CPythonはSupport対象外であり、これらの構成に必要な
Interpreter-local state管理と検証Matrixは提供しません。

## Java Adapter

Java AdapterはJNIと`--release 17`でBuildします。`WseRuntime`、`TimerHandle`、`NativeBuffer`、
`WebCamera`およびProjection出力の所有Objectは`AutoCloseable`であり、`Cleaner`はClose漏れに対する
Safety netだけに使います。`WseException`は数値`category`、`code`、`nativeCode`を保持します。

JNIは`JNIEnv`をThread間で共有しません。Native callback threadはJVMへDaemon attachし、終了前にdetachします。
入力`ByteBuffer`はNative ownershipへCopyし、出力`NativeBuffer`はRead-only DirectByteBufferと明示的Native ownerを
持ちます。Javaの公開APIはNative pointerやOS／Graphics handleを返しません。

## C# Adapterと平坦C ABI

P/InvokeはC++ Classを解決できないため、C# Adapterは`wse::binding`を直接呼ばず平坦C ABIを呼びます。
C言語の公開入口は`api/wse/capi`配下の`<wse/capi/stew.h>`で、実装は`lang/cs/native`に置き、同じ
`wse::binding` facadeへ委譲します。したがってC ABIは意味論の第2の正本を作りません。

C ABIは次の点を正式契約とします。

- Handleは不透明Struct Pointer（`wse_capi_runtime`、`wse_capi_frame_buffer`、
  `wse_capi_cancellation`）です。Raw `void *`もOS Native handleも境界を越えません。
- 失敗し得る操作は`wse_capi_status`を返し、その`category`は`wse::binding::eErrorCategory`と同値です。
  失敗MessageはThread単位に保持し、`wse_capi_last_error_message()`で読みます。
- Status操作は共通の例外Guardを使用します。純粋Getter／Destructorは別の返却型を持ち、Error構築中の
  確保を避けるため診断は1024 Byte固定TLSへ保存します。切詰めと注入範囲はC ABI設計を参照します。
- HTTP 4xx／5xxは失敗とともに所有Responseを返し、その経路でも利用側が破棄します。転送ではErrorと
  進捗Countが共存します。その他の失敗も出力Slotを常に上書きするわけではなく、HandleをNULL初期化して
  各操作の出力規則に従います。
- Callbackの呼出元Tokenは`wse_capi_user_data`（`intptr_t`）で受け渡し、Library側は中身を読みません。
  平坦C ABIにRaw `void *`は1箇所も無く、Callbackの追加でも増やしません。
- 文字列とBufferの取得は2回呼出方式です。格納先へ`NULL`を渡すと必要Byte数を返します。
- 引数はC++ APIと同じ規約に従います。不透明Handleは`this`のC表記として第1引数に固定し、その呼出が
  対象を変更するかどうかを接尾辞で示します。続いて出力引数を`p_<name>_out`として並べ、入力引数を
  `<name>_in`として最後に置きます。呼出側が用意する格納先Bufferは出力引数であり、2回呼出方式は
  `( handle, char* p_buffer_out, size_t* p_size_out, size_t capacity_in )`という並びになります。
- `WSE_CAPI_ABI_VERSION`は平坦C ABI自体の版です。`info()`が返すBinding ABI版とは独立した番号で、
  現在は1です。呼出側は`wse_capi_abi_version()`で実際にLoadしたLibraryの版を読み、構築時の
  `WSE_CAPI_ABI_VERSION`と一致することを確認します。関数の追加は互換であり版を変えません。構造体の
  拡張と引数の変更は非互換であり版を上げます。
- C ABIのExport定義は`WSE_STATIC`と独立に判定します。P/Invokeは共有Libraryしか解決できないため
  Shimは常に共有Libraryとして生成し、Coreを静的Linkした構成でもExportを抑止しません。

Managed側の`WseRuntime`、`FrameBuffer`および`CancellationSource`は単一の`SafeHandle`でHandleを所有し、
`IDisposable`を実装します。`Dispose()`は冪等で、解放後の使用は`ObjectDisposedException`とします。
`WseException`は数値の`Category`、`Code`および`NativeCode`を保持します。Native Libraryは明示Path、
環境変数`WSE_CAPI_LIBRARY`、既定探索順の順に解決します。Opaque入力はSafeHandle marshalling、
省略可能なCancellationは明示Leaseを使います。Native解放は最後の参照まで延期しますが、
無関係な呼出しの取消／Joinは行いません。可変操作の呼出元制約は[C ABI寿命規則](CAbiContract.md)に従います。

C# AdapterはCore、XPT、IUI、OUIおよびTmrを公開し、他の3 Bindingと同等です。

## Tmr／OUIの高水準境界

4言語すべてで公開Camera ownerを`WebCamera`へ統一し、列挙、Capability、Open、Start／Stop、Frame、
Control、Extension UnitおよびCloseを提供します。C++の低水準`CameraSession`はOS Backendを隠す基盤で
あり、多言語PackageのCamera ownerではありません。
利用側はMedia Foundation、V4L2またはlibcameraを直接扱いません。CallbackとFrameはBackend lifetimeから独立し、
各RuntimeのThread／Ownership規則に従います。

CameraのCloseはDevice Sessionを終えるだけで、Objectを退役させません。同じInstanceを再びOpenでき、
SampleはこれでStream解像度を切り替えます。終端的な解放も必要な言語では、両者を別の呼出に分けます。
C#は`Close`と`Dispose`、Javaは`closeCamera()`と`close()`、Pythonは`close()`と`release()`、
JavaScriptは`close()`とObjectの回収です。KeyboardはCoreがOpen／Closeの対を持たないため分けません。

| 契約 | JavaScript | Python | Java | C# |
| --- | --- | --- | --- | --- |
| Owner | `new WebCamera()` | `WebCamera()`／`with` | `new WebCamera()`／try-with-resources | `new WebCamera()`／`using` |
| Device/Profile | plain owned object | pybind owned value | immutable record | Dispose可能な`CameraDevice`／`CameraStreamProfile` record |
| Frame bytes | `Buffer` copy | `FrameBuffer` | `NativeBuffer` | 所有`CameraFrame`、`ToArray()`でByte列をCopy |
| XU payload | `Buffer` copy | `bytes` copy | `byte[]` clone | `byte[]` copy |
| Close | 冪等、Session終了で再Open可 | 冪等、Session終了で再Open可 | 冪等、Session終了で再Open可 | 冪等、Session終了で再Open可 |

OUIはC++の`renderProjection()`を共通境界にし、各言語からRGBA Layer、Alpha、Mesh、Sampling、Opacity、
Edge blend、出力SizeおよびBackend policyを渡します。Renderer、Surface、Texture、Mesh、FenceおよびReadbackは
内部所有とし、返却するpacked RGBA8 Frameだけを言語側が所有します。

要求では描画するAdapterを指名でき、結果は実際に描いたAdapter名を報告します。存在しないAdapterを
指名した場合は、別のAdapterで黙って描かずに失敗します。Software Adapterの要求と同時に指名した場合も、
別々の要求であるため拒否します。

この境界が運ばないのはRenderer自身です。`Renderer`とそのTexture、したがって`eRendererPixelFormat`は
C++限定であり、`Rgba16Unorm`と`Rgba16Float`にBinding面はありません。Projectionは常にpacked RGBA8へ
描いて返すためです。より広い形式を扱うにはC++から`Renderer`を使います。Binding へ出すには、
Projectionの返却契約を変えるか、GPU Resourceの所有を4つの言語Runtimeへ渡すかのどちらかになります。

## IUI Keyboardの境界

4言語すべてが`Keyboard`を唯一の公開Keyboard ownerとします。生成時に監視Threadを開始し、所有者は
決定的に解放します。JavaScriptは`close()`、Pythonは Context managerまたは`close()`、Javaは
try-with-resourcesまたは`close()`、C#は`using`または`Dispose()`です。Closeは冪等かつ終端です。

BindingはCallbackではなくPollingを公開します。`snapshot()`は5つのKey groupを同一更新時点で返すため、
呼出側が独立した複数回の読取を継ぎ合わせることはありません。`accessState()`は常に成功し、`Starting`、
`Ready`、`Unavailable`、`PermissionDenied`または`Disconnected`を返します。`snapshot()`と押下ASCII取得は
そのStateから導いたStructured Errorで失敗するため、読取不能なKeyboardを空Snapshotとして報告しません。
この写像は`fromIuiKeyboardState()`が所有し、言語Adapterは重複実装しません。

| 契約 | JavaScript | Python | Java | C# |
| --- | --- | --- | --- | --- |
| Owner | `new Keyboard()` | `Keyboard()`／`with` | `new Keyboard()` | `new Keyboard()` |
| Snapshot | 所有Object | `list[bool]`の`dict` | `KeyboardState` record | `KeyboardState` |
| 読取可否 | `accessState()` | `access_state` | `accessState()` | `AccessState` |
| Close | 冪等かつ終端 | 冪等かつ終端 | 冪等かつ終端 | 冪等かつ終端 |

## XPT Transportの境界

4言語すべてが`TcpClient`、`UdpClient`、`SerialPort`およびHTTP実行入口を公開します。XPTはDefault
Timeoutを持たないため、全Operationが明示DeadlineとOptionalなCancellationを受け取り、呼出元が
Deadline観測には[XPT](XptTransport.md)のResolver／OS呼出しの制約があり、実時間の上限保証ではありません。
StatusはXPTの安定Codeを保持し、Categoryは`fromXptError()`で
正規化します。

Bindingは同期です。呼出は完了またはDeadline／取消を観測するまで呼出Threadを塞ぎます。JavaScriptではMain thread上で
Deadlineを短く保つか、Workerで実行します。BindingはRetryを代行しません。HTTP Requestは非idempotent
として扱い、一時失敗のRetryは行いません。再実行の安全性は呼出元が判断します。

4xx／5xxは、値を伴う失敗として報告します。通信は完了しServerが応答しているためです。各Bindingは
構造化Errorを送出し、そこへResponseを添えます。C#は`HttpStatusException.Response`、Javaは
`HttpStatusException.response()`、Pythonは`WseHttpStatusError.response`、JavaScriptは
`error.response`です。いずれも通常のError型を継承するか同一であるため、既存のcatchはそのまま
機能します。それ以外の失敗ではResponseは存在せず、当該Propertyも現れません。

公開するのは正準のTransport APIのみです。

## Build、Packageおよび検証境界

`WSE_BUILD_NODE_BINDING`、`WSE_BUILD_PYTHON_BINDING`、`WSE_BUILD_JAVA_BINDING`および
`WSE_BUILD_DOTNET_BINDING`の既定値は`OFF`です。Nodeを有効化する前に、固定Node 24.19.0 Header archiveとMIT
Licenseを`bootstrap.py --package node-api-headers`で準備します。CMake ConfigureはDownloadしません。
Addon targetは`WSE::Node`、出力名は`wse.node`です。

Pythonを有効化する前に、固定pybind11 3.1.0 Source archiveとBSD-3-Clause Licenseを
`bootstrap.py --package pybind11`で準備し、CPython 3.11以降のInterpreter／Development.Moduleを用意します。
Targetは`WSE::Python`、Module名は`_wse`です。Cross buildでは対象CPython Headerを
`WSE_PYTHON_TARGET_INCLUDE_DIR`、対象ABI suffixを`WSE_PYTHON_EXTENSION_SUFFIX`で明示します。

JavaはJDK 17以降を要求し、`WSE_JAVA_HOME`でBuild用JDKを明示できます。Test時は
`WSE_JAVA_17_EXECUTABLE`、`WSE_JAVA_21_EXECUTABLE`および`WSE_JAVA_25_EXECUTABLE`を指定すると、
同じJava 17互換Classを3 Runtimeで検証します。Targetは`WSE::Java`、Native moduleは
`wse_jni`、ArchiveはJava 17互換の`wse.jar`です。Cross buildでは`WSE_JNI_TARGET_INCLUDE_DIRS`へ対象JNI Headerを
指定します。Java 25実行時は`--enable-native-access=ALL-UNNAMED`を指定します。WSEはJDK RuntimeをVendorまたは
Packageへ同梱しません。

Install Packageは`wse.node`とWSE Runtime libraryを`bin`、JavaScript／TypeScript入口を
`lang/js`、Node License／Identityを`vendor/node-api`へ置きます。JavaScript loaderはCurrent working
directoryやPlatform固有Pathへ依存せずInstall配置を解決します。`WSE_NODE_ADDON`はTest／開発用の明示Overrideです。
Python Module／型情報は`lang/python/wse`、pybind11 License／Identityは`vendor/pybind11`へ置きます。
Windows loaderはPackage内`bin`をDLL検索へ追加し、Linux Shared Moduleは`$ORIGIN/../../../lib`でWSEを解決します。
Javaは`wse_jni`を`bin`、`wse.jar`を`lang/java`へInstallします。

C#はBuild時に.NET 8 SDKを要求し、`WSE_DOTNET_EXECUTABLE`で明示できます。Native targetは
`WSE::Dotnet`（出力名`wse_capi`）、Managed Assemblyは`net8.0`の`WapitiStew.Wse.dll`です。
Native Libraryを`bin`、Managed AssemblyとXML Documentationを`lang/cs`へInstallします。
WSEは.NET RuntimeをVendorまたはPackageへ同梱しません。

自動MatrixはWindows x86-64／Linux x86-64 Shared・Static Runtime、JavaScript／Python／Java／C#同時選択、
Tmr／OUI Contract、Install後Package load、単一FixtureによるC++／JavaScript／Python／Javaの全64 byte＋FNV-1a hash比較、
Lifecycle／Cancellation／bounded-memory Stress、Java 17／21／25およびLinux ARM64 Shared・Static Cross buildを
含みます。ARM64 Runtime実行はRaspberry Piの別Gateであり、Cross build成功から実機対応を推測しません。

C#にはFrame処理とDisposeを含む独自のRuntime／Component Contractがあります。OUI Contractでは
出力寸法、Row pitch、64 byteのFrame長、Adapter情報および基準Pixelを検査します。
上記の全Frame FNV-1a比較は実施しません。

Runtime回帰Testは未OpenのCameraへのCallback付きStartの反復失敗、およびPython／C#のHTTP
200／404／503応答を通常・認証付きの両入口で検証します。Pythonの応答側は同じProcess内の別Threadで
動作させ、HTTP処理中のGIL解放を確認します。Error応答でもStatus、Body、Headerを保持し、
Adapterは通信をRetryしません。

## 実装とData経路: BIND-STRUCT-01

| 層 | Source | 責務 |
| --- | --- | --- |
| 共通Core境界 | [Runtime.cpp](../../../core/wse/binding/Runtime.cpp)、[FrameBuffer.cpp](../../../core/wse/binding/FrameBuffer.cpp)、[Cancellation.cpp](../../../core/wse/binding/Cancellation.cpp) | Runtime metadata、Copyした不変Byte、共有Atomic取消Flag |
| JavaScript | [addon.cpp](../../../lang/js/native/addon.cpp)、[Package loader](../../../lang/js/index.js) | EnvironmentごとのCore操作Registry、個別Tmr／OUI／IUI／XPT adapter、Node-API変換 |
| Python | [module.cpp](../../../lang/python/native/module.cpp)、[Package入口](../../../lang/python/wse/__init__.py) | pybind型、GIL scope、Python所有Wrapper、例外変換 |
| Java | [jni.cpp](../../../lang/java/native/jni.cpp)、[WseRuntime.java](../../../lang/java/src/main/java/io/wapitistew/wse/WseRuntime.java) | 非公開Native holder、JNI参照、AutoCloseable所有 |
| C# | [NativeMethods.cs](../../../lang/cs/Wse/NativeMethods.cs)、[C ABI設計](CAbiContract.md) | Sequential marshalling、共有Native shim、SafeHandle解放 |

```text
language API -> runtime adapter -> public Core / XPT / IUI / Tmr / OUI facade
                                      -> platform adapter where needed
C# API -> P/Invoke -> C ABI guard/holders -> same public facades
native result -> shared category mapping -> language value / structured exception
```

Runtimeは全DeviceのManagerではありません。Core操作を持ち、Camera／Keyboard／Transport／Projection adapterは
それぞれの公開Facadeを呼びます。Feature flagはLibraryのBuild内容を示し、全Core／GEF型への一対一の言語Wrapperの
存在を示すものではありません。

### Buffer所有: BIND-BUFFER-02

FrameBufferはconst byte vectorへのShared pointerを保持します。値Copyは不変Storageを共有し、呼出元Byteからの
構築はCopyします。このByte容器にStride／Formatはなく、CameraFrameやProjectionの記述が意味を補います。
入力`[00,7f,80,ff]`は入力変更と生成元Runtime破棄の後も維持します。

| 境界 | 入力Copy | 出力と解放 |
| --- | --- | --- |
| Node copyFrame | Async workをQueueする前にBuffer／ViewをCopy | 独立Node Buffer。GCがStorageを所有 |
| Python copy_frame | GIL保持で連続Bufferを取得・Copyし入力Viewを解放。Native copy中はGIL解放 | Read-only FrameBuffer／Buffer protocol exportがPython ownerを保持 |
| Java copyFrame | DuplicateしたByteBufferのRemaining領域をDirect入力へCopyし、Native FrameBufferへ再Copy | NativeBufferが所有。返却Read-only ByteBufferはNativeBufferがOpenの間だけ有効な借用View |
| C# CopyFrame | ArrayをC ABI経由でFrameBufferへCopy | SafeHandleがNative byteを所有。ToArrayは別のManaged copy |

Javaの`NativeBuffer.buffer()`は新しい所有領域を作りません。CloseでNative memoryを解放しても取得済みViewは
取り消されません。全Viewの使用中はNativeBufferへの強い参照とOpen状態を保ち、Close後も必要なら先にJava所有Arrayへ
Copyします。Read-onlyは変更を防ぐだけでUse-after-closeを防ぎません。Buffer ownerのClose／Cleaner／Disposeは終端です。

### Core処理と終了: BIND-ASYNC-03

| 操作 | Native実行／完了 | 停止境界 |
| --- | --- | --- |
| Node copyFrame | AsyncOperationが入力を所有、napi_async_work実行、完了時にBuffer生成とPromise解決／拒否 | Environment registryがCleanupへ参加。入力を借用しない |
| Node wait | PromiseOperationが取消とWorkerを所有。Thread-safe functionで完了通知 | CleanupがCancel／Join。Atomic exchangeでRuntime参照の二重解放を防止 |
| Node runAfter | CallbackOperation workerが待機しThread-safe functionへQueue | cancelはFlag設定、closeはCancel／Join／Dispatch abort。Cancel時点でQueue済みCallbackは実行され得る。Dispatch側に再検査はない |
| Python Runtime.wait | State mutex内で共有取消Tokenを取得、GIL解放してWait、再取得して返却／例外 | closeはMutex内でClosed／Cancel設定。Wait中の呼出ThreadをJoinしない |
| Java wait／timer | JNI holderと取消、JVM attachとGlobal referenceでCallback | 明示Close、Native worker回収、Handleの終端クリア。Lifecycle変更を競合させない |
| C# Wait | 呼出Threadで同期P/Invoke、任意Cancellation holder | Cancelは中断要求。DisposeはHandle解放で、同時Waitの一般的な完了境界ではない |

Native binding waitは単調Clock deadlineを使い、最大5 ms単位でSleepし、各区間前と最後に取消を確認します。
負時間は不正で、Python／Nodeは24時間の上限も持ちます。Sleep区間はリアルタイム保証ではなく、C ABIには同じ上限が
ありません。全Closeが全外部Threadの処理をCancel／待機すると仮定せず、Native owner破棄を直列化します。

### Camera callbackとProjection: BIND-DEVICE-04

CameraのSession終了とOwner終端解放は前掲表に従います。Runtime境界を越えるFrameは分離されたByteを所有します。
CallbackのDispatchと例外処理はAdapterごとです。PythonはCamera worker上でGILを取得し、CameraFrame／Errorを
作って登録Callableを呼びます。Python callback例外はUnraisableとして報告・消費するため、C++ Camera loopへ
伝播せず、Nativeの「CallbackがThrowしたら配信停止」とは異なります。Stop／CloseはJoin中のGILを解放し、
GILを必要とするCallbackが終了できるようにします。Managed callback内のOwner破棄は非対応です。
外部Ownerを保持し、その側から停止します。

C callbackはFrame handleを移譲し、C#は所有CameraFrameへWrapして登録中のDelegateを保持します。
C#のReverse P/Invoke BridgeはManaged受入／Application例外を捕捉し、最初の例外を
`WebCamera.CallbackException`へ記録して後続配信を抑止し、Stopを要求します。Owner側Stop／Join後に確認します。
Close／Dispose後も読め、Start成功で消去、Start失敗では以前の値を復元します。Applicationへ渡したFrameはThrow後も
利用側が所有するため、保持する意図がなければusingで解放します。Stop要求の失敗でも配信は再開しません。
CLRの致命的障害やHeap全体の回復保証ではありません。部分生成の巻戻しとNative handle確保失敗は
[CABI-ADOPT-06](CAbiContract.md)を参照します。JNIはJava callbackの例外を報告・Clearするため、この例外でもNativeの
Throw時配信停止方針は発動しません。
JNIはGlobal referenceを保持してWorkerをAttachし、NodeはCopyしたEventをEnvironmentへ配送します。
Native callback完了とManaged queue配信は別境界で、DeviceなしTestだけでは全Close／Queue競合を認証しません。
Status／TLS／Token寿命は[C ABI](CAbiContract.md)で定義します。

Projection adapterはLayer byte／Vertex／Indexを公開RequestへCopyし、`renderProjection`を呼び、所有Packed RGBA8と
Adapter metadataを返します。Native Renderer／Resourceはその操作内だけで所有します。Scale 2の丸めと部分失敗は
[Projection](OuiProjection.md)に従います。XPT WrapperはStatus errorのHTTP Responseを保ち、自動Retryしません。
Deadlineの限界も[XPT](XptTransport.md)に従い、OS呼出し全体の絶対時間上限へ強めません。

### Raw Transport失敗時の値: BIND-TRANSFER-05

TCP／Serialの`send`とUDPの`sendTo`は、返されたTransportResultが失敗してもNative完了Byte数を保持します。
UDPの`receiveFrom`は`DatagramTruncated`の場合だけ受信済みByteと送信元を保持します。
転送Errorは各言語の基底例外型でCatchできます。

- Pythonは`WseTransferError(WseError)`に`bytes_transferred`、`received_data`、`source_endpoint`を持たせます。
  切詰め時の後二者は独立したByte列とhost／portのTuple、Send失敗時はNoneです。
  XPT有効時はPython Packageと型Stubから派生例外を公開します。
- JavaScriptは既存の`Error`と数値category／code／nativeCodeを維持します。TypeScriptの`TransferError` Interfaceが
  `bytesTransferred`を定義し、切詰め時は所有`receivedData` Bufferと`sourceEndpoint`も取得できます。
  Send失敗では後二者のPropertyはありません。`TransferError`は型Interfaceで、Runtime Constructorではありません。
  Countは成功Sendと同じJavaScript number表現で、より広い整数精度を保証しません。
- Javaは`TransferException extends WseException`に`bytesTransferred()`、`receivedData()`、`sourceEndpoint()`を
  定義します。PrefixはJava所有で、Accessorは防御的Copyを返します。Send失敗のData／Sourceはnullです。
  例外にClose責任はありません。
- C#の既存`TransferException`は`BytesTransferred`、`ReceivedData`、`SourceEndpoint`を保持します。
  C入口の互換性とTLS規則は[CABI-TRANSFER-07](CAbiContract.md)を参照します。

例えば`[00,7f,ff,01,02]`を容量3で受信すると、Count 3、Prefix `[00,7f,ff]`、送信元を持つDatagramTruncatedです。
Suffixは破棄され、後の受信やSocket Closeで保持済みPrefixは変わりません。PacketなしのTimeout／取消は通常の基底Errorで、
Dataを作りません。現TCP／Serial受信は成功時だけ完了Chunkを返し、Bindingは失敗Prefixを生成しません。
失敗SendのCountはローカル進捗でRemote受領の証明ではなく、取消や部分進捗から自動再送を判断しません。

内部の[変換View](../../../lang/common/transfer_failure.h)は例外生成中だけNative Resultを借ります。
各AdapterはNative Result破棄前にRuntime所有へCopyします。PythonはGIL下で生成します。
JNIはLocal参照をScopeで解放し、Java例外がPendingなら処理を止めます。Nodeも値／Property生成の失敗後は止め、
PendingのVM例外を保持します。JNI／NodeのTransfer例外生成はC++例外を境界外へ出しません。
[JNI Local Frame](https://docs.oracle.com/en/java/javase/17/docs/specs/jni/functions.html#pushlocalframe)と
[Node-API例外](https://nodejs.org/api/n-api.html#exceptions)を参照します。
確保／変換の失敗はTransport Errorを置き換え、Prefixを回復できない場合があります。呼出し前検査、破棄済みHandle、
Resultを返す前に投げるNative処理、致命的Runtime枯渇、他のBinding入口全体はこの境界の保証外です。

`wse.binding.transfer_failure_contract`は15件のNative人工変換を検証します。非零Send進捗とTimeout／取消／I/O失敗の
共存、および他の受信ErrorにDataを付けない分岐を含みます。`wse.binding.python_xpt`、`wse.binding.node_xpt`、
`wse.binding.java_contract`は実Loopback切詰め、送信元／Prefix保持、後続Datagram、Close、基底Catch互換性、
Count 0の送信失敗を検証します。size_t最大値の人工ケースはNativeだけで、Managed整数上限を証明しません。
全Heap故障注入、Serial実機、全OS／Runtime経路での非零Send失敗の再現を認証するTestではありません。
Python XPT contractはModule path引数なしでも実行でき、Install済みのwse PackageをImportします。
例外のModule識別と、Socket Close後のpickle往復を確認します。

## 詳細な根拠対応

| 契約 | Gate／Source | 限界 |
| --- | --- | --- |
| BIND-STRUCT-01 / BIND-BUFFER-02 | `wse.binding.facade_contract`、言語Core contract、`wse.capi.runtime_contract` | 独立Copy vector。Native owner Close後のJava viewは安全ではない |
| BIND-ASYNC-03 | Node lifecycle／stress、Python lifecycle／stress、Java contract／stress | Runtime固有の事例。Queue後取消の競合や任意User処理の共通上限ではない |
| BIND-DEVICE-04 | 言語Tmr／OUI／IUI／XPT contractと共通Projection／Frame fixture | 有効Componentのみ。実Camera callback認証ではない |
| BIND-TRANSFER-05 | Native transfer-failure contractとPython／Node XPT／Java contract | 非零Send失敗は人工検証、UDP切詰めは実Loopback。全Heap故障の認証ではない |
| C ABI Layout／出力 | [C ABI設計](CAbiContract.md) | Header snapshotとLoadしたCore shimのTestは別根拠 |

再現は4 Byte CopyとCの二回呼出しから始め、入力、返却Owner、Viewの寿命を別々に説明します。
Componentの有効化だけでは操作がBindingされた証明になりません。Test登録は[WseTests.cmake](../../../cmake/WseTests.cmake)です。

Managed所有権Fixtureは53巻戻し注入とNative worker Callback 8ケース、Native配信Helperは5ケースを追加します。
範囲と再現方法は[C ABIの根拠](CAbiContract.md)を参照します。

## 拡張の組み込み

Optionalな拡張所有のCamera Adapterは、Compile時のFragmentを通じて既存のFrame・能力・
Error変換を再利用できます。Overlayがない場合、このHookは無効です。登録、Managed入口、
Native Export、Test、文書はOverlay側で保持し、公開専用BuildではInstallしません。
統制対象Componentの1つを選んでも、別ComponentのManaged Classを暗黙に含めません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
