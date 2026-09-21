# WSE Public API Policy

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose

This document defines public API rules shared by Core, XPT, GEF, IUI, OUI, Tmr, and the language
bindings. A narrower component design takes precedence where applicable. Future compatibility
removals follow the version policy; the [deprecation and migration guide](../../en/DeprecationMigration.md)
and machine-readable ledger record replacements and SemVer-major removal conditions.

## Error boundary

- New fallible operations return the component `Result<T>`/`Status` and stable categories/codes.
  Messages and native codes are diagnostic and are not branching contracts. The canonical
  construction and state rules are the [Result and Status contract](ResultContract.md).
- `fromXptError()`, `fromOuiError()`, and `fromTmrError()` are the sole canonical conversions into
  the binding error contract. Language adapters do not duplicate category switches.
- The C++ native-code accessor is `nativeCode() const noexcept`.
- Modern XPT/OUI/Tmr/binding APIs do not leak operating-system exceptions. Core usage violations use
  standard exceptions; expected operational failures use the appropriate component result.

## Unchecked data access

The mutable and const `operator[]` overloads of Core data classes are supported public APIs.
Preserve their direct, non-owning memory access contract defined in [Portable Core](PortableCore.md#fast-data-access).
They do not require a Result wrapper or automatic bounds checks.

## Logging

`formatLogMessage()` reuses the standard Logger tag, timestamp, filename and line layout without dispatching to any sink. Caller-owned asynchronous loggers may use this formatting entry point; their output failures remain the caller's responsibility.


- Library code does not emit diagnostics directly through `std::cout`, `std::cerr`, `printf`, or a
  platform debug channel. Only WSE logging sinks perform console or file output.
- Structured records carry level, component source, message, and optional details. Credentials,
  private addresses, device serials, protocol dumps, camera frames, and model-specific data are not
  logged by default.
- Returning an error and logging are distinct decisions. A recoverable caller-facing error is an
  error value and does not force duplicate library logging.

## Constness, ownership, and threading

- Observers are `const`; simple non-failing observers are also `noexcept`.
- Getters return owned values or const views by default. Mutable access is explicit in name and type.
- Owners are non-copyable/movable or explicitly shared and use one RAII owner for PIMPLs, native
  handles, threads, and callback state. Raw pointers are not owners.
- Every callback API documents execution thread, retention, removal synchronization, reentrant
  operations, and exception behavior in its component design.

## Function parameters

A parameter's name says what it is for, its position says how urgent it is to the reader, and its
type says who may write to it. The three agree, so a call site can be read without opening the
declaration.

- Every parameter carries a direction suffix: `_in` for input, `_out` for output, `_inout` for a
  parameter that is read and then written. A pointer parameter also carries a `p_` prefix, so the
  name states the passing kind as well as the direction.
- Parameters are ordered `[in,out]`, then `[out]`, then `[in]`. A member function's `this` is the
  implicit in-out, so its explicit parameters run `[out]` then `[in]`.
- An `_out` or `_inout` parameter is a pointer. The implementation aliases it to a reference on its
  first line, so the body reads as a reference while the call site still shows, at the call, that the
  argument is written:

  ```cpp
  bool coreFormatOf( wse::ePixFormat* p_format_out, const eCameraPixelFormat format_in ) noexcept
  {
      wse::ePixFormat& format_out = *p_format_out;
      ...
  ```

- An `_in` parameter of a non-trivial type is a `const` reference. An arithmetic type, an enumeration,
  a pointer, and an opaque handle such as `sTextureHandle` stay `const` by value, because a reference
  to them costs more than it saves.
- Four exceptions, all forced from outside rather than chosen:
  - A move constructor and a move assignment take an rvalue reference and name it `X&& other_inout`.
  - A flat C ABI function in `api/wse/capi/` cannot take a reference, so its out parameters stay
    pointers. It follows the ordering and the suffix rules like every other function.
  - A function whose address is handed to a foreign callback typedef keeps that typedef's parameter
    list exactly, because the caller is the one that decides the order. Its parameters still carry
    direction suffixes. The three in the tree are the COM `QueryInterface` override in the Media
    Foundation camera backend and the two libcurl write callbacks in the HTTP client; each carries a
    comment saying so, because the shape otherwise reads as an oversight.
  - A `swap` overload keeps its two operands as references, because the standard library decides that
    shape: an unqualified `swap( a, b )` after `using std::swap` only finds an overload that takes
    references. Turning them into pointers would quietly drop the type out of that protocol. The
    names still say the direction, so the signature reads `swap( X& obj1_inout, X& obj2_inout )`.

## Naming and namespaces

- Public declarations live under `wse` or `wse::<component>`. Public headers do not use
  `using namespace` or add declarations to `std`, third-party, or platform namespaces.
- Follow the type-specific naming table in [Coding Rule](CodingRule.md): classes use `PascalCase`,
  structs use an `s` prefix, enumeration types use an `e` prefix, and constants use `UPPER_SNAKE_CASE`.
  Methods use `lowerCamelCase`; simple data-class getters use `snake_case`. The explicitly
  standardized native-code accessor is `nativeCode()`.
- These documented prefixes and getter spellings are not deprecation candidates by themselves.
  For an actual naming correction, add a canonical replacement first, deprecate the old name,
  and record it in the deprecation ledger rather than proliferating parallel spellings.
- A file matches the case of its primary public type. `stew.h` is reserved for component aggregates.

## Header placement and third-party types

- Public headers live under `api/<component>/`; shared values use `api/wse/`; the common binding
  boundary uses `api/wse/binding/`. They never include `core/`, `platform/`, or `vendor/` paths.
- New public APIs do not expose OS headers/types, backend objects, native handles, or third-party
  library types. Adapters copy or convert them into WSE-owned values.
- The third-party lexical baseline is empty. Public data headers do not depend on OpenCV, and
  the scan forbids any baseline entry. The single sanctioned
  third-party surface is the opt-in adapter directory `api/cv/` (`cv/OpenCvAdapter.h`), which the
  scan documents and skips; consumers who include it provide OpenCV themselves.
- The public raw `void*` ratchet baseline is empty: the public API carries no raw `void*` and
  the boundary gate forbids any new entry. It counts `void*` only: a typed `T*` out parameter
  is the convention, not debt.
- A package with an optional component disabled contains none of its headers, symbols, compile
  definitions, or third-party dependency metadata.

## Mechanical gates

`wse.public_api_policy` rejects public namespace pollution, linker directives, internal includes,
unreviewed third-party types, missing `nativeCode() const noexcept` accessors, direct diagnostics
outside logging sinks, and language-specific OUI/Tmr error mapping. It complements
`wse.public_header_boundary`, component ownership gates, and the installed-package verifier. A
baseline update must include its debt-reduction reason, compatibility impact, and design update.
`wse.deprecation_ledger` maps every `[[deprecated]]` declaration and reviewed OUI opaque alias to a
removal floor, migration documentation, and removal gate.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
