# WSE Samples

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

Samples are organized as `example/<language>/<component>/`. Every sample is hardware-free unless
its own header says otherwise: a machine without the physical device reports that through a
readiness state or a structured error instead of failing.

## Layout

| Directory | Contents |
| --- | --- |
| `example/<language>/core/` | Runtime information, owned frame buffers, waits, cancellation |
| `example/<language>/xpt/` | Portable transport with explicit deadlines |
| `example/<language>/tmr/` | Camera enumeration, profile selection, one owned frame |
| `example/<language>/iui/` | Keyboard readiness and one consistent key snapshot |
| `example/<language>/oui/` | Handle-free projection rendering |
| `example/offline_consumer/` | External CMake consumer of an installed package |

## Coverage

Every component has a sample in every language, and the transport and camera sets match the C++
set sample for sample.

| Sample | C++ | C# | Java | JavaScript | Python |
| --- | --- | --- | --- | --- | --- |
| Core quick start | yes | yes | yes | yes | yes |
| XPT UDP loopback | yes | yes | yes | yes | yes |
| XPT TCP client | yes | yes | yes | yes | yes |
| XPT serial send and receive | yes | yes | yes | yes | yes |
| XPT HTTP GET | yes | yes | yes | yes | yes |
| Tmr one frame | yes | yes | yes | yes | yes |
| Tmr exposure change | yes | yes | yes | yes | yes |
| Tmr resolution change | yes | yes | yes | yes | yes |
| Tmr ten-frame stream | yes | yes | yes | yes | yes |
| IUI keyboard snapshot | yes | yes | yes | yes | yes |
| OUI projection render | yes | yes | yes | yes | yes |

The C# binding reaches the native facade through the flat C ABI in `api/wse/capi`; the other three
language bindings use their own idiomatic native layer. Every binding exposes the current API only:
the deprecated camera methods, the callback-based legacy serial connector, and the legacy
projector-control surface are not bound.

Access-controlled components are additionally gated by their build option and are present only in
builds that enable them.

## Additional C++ samples

These stay C++ only because the language bindings do not reach what they show: the Core data
classes and the log facility are not bound, and neither is the windowing renderer. Everything the
bindings do reach now has a sample in every language, listed in Coverage above. The C++ sample set
covers one usage per file. Every C++
sample reports through the WSE stream logger (`wse::registDefaultLog()` followed by
`wse::WLog() << ...`) rather than `std::cout`; only the logging sample additionally shows the
structured `wse::writeLog()` layer. Data-class samples prefer the `float64_*` type aliases over
the `double_*` spellings. A hardware-dependent sample keeps its device names and addresses as
source constants near the top of the file.

| Sample | Demonstrates |
| --- | --- |
| `example/cpp/core/size.cpp` | The `wse::Size_` data class: construction, accessors, conversion |
| `example/cpp/core/range2.cpp` | The `wse::Range2_` rectangle: bounds, containment checks |
| `example/cpp/core/matrix.cpp` | The `wse::Matrix_` class: determinant, transpose, inverse round trip |
| `example/cpp/core/image.cpp` | The `wse::Image_` class: adopting a strided buffer, channel order in the type, orientation, writing back out |
| `example/cpp/core/logging.cpp` | The log facility: structured records, levels, sinks, stream logger, file output |
| `example/cpp/tmr/single_frame.cpp` | One owned frame from the first camera, saved as a BMP |
| `example/cpp/tmr/camera_image.cpp` | Reading a frame into a Core `wse::Image_` that keeps the camera's own channel order, then converting explicitly |
| `example/cpp/oui/windowed_projection.cpp` | Thirty generated frames through source → screen → projection → window |
| `example/cpp/oui/borderless_fullscreen.cpp` | Borderless-fullscreen transition on the primary display, then windowed restore |

Every sample that handles picture data does so as a `wse::Image_` rather than as raw bytes: the
camera samples read into one, the OUI samples paint into one and upload it, and the projector
samples submit one. The image type carries the channel count and the channel order, so a conversion
between orders or channel counts is always a call the sample writes out.

The tmr samples share `example/cpp/tmr/example_camera_utility.h`, which selects the first
enumerated camera, converts a frame to an RGB image, and carries a minimal BMP writer that takes an
image; it belongs to the samples, not to the public API.

## Running the samples

Each sample names the build option it needs in its own header comment. The quick-start samples are
the ones covered by CTest:

```text
ctest --test-dir <build-directory> [-C <configuration>] -R "wse.documentation.quickstart" --output-on-failure
```

The remaining samples are reference code. C++ samples are compiled by the build when their component
is enabled. The other languages run against the artifacts the build produced:

```bat
rem C#: the sample project references the built binding assembly.
dotnet run --project example\cs\core\QuickStart.csproj

rem Python: pass the built extension module.
python example\python\core\quickstart.py <build-directory>\x64\Release\wse.pyd

rem JavaScript: pass the installed lang\js package, or the built addon during development.
node example\js\core\quickstart.js <build-directory>\x64\Release\wse.node

rem Java: compile against the binding classes, then run with the engine directory on PATH.
javac --release 17 -cp <build-directory>\java\classes -d <output> example\java\core\QuickStart.java
set PATH=<build-directory>\x64\Release;%PATH%
java "-Dwse.runtime.path=<build-directory>\x64\Release\WonderStewEngine.dll" ^
  -cp "<build-directory>\java\classes;<output>" QuickStart
```

A JavaScript sample accepts either the installed `lang/js` directory or a built `wse.node`; given
the addon it still loads the package, so the sample always exercises the same public surface an
application uses. A Java sample loads `WonderStewEngine` by absolute path, which on Windows does not
add that directory to the dependency search path, so the engine directory must be on `PATH` when the
build enables a component with its own runtime library.

See the [Build and Install Guide](../doc/en/BuildGuide.md), the [How-to guide](../doc/en/HOWTO.md),
and the [Language Binding design](../doc/design/en/LanguageBindings.md).

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
