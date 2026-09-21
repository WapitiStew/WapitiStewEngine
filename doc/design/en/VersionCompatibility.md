# WSE Version and Compatibility Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

WSE uses Semantic Versioning. The current version is 1.0.0, and it stays 1.0.0 until first
publication. While WSE remains unpublished, the owner may approve a
reviewed in-place change instead of a deprecation cycle; every such change is recorded with its
rationale in the [deprecation and migration guide](../../en/DeprecationMigration.md).

## Change classes

- Patch: compatible fixes, documentation, tests, and implementation changes with no intended public
  source behavior change.
- Minor: additive API, component, backend, or binding capability. Existing names keep their meaning.
- Major: approved incompatible removal or semantic change after migration and release gates pass.

## Compatibility guarantees

The C++ API and the flat C ABI make different promises, because they cross different boundaries.

### C++ API

- Source compatibility is preserved within WSE 1.0.0. A replacement and a migration path precede a
  removal, and the removal itself requires the documented SemVer-major gate.
- Binary compatibility holds only between builds that share the compiler family, toolset, standard
  library, architecture, and library type. Public headers expose standard-library types, so no
  compiler-neutral C++ ABI is promised. The installed package records its toolset and library type,
  and the package configuration rejects a mismatched consumer instead of silently loading.

### C ABI (`api/wse/capi`)

The flat C ABI is the stable boundary between runtimes, and every guarantee below is load-bearing:

- `WSE_CAPI_ABI_VERSION` names the contract. A caller reads `wse_capi_abi_version()` from the
  library it actually loaded and verifies it against the value it compiled with.
- Only opaque handles and fixed-width integer types cross the boundary, and every function returns
  `wse_capi_status` by value: a portable `category`, a component-specific `code`, and the raw
  `native_code`.
- The calling convention is declared explicitly (`__cdecl` on Windows).
- Every handle is caller-owned and is released through its matching `*_destroy` function.
- The failure message from `wse_capi_last_error_message()` is stored per calling thread and stays
  valid until that thread's next C ABI call.
- An incompatible change to any of these - a field layout, an ownership rule, or a numeric
  reinterpretation - increments `WSE_CAPI_ABI_VERSION`.

## Deprecation and removal

Deprecation registration is not a scheduled removal. Removal requires owner approval, at least one
minor release interval with a documented replacement, zero known public/private/hardware consumers
as applicable, warning-as-error package consumers, and a release compatibility review. The machine
ledger and migration details are in the
[deprecation guide](../../en/DeprecationMigration.md).

## Package identity

Installed packages record WSE version, library type, platform, architecture, compiler/toolset,
enabled components, binding ABI, and dependency identities. An incompatible package is rejected
instead of silently loaded. Binding ABI 1 is shared by Node, Python, and Java, and the C ABI serves
the .NET binding; an incompatible field, ownership, or numeric-error reinterpretation requires a new
binding ABI.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
