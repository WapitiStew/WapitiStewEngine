# WSEの使い方

> Canonical source: [English How-to](../en/HOWTO.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

Hardwareを開かない[C++](../../example/cpp/core/quickstart.cpp)、
[JavaScript](../../example/js/core/quickstart.js)、[Python](../../example/python/core/quickstart.py)、
[Java](../../example/java/core/QuickStart.java) Quick startをCTestでCompile／実行します。SourceからInstall Consumerまでの
手順は[Getting Started](GettingStarted.md)を参照してください。

## Packageを利用する

WSEを先にInstallし、利用側の`CMakeLists.txt`では必要なComponentだけをLinkします。

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE WSE::Core WSE::Oui)
target_compile_features(my_application PRIVATE cxx_std_17)
```

Shared／Static、Component選択、Install手順は[Build／Install Guide](BuildGuide.md)を参照してください。

## Portable Projectionを実行する

OUIを有効にしたPresetをConfigure／Buildします。

Windows:

```bat
cmake --preset windows-msvc-shared-oui
cmake --build --preset windows-msvc-shared-oui-debug
build\windows-msvc-shared-oui\x64\Debug\wse.oui.backend_projection_golden.exe d3d12
```

Linux:

```sh
cmake --preset linux-gcc-shared-oui
cmake --build --preset linux-gcc-shared-oui-debug
build/linux-gcc-shared-oui/linux-x86_64/Debug/wse.oui.backend_projection_golden vulkan
```

共通Sourceは[`example/cpp/oui/portable_projection.cpp`](../../example/cpp/oui/portable_projection.cpp)である。
`Renderer`をBackend指定で初期化し、Texture、Mesh、Surfaceを作成して`ProjectionPipeline::execute()`へ
送信する。返却Fenceを待ってから結果を使用し、Surface、Mesh、Textureを明示的に破棄して`shutdown()`する。

Alpha map、Supersample、Edge blend、Multi-source、Dynamic Meshの座標、色、許容差およびOwnershipは
[OUI Projection設計](../design/ja/OuiProjection.md)を参照してください。失敗時は`RendererResult`／
`RendererStatus`の`error()`を確認し、未実装機能を成功として扱わないでください。

## Backendを意識せずCameraを制御する

Tmrを有効化したPackageへ`WSE::Tmr`をLinkし、通常のUSB Web Cameraは`WebCamera`で操作します。Windowsの
Media Foundation、LinuxのV4L2／libcameraを利用側で直接呼ぶ必要はありません。

```cpp
#include <tmr/stew.h>

using namespace wse::tmr;

const auto devices = WebCamera::enumerate();
if (!devices.succeeded() || devices.value().empty())
    return;

const auto capability = WebCamera::capabilities(devices.value().front());
if (!capability.succeeded() || capability.value().stream_profiles.empty())
    return;
const auto& profile = capability.value().stream_profiles.front();
if (profile.output_formats.empty()) return;

WebCamera camera;
const sCameraStreamConfiguration configuration{
    profile.native_format, profile.output_formats.front(), true};
if (!camera.open(devices.value().front(), configuration).succeeded())
    return;
if (!camera.start().succeeded()) return;

const auto frame = camera.readFrame(2000U); // 所有Byte列。次のFrameで無効にならない

for (const auto& control : capability.value().controls)
{
    if (control.control == eCameraControl::Exposure && control.readable)
    {
        const auto exposure = camera.getControl(control.control);
        if (!exposure.succeeded()) return;
    }
}
camera.stop();
camera.close();
```

`native_format`はCameraが出す形式、`output_format`はWSEが返す形式です。変換を許可しない場合は
`allow_conversion=false`にします。Controlを書き換える場合は`writable`とModeを確認し、変更前の値を保存して
終了時に復元してください。Extension UnitはCapabilityが広告した読書き可能Selectorだけを、既知の機器Schemaと
一致するPayload長で使用します。未知Selectorへの探索的Writeは禁止です。`close()`は終端操作なので再利用時は
新しい`WebCamera`を作成します。完全なSampleは
[`example/cpp/tmr/webcamera.cpp`](../../example/cpp/tmr/webcamera.cpp)、Migrationは
[WebCamera移行Guide](WebCameraMigration.md)を参照してください。Frame、Callback、ErrorおよびOwnershipの
契約は[Tmr Camera設計](../design/ja/TmrCamera.md)を参照してください。Linux libcameraの固定Dependency
準備は[Build／Install Guide](BuildGuide.md)を参照してください。

## JavaScriptからWSEを利用する

`WSE_BUILD_NODE_BINDING=ON`でWSEをBuild／Installし、Install Package内のJavaScript Directoryを
`require`します。Loaderは同じPackage内のPlatform AddonとWSE Runtimeを解決します。

```js
const wse = require('C:/wse-sdk/lang/js'); // Linuxでは /opt/wse-sdk/... を指定

async function main() {
  console.log(wse.runtimeInfo());

  const input = new Uint8Array([1, 2, 3, 4]);
  const frame = await wse.copyFrame(input);
  input[0] = 99;
  console.log(frame[0]); // 1: 返却Bufferは独立したCopyを所有

  await new Promise((resolve) => {
    const timer = wse.runAfter(10, () => {
      timer.close();
      resolve();
    });
  });
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
```

不要になったTimerは`cancel()`または`close()`します。Runtime cleanupも未完了CallbackをCancel／Joinしますが、
明示CloseによりApplication終了順序を決定的にできます。ABI、Promise、Callback、OwnershipおよびError契約は
[多言語Binding設計](../design/ja/LanguageBindings.md)を参照してください。

Cameraは`WebCamera.enumerate()`、`WebCamera.capabilities()`、`new WebCamera()`の順で使用します。Profileの
`nativeFormat`と`outputFormats`を選び、`open()`、`start()`、`await readFrame()`、`stop()`、`close()`を
実行してください。完全な例は[`example/js/tmr/webcamera.js`](../../example/js/tmr/webcamera.js)です。

## PythonからWSEを利用する

pybind11をBootstrapした後、`WSE_BUILD_PYTHON_BINDING=ON`でWSEをBuild／Installします。Install先の
`lang/python`を`PYTHONPATH`へ追加してください。同じInstall内のNative moduleと、WindowsではWSE Runtimeも
Packageが自動的に解決します。

```python
import threading
import wse

with wse.Runtime() as runtime:
    print(runtime.info())

    source = bytearray([1, 2, 3, 4])
    frame = runtime.copy_frame(source)
    source[0] = 99
    print(frame.tobytes()[0])  # 1: FrameBufferは独立したCopyを所有

    worker = threading.Thread(target=lambda: runtime.wait(10))
    worker.start()
    worker.join()

try:
    wse.wait(-1)
except wse.WseError as error:
    print(error.category, error.code, error.native_code, error)
```

`FrameBuffer`はRead-only Bufferを公開するため、`memoryview(frame)`はNative ownerを保持しながら追加Copyを
避けられます。NativeのCopy／Wait中はGILを解放します。`Runtime`はContext managerで使用するか、明示的に
`close()`してください。Closeは未完了WaitをCancelします。最初のIncrementではCPython Subinterpreterと
Free-threaded buildをSupportしません。完全な境界は
[多言語Binding設計](../design/ja/LanguageBindings.md)を参照してください。

Pythonでは`WebCamera`からTmrを、`render_projection()`からOUI Projectionを利用できます。
どちらもBackend handleを公開せず、返却FrameがByte列を所有します。
Cameraは`with wse.WebCamera() as camera:`で所有し、`enumerate()`で得たDeviceの
`stream_profiles`からNative／Output形式を選択します。完全な例は
[`example/python/tmr/webcamera.py`](../../example/python/tmr/webcamera.py)です。

## JavaからWSEを利用する

JDK 17以降を用意し、`WSE_BUILD_JAVA_BINDING=ON`でBuild／Installします。必要なら
`WSE_JAVA_HOME`でBuild用JDKを指定します。ApplicationはInstall済み`lang/java/wse.jar`をClass pathへ、
`bin`を`java.library.path`へ追加してください。JDK自体はWSE Packageへ同梱されません。
JARはJava 17互換です。Java 25で実行する場合は`--enable-native-access=ALL-UNNAMED`をJava commandへ
追加してください。

```java
import io.wapitistew.wse.NativeBuffer;
import io.wapitistew.wse.WseException;
import io.wapitistew.wse.WseRuntime;
import java.nio.ByteBuffer;

try (WseRuntime runtime = new WseRuntime()) {
    System.out.println(WseRuntime.info());
    ByteBuffer input = ByteBuffer.allocateDirect(4);
    input.put(new byte[] {1, 2, 3, 4}).flip();
    try (NativeBuffer frame = runtime.copyFrame(input)) {
        System.out.println(frame.buffer().get(0)); // 1: Native owner付きRead-only Buffer
    }
} catch (WseException error) {
    System.err.println(error.category() + " " + error.code()
            + " " + error.nativeCode() + " " + error.getMessage());
}
```

`WseRuntime`、`TimerHandle`、`WebCamera`および`NativeBuffer`は`AutoCloseable`なので、
try-with-resourcesまたは明示`close()`で終了します。Tmrは`WebCamera`、OUIは
`Projection.render()`を入口とし、Media Foundation／V4L2／libcameraやGraphics handleを扱いません。
Java Cameraの列挙、Profile選択、try-with-resources、所有Frameの例は
[`example/java/tmr/WebCameraExample.java`](../../example/java/tmr/WebCameraExample.java)です。

## C#からWSEを利用する

.NET 8 SDKを用意し、`WSE_BUILD_DOTNET_BINDING=ON`でBuild／Installします。Install済みの
`lang/cs/WapitiStew.Wse.dll`を参照し、`wse_capi` Native Libraryを解決可能にします。実行File隣へ
配置するか、`WseRuntime.SetNativeLibraryPath`または環境変数`WSE_CAPI_LIBRARY`でFileを明示します。
.NET RuntimeはWSEへ同梱しません。

```csharp
using System;
using WapitiStew.Wse;

using var runtime = new WseRuntime();
Console.WriteLine(runtime.Info());

byte[] input = { 1, 2, 3, 4 };
using (FrameBuffer frame = runtime.CopyFrame(input))
{
    input[0] = 99;
    Console.WriteLine(frame.ToArray()[0]); // 1: Bufferは独立したCopyを所有する
}

using var cancellation = new CancellationSource();
cancellation.Cancel();
try
{
    runtime.Wait(TimeSpan.FromSeconds(5), cancellation);
}
catch (WseException error)
{
    // Cancellation 0 0: 待機は完了せず取消要求を観測した。
    Console.Error.WriteLine($"{error.Category} {error.Code} {error.NativeCode}");
}
```

`WseRuntime`、`FrameBuffer`および`CancellationSource`は`IDisposable`です。`using`または明示的な
`Dispose()`で解放します。P/InvokeがC++ Classを解決できないため、本Bindingは`api/wse/capi`の平坦
C ABIを呼びます。現在の公開面はCoreのみです。実機不要の出発点は
[`example/cs/core/QuickStart.cs`](../../example/cs/core/QuickStart.cs)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
