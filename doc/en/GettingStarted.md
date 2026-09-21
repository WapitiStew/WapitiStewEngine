# WSE Getting Started

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This guide takes a new consumer from source to a tested installed CMake package. Run commands from
the WSE source root. CMake configure does not download dependencies.

## 1. Choose a configuration

Start with Core only. Choose a public component preset only when the application needs that module.

| Goal | Windows preset | Linux preset |
| --- | --- | --- |
| Smallest shared package | `windows-msvc-shared-core` | `linux-gcc-shared-core` |
| Smallest static package | `windows-msvc-static-core` | `linux-gcc-static-core` |
| XPT | `windows-msvc-shared-xpt` | `linux-gcc-shared-xpt` |
| GEF | Enable `WSE_BUILD_GEF=ON` from a Core preset | `linux-gcc-shared-gef` |
| IUI | Enable `WSE_BUILD_IUI=ON` from a Core preset | `linux-gcc-shared-iui` |
| OUI | `windows-msvc-shared-oui` | `linux-gcc-shared-oui` |
| Tmr | `windows-msvc-shared-tmr` | `linux-gcc-shared-tmr` |
| All public Windows modules | `windows-msvc-shared-public` | Not one preset; enable supported modules explicitly |

GEF and IUI are supported on Windows and Linux. Linux IUI is a read-only evdev keyboard-state API;
check its availability before reading state. Raspberry Pi ARM64 presets are cross-build gates, not
runtime or hardware certification.

## 2. Configure, build, and test

Windows shared Core, Release:

```bat
cmake --preset windows-msvc-shared-core
cmake --build --preset windows-msvc-shared-core-release
ctest --test-dir build\windows-msvc-shared-core -C Release --output-on-failure
cmake --install build\windows-msvc-shared-core --config Release --prefix C:\wse-sdk
```

Linux shared Core, Debug:

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
```

For XPT, OUI, Tmr, or a language binding, prepare the pinned dependency cache with the explicit
bootstrap procedure in the [Build and Install Guide](BuildGuide.md). Offline mode never falls back
to the network.

## 3. Create a consumer

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(wse_consumer LANGUAGES CXX)

find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS Core)
add_executable(wse_consumer main.cpp)
target_compile_features(wse_consumer PRIVATE cxx_std_17)
target_link_libraries(wse_consumer PRIVATE WSE::Core)
```

`main.cpp` is the tested [C++ quick-start source](../../example/cpp/core/quickstart.cpp). Configure the
consumer with `-DCMAKE_PREFIX_PATH=C:/wse-sdk` on Windows or `/opt/wse-sdk` on Linux.

## 4. Optional language bindings

- JavaScript: Node.js 18+ and the Node-API package; run
  [example/js/core/quickstart.js](../../example/js/core/quickstart.js) with the installed package directory.
- Python: CPython 3.11+ and pybind11; run
  [example/python/core/quickstart.py](../../example/python/core/quickstart.py) with the installed native module.
- Java: JDK 17+ and JNI; compile/run [example/java/core/QuickStart.java](../../example/java/core/QuickStart.java)
  against `wse.jar` and the installed native module.

These quick starts inspect runtime/component information and do not open hardware. Camera capture is
documented separately in the [How-to](HOWTO.md).

## 5. Next documents

- [Build and Install Guide](BuildGuide.md): bootstrap, every option, package, ARM64, and troubleshooting
- [How-to](HOWTO.md): XPT, OUI, Tmr, and language operations
- [API Reference](API_REFERENCE.md): installed public surface
- [Architecture](../design/en/Architecture.md): module and platform boundaries

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
