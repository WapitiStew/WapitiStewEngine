# WSE Result and Status Contract

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This document defines the current C++ Result/Status representation, access rules and component
boundaries. The implementation is [wse_Result.h](../../../api/wse/utility/wse_Result.h).

## Strict result state

`wse::Result<T, E>` represents either success with a value or failure with an error.
Its public construction API consists of `success(T)` and `failure(E)`; there is no public default,
payload-only or value/error constructor. `failure(E)` rejects an empty error (`E::ok() == true`)
with `wse::ResultAccessError`, derived from `std::logic_error`.

The implementation owns `std::optional<T>` and an `E`. The optional is engaged exactly on success;
a failed result does not construct a `T`. The stored error is meaningful and accessible only on
failure. This is a semantic exclusivity contract, not a union-layout or binary-serialization promise.
The result owns its value/error; references returned by accessors require that result to remain alive
and its storage not to be replaced.

`wse::Status<E>` aliases `Result<void, E>`. This specialization owns a success flag and an error;
it has no payload. Its factories are `success()` and `failure(E)`. Neither a boolean payload nor
a public boolean constructor is available. Result, Status and `succeeded()` are `[[nodiscard]]`.

| Operation | Success | Failure |
| --- | --- | --- |
| `succeeded()` | true | false |
| `value()` on `Result<T,E>` | Const or mutable reference to owned value | Throws `ResultAccessError` with stored category/code/message |
| `valueOr(fallback)` on `Result<T,E>` | Returns a copy of the value | Returns the fallback value |
| `error()` | Throws `ResultAccessError` | Const reference to owned error |

`valueOr` does not throw `ResultAccessError` for a failed result. It is not `noexcept`: copying or
moving `T`, including argument construction, may throw. Factory allocation, value/error construction
and diagnostic-string construction can also throw; the result abstraction is not a universal
exception boundary.

## Component errors and aliases

`E` supplies `category()`, `code()`, `message()`, `nativeCode()` and `ok()`. Error categories follow
the shared 12-value taxonomy. Category and component code are branching contracts; messages and
native codes are diagnostic. [Public API Policy](PublicApiPolicy.md) defines normalization rules.

| Component | Error | Strict result / status |
| --- | --- | --- |
| Core computation | `wse::CoreError` | `CoreResult<T>` / `CoreStatus` |
| License | `wse::LicenseError` | `LicenseResult<T>` / `LicenseStatus` |
| GEF | `wse::gef::GefError` | `GefResult<T>` / `GefStatus` |
| XPT | `wse::xpt::TransportError` | `TransportResult<T>` / `TransportStatus` |
| OUI | `wse::oui::RendererError` | `RendererResult<T>` / `RendererStatus` |
| Tmr | `wse::tmr::CameraError` | `CameraResult<T>` / `CameraStatus` |
| Binding facade | `wse::binding::Error` | `binding::Result<T>` / `binding::Status` |

These names are aliases of the common templates, not independent state machines.
IUI `KeyboardAccessState` is a separate component observation contract; it is not a strict Result.

## Partial progress

`wse::PartialResult<T,E>` owns both a value and an error. Its explicit value constructor supplies an
empty error; its value/error constructor accepts both. `succeeded()` tests `E::ok()`. `value()` and
`error()` are readable in either state: a failed operation may have meaningful progress or response.

XPT `TransferResult<T>` uses this shape for transfer counts/data, and `HttpResult` carries an HTTP
response alongside an error, including completed 4xx/5xx exchanges. Partial values do not imply
complete delivery, acknowledgement or safe replay. Consult [XPT](XptTransport.md) for operation-specific
meaning and retry rules. Strict Result access preconditions do not apply to PartialResult.

## Asynchronous and language boundaries

An asynchronous API that delivers a result by const reference borrows it for that callback;
callers copy any value they need to retain. Callback threads, cancellation, exceptions and shutdown
follow the component design and [Thread Ownership](ThreadOwnership.md).

The `fromXptError` / `fromOuiError` / `fromTmrError` / `fromIuiKeyboardState` adapters normalize into
`binding::Error`. The [C ABI](CAbiContract.md) reports `wse_capi_status` and per-thread
`wse_capi_last_error_message`; language adapters expose the corresponding language error contract.
Ownership and partial-progress transfer follow [Language Bindings](LanguageBindings.md).

## Verification

[The result contract test](../../../test/characterization/wse_result_contract.cpp), registered as
`wse.core.result_contract`, checks success/failure access, empty-error rejection, fallback values,
Status and partial progress. XPT/component tests verify the meaning of actual operation results.
These tests do not establish exhaustive allocation-failure handling or physical-device behavior.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
