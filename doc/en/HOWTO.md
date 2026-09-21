# Using WSE

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

Hardware-free quick starts are compiled or executed by CTest:
[C++](../../example/cpp/core/quickstart.cpp), [JavaScript](../../example/js/core/quickstart.js),
[Python](../../example/python/core/quickstart.py), and [Java](../../example/java/core/QuickStart.java).
Use [Getting Started](GettingStarted.md) for the source-to-installed-consumer path.

## Consume an installed package

Install WSE first, then link only the components required by the consuming project.

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE WSE::Core WSE::Oui)
target_compile_features(my_application PRIVATE cxx_std_17)
```

See the [Build and Install Guide](BuildGuide.md) for shared/static selection, optional components,
and installation.

## Run the portable Projection example

Configure and build an OUI preset.

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

The shared source is [`example/cpp/oui/portable_projection.cpp`](../../example/cpp/oui/portable_projection.cpp).
It initializes `Renderer` with the selected backend, creates a texture, mesh, and surface, submits a
`ProjectionPipeline::execute()` call, waits for its fence, explicitly releases resources, and calls
`shutdown()`.

See the [OUI Projection design](../design/en/OuiProjection.md) for coordinate, color, tolerance, and
ownership rules covering alpha maps, supersampling, edge blend, multi-source, and dynamic meshes.
On failure, inspect `error()` on `RendererResult` or `RendererStatus`; never treat an unsupported
operation as successful.

## Control a camera without backend-specific APIs

Link `WSE::Tmr` from a Tmr-enabled package and use `WebCamera` for ordinary USB web cameras. Consumers do not call
Media Foundation on Windows or V4L2/libcamera on Linux directly.

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
const auto frame = camera.readFrame(2000U); // owns its bytes

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

`native_format` is produced by the camera; `output_format` is returned by WSE. Set
`allow_conversion=false` to reject conversion. Before changing a control, verify `writable` and its mode,
save its current value, and restore it when finished. Use only advertised readable/writable extension-unit
selectors with a payload length defined by a known device schema; never probe unknown selectors with writes.
`close()` is terminal, so create a new owner to reopen. See the complete
[`example/cpp/tmr/webcamera.cpp`](../../example/cpp/tmr/webcamera.cpp), the
[WebCamera migration guide](WebCameraMigration.md), and the
[Tmr Camera design](../design/en/TmrCamera.md) for frame, callback, error, and ownership rules.
See the [Build and Install Guide](BuildGuide.md)
for provisioning the pinned Linux libcamera dependency.

## Use WSE from JavaScript

Build and install WSE with `WSE_BUILD_NODE_BINDING=ON`, then require the installed JavaScript
directory. The loader locates the platform addon and WSE runtime inside the same package.

```js
const wse = require('C:/wse-sdk/lang/js'); // use /opt/wse-sdk/... on Linux

async function main() {
  console.log(wse.runtimeInfo());

  const input = new Uint8Array([1, 2, 3, 4]);
  const frame = await wse.copyFrame(input);
  input[0] = 99;
  console.log(frame[0]); // 1: the returned Buffer owns an independent copy

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

Call `cancel()` or `close()` when a timer is no longer needed. Runtime cleanup also cancels and
joins outstanding callbacks, but deterministic close keeps application shutdown predictable. See
the [Language Binding design](../design/en/LanguageBindings.md) for the ABI, Promise, callback,
ownership, and error contracts.

For cameras, call `WebCamera.enumerate()` and `WebCamera.capabilities()`, choose a native/output
profile, then use `new WebCamera()`, `open()`, `start()`, `await readFrame()`, `stop()`, and `close()`.
See the complete [`example/js/tmr/webcamera.js`](../../example/js/tmr/webcamera.js).

## Use WSE from Python

Provision pybind11, then build and install WSE with `WSE_BUILD_PYTHON_BINDING=ON`. Add the installed
`lang/python` directory to `PYTHONPATH`; the package locates the native module and, on Windows, the
WSE runtime in the same installation automatically.

```python
import threading
import wse

with wse.Runtime() as runtime:
    print(runtime.info())

    source = bytearray([1, 2, 3, 4])
    frame = runtime.copy_frame(source)
    source[0] = 99
    print(frame.tobytes()[0])  # 1: FrameBuffer owns an independent copy

    worker = threading.Thread(target=lambda: runtime.wait(10))
    worker.start()
    worker.join()

try:
    wse.wait(-1)
except wse.WseError as error:
    print(error.category, error.code, error.native_code, error)
```

`FrameBuffer` exports a read-only buffer, so `memoryview(frame)` avoids another copy while keeping
the native owner alive. Blocking copy and wait work releases the GIL. Use `Runtime` as a context
manager or call `close()` explicitly; close cancels an outstanding wait. CPython subinterpreters
and free-threaded builds are not supported in this first increment. See the
[Language Binding design](../design/en/LanguageBindings.md) for the complete boundary.

Python exposes Tmr through `WebCamera` and OUI Projection through `render_projection()`.
Neither API exposes backend handles, and returned frames own their bytes.
Own a camera with `with wse.WebCamera() as camera:` and select native/output formats from the
device's `stream_profiles`. See [`example/python/tmr/webcamera.py`](../../example/python/tmr/webcamera.py).

## Use WSE from Java

Provide JDK 17 or later, then build and install with `WSE_BUILD_JAVA_BINDING=ON`. Use
`WSE_JAVA_HOME` when an explicit build JDK is required. Add the installed `lang/java/wse.jar` to the
application class path and `bin` to `java.library.path`. WSE does not package a JDK runtime.
The JAR remains Java 17 compatible. Add `--enable-native-access=ALL-UNNAMED` to the Java command
when running on Java 25.

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
        System.out.println(frame.buffer().get(0)); // 1: owned read-only native buffer
    }
} catch (WseException error) {
    System.err.println(error.category() + " " + error.code()
            + " " + error.nativeCode() + " " + error.getMessage());
}
```

`WseRuntime`, `TimerHandle`, `WebCamera`, and `NativeBuffer` implement `AutoCloseable`; use
try-with-resources or explicit `close()`. Tmr starts at `WebCamera`, and OUI starts at
`Projection.render()`, so callers never handle Media Foundation, V4L2/libcamera, or graphics
handles.
See [`example/java/tmr/WebCameraExample.java`](../../example/java/tmr/WebCameraExample.java) for enumeration,
profile selection, try-with-resources, and owned-frame handling.

## Use WSE from C#

Provide the .NET 8 SDK, then build and install with `WSE_BUILD_DOTNET_BINDING=ON`. Reference the
installed `lang/cs/WapitiStew.Wse.dll` and make the `wse_capi` native library loadable: place it
beside the application, or name the file with `WseRuntime.SetNativeLibraryPath` or the
`WSE_CAPI_LIBRARY` environment variable. WSE does not package a .NET runtime.

```csharp
using System;
using WapitiStew.Wse;

using var runtime = new WseRuntime();
Console.WriteLine(runtime.Info());

byte[] input = { 1, 2, 3, 4 };
using (FrameBuffer frame = runtime.CopyFrame(input))
{
    input[0] = 99;
    Console.WriteLine(frame.ToArray()[0]); // 1: the buffer owns an independent copy
}

using var cancellation = new CancellationSource();
cancellation.Cancel();
try
{
    runtime.Wait(TimeSpan.FromSeconds(5), cancellation);
}
catch (WseException error)
{
    // Cancellation 0 0: the wait observed the request instead of running to completion.
    Console.Error.WriteLine($"{error.Category} {error.Code} {error.NativeCode}");
}
```

`WseRuntime`, `FrameBuffer`, and `CancellationSource` implement `IDisposable`; use `using` or an
explicit `Dispose()`. The binding calls the flat C ABI in `api/wse/capi` because P/Invoke cannot bind
C++ classes, and it currently exposes the Core surface only. See
[`example/cs/core/QuickStart.cs`](../../example/cs/core/QuickStart.cs) for a hardware-free starting point.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
