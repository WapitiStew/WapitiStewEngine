# WSE Error Handling Cookbook

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

Every fallible public operation follows the
[Result and status contract](../design/en/ResultContract.md): a strict `Result`/`Status` carries a
value or an error, never both, and the partial-progress contracts (`TransferResult`, `HttpResult`)
carry a value that stays meaningful alongside the error. Branch on `category()` and `code()`;
`message()` and `nativeCode()` are diagnostic only. This page is the recipe collection: what each
situation looks like at a call site and what the correct reaction is.

## The base pattern

```cpp
const wse::xpt::TransportStatus connected = client.connect( endpoint, context );
if ( !connected.succeeded() )
{
    switch ( connected.error().category() )
    {
        case wse::xpt::eTransportErrorCategory::Timeout:      /* retry or surface */ break;
        case wse::xpt::eTransportErrorCategory::Cancellation: /* stop quietly */     break;
        default:                                              /* surface */          break;
    }
}
```

Reading `value()` on a failed strict result, or `error()` on a successful one, throws
`wse::ResultAccessError` (a `std::logic_error`) - a contract violation in the caller, not a
runtime condition to handle. Use `succeeded()` first, or `valueOr()` for a fallback read.

## Core data types

The Core data surfaces (`Map`, `Matrix_`, `Image_`, `Pixel_`, points, ranges, meshes,
`Homography`) throw standard exceptions for usage violations - `std::invalid_argument`,
`std::out_of_range`, `std::domain_error`, `std::logic_error` - and never for expectable
outcomes. The one expectable computation failure today is a singular matrix:
`Matrix_::tryInverse()` returns `wse::CoreResult<Matrix_>` whose failure carries
`eCoreErrorCategory::Computation` / `eCoreErrorCode::SingularMatrix`.

```cpp
const wse::CoreResult< wse::Matrix > inverted = matrix.tryInverse();
if ( !inverted.succeeded() )
{
    // A singular input is data, not a bug: fall back, regularize, or surface it.
}
```

## GEF file I/O

`BINController` and `CSVController` return `GefStatus` / `GefResult<T>` for I/O, format and
budget failures. Branch on `Io`, `Format` or `Resource` and the stable error code. Allocation and
length failures remain standard exceptions; even error construction can allocate. Empty BIN
`last_index()` is a usage violation (`std::logic_error`).

Use a bounded replacement for reloading a settings table. CSV `read` and `readWithLimits` append
all incoming rows, including headers; `readReplace` replaces only after the whole input succeeds.
BIN reads also replace only on success. Failed reads leave the prior state available.

```cpp
wse::gef::CSVController csv;
wse::gef::ReadLimits limits;
limits.max_input_bytes = 1024 * 1024;
limits.max_rows = 1000;
const auto loaded = csv.readReplace(path, limits);
if (!loaded.succeeded()) {
    // LimitExceeded: adjust the accepted input or budget; the old table is intact.
    // FileOpenFailed / ReadFailed: resolve the input problem and retry.
} else {
    const auto saved = csv.writeAtomic(output_path);
    if (!saved.succeeded()) {
        // The prior destination is intact; resolve the cause and retry this controller.
    }
}
```

`writeAtomic` stages, checks flush/close, then replaces an ordinary file in a trusted local directory.
Direct `write` checks late failures (`Io/WriteFailed`) but can leave partial output, including during
BIN append. Atomic replacement does not promise crash durability, metadata retention or atomic append.
See [GEF File Formats](../design/en/GefFileFormats.md) for exact budgets and recovery boundaries.

## License operations

`License::load`, `Licensekey::genrate`/`load`, and `LicenseWriter::save` report operational and
verification failures - a missing key file, an out-of-period license, an unlicensed device -
through `wse::LicenseStatus` / `wse::LicenseResult<T>` with the stable `eLicenseErrorCategory`
(`Io`, `Verification`) and `eLicenseErrorCode` values. Loading twice in one process is a usage
violation (`std::logic_error`), and an out-of-range writer enum is `std::invalid_argument`.

```cpp
const wse::LicenseStatus loaded = wse::License::load( key );
if ( !loaded.succeeded() &&
     loaded.error().code() == wse::eLicenseErrorCode::LicenseExpired )
{
    // An expired license is an expectable deployment state: run the Free tier or surface it.
}
```

## Timeout

Category `Timeout` (XPT, OUI) means the explicit deadline in the `OperationContext` or the frame
wait expired. The operation stopped; the connection or device is still considered usable unless a
later call reports otherwise. React by retrying with the same or a longer deadline, or by
surfacing the wait to the user. `Tmr` reports a frame that did not arrive in time the same way
through `readFrame( timeout_ms )`.

```cpp
const auto frame = camera.readFrame( 100U );
if ( !frame.succeeded() &&
     frame.error().category() == wse::tmr::eCameraErrorCategory::Timeout )
{
    // No frame inside 100 ms. The stream is still running; poll again.
}
```

## Cancellation

Category `Cancellation` means your own `CancellationSource` fired. It is a cooperative, expected
outcome - do not log it as a failure, and do not retry it. A pre-cancelled context makes the
operation give up promptly without waiting for its timeout.

```cpp
if ( !sent.succeeded() &&
     sent.error().category() == wse::xpt::eTransportErrorCategory::Cancellation )
{
    return; // the caller asked for this
}
```

## Camera startup and callback failures

Windows MF enumeration/name allocations, capability sources and partial open have scoped cleanup
(TMR-MF-INIT-07). An allocation exception can still propagate after cleanup. Perform final close
on the opening owner thread; a Shutdown attempt is not proof of driver recovery. See
[Tmr initialization ownership](../design/en/TmrCamera.md).

For CameraSession callback capture, a callable-copy or thread-launch exception can propagate from
`start(callback)`. Preparation occurs before native start; launch failure after native start rolls
capture back. After catching the exception, check `isOpen()`: retry start if still open, otherwise
reopen explicitly. A rollback failure closes the backend. An exception from the worker's read is
instead delivered once as `ResourceExhausted` (bad_alloc) or `BackendFailure`; stop from the owner
before restarting. Backend streaming and callback delivery are separate states. See
[Tmr callback recovery](../design/en/TmrCamera.md) for shutdown and native-adapter limits.

Libcamera uses transactional request/mapping ownership (TMR-LIBCAMERA-06). After a copy/control
exception or requeue failure, stop and check the result before restarting. Pending requests remain
owned after failed stop; close can wait indefinitely for a disconnected pipeline. See
[Tmr resource and shutdown limits](../design/en/TmrCamera.md).

## Device disconnected

`Tmr` reports removal as category `Device` with code `DeviceDisconnected`. The session is over:
stop the stream, release the camera, and re-`enumerate()` before reopening - the same device may
return under a new description. `WebCamera::lastError()` retains the most recent failure for
callers a constructor or callback boundary separates from the returning call.

```cpp
if ( !result.succeeded() &&
     result.error().category() == wse::tmr::eCameraErrorCategory::Device )
{
    (void)camera.stop();
    (void)camera.close();
    // enumerate() again before reopening.
}
```

`OUI` reports the GPU equivalent as code `DeviceLost` under category `Backend`: shut the renderer
down and initialize a new one; existing handles are gone.

## Unsupported backend or operation

Category `Unsupported` means the platform, backend, or operation combination cannot work - it is a
configuration outcome, not a transient. Do not retry; select another backend (for example the
software adapter), reduce the request, or surface the limitation. The support matrix records what
each platform carries.

## Invalid lifecycle

Category `Lifecycle` (OUI, Tmr) and `Validation` mark calls the current state or arguments never
allowed: starting an unopened camera, using a destroyed texture handle, a reversed range. These
are caller bugs; fix the order of calls rather than handling the error at runtime. The renderer's
repeated-`initialize` contract is the model: an identical configuration succeeds idempotently, a
different one is refused with `AlreadyInitialized`.

This comparison includes the exact `adapter_name` string, even if two requests would select the
same physical adapter. Stop using the old resources, call `shutdown()`, and then initialize the
new configuration. Move transfers the backend and its handles to the destination; it does not
create new resources. Handles from another renderer or a prior initialization return
`ResourceNotFound` on a ready backend. Recreate them from their descriptions; do not edit their
value/generation fields or retry the same invalid identity. See [handle lifetime](../design/en/OuiRenderer.md).

OUI Vulkan has one recoverable lifecycle case: after a submission wait times out, later mutating
operations may return `Lifecycle/ResourceInUse` while the GPU still uses retained resources.
Keep the handles and retry the rejected operation later; each attempt polls completion without
waiting. A timed-out readback returns no frame. Once completion is observed, a new readback can
succeed. Do not automatically replay a failed draw: it may already have executed. DeviceLost
requires shutdown and reinitialization. See the [renderer failure sequence](../design/en/OuiRenderer.md).

## Partial transfer

`TransferResult` and `HttpResult` are the only shapes where the value is meaningful beside the
error. A failed `send` still tells you how many bytes left; a 4xx/5xx HTTP exchange still carries
the response. Read both sides.

```cpp
const wse::xpt::TransferResult< std::size_t > sent = client.send( payload, context );
if ( !sent.succeeded() )
{
    const std::size_t delivered = sent.value(); // bytes the peer may already have
    // Decide between resume-from-offset and abandon; delivered bytes are not undone.
}
```

## Binding exceptions

Python WseTransferError, Java TransferException, JavaScript TransferError fields and C#
TransferException preserve failed-send counts and truncated UDP prefix/source (BIND-TRANSFER-05).
Existing base exception handlers still catch them. Read the count before deciding how to recover;
never automatically replay the original payload. See [binding error values](../design/en/LanguageBindings.md).

Runnable prefix examples: [Python](../../example/python/xpt/udp_loopback.py),
[JavaScript](../../example/js/xpt/udp_loopback.js), [Java](../../example/java/xpt/UdpLoopbackExample.java).

The language bindings convert every error through one normalization (`binding::Error`) and throw
idiomatic exceptions - Python `WseError(RuntimeError)`, Java/C# `WseException`, JavaScript `Error`
with attached fields - all carrying the same numeric `category`, `code`, and `nativeCode` the C++
caller would branch on. HTTP failures throw subclasses that attach the response, mirroring
`HttpResult`. The C ABI returns `wse_capi_status` by value and keeps the message in
`wse_capi_last_error_message()`, valid on the calling thread until its next C ABI call.

For C# sends, catch TransferException before WseException when the completed count matters.
For truncated UDP receives, it also carries a detached prefix and source. Neither a known
count nor a cancellation proves remote acceptance or permits automatic replay.

```csharp
try
{
    using UdpDatagram packet = receiver.ReceiveFrom(3, context);
    byte[] complete = packet.Payload();
}
catch (TransferException error) when (error.Code == (int)TransportErrorCode.DatagramTruncated)
{
    byte[] prefix = error.ReceivedData!;
    Endpoint source = error.SourceEndpoint!.Value;
    // Detached managed bytes; no Dispose. The discarded suffix is gone.
}
```

The [runnable UDP example](../../example/cs/xpt/UdpLoopbackExample.cs) verifies this path.
Allocation/adoption failures can still replace the operation error while cleaning up;
they do not guarantee a recoverable payload. See [C ABI](../design/en/CAbiContract.md).

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
