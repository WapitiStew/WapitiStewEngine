# WSE Build and Install Guide (Windows and Linux)

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

WSE requires C++17 and CMake 3.24 or later (3.25 or later for presets). The supported build
environments are:

- Windows x86-64 with Visual Studio 2022 / MSVC for Core and all optional components.
- Linux x86-64 with GCC or Clang for Core, GEF, IUI evdev Keyboard, XPT TCP/UDP/HTTP/SerialPort/RetryPolicy, Vulkan 1.2 OUI, and Tmr V4L2/libcamera.
- Linux ARM64 is cross-buildable with GCC and accepted for native Core/GEF/IUI/XPT/Vulkan 1.2 OUI/Tmr builds intended for Raspberry Pi 4/5, but hardware
  certification is pending.

Python 3.8+ is required by the dependency bootstrap. Git is additionally required for a dependency
whose pinned source is provisioned from a checkout or bundle.

## Compiler and documentation quality gates

`WSE_WARNING_SWEEP=ON` enables `/W4` on MSVC or
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow` on GCC/Clang for the native library.
`WSE_WARNINGS_AS_ERRORS=ON` also enables that set and treats warnings as build errors.
Both options default to `OFF` and do not change consumer compile flags or third-party targets.
The Core GCC CI gate uses warnings-as-errors and an empty baseline. The Windows Core gate
uses a reviewed per-file baseline for existing DLL-interface warnings; new or increased warnings
fail. These Core gates do not certify every optional component or language compiler as warning-free.

```sh
cmake --preset linux-gcc-shared-core -DWSE_WARNINGS_AS_ERRORS=ON
cmake --build build/linux-gcc-shared-core --parallel 4
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
python3 doxy/RunDoxygen.py --preset user-public --check
python3 doxy/RunDoxygen.py --preset developer-public --check
```

On Windows, use `cmake --preset windows-msvc-shared-core -DWSE_WARNING_SWEEP=ON`, then
`cmake --build build/windows-msvc-shared-core --config Debug` and
`ctest --test-dir build/windows-msvc-shared-core -C Debug --output-on-failure`.
The [Doxygen entry](../../doxy/README.md) checks both languages with Doxygen 1.13.2 or later.
Omit `--check` to generate HTML, LaTeX and graphs; the full run also requires Graphviz `dot`.
Warnings and generation failures fail the command, and warning logs remain available for diagnosis.
Tool provisioning is separate from configure/build. CI generates both public audiences in both languages.
The [documentation generation design](../design/en/DocumentationGeneration.md) describes the
User/Developer profiles, controlled overlay selection, launchers, and separate output directories.

## Guided bootstrap for a language and component selection

`bootstrap.py` can prepare a machine from the languages and components you intend to build instead
of from package names. It resolves the selection into pinned dependencies, verification toolchains,
and CMake options, then writes a user-local configure preset:

```bat
bootstrap.bat --language js --language python --component xpt --component tmr
cmake --preset wse-local
cmake --build build\wse-local --config Release
ctest --test-dir build\wse-local -C Release --output-on-failure
```

The languages are `cpp`, `cs`, `java`, `js`, and `python`. The components are `xpt`, `gef`, `iui`,
`oui`, and `tmr`. A component implies whatever it requires, mirroring the auto-enable rules in
`CMakeLists.txt`.

An explicit selection is authoritative. Only the packages that selection needs are provisioned; a
language that needs no vendored dependency provisions nothing. Without `--language` and
`--component`, bootstrap behaves exactly as before and prepares the default package set.

### Verification toolchains

A language binding also needs a tool that WSE never links against and never ships: a Node.js
runtime, a JDK, a CPython interpreter, or a .NET SDK. Bootstrap resolves each one in a fixed order:

1. An already-installed copy, found through `WSE_NODE_HOME`, `WSE_JAVA_HOME` or `JAVA_HOME`,
   `WSE_PYTHON_HOME`, `WSE_DOTNET_ROOT` or `DOTNET_ROOT`, and then `PATH`. Anything that meets the
   pinned minimum version wins, so a machine keeps using the toolchain it already has.
2. A copy already provisioned into `.bootstrap-cache/toolchains/`.
3. The pinned archive for this host, verified against its manifest SHA-256.

Because a toolchain is a verification tool and not a dependency, a fetched one is extracted into
`.bootstrap-cache/toolchains/` only. It never enters `vendor/`, an install package, or the
redistribution surface, so provisioning one does not change what WSE ships or what its SBOM lists.

Node.js 24.19.0, Eclipse Temurin JDK 21.0.12.1+1, and the .NET SDK 8.0.424 are pinned for Windows
and Linux on both x86-64 and ARM64. CPython 3.13.15 is pinned for Windows x86-64 and ARM64 through
the redistributable package that carries the development headers and import library the binding
needs; on Linux the interpreter and its headers come from the system package manager, so bootstrap
detects one and reports `apt install python3-dev` when it finds none. `--no-fetch-toolchain`
disables fetching entirely, which is what a CI image that supplies its own toolchains should use.

Every pinned archive carries the digest its vendor publishes: SHA-256 from the Node.js
`SHASUMS256.txt` and the Adoptium release metadata, SHA-512 from the .NET release metadata and the
NuGet catalog. Bootstrap verifies every digest an entry pins and rejects an entry that pins none
before it downloads anything. A toolchain is selected by host platform and host architecture even
during a cross build, because a verification tool has to run on the machine that builds;
`--target-architecture` selects only the dependency artifacts WSE links against.

A verification toolchain is extracted with its internal relative links preserved, because Node.js
points `bin/npm` into `lib/node_modules` and a JDK points its per-module licence files at a shared
copy. A link whose target would resolve outside the extracted tree, or that is absolute, is rejected.
A dependency archive is still extracted strictly and may contain no links at all, because its
contents are copied into `vendor/` and redistributed.

Windows x86-64 and Linux x86-64 are both exercised end to end: bootstrap provisions the toolchains
and the whole test suite runs against them. Linux ARM64 is exercised on a Raspberry Pi 4 running 64-bit
Debian 13: the pinned Node.js, Temurin JDK, and .NET SDK archives are fetched and executed there,
WSE builds natively with all four language bindings, and its test suite passes. The Windows ARM64
archives are verified but have not been executed on that host.

`tools/bootstrap/verify_toolchain_pins.py` checks every pinned archive from any host. It compares
each digest with the one its vendor publishes today, confirms from the archive's own member table
that `extracted_root` and every `required_files` entry are present, and runs the real extraction so
a member bootstrap would refuse is caught before the host that provisions it hits it.

```bat
python tools\bootstrap\verify_toolchain_pins.py
python tools\bootstrap\verify_toolchain_pins.py --identity-only
python tools\bootstrap\verify_toolchain_pins.py --toolchain nodejs --keep
```

It reaches the vendors, so the default gates leave it out. `WSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION`
registers it as two opt-in tests labelled `network;bootstrap`. Run them whenever a pinned version
changes:

```bat
cmake --preset windows-msvc-shared-core -DWSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION=ON
ctest --test-dir build\windows-msvc-shared-core -C Release -L network --output-on-failure
```

`wse.bootstrap.toolchain_pin_identity` only re-reads the published digests and finishes in seconds.
`wse.bootstrap.toolchain_pin_contents` downloads every archive, so it costs about two gigabytes.

### Cross builds

`--target-architecture` selects the artifacts WSE links against. The generated preset then inherits
the official cross preset that carries the toolchain file, and pins the dependency roots CMake
cannot infer for that target. Selecting XPT for Linux ARM64 therefore also provisions the pinned
OpenSSL that its libcurl links against:

```sh
python3 bootstrap.py --target-architecture arm64 --component xpt
cmake --preset wse-local
```

The verification toolchains stay host-native throughout, because a JDK or a Node.js runtime has to
run on the machine that builds rather than on the machine being built for.

### The generated preset

The resolved selection is written to `CMakeUserPresets.json` as the configure preset `wse-local`,
which inherits the host core preset and adds the resolved toolchain paths and component options.
`CMakeUserPresets.json` is CMake's own user-local mechanism and is gitignored, so machine-specific
paths never reach the repository. Change the selection and re-run bootstrap instead of editing the
generated file. `--preset-name`, `--base-preset`, `--user-presets`, and `--no-write-presets` rename,
re-base, relocate, or suppress the output.

Selection resolution stays offline-safe. `--offline` still refuses every remote operation, including
a toolchain fetch, and reports the missing cached archive by name and version. `--check` reports
whether each selected toolchain is installed without changing anything.

## Dependency bootstrap for optional components

The WSE-owned manifest pins libjpeg-turbo 3.0.4, libcurl 8.21.0,
libcamera 0.7.2, nlohmann/json 3.12.0, pybind11 3.1.0, and the Node 24.19.0 headers used by the optional bindings.
CMake configure never downloads them. On a connected staging PC, create the cache from the WSE
source root:

```bat
bootstrap.bat --cache-dir C:\wse-offline-cache
bootstrap.bat --verify-cache --offline --cache-dir C:\wse-offline-cache
```

Move that cache to the offline PC and provision only from it:

```bat
bootstrap.bat --offline --verify-cache --cache-dir D:\wse-offline-cache
bootstrap.bat --offline --cache-dir D:\wse-offline-cache --vendor-root D:\wse-dependencies
bootstrap.bat --check --vendor-root D:\wse-dependencies
cmake --preset windows-msvc-shared-public -DWSE_DEPENDENCY_ROOT=D:/wse-dependencies
```

Strict offline mode never falls back to a URL. A missing item reports its dependency name,
version, and expected Git commit or SHA-256. The portable cache contains `3.0.4.zip` with SHA-256
`0c58853494f31a65329e567569d8614f35a74c1251bdcca10bb3d01689b35035` for libjpeg-turbo.

For libcurl it contains `curl-8.21.0.tar.xz`, SHA-256
`aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6`, and the official
`curl-8.21.0.tar.xz.asc` sidecar. The manifest records release-manager fingerprint
`27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2`. Bootstrap requires an ASCII-armored sidecar and
verifies the archive SHA-256; independent OpenPGP verification remains a release-staging gate.

For libcamera, the portable cache contains a `libcamera-0.7.2` checkout or Git bundle at commit
`191e202178f02430b5942397c70d215cdd2056fa`.

To prepare only the upcoming XPT HTTP dependency, use:

```bat
bootstrap.bat --package libcurl --cache-dir C:\wse-offline-cache
bootstrap.bat --package libcurl --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package libcurl --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
bootstrap.bat --package libcurl --check --vendor-root C:\wse-dependencies
```

On Linux, use the same options through `python3 bootstrap.py`; for example:

```sh
python3 bootstrap.py --package libcurl --offline \
  --cache-dir /media/wse-offline-cache --vendor-root /opt/wse-dependencies
python3 bootstrap.py --package libcurl --check --vendor-root /opt/wse-dependencies
```

The same source archive is portable, but extracted sources and generated libraries use separate
Windows and Linux cache directories. Windows produces static Debug/Release libraries with
Schannel. Linux produces static Debug/Release libraries with OpenSSL and requires its development
package (`libssl-dev` on Ubuntu and Raspberry Pi OS). Each installed dependency includes
`wse-dependency.json`; a different platform or architecture is rejected instead of silently reused.
Libcurl is linked privately by XPT. Its headers and targets are not part of the public WSE API.

For Linux x86-64 Tmr, install the libcamera build prerequisites and provision the pinned source:

```sh
sudo apt install meson ninja-build pkg-config python3-ply python3-yaml python3-jinja2 \
  libyaml-dev libudev-dev libssl-dev
python3 bootstrap.py --package libcamera --cache-dir /media/wse-offline-cache
python3 bootstrap.py --package libcamera --offline --verify-cache \
  --cache-dir /media/wse-offline-cache
python3 bootstrap.py --package libcamera --offline \
  --cache-dir /media/wse-offline-cache --vendor-root /opt/wse-dependencies
```

When provisioned outside the source tree, configure with
`WSE_LIBCAMERA_ROOT=/opt/wse-dependencies/libcamera`. `WSE_ENABLE_LIBCAMERA` defaults to `ON` for
Linux x86-64 Tmr; a missing or mismatched pinned root is a configuration error. Set it to `OFF`
only to build the explicit unsupported-backend variant. Installed packages bundle the libcamera
runtime, IPA, license texts, and metadata, while libudev, OpenSSL, and YAML runtimes remain operating
system prerequisites. Libcamera 0.7.2 requires C++20, but only the private adapter source uses it;
the public WSE target and consumers remain C++17.

For the optional JavaScript binding, provision only the pinned Node-API headers and their MIT
license. The header archive is platform independent; a Node.js 18 or later runtime is still needed
to run addon tests and applications:

```bat
bootstrap.bat --package node-api-headers --cache-dir C:\wse-offline-cache
bootstrap.bat --package node-api-headers --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package node-api-headers --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
```

The cache identity is `node-v24.19.0-headers.tar.gz`, SHA-256
`54f14a297d47ea0794fe272363703d9dc419c96ac68f20d890f98b63754a3e4c`, plus the separately pinned
official `LICENSE` file. Strict offline verification checks both files. Use
`WSE_NODE_API_ROOT` when the provisioned `node-api` directory is outside `WSE_DEPENDENCY_ROOT`.

For the optional Python binding, provision pybind11 and install CPython 3.11 or later with its
development headers. The initial support matrix is CPython 3.11 through 3.14.

```bat
bootstrap.bat --package pybind11 --cache-dir C:\wse-offline-cache
bootstrap.bat --package pybind11 --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package pybind11 --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
```

The pinned source archive is `pybind11-3.1.0.tar.gz`, SHA-256
`a1cc06b524ab3edca51f8ad3895f9c4fa20b8b19283173dff4ae781449dc9639`, under BSD-3-Clause.
Use `WSE_PYBIND11_ROOT` when its provisioned directory is outside `WSE_DEPENDENCY_ROOT`.

The default dependency root is `vendor`. `WSE_NODE_API_ROOT` and `WSE_PYBIND11_ROOT` are optional per-dependency overrides. Partial targets are rejected
without deletion; use `--force` only when replacing the named bootstrap-managed target is
intended. The deprecated `--skip-existing` spelling does not bypass validation.

The extraction cache records the build recipe that produced its outputs. Changing a build command
in the manifest therefore rebuilds the dependency instead of reusing the previous outputs, and an
installed target built from the earlier recipe is reported as such rather than silently reused.
Replacing it still requires `--force`.

Linux Core-only builds do not link these dependencies. Core-plus-XPT builds link the pinned
libcurl implementation and require `WSE_LIBCURL_ROOT`; ARM64 also requires `WSE_OPENSSL_ROOT`.
CMake configuration never downloads either dependency.

## Windows build presets

Run presets from the WSE source root:

```bat
cmake --preset windows-msvc-shared-public
cmake --build --preset windows-msvc-shared-public-release
cmake --install build\windows-msvc-shared-public --config Release
ctest --test-dir build\windows-msvc-shared-public -C Release --output-on-failure
```

Windows configure presets cover shared/static and all-components, all-public-components, core-only, Core-plus-XPT,
Core-plus-OUI, or Core-plus-Tmr combinations. The component-specific additions are `windows-msvc-shared-xpt`,
`windows-msvc-static-xpt`, `windows-msvc-shared-oui`, and `windows-msvc-static-oui`. Manual
Tmr presets are `windows-msvc-shared-tmr` and `windows-msvc-static-tmr`. Manual
configuration uses `WSE_LIBRARY_TYPE` and the public `WSE_BUILD_XPT`, `WSE_BUILD_GEF`, `WSE_BUILD_IUI`,
`WSE_BUILD_OUI`, and `WSE_BUILD_TMR` options. `WSE_BUILD_NODE_BINDING`, `WSE_BUILD_PYTHON_BINDING`,
`WSE_BUILD_JAVA_BINDING`, and `WSE_BUILD_DOTNET_BINDING` add the optional language adapters and
default to `OFF`.
Use `windows-msvc-shared-public` or `windows-msvc-static-public` for a distributable package with
XPT/GEF/IUI/OUI/Tmr and with private components/extensions explicitly disabled. The `*-all`
presets are access-controlled integration configurations and must not produce a public package or
sanitized export.
Access-controlled components are not in this repository. They come from an extension overlay, and
`WSE_EXTENSION_ROOT` points at one:

```
cmake --preset windows-msvc-shared-all -DWSE_EXTENSION_ROOT=<path to the overlay>
```

Every preset that selects such a component also reads `WSE_EXTENSION_ROOT` from the environment,
so exporting it once is enough:

```
set WSE_EXTENSION_ROOT=<path to the overlay>
cmake --preset windows-msvc-shared-all
```

The overlay supplies the implementation, tests, and install rules for the components it owns, and
adds nothing for a component that was not selected. Selecting a component this repository cannot
build on its own without naming an overlay is refused at configure time rather than producing a
library that reports the component and cannot do anything.

Standalone builds enable `WSE_BUILD_TESTING` by default. Physical camera tests remain disabled
unless `WSE_ENABLE_CAMERA_HARDWARE_TESTS=ON` is selected explicitly.
On Linux this adds `wse.tmr.webcamera_hardware_smoke`, which uses the public `WebCamera` facade
to acquire one in-memory frame without logging device identity or changing camera controls.
Interactive physical-keyboard tests remain disabled unless
`WSE_ENABLE_IUI_HARDWARE_TESTS=ON` is selected explicitly.
`WSE_ENABLE_LIBCAMERA` defaults to `ON` for Linux x86-64 Tmr and to `OFF` for ARM64 until the
Pi 4 native and hardware gates accept the pinned ARM64 dependency.

To build the Node-API addon with a shared Core package on Windows, add the binding option to the
Core preset. Set `WSE_NODE_EXECUTABLE` when `node.exe` is not on `PATH`:

```bat
bootstrap.bat --package node-api-headers
cmake --preset windows-msvc-shared-core -DWSE_BUILD_NODE_BINDING=ON -DWSE_NODE_EXECUTABLE=C:/tools/node/node.exe
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
node -e "const wse=require('C:/wse-sdk/lang/js'); console.log(wse.runtimeInfo())"
```

Use `windows-msvc-static-core` for a statically linked WSE Core inside `wse.node`. The addon itself
remains a loadable `.node` module in both configurations.

To build and install the Python module on Windows:

```bat
bootstrap.bat --package pybind11
cmake --preset windows-msvc-shared-core -DWSE_BUILD_PYTHON_BINDING=ON -DWSE_PYTHON_EXECUTABLE=C:/Python314/python.exe
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
set PYTHONPATH=C:\wse-sdk\lang\python
C:\Python314\python.exe -c "import wse; print(wse.runtime_info())"
```

Use `windows-msvc-static-core` to link a static WSE Core into the loadable Python module.

Build and install the Windows Java JNI module and Java 17-compatible JAR as follows. WSE does not
package the JDK:

```bat
cmake --preset windows-msvc-shared-core -DWSE_BUILD_JAVA_BINDING=ON -DWSE_JAVA_HOME=C:/tools/jdk
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
java -Djava.library.path=C:\wse-sdk\bin -cp C:\wse-sdk\lang\java\wse.jar com.example.Main
```

`windows-msvc-static-core` statically links Core into the loadable `wse_jni` module. A shared
package loads the WSE runtime from the same `bin` directory.

To verify the Java 17-compatible JAR on multiple runtimes, provide each executable at configure
time. CTest registers only the runtimes that are specified. These JDKs are verification tools and
are never copied into the vendor or install package.

```bat
cmake -S . -B build\java-matrix ^
  -DWSE_BUILD_JAVA_BINDING=ON ^
  -DWSE_JAVA_17_EXECUTABLE=C:/tools/jdk-17/bin/java.exe ^
  -DWSE_JAVA_21_EXECUTABLE=C:/tools/jdk-21/bin/java.exe ^
  -DWSE_JAVA_25_EXECUTABLE=C:/tools/jdk-25/bin/java.exe
cmake --build build\java-matrix --config Debug
ctest --test-dir build\java-matrix -C Debug -R "wse.binding.java" --output-on-failure
```

CTest adds `--enable-native-access=ALL-UNNAMED` for Java 25. Applications running `wse.jar` on
Java 25 should provide the same option.

Build and install the C# binding as follows. WSE does not package the .NET SDK. The binding is a
`net8.0` assembly over the flat C ABI, so the build produces both `wse_capi` and
`WapitiStew.Wse.dll`. Set `WSE_DOTNET_EXECUTABLE` when `dotnet` is not on `PATH`:

```bat
cmake --preset windows-msvc-shared-core -DWSE_BUILD_DOTNET_BINDING=ON
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
```

`windows-msvc-static-core` statically links Core into the `wse_capi` shared library. The C ABI
itself always builds as a shared library because P/Invoke can only resolve shared libraries.
Reference `C:\wse-sdk\lang\cs\WapitiStew.Wse.dll` and let the loader find `wse_capi` beside the
application, or name the file explicitly with `WseRuntime.SetNativeLibraryPath` or the
`WSE_CAPI_LIBRARY` environment variable.

## Linux Core, XPT, GEF, IUI, OUI, and Tmr presets

Run the following commands on Linux from the WSE source root. GCC shared Core is the shortest
entry point:

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core
```

The other supported configure presets are `linux-gcc-static-core`,
`linux-clang-shared-core`, and `linux-clang-static-core`, with matching `-debug` build presets.
Use the corresponding `linux-gcc-shared-xpt`, `linux-gcc-static-xpt`,
`linux-clang-shared-xpt`, or `linux-clang-static-xpt` preset to include XPT TCP/UDP/HTTP/SerialPort/RetryPolicy.
Use `linux-gcc-shared-gef`, `linux-gcc-static-gef`, `linux-clang-shared-gef`, or
`linux-clang-static-gef` for GEF binary/CSV operations; GEF adds no third-party dependency.
Use `linux-gcc-shared-iui`, `linux-gcc-static-iui`, `linux-clang-shared-iui`, or
`linux-clang-static-iui` for the read-only evdev Keyboard. IUI adds no third-party dependency and
does not change `/dev/input` permissions. The user must already have read access, and applications
must check `Keyboard::isAvailable()` before interpreting state.
Use `linux-gcc-shared-oui` or `linux-gcc-static-oui` for OUI. Optional components default to `OFF`
on Linux. Use `linux-gcc-shared-tmr`, `linux-gcc-static-tmr`,
`linux-clang-shared-tmr`, or `linux-clang-static-tmr` for Tmr. Enabling an
unsupported optional component is an explicit configuration error. Linux XPT packages TCP, UDP,
portable `SerialPort`, and `RetryPolicy`.

The matching Linux Node-API build is:

```sh
python3 bootstrap.py --package node-api-headers
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_NODE_BINDING=ON -DWSE_NODE_EXECUTABLE=/opt/node/bin/node
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
/opt/node/bin/node -e "const wse=require('/opt/wse-sdk/lang/js'); console.log(wse.runtimeInfo())"
```

The installed shared addon uses an origin-relative runpath to find WSE in the same package. No
`LD_LIBRARY_PATH` is required for the documented layout. Set `WSE_NODE_ADDON` only when explicitly
loading a development or test addon from another location.

The matching Linux Python build is:

```sh
sudo apt install python3-dev
python3 bootstrap.py --package pybind11
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_PYTHON_BINDING=ON -DWSE_PYTHON_EXECUTABLE=/usr/bin/python3
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
PYTHONPATH=/opt/wse-sdk/lang/python python3 -c "import wse; print(wse.runtime_info())"
```

The installed module uses `$ORIGIN/../../../lib` to locate the shared WSE runtime.

The matching Linux Java JNI and JAR build is:

```sh
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_JAVA_BINDING=ON -DWSE_JAVA_HOME=/opt/jdk
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
java -Djava.library.path=/opt/wse-sdk/bin \
  -cp /opt/wse-sdk/lang/java/wse.jar com.example.Main
```

Ubuntu 24.04 OUI builds require the Vulkan loader and headers, a Wayland client and protocols,
libdrm, the shader compiler, and pkg-config:

```sh
sudo apt install libvulkan-dev mesa-vulkan-drivers vulkan-tools \
  libwayland-dev wayland-protocols libdrm-dev glslang-tools pkg-config
cmake --preset linux-gcc-shared-oui
cmake --build --preset linux-gcc-shared-oui-debug
ctest --test-dir build/linux-gcc-shared-oui --output-on-failure
```

The Wayland test exercises window, fullscreen, resize, and presentation when a compositor is
available. DRM/KMS direct-display restoration can change the active display mode, so it is disabled
by default and must be enabled with `WSE_ENABLE_DISPLAY_MODE_TESTS=ON` only on a dedicated console
with a recovery path.

On a native 64-bit Raspberry Pi OS environment, use a GCC or Clang Core, GEF, or XPT preset. The build uses
the `linux-arm64` output directory when CMake reports `aarch64` or `arm64`. Pi 4/5 hardware
results must not be reported as certified until the separate device gate has passed.

Before configuring, run `sh tools/pi/inspect_pi4_environment.sh` and retain its privacy-filtered
inventory with the hardware evidence. The script does not output host names, addresses, account
names, or device identifiers and does not open a display or camera. See the
[Pi 4 tool guide](../../tools/pi/README.md) for the output and acceptance boundary.

For a GCC AArch64 cross build, first provision the pinned ARM64 OpenSSL and libcurl trees:

```sh
python3 bootstrap.py --package openssl --target-architecture arm64 \
  --cache-dir /media/wse-cache --vendor-root vendor
python3 bootstrap.py --package libcurl --target-architecture arm64 \
  --cache-dir /media/wse-cache --vendor-root vendor
cmake --preset linux-arm64-gcc-shared-xpt
cmake --build --preset linux-arm64-gcc-shared-xpt-debug
cmake --install build/linux-arm64-gcc-shared-xpt
```

The matching static preset is `linux-arm64-gcc-static-xpt`. Cross-built tests compile but do not
run on an x86-64 host. Copy the `linux-arm64/Debug` artifact directory and
`tools/pi/run_pi4_smoke.sh` to a 64-bit Pi 4, then run
`sh run_pi4_smoke.sh <artifact-directory>` there to complete the hardware gate.

Verify the complete dependency cache without remote fallback before moving it to an offline ARM64
environment:

```sh
python3 bootstrap.py --offline --verify-cache --target-architecture arm64 \
  --package openssl --package libcurl --package nlohmann-json \
  --cache-dir /media/wse-cache
```

A missing archive or signature is a failure; strict offline mode does not fetch it. Access-controlled
extension packages and profiles remain outside the public offline kit and public Engine export.

GEF needs no external dependency. Use `linux-arm64-gcc-shared-gef` or
`linux-arm64-gcc-static-gef` to cross-build its binary and CSV boundary; native Pi CTest remains
the acceptance gate.

IUI needs no external dependency. Use `linux-arm64-gcc-shared-iui` or
`linux-arm64-gcc-static-iui` to cross-build its evdev boundary. For native hardware evidence,
configure with `WSE_ENABLE_IUI_HARDWARE_TESTS=ON` and run only
`wse.iui.keyboard_hardware_smoke` while a physical keyboard is attached; press and release Q when
prompted. A lifecycle test or CEC remote source is not physical-keyboard evidence.

The Core ARM64 presets also accept `-DWSE_BUILD_NODE_BINDING=ON -DWSE_BUILD_TESTING=OFF`. This
cross-builds an AArch64 `wse.node`; it does not execute Node or certify Raspberry Pi 4/5 runtime
behavior. Native addon load/unload is an acceptance item in the active Pi 4 hardware gate.

An ARM64 Python cross build additionally requires the target CPython 3.11+ development headers.
Set `WSE_PYTHON_TARGET_INCLUDE_DIR` to the target include directories and
`WSE_PYTHON_EXTENSION_SUFFIX` to the target CPython ABI suffix, enable
`WSE_BUILD_PYTHON_BINDING`, and disable runtime tests. The resulting AArch64 ELF verifies the build
boundary only; import, lifecycle, GIL, and cancellation are acceptance items in the Pi 4 native gate.

An ARM64 Java cross build uses host-JDK `javac`/`jar` plus target Linux JNI headers. Enable
`WSE_BUILD_JAVA_BINDING`, disable runtime tests, and set `WSE_JAVA_HOME` and
`WSE_JNI_TARGET_INCLUDE_DIRS`. The resulting AArch64 `wse_jni` and Java 17-compatible `wse.jar`
verify only the build boundary; Pi load, callback attach/detach, and lifecycle are Pi 4 native-gate
acceptance items.

For ARM64 OUI, install the host `wayland-scanner` and `glslangValidator` tools plus ARM64 Vulkan,
Wayland, and DRM development packages, then use `linux-arm64-gcc-shared-oui` or
`linux-arm64-gcc-static-oui`. Cross-build success is not Pi 4/5 rendering certification.

Use `linux-arm64-gcc-shared-tmr` or `linux-arm64-gcc-static-tmr` for ARM64 Tmr. A cross
build validates the V4L2 header and ABI boundary; it does not certify frames, controls,
removal/reconnection, or performance. `WSE_ENABLE_LIBCAMERA` defaults to `OFF` for ARM64 until the
Pi 4 native and hardware gates accept the pinned ARM64 dependency. Enabling it requires a pinned
ARM64 build supplied through `WSE_LIBCAMERA_ROOT`, followed by separate native and hardware gates.
The Pi camera gates remain `HARDWARE_NOT_RUN`; Pi 5 and Pi CSI camera tests have not run.
The accepted Pi 4 Core/XPT, four-binding runtime smoke, and OUI offscreen results are separate;
see the [Hardware Validation design](../design/en/HardwareValidation.md) for their exact scope.

## Offline kit acceptance

`tools/offline/create_offline_kit.py` creates an immutable Windows Core/XPT candidate from an
installed package. `tools/offline/verify_offline_kit.py` accepts it only on Windows after every
network adapter is disabled and the packaged consumer configures, builds, and passes locally.
See `tools/offline/README.md`. A candidate without an accepted manifest is not a release artifact.

Consume an installed package with:

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE WSE::Core WSE::Oui)
```

The package exposes only the `WSE::*` component targets; the consumer-facing `WonderStewEngine`
compatibility alias was removed on 2026-09-13 under the legacy removal program. A project that
wants the old spelling defines its own local alias over `WSE::Core`.
## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
