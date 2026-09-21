# WSE Build and Packaging Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-21

## Build model

Core is always built. `WSE_BUILD_XPT`, `WSE_BUILD_GEF`, `WSE_BUILD_IUI`, `WSE_BUILD_OUI`, and
`WSE_BUILD_TMR` select public components. JavaScript, Python, Java, and C# bindings are independent options.
`WSE_LIBRARY_TYPE=SHARED|STATIC` controls the native library form. Unsupported platform/component
combinations fail during configure.

For this contract, Core is the API under `api/wse`. The `WSE::Xpt`-style component targets are
feature and dependency facades, not separate binaries: every selected native source builds into the
one `WonderStewEngine` library, and each facade carries only its feature macro and link edge while
enforcing its consumer graph. This is a deliberate decision, not a deferral - the package and
consumer verification is built on the single library, and no measured size, deployment, or link-time
need justifies the symbol, install, and ABI churn a split would cost today. The facades become real
binaries only when one of these appears: a core-only package measurably too large, a deployment
unit that must ship separately, a binding that must update independently, an optional dependency
that must leave the runtime entirely, or a link-time cost that shows up in measurements. A split
would not relax the dependency rules.

Presets are convenience configurations, not capability claims. Public Windows artifacts use a
`*-public` preset or explicit public options with all access-controlled options disabled. Linux has
component presets; GEF and the IUI evdev Keyboard are portable. ARM64 presets cross-build only and
do not claim physical-keyboard success.

## Dependencies and offline operation

CMake configure and build never download dependencies. `bootstrap.py`/`bootstrap.bat` is a separate,
explicit provisioning step that verifies pinned version, source identity or hash, license, target
platform, and architecture. Strict offline mode fails on a cache miss. Generated dependency output
may live under `vendor/` or an external `WSE_DEPENDENCY_ROOT`; it is not source-controlled.

## Package contract

Standalone builds install public headers, selected native targets, package configuration, dependency
runtime/license payloads required by that configuration, and optional binding packages. Examples and
user documentation stay in the source distribution. Consumers call `find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS ...)` and link
`WSE::*`. `WonderStewEngine` names the in-tree build target only; installed packages expose
component targets under `WSE::*` and do not define a `WonderStewEngine` link target.

Package verification rejects private headers/metadata, missing component targets, dependency leakage,
incorrect library type, and platform/architecture reuse. External consumers must pass for the
`WSE::*` targets and for shared/static forms, and the package must not define a
`WonderStewEngine` compatibility alias. Detailed commands are in the
[Build and Install Guide](../../en/BuildGuide.md).

## Build graph and source map: PKG-GRAPH-01

| Stage | Source | Input / output |
| --- | --- | --- |
| Selection/provisioning | [bootstrap.py](../../../bootstrap.py), [dependency manifest](../../../bootstrap-manifest.json) | Requested languages/components, platform/architecture, verified cache -> dependency roots, host verification tools, generated user preset |
| Options/dependencies | [WseOptions.cmake](../../../cmake/WseOptions.cmake), [WseDependencies.cmake](../../../cmake/WseDependencies.cmake) | Options and explicit roots -> validated local dependencies; no downloading |
| Native graph | [WseCore.cmake](../../../cmake/WseCore.cmake) | Selected source lists -> one native library and component INTERFACE targets |
| Language graph | [WseBindings.cmake](../../../cmake/WseBindings.cmake) | Core/components plus language SDK headers/tools -> optional modules and managed packages |
| Install/export | [WseInstall.cmake](../../../cmake/WseInstall.cmake), [package config](../../../cmake/WonderStewEngineConfig.cmake.in) | Built artifacts -> selected headers/libraries, WSETargets, relocatable dependency lookup |
| Identity | [wse-package.json.in](../../../cmake/wse-package.json.in) | Version, library form, selected components, source tree identity and dependency catalog |
| External verification | [consumer](../../../test/consumer/CMakeLists.txt), [package verifier](../../../test/package/verify_public_package.py) | Installed prefix only -> compile/link/load and package shape evidence |

```text
selection -> bootstrap cache / dependency root -> configure -> native library
                                                   +------> language modules
native library -- exported as --> WSE::Core
selected public facade -- transitive link --> WSE::Core
build -> CTest -> fresh install prefix -> package scan -> external consumer -> runtime test
```

One selected native component does not create another engine binary. Python/Node/JNI modules and
the C ABI shim are additional loadable binaries. STATIC describes the engine linkage; it does not
turn a `.node`, Python extension, JNI library or P/Invoke shim into a static language package.
Static engine objects used by Linux modules must be position-independent. Selecting any of the
four bindings, including C# alone, enables `POSITION_INDEPENDENT_CODE` on the static engine target.

## Provisioning sequence and failure: PKG-PROVISION-02

1. Resolve `--language`/`--component` selections to native options, dependencies and verification
   toolchains. Host tools run on the host even when libraries target another architecture.
2. Detect suitable installed verification tools first; otherwise use verified cache/provisioning.
   Fetched Node/JDK/CPython/.NET tools stay in the bootstrap cache and never become vendor payloads,
   install runtime dependencies or SBOM entries merely because tests need them.
3. Verify dependency version, license, hash/source identity and target platform/architecture.
   Offline mode rejects a missing required cache item. `--no-fetch-toolchain` prohibits fetching a
   missing tool, and `--check` verifies available material without provisioning it.
4. Write the generated user preset unless disabled. Do not hand-edit generated files: change the
   selection, manifest or generator. A preset records roots/options; it is not a package or hardware result.
5. Configure consumes only those local inputs, then build/CTest uses the selected runtimes. Cross
   compilation supplies target Python/JNI headers separately; a successful target compile cannot run
   an ARM64 module inside an x86-64 interpreter. Managed Java/C# artifacts skipped by cross build
   need separate preparation before claiming a complete language package.

Do not reuse one dependency/build directory across incompatible target identities. A configure error
is not repaired by downloading from inside CMake or silently turning off a requested component.
Dependency archive extraction rejects links; toolchain extraction preserves only validated internal
relative links. Network pin verification stays opt-in, outside default offline gates.

## Installed layout and loading: PKG-LAYOUT-03

| Path beneath prefix | Contents / owner |
| --- | --- |
| include/wse/api/wse | Core public headers, including binding/C ABI declarations |
| include/wse/api/{xpt,gef,iui,oui,tmr} | Only selected public component headers; OS/private implementation headers excluded |
| bin | Windows engine/runtime DLLs; selected Node, JNI and C ABI shared modules on their supported platforms |
| lib | Native static libraries, Windows import libraries, Linux shared engine library |
| cmake | WonderStewEngineConfig and WSETargets imports |
| lang/python/wse | Python extension, loader, typing and py.typed |
| lang/js | JS loader, TypeScript declarations and package metadata |
| lang/java | Java 17-compatible wse.jar when produced by the build |
| lang/cs | net8.0 managed assembly and XML documentation when produced |
| vendor | Selected dependency payloads, licenses and identity metadata; no verification toolchain |
| wse-package.json | Package identity and selected component/binding catalog |

Headers have one physical copy. `WSE::Core` publishes both `include` and `include/wse/api` search
roots; the selected native component facades inherit these through their link to Core. STATIC may also require
private backend link dependencies at final link: XPT reconstructs its packaged curl target; Linux OUI
locates Vulkan/Wayland/DRM; libcamera follows its enabled package policy. These are link requirements,
not third-party types in the public API. A SHARED consumer need not recreate the same static backends.

**PKG-BINDLOAD-06:** Linux loadable bindings resolve bundled libcamera from their own installed
location for both SHARED and STATIC engines. Python uses an origin-relative path three levels up
to `vendor/libcamera/lib`; Node, JNI and the C ABI shim use one level up. A static engine archive
cannot supply its own runtime search path to the module that absorbs it.
`wse.binding.installed_libcamera` creates a fresh install prefix, clears build-time loader overrides,
imports the enabled Python/Node packages, and loads the enabled JNI/C ABI binaries. It checks the
mapped libcamera image comes from that prefix. JNI/C ABI loading alone does not exercise managed
methods or camera hardware. The [installed-load test](../../../test/package/installed_libcamera_runtime_contract.py)
is enabled only for native Linux standalone builds that bundle libcamera and have binding runtimes.

`WSE::Dotnet` is the imported C ABI shared shim, not a native component facade. It currently exports
neither include search roots nor a transitive `WSE::Core` link. A direct C/C++ C ABI client links
`WSE::Dotnet` and explicitly adds the installed `include/wse/api` directory for
`#include <wse/capi/wse_capi_core.h>`. For example, with `WSE_PACKAGE_PREFIX` set by the caller to
the installed prefix and `cabi_client` already defined:

```cmake
find_package(WonderStewEngine CONFIG REQUIRED
    PATHS "${WSE_PACKAGE_PREFIX}/cmake" NO_DEFAULT_PATH)
if(NOT TARGET WSE::Dotnet)
    message(FATAL_ERROR "This consumer requires the C ABI shared shim")
endif()
target_include_directories(cabi_client PRIVATE "${WSE_PACKAGE_PREFIX}/include/wse/api")
target_link_libraries(cabi_client PRIVATE WSE::Dotnet)
```

The include directory must come from that same installed package. This supplies the public C
declarations; it does not change runtime loader search or add a source/build-tree fallback.

Linux shared Node/JNI/C ABI modules use `$ORIGIN/../lib` for the engine, while the Python module uses
`$ORIGIN/../../../lib`. Windows Python adds the installed bin directory to DLL search. JS resolves its
installed module relative to the loader or `WSE_NODE_ADDON`; C# uses explicit path, `WSE_CAPI_LIBRARY`,
then default probing. A top-level module path alone does not guarantee that every dependent DLL can
be found. Test from outside the source/build directories, with only the intended runtime search roots.

## Consumer selection and identity limits: PKG-CONSUME-04

The generated config currently imports the selected targets but does not implement per-request
`*_FIND_COMPONENTS` validation. A successful `find_package(... COMPONENTS ...)` alone is therefore
insufficient proof that the component is present. Explicitly check and link the required target:

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
if(NOT TARGET WSE::Tmr)
    message(FATAL_ERROR "This consumer requires a package built with Tmr")
endif()
target_link_libraries(camera_app PRIVATE WSE::Tmr)
```

`camera_app` denotes the consumer's existing executable. Pass the installed `cmake` directory via
WonderStewEngine_DIR or the prefix via CMAKE_PREFIX_PATH; do not include source/build-tree headers
as a fallback. The installed package does not provide a WonderStewEngine compatibility target.

`source_commit` in wse-package.json is currently the Git tree identity queried from HEAD, not a hash
of all working-tree bytes; uncommitted edits are not reflected. Dependency catalog entries can
describe known dependencies even when unselected; use component/bundled flags and actual contents.
The manifest alone does not prove freshness, absence of stale files, binary ABI compatibility or
hardware execution. Use a fresh install prefix per selected shape because install does not remove
disabled components left by an older install. Retain the build configuration and validation results
alongside the artifact; do not invent stronger identity guarantees from the field name.

## Verification and reconstruction: PKG-VERIFY-05

Use the [Build Guide](../../en/BuildGuide.md) commands and the external consumer project under
`test/consumer`, with a fresh prefix. The sequence
is configure/build, CTest, install, verify_public_package with exact selected components/bindings,
configure/build the external consumer, and its CTest. Exercise SHARED and STATIC for package changes.
Binding tests run only when enabled and supported by the host; compile-only cross results remain separate.

| Evidence | Establishes | Does not establish |
| --- | --- | --- |
| Bootstrap/offline-kit contracts | Resolver, identity validation, offline failure/extraction cases | Availability of every remote archive without opt-in network checks |
| Public-header/API/deprecation gates | Selected source/header policy | Correct installed directory contents by themselves |
| Package scan | Expected headers, targets, selected binding files and excluded metadata | That every module loads in a clean process |
| External consumer build/CTest | Installed target/header/link/runtime workflow for that shape | Other OS/architecture/library forms |
| Five language quickstarts | Built runtime entry points when enabled | Installed loading unless explicitly pointed at the installed package |

The initial reconstruction task should create a Core-only consumer using only installed public
headers/targets, then deliberately request an absent component and diagnose it using the target
check above. A prepared package is permitted input; implementation sources and in-tree build fallback
are not. [Design Verification](DesignVerification.md) separates this from running existing examples.

## SDK offline-kit identity

The offline tools (`tools/offline/README.md` in the checkout) create `wse-offline-kit-candidate-v2`
and accept `schemaVersion: 2` manifests using the schema shipped beside the candidate.
`wse.repository` is the logical SDK identity `Engine`; `commit` and `tree` identify this SDK
checkout, and the tree must match the installed package's `source_commit`. Checkout folder
names and consuming application identities are not SDK metadata. An application distribution
tool owns its own revision record and ties it to the SDK candidate by checksum.
`bootstrap-manifest.json` describes SDK dependencies and verification tools. Application
package destinations and application build recipes belong to the consuming repository.

## Package licensing

Every installed package retains the public SDK's Apache-2.0 `LICENSE` and attribution `NOTICE`.
`license` in `wse-package.json` is the license expression for the selected SDK/extension combination;
`license_files` lists its accompanying root-level terms and notices. Dependency terms remain separate.
The SBOM generator reads this explicit expression and refuses missing license metadata or files; it
must not silently assign new terms to an older package. Public package verification checks the exact
SDK license and notice bytes. Installed Node package metadata uses the same combined expression.

An overlay supplies `WSE_EXTENSION_LICENSE_IDENTIFIER` and `WSE_EXTENSION_LICENSE_FILE` from its
target hook. A selected overlay without these terms fails configure. Its license is installed as
`LICENSE.extension`, while the public LICENSE and NOTICE remain unchanged. The package and SBOM
record `Apache-2.0 AND <extension-license-identifier>`; this describes separately licensed parts,
not an Apache grant over the extension. The extension identifier and terms come only from the overlay.
The offline fixture in `test/ci/test_licensing.py` exercises public/mixed installs, third-party
attribution and rejection of missing or incorrectly narrowed terms.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-21 | Added public Apache notices and explicit license metadata for public and combined packages. |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
