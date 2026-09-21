# WonderStewEngine

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-21

WonderStewEngine (WSE) is a reusable C++17 library suite. Core is always built; communication,
files, input, rendering/projection, cameras, and language bindings are selected per consumer.

| Component | CMake target | Purpose | Current public platforms |
| --- | --- | --- | --- |
| Core | `WSE::Core` | Data, logging, utilities, binding facade | Windows x86-64, Linux x86-64/ARM64 build |
| XPT | `WSE::Xpt` | TCP, UDP, HTTP, serial, retry policy | Windows and Linux |
| GEF | `WSE::Gef` | Generic binary and CSV operations | Windows and Linux |
| IUI | `WSE::Iui` | Keyboard input state | Windows, Linux evdev |
| OUI | `WSE::Oui` | Renderer, display, portable projection | Windows D3D12, Linux Vulkan 1.2 |
| Tmr | `WSE::Tmr` | Backend-independent camera control | Windows MF, Linux V4L2/libcamera |

WSE builds shared or static libraries and installable CMake packages. Optional Node-API,
pybind11, JNI, and flat C ABI adapters expose the common facade to JavaScript, Python, Java,
and C# (.NET 8). Linux ARM64 cross-builds are verified. Raspberry Pi 4 Core/XPT and all four
bindings have accepted `HARDWARE_SMOKE_PASS` evidence; camera/display certification remains open.
See the [Hardware Validation design](doc/design/en/HardwareValidation.md) for the exact scope,
including OUI offscreen validation and the unrun Pi 5 gates. A build or smoke pass is not hardware certification.

## Quick start

Obtain the public source checkout:

```sh
git clone https://github.com/WapitiStew/WapitiStewEngine.git
cd WapitiStewEngine
```

From the WSE source root, a Windows Core-only build is:

```bat
cmake --preset windows-msvc-shared-core
cmake --build --preset windows-msvc-shared-core-release
ctest --test-dir build\windows-msvc-shared-core -C Release --output-on-failure
```

The corresponding Linux command is:

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
```

Use `windows-msvc-*-public` for distributable Windows packages. Do not use an access-controlled
integration preset for a public artifact. CMake configuration never downloads dependencies;
run the explicit bootstrap step first when an optional component needs them.

To prepare a machine from what you intend to build, name the languages and components instead of
the packages. Bootstrap resolves the selection into pinned dependencies and verification
toolchains, then writes the configure preset `wse-local` into the gitignored
`CMakeUserPresets.json`:

```bat
bootstrap.bat --language js --language python --component xpt --component tmr
cmake --preset wse-local
```

A detected toolchain is always preferred over a fetched one, and a fetched one stays in the
bootstrap cache rather than entering `vendor/` or an install package. See the
[Build and Install Guide](doc/en/BuildGuide.md) for the full selection.

An installed consumer uses component targets:

```cmake
find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_application PRIVATE WSE::Core)
target_compile_features(my_application PRIVATE cxx_std_17)
```

The tested C++ quick-start source is [example/cpp/core/quickstart.cpp](example/cpp/core/quickstart.cpp).

## Documentation

- [Getting Started](doc/en/GettingStarted.md)
- [Build and Install Guide](doc/en/BuildGuide.md)
- [How-to](doc/en/HOWTO.md)
- [Public API Reference](doc/en/API_REFERENCE.md)
- [Architecture](doc/design/en/Architecture.md)
- [Documentation index](doc/README.md)
- [日本語README](README.ja.md)

Internal reports are non-normative. Access-controlled extension names, source, device profiles,
protocol details, and identifying metadata must never enter a public package, sanitized history,
document, log, or artifact.

## License

The public SDK is licensed under the [Apache License 2.0](LICENSE). Commercial use,
modification and redistribution are permitted under its terms. See [NOTICE](NOTICE) and
the [license guide](doc/en/LICENSE.md) for attribution and third-party license boundaries.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-21 | Licensed the public SDK under Apache-2.0. |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
