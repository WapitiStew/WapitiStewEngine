# WSE Agent Guide

This file applies to the entire WSE source tree. Repository-level rules still apply. Read this
file, the documentation index, the relevant normative design, and its tests before changing code.

## Source of truth

- Normative behavior lives in public headers and `doc/design/en/`; English design is canonical.
- Japanese translations, user guides, API references, and migration guides must remain synchronized
  through `doc/DocumentationManifest.tsv`.
- Internal evidence and history live with the controlled material in the extension repository.
  They do not override normative design.
- Update the change-history section whenever a normative document changes.

## Public and private boundary

- Public components are Core, XPT, GEF, IUI, OUI, and Tmr. Language bindings expose only their
  common public facade. The C# binding reaches that facade through the flat C ABI in `api/wse/capi`,
  which is public API and is governed by the same header, policy, and deprecation gates.
- Access-controlled extension names and compatibility code, device profiles, private protocol
  details, and identifying metadata must not enter public documentation, tests, packages, logs,
  commit messages, or sanitized exports. The private denylist belongs outside the public tree.
- Never publish or change repository visibility, merge either repository's `main`, or generate a
  public release without explicit owner instruction.
- 日本語補足: Access-controlled Extension情報は別環境だけで扱い、公開候補へ転記しません。

## Build and dependency rules

- WSE requires C++17 and CMake 3.24+; use CMake presets from the WSE root.
- In dependency rules, Core means the public API under `api/wse`, not every implementation file in
  the source directory named `core`. VPJ may depend on this Core, XPT, and GEF only when a concrete
  feature requires GEF. VPJ must not depend on OUI, IUI, or Tmr. A VPJ consumer links only
  `WSE::Vpj`; XPT/Core are transitive and private third-party backends must not leak.
- Use `windows-msvc-shared-core` for the smallest Windows gate and `windows-msvc-*-public` for all
  distributable public components. On Linux, enable only supported Core/XPT/GEF/IUI/OUI/Tmr combinations.
- Build both `SHARED` and `STATIC` when changing public headers, ownership, packaging, or platform code.
- CMake configure must not access the network. Bootstrap is an explicit, separate operation. Preserve
  offline verification, hashes, versions, and platform/architecture identity.
- Do not hand-edit generated files or bootstrap-managed `vendor/` contents. Change the generator,
  manifest, adapter, or build definition instead.
- `bootstrap.py --language/--component` resolves a selection into dependencies, verification
  toolchains, and a generated `CMakeUserPresets.json`. A verification toolchain (Node.js, a JDK, a
  CPython interpreter, a .NET SDK) is detected before it is fetched, is installed only into the
  bootstrap cache, and must never reach `vendor/`, an install package, or the SBOM. It is selected
  for the host platform and host architecture even during a cross build, and every pinned archive
  carries the digest its vendor publishes. A toolchain archive is extracted with validated internal
  relative links preserved; a dependency archive is still extracted strictly with no links at all.
  `tools/bootstrap/verify_toolchain_pins.py` checks a pinned archive for a host this machine cannot
  run. It needs the network, so it is registered only under
  `WSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION` and labelled `network`; the default gates stay offline.

## Required validation

For a normal public change, run the applicable configure/build and:

```text
ctest --test-dir <build-directory> [-C <configuration>] --output-on-failure
```

The suite must retain public-header, public-API, deprecation-ledger, documentation, ownership,
binding, package, and consumer gates. Package changes also require installation followed by the
external consumer project under `test/consumer` against the modern `WSE::*` component targets.
Run Windows and Linux gates when changing portable code or public documentation commands.

Physical keyboard, display, and camera tests are opt-in. Record `BUILD_VERIFIED`, `CONTRACT_VERIFIED`,
`HARDWARE_SMOKE_PASS`, `HARDWARE_CERTIFIED`, `HARDWARE_NOT_RUN`, `HARDWARE_ON_HOLD`, or
`UNSUPPORTED` precisely; never convert a mock, cross-build, or OS access failure into hardware
certification. Raspberry Pi 4 results are scoped in [Hardware Validation](doc/design/en/HardwareValidation.md):
Core/XPT and public binding smoke evidence is accepted, while unrun physical gates remain
`HARDWARE_NOT_RUN`. Do not generalize one accepted gate to all hardware.

日本語補足: 実機を開くTestは明示許可された機材とOptionだけで実行します。Pi 4の受理済みSmokeと
未実施の実機Gateを区別し、未実施項目は`HARDWARE_NOT_RUN`のまま記録します。

## Change rules

- Preserve source compatibility within WSE 1.0.0. Add a replacement and migration path before removal;
  removals require the documented SemVer-major gate. While WSE remains unpublished the owner may
  waive this for a reviewed in-place change; such a change is recorded in the migration guide with
  its rationale, and the version stays 1.0.0 until publication.
- Follow the function-parameter convention in the public API policy: direction suffixes on every
  parameter, `[in,out]`/`[out]`/`[in]` ordering, pointer out-parameters aliased on the first line of
  the body, and const-reference inputs for non-trivial types.
- Keep ownership in one RAII owner. Document callback lifetime, execution thread, cancellation,
  re-entry, shutdown, and exception/error boundaries.
- Keep platform types and third-party types behind adapters. Do not expose new OS headers, raw owning
  pointers, or dependency types through public headers.
- Keep optional components independently selectable and avoid dependency cycles.
- Update user documentation, examples, tests, and both language versions in the same change.
- Preserve unrelated user changes and keep commits scoped. Do not push an application repository's
  work branch unless explicitly instructed; the Engine integration branch may be pushed only when authorized.

## Documentation checks

`wse.documentation.contract` validates local Markdown links, required entry points, translation
metadata, public presets/options, and all five quick-start sources. `wse.documentation.quickstart_*`
builds or runs C++, Python, JavaScript, Java, and C# samples when their runtimes are enabled. A document
change is incomplete until these gates and `git diff --check` pass.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
