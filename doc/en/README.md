# WonderStewEngine SDK

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

WonderStewEngine (WSE) is a reusable C++ library suite for multiple projects. Core data, logging,
and utilities are always available. Communication (XPT), files (GEF), input (IUI), output and
projection (OUI), and cameras (Tmr) can be selected as optional components.

## Current support

- Windows x86-64: Core and all optional components, Visual Studio 2022 / MSVC
- Linux x86-64: Core, XPT TCP/UDP/HTTP/SerialPort/RetryPolicy, GEF, IUI evdev Keyboard, OUI Vulkan 1.2/Wayland/DRM-KMS, and Tmr V4L2/libcamera, GCC or Clang
- C++17
- Shared and static libraries
- Installable CMake packages
- Optional Node-API JavaScript binding for Node.js 18+
- Optional pybind11 Python binding for CPython 3.11+
- Optional JNI binding for Java 17+
- Optional flat C ABI / P/Invoke binding for C# on .NET 8

Linux ARM64 GCC cross builds have passed for Core/XPT/GEF/IUI/OUI/Tmr and the Node-API/Python/Java JNI
native modules. Raspberry Pi 4 Core/XPT and all four public bindings have accepted
`HARDWARE_SMOKE_PASS` evidence. OUI offscreen validation and the remaining camera/display and Pi 5
gates are scoped in the [Hardware Validation design](../design/en/HardwareValidation.md).
JavaScript, Python, Java, and C# expose Tmr and the handle-free OUI Projection API through Node-API,
pybind11, JNI, and the flat C ABI. The [Language Binding design](../design/en/LanguageBindings.md)
records the language-specific golden, lifecycle, cancellation, and runtime verification coverage.
Linux XPT supports TCP, UDP, HTTP,
portable serial, and explicit retry policy. Unsupported optional components are rejected during Linux CMake
configuration when enabled.

Linux IUI monitors readable kernel evdev keyboard sources without grabbing devices or changing
permissions. Callers check `Keyboard::accessState()` or `isAvailable()` before interpreting a
snapshot. See the [IUI Keyboard design](../design/en/IuiKeyboard.md).

## CMake targets

`WSE::Core` is always present. Optional targets are `WSE::Xpt`, `WSE::Gef`, `WSE::Iui`,
`WSE::Oui`, and `WSE::Tmr`. Consumers link these component targets. The consumer-facing
`WonderStewEngine` compatibility alias was removed; the package name used by `find_package`
remains `WonderStewEngine`. See the [Build and Packaging design](../design/en/BuildPackaging.md).

```cpp
#include <wse/stew.h>

int main()
{
    wse::double_xy point(10.0, 50.0);
    return point.x == 10.0 ? 0 : 1;
}
```

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE WSE::Core)
```

Start with [Getting Started](GettingStarted.md), then see the
[Build and Install Guide](BuildGuide.md), [How-to](HOWTO.md),
[Public API Reference](API_REFERENCE.md), and
[WebCamera Migration Guide](WebCameraMigration.md), and the
[Deprecation and Migration Guide](DeprecationMigration.md). Access-controlled legacy extensions must not be
included in a public package or history export.

See also the [Portable Core design](../design/en/PortableCore.md) for current OS-boundary and
unsupported-operation contracts, and the [XPT Transport design](../design/en/XptTransport.md)
for the TCP/UDP/serial, timeout, cancellation, retry, packet-boundary, and error contract. The
[OUI Projection design](../design/en/OuiProjection.md) defines portable mesh coordinates, scaling,
alpha-map composition, and the shared D3D12/Vulkan verification boundary.
The [Tmr Camera design](../design/en/TmrCamera.md) defines device identity, lifecycle,
owned frames, controls, errors, calibration values, and the MF/V4L2 backend boundary.
The [Language Binding design](../design/en/LanguageBindings.md) defines binding ABI 1, common errors,
cancellation, owned buffers, Node-API promises/callbacks, and runtime cleanup.
The [Architecture](../design/en/Architecture.md) links the cross-cutting thread/ownership,
build/package, version/compatibility, security/privacy, and hardware-validation contracts.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
