# WSE Architecture

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope

WSE is a general-purpose, reusable C++17 library suite. The public architecture separates
domain-independent API, portable implementation, operating-system adapters, optional components,
and language adapters. Unsupported operations fail explicitly.

## Dependency direction

WSE supplies library objects; the application owns the main loop, device selection, scheduling
and composition. There is no global Engine object that implicitly starts all components.

The arrows below mean "calls/uses", not inheritance or separate shared libraries:

```text
C++ application                  JS / Python / Java             C#
       |                          Node-API / pybind11 / JNI      P/Invoke
       |                                    |                     |
       |                                    |                flat C ABI
       |                                    +----------+----------+
       |                                               |
       +---------------------+             public binding operations
                             |                         |
                    enabled public components <--------+
                  GEF    IUI    OUI    Tmr    XPT
                             |
                     Core domain API

component facades -> portable implementation -> selected OS/backend adapter
```

Bindings expose the documented common facade, not every C++ type. GEF and low-level OUI Renderer
objects, for example, are not direct language-binding surfaces. The complete mapping belongs to
[Language Bindings](LanguageBindings.md).

- In dependency statements, **Core means the public domain-independent API installed from
  `api/wse/`**. It does not mean every implementation file under the source directory named
  `core/`.
- Core is always present and cannot depend on an optional component.
- Optional public components depend on Core. They do not depend on a language runtime.
- Language adapters depend on the common binding facade and only the enabled public components.
- Platform implementations satisfy public contracts behind PIMPL or adapter boundaries. Public
  headers do not expose operating-system or third-party implementation types.
- Public component cycles are forbidden. A protocol- or product-specific policy remains in its
  consumer rather than entering XPT, OUI, or Tmr.

## Source layout

| Path | Responsibility |
| --- | --- |
| `api/wse/` | Installed public Core headers, including binding declarations and the flat C ABI |
| `api/{xpt,gef,iui,oui,tmr}/` | Installed component C++ headers |
| `core/` | Platform-neutral implementation and component logic |
| `platform/` | Windows/Linux adapters and hardware seams |
| `api/wse/binding/`, `core/wse/binding/` | Common binding values, errors, buffers and runtime implementation |
| `core/{xpt,iui,oui,tmr}/binding/` | Component error/operation adaptation for bindings |
| `lang/` | Node-API, pybind11, JNI and C# adapters; `lang/cs/native/` implements the C ABI |
| `cmake/` | Installed package definitions and dependency targets |
| `example/` | Compiled or executed user examples |
| `test/` | Contract, boundary, golden, package, and consumer gates |
| `doc/design/` | Normative design; English is canonical |

Internal evidence and historical reports live outside the public source tree. They do not
override the public headers or these normative designs.

## Component responsibilities and implementation entry points

| Component | Owns | Does not own | Implementation / verification entry |
| --- | --- | --- | --- |
| Core | Data values, mathematics, image operations, timers, logging, license values, common binding runtime | Camera capture, transport sessions, GPU presentation | [data](../../../api/wse/data/), [runtime contract](../../../test/characterization/core_runtime_contract.cpp) |
| GEF | Typed binary blocks and setting CSV tables | Device-specific setting semantics | [BIN](../../../core/gef/bin/binController.cpp), [CSV](../../../core/gef/csv/csvController.cpp) |
| XPT | Synchronous transport and explicit retry decisions | Product protocols or implicit replay of commands | [network](../../../platform/xpt/network/), [HTTP](../../../platform/xpt/http/HttpClient.cpp) |
| IUI | Keyboard snapshots, access state and monitor worker | Text input, IME or permission changes | [Windows](../../../platform/iui/win/device/Keyboard.cpp), [Linux](../../../platform/iui/linux/device/Keyboard.cpp) |
| OUI | Render backend, surfaces, generation-bearing resource identities, projection passes | Camera acquisition or calibration policy | [Renderer](../../../core/oui/renderer/Renderer.cpp), [backend seam](../../../core/oui/renderer/RendererBackend.h) |
| Tmr | Device session, capabilities, capture, controls and detached frames | Application frame queues or projection scheduling | [CameraSession](../../../core/tmr/camera/Camera.cpp), [backend seam](../../../core/tmr/camera/CameraBackend.h) |

## Build-time and runtime structure

`WSE::Core` names the native library target. Enabled component sources compile into the same
`WonderStewEngine` binary; `WSE::Xpt`, `WSE::Gef`, `WSE::Iui`, `WSE::Oui` and `WSE::Tmr`
are component/dependency facades. Selecting a facade is not loading another component DLL.
Bindings build their own native adapters. See [Build and Packaging](BuildPackaging.md) for the
reason for this layout, static dependencies and installed targets.

A representative C++ camera-to-display application has these *independent* owners:

```text
application
  +-- WebCamera -> CameraSession -> selected camera backend -> OS capture buffers
  |                   |
  |                   +-- copies -> owned sCameraFrame
  |
  +-- Core Image <--- explicit frame conversion (owned copy)
  |
  +-- Renderer -> selected renderer backend -> GPU resources / surfaces / fences
         ^                 ^
         |                 +-- keeps submitted resources until GPU completion
    ProjectionPipeline
    (borrows Renderer; caller supplies texture/mesh identities)
```

The application chooses when to convert, upload, submit and present. Camera capture does not
implicitly invoke OUI. CPU frames outlive capture-buffer reuse; submitted GPU work can outlive
the caller's resource identities. A single renderer is caller-serialized. Each owner follows
its own stop/close/shutdown contract rather than a global Engine shutdown.

## Reading and reconstruction route

1. Read this architecture, then [Core Data Model](CoreDataModel.md) for shared representation,
   [Core Algorithms](CoreAlgorithms.md) for numerical rules, and [Core Services](CoreServices.md)
   for Timer, logging and shared runtime state.
2. Read [GEF File Formats](GefFileFormats.md), XPT, IUI, OUI or Tmr for the operation being implemented.
3. Follow [Thread and Ownership](ThreadOwnership.md) and [Result](ResultContract.md) at each boundary.
4. Follow [Language Bindings](LanguageBindings.md) only when crossing a managed runtime boundary.
5. Use [Design Verification](DesignVerification.md) to locate executable evidence and coverage limits.
6. Follow [Developer Walkthrough](DeveloperWalkthrough.md) for an offscreen pipeline, fake-camera
   recovery and input/memory/timing measurement boundaries.

The [documentation design](DocumentationGeneration.md#detailed-design-authoring) defines the
common detail level. Data layouts and observable contracts are normative; internal source maps
describe the current implementation and may change without an API change.

## Cross-cutting contracts

- [Public API policy](PublicApiPolicy.md) defines names, errors, logging, headers, and third-party
  boundaries.
- [Thread and ownership model](ThreadOwnership.md) defines RAII, callbacks, cancellation, and shutdown.
- [Build and packaging](BuildPackaging.md) defines selection, install targets, and offline dependencies.
- [Version and compatibility](VersionCompatibility.md) defines the current WSE 1.0.0 compatibility promise.
- [Security and privacy](SecurityPrivacy.md) defines data, dependency, and public/private boundaries.
- [Hardware validation](HardwareValidation.md) separates build, contract, smoke, and certification states.

Component contracts are defined by [Portable Core](PortableCore.md), [GEF](GefFileFormats.md), [XPT](XptTransport.md),
[IUI Keyboard](IuiKeyboard.md),
[OUI Renderer](OuiRenderer.md), [OUI Projection](OuiProjection.md),
[Tmr Camera](TmrCamera.md), and [Language Bindings](LanguageBindings.md).

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
