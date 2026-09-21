# WSE Flat C ABI Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope and structure: CABI-OWNER-01

The installed [aggregate header](../../../api/wse/capi/stew.h) describes a C-compatible boundary
used by C# P/Invoke. It delegates to the public Core/component facades; it exposes no platform
backend. The shared shim is selected by `WSE_BUILD_DOTNET_BINDING`, even with STATIC Core.
Headers being installed does not mean the shim was built. See [Build Packaging](BuildPackaging.md).

| Layer | Source | Role |
| --- | --- | --- |
| Public representation | [wse_capi.h](../../../api/wse/capi/wse_capi.h), [Core functions](../../../api/wse/capi/wse_capi_core.h) | Calling convention, status, opaque types, version, buffer and cancellation API |
| Common bridge | [capi_internal.h](../../../lang/cs/native/capi_internal.h), [capi_internal.cpp](../../../lang/cs/native/capi_internal.cpp) | Concrete holders, exception guard, shared thread-local diagnostic, string copies |
| Core operations | [wse_capi_core.cpp](../../../lang/cs/native/wse_capi_core.cpp) | Runtime/FrameBuffer/Cancellation ownership |
| Components | [XPT](../../../lang/cs/native/wse_capi_xpt.cpp), [IUI](../../../lang/cs/native/wse_capi_iui.cpp), [Tmr](../../../lang/cs/native/wse_capi_tmr.cpp), [OUI](../../../lang/cs/native/wse_capi_oui.cpp) | Typed conversion and delegation, compiled with the selected component |
| Managed bridge | [NativeMethods.cs](../../../lang/cs/Wse/NativeMethods.cs), [FrameBuffer.cs](../../../lang/cs/Wse/FrameBuffer.cs) | Sequential layouts, P/Invoke, error capture, SafeHandle ownership |

```text
C / P/Invoke caller -- owns --> opaque C holder -- owns --> public C++ value/owner
                            destroy exactly once
C# object -- owns --> SafeHandle -- releases --> opaque C holder
input pointer -- borrowed for call --> copy into native-owned storage
output buffer -- owned by caller <-- copy from native-owned storage
```

An opaque handle is a pointer to an incomplete struct, not a validated integer resource ID.
Only a live handle returned by its matching API is legal. `destroy(NULL)` is harmless; destroying
the same non-null address twice or calling through a freed/foreign pointer is invalid. Clear the
caller's variable after destruction. The wrapper's idempotent Dispose comes from SafeHandle,
not from native double-destroy detection. Native callers serialize owner operations and destruction.
Managed opaque inputs are passed as SafeHandle: P/Invoke retains the owner until that native call
returns. Optional cancellation pointers, including the pointer inside an XPT operation context,
use a [NativeHandleLease](../../../lang/cs/Wse/NativeHandleLease.cs) with DangerousAddRef/Release
for the enclosing synchronous operation. Dispose releases its reference; the last active call/lease
performs native destruction. It does not cancel the operation or wait for every external caller.
An overlapping call may finish or throw ObjectDisposedException. Two-call queries may fail on the
second call if disposal intervenes. This lifetime protection does not serialize mutable operations;
retain the caller-confined rule and serialize camera Start/Stop/Close and owner destruction.

## Representation and versions: CABI-LAYOUT-02

Windows uses `__cdecl`; symbols use C linkage. Linux uses its native C calling convention.
`wse_capi_bool` is int32, zero false and nonzero true. `size_t` is native pointer-width, not uint32;
`wse_capi_user_data` is intptr_t. Do not marshal these as a one-byte Boolean or a fixed-width size.

| Status field | C type | Offset / size in the ABI snapshot |
| --- | --- | --- |
| category | int32_t | 0 / 4 bytes |
| code | int32_t | 4 / 4 bytes |
| native_code | int64_t | 8 / 8 bytes |
| Entire status | struct | 16 bytes |

The following snapshot covers every public value struct on the 64-bit Windows/Linux ABI.
Names below have the `wse_capi_` prefix. Field order, individual offsets and field sizes are
listed in [CapiLayout.def](../../../test/support/CapiLayout.def); the C and C++ compilers check
that snapshot independently. The managed test compares actual C measurements with `Marshal.SizeOf`,
`Marshal.OffsetOf`, inline-array lengths and the alignment of each nested managed struct.
The inventory gate rejects newly added or omitted public structs/fields.

<div class="wse-cabi-layout" style="max-width:100%; overflow-x:auto;"><div style="min-width:34rem;">

| Value struct | Size (bytes) | Alignment (bytes) |
| --- | --- | --- |
| <span style="white-space:nowrap;">`status`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`components`</span> | 24 | 4 |
| <span style="white-space:nowrap;">`keyboard_state`</span> | 672 | 4 |
| <span style="white-space:nowrap;">`projection_vertex`</span> | 16 | 4 |
| <span style="white-space:nowrap;">`renderer_color`</span> | 16 | 4 |
| <span style="white-space:nowrap;">`edge_blend`</span> | 20 | 4 |
| <span style="white-space:nowrap;">`projection_layer`</span> | 104 | 8 |
| <span style="white-space:nowrap;">`projection_request`</span> | 72 | 8 |
| <span style="white-space:nowrap;">`projection_frame`</span> | 272 | 8 |
| <span style="white-space:nowrap;">`operation_context`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`camera_format`</span> | 20 | 4 |
| <span style="white-space:nowrap;">`camera_stream_configuration`</span> | 28 | 4 |
| <span style="white-space:nowrap;">`camera_control_value`</span> | 16 | 8 |
| <span style="white-space:nowrap;">`camera_control_capability`</span> | 72 | 8 |
| <span style="white-space:nowrap;">`camera_usb_identity`</span> | 12 | 4 |
| <span style="white-space:nowrap;">`camera_frame_description`</span> | 24 | 8 |

</div></div>

These are 16 structs and 87 fields, including optional components even in a Core-only build.
The numeric snapshot is scoped to 64-bit targets; this is not 32-bit or ARM64 execution evidence.
`enum wse_capi_keyboard_access_state` retains its tag and values; use
`wse_capi_keyboard_access_state_value` when a typedef name is needed. It differs from the
function `wse_capi_keyboard_access_state`, as C requires. See the
[migration note](../../en/DeprecationMigration.md#c-abi-keyboard-type).

Category values 0..11 are None, InvalidArgument, NotFound, InvalidState, InputOutput, Timeout,
Cancellation, Protocol, Security, Unsupported, ResourceExhausted and Internal. Code remains
component/operation-specific; compare it together with category and operation context.
These in-process structures are not a packed, endian-independent file or network protocol.

`WSE_CAPI_ABI_VERSION == 1` and binding ABI version 1 are separate contracts. Query
`wse_capi_abi_version()` before other use, compare with the compiled header, and reject mismatch.
The managed NativeAbiVersion property exposes the query; it is not automatic negotiation of layouts.
Adding compatible functions does not increment either version; changing layout/signatures/meaning
requires the documented [version gate](VersionCompatibility.md).

## Status, diagnostics and outputs: CABI-ERROR-03

Fallible operations return status by value; category None is success. Destroy functions, ABI version,
the diagnostic pointer and several pure value helpers have other return types; not every export
returns status. A successful status-producing call clears the calling thread's previous diagnostic.
Failure records it in one fixed thread-local buffer shared by all component bridges.

`wse_capi_last_error_message()` borrows a UTF-8 NUL-terminated pointer. Copy it immediately on the
same thread, before another status-producing call (including a size query or cleanup that returns
status). It is not an independently owned error object. Another thread has a different diagnostic.
Branch on the saved category/code; messages and native codes are diagnostic, not stable text.

The common guard maps bad_alloc to ResourceExhausted and other C++ exceptions to Internal, with
code zero. The guard and diagnostic writer are noexcept and perform no diagnostic allocation.
Each thread reserves 1024 bytes: up to 1023 message bytes and a NUL. Longer valid UTF-8 messages
are truncated before a partial code point; category/code/native_code are preserved. Truncation has
no extra marker and does not change two-call getters for ordinary data strings. A malformed input
message is not repaired. This removes secondary allocation from catch handling, without proving
resource rollback at every native or managed allocation site.
Neither guard nor NULL validation catches invalid memory addresses or arbitrary foreign exceptions.

Initialize every output handle to NULL and scalar output to a known value before a call. Many current
creation/error paths leave outputs untouched until success. Do not pass a live owned handle as an output slot without first
arranging its release. Output behavior is operation-specific:

| Operation | Meaningful output on failure |
| --- | --- |
| Ordinary strict-result create/read/convert | No new owned value; do not read an uninitialized output |
| TCP/Serial Send, UDP SendTo | Count zeroed before validation; known native progress retained beside an error; not a peer acknowledgement |
| UDP receive_from_with_progress | Owned prefix/source datagram on DatagramTruncated; destroy it even on failure. Existing receive_from remains success-only |
| HTTP execute with HttpStatusError | Owned response handle, including body/status/headers; destroy it even though status failed |
| HTTP argument/context/transport failure | No response; initialize the slot yourself because validation may precede internal clearing |
| Size-query/copy functions | Required-size and written-count behavior below |

The HTTP shim disables transient retries and marks requests non-idempotent. A 4xx/5xx response is
not interchangeable with a transport failure. [XPT](XptTransport.md) defines partial progress.
CABI-TRANSFER-07 below defines retained progress and the compatible UDP entry points.

## Two-call copying: CABI-BUFFER-04

| API family | Query with destination NULL | Too-small non-null destination | Success |
| --- | --- | --- | --- |
| UTF-8 string through copyString | Required bytes include trailing NUL; capacity ignored | InvalidArgument, required size retained, destination untouched | Copy bytes plus NUL |
| Core frame_buffer_copy_to | Exact payload size, no terminator | InvalidArgument, written count becomes 0, destination untouched | Copy exactly payload bytes |
| XPT byte getter through copyBytes | Exact payload size | InvalidArgument, required size retained | Copy exactly payload bytes |

Always provide a non-null size/count pointer. An earlier invalid owner check may leave it unchanged.
For the current version string `1.0.0`, query returns 6; capacity 5 fails without truncating; capacity
6 copies `31 2e 30 2e 30 00` in hexadecimal. Querying an empty payload returns 0; no terminator is added.
Allocation and the second call belong to the caller. Retain the source owner and serialize mutation
across both calls; this is not an atomic snapshot of a mutable object.

For a buffer input `[0x00,0x7f,0x80,0xff]`, runtime_copy_frame owns an independent copy. Mutating the
input and destroying the Runtime do not change the returned FrameBuffer. Query 4, allocate 4,
copy 4, then destroy that buffer separately. The Runtime is not the lifetime owner of all its outputs.

## Cancellation and callbacks: CABI-CALL-05

The cancellation holder owns both binding and (when selected) XPT sources. Cancel requests both;
tokens retain their shared flags. Cancel is repeatable and sticky; create a new source for a fresh
operation. Keep owner/handle storage valid during native calls and serialize destruction. Runtime
wait observes cancellation in up-to-5-ms sleep slices and checks again at completion, including a
zero-duration wait. Scheduling and OS delays mean 5 ms is not a strict response-time bound.

Camera callback execution is on the camera worker. A successful callback receives a newly owned
camera-frame handle and must destroy it exactly once or transfer it into an owner. On error the
frame is NULL and status/diagnostic are meaningful on that worker thread. The intptr_t user token is
opaque to the library; the caller retains both callback function and token target until owner-side
Stop has joined, then releases registrations. A callback-side Stop is not that join barrier.
The managed delegate is rooted on the Camera SafeHandle through native destruction; failed Start
restores the previous root, and callback-side Stop does not clear it. Owner-side Close or a subsequent
successful Start can release/replace that root. Do not Close or Dispose the managed owner inside its callback. See [Tmr](TmrCamera.md) for delivery versus
streaming and [Language Bindings](LanguageBindings.md) for managed exception behavior.

## Managed output adoption: CABI-ADOPT-06

[NativeOutput](../../../lang/cs/Wse/NativeOutput.cs) is a temporary owner allocated before a native
create/output call. Its pointer starts at zero; even error returns with a response are owned by
that scope. Allocate the destination wrapper and SafeHandle while this owner remains live, then
move the pointer with `Take()` immediately into `Attach()`. Do no allocating work between those
two calls. The emptied temporary owner does nothing; the SafeHandle becomes the sole owner.
If wrapper allocation, status-to-exception conversion or adoption fails first, scope disposal
destroys the unpublished native output without waiting for garbage collection.

Post-adoption frame/selector metadata failure disposes its SafeHandle before rethrowing. Device
and selector collections are all-or-nothing: if element creation or insertion fails, dispose every
previously created element and the native list. On successful return each element belongs to the
caller. HTTP status failures retain their response inside `HttpStatusException`; if creating that
exception fails, dispose the response. A transport failure may leave the initially zero slot untouched.
Projection disposes its adopted FrameBuffer if adapter-name/result construction fails, and releases
a newly pinned array if adding its GCHandle to the cleanup list fails.

[CameraCallbackRegistration](../../../lang/cs/Wse/CameraCallbackRegistration.cs) has a non-allocating
raw-pointer fallback before allocating a temporary owner. Catchable managed adoption, metadata and
application exceptions stay inside the reverse-P/Invoke bridge. The first is exposed as
`WebCamera.CallbackException`; that registration suppresses subsequent delivery and requests Stop.
Even if that request fails, later frames are destroyed without invoking the application. Owner-side
Stop is still required to join before restart. Close/Dispose retain the recorded exception; successful
Start clears it and failed Start restores the previous registration. Serialize lifecycle operations.
An ordinary native error delivered to the application is not itself CallbackException; only a managed
exception thrown while handling it is recorded. The property may be read to observe worker failure.

Ownership passes to the application immediately before entering its callback. A frame it retains
and then throws remains its responsibility; the bridge must not revoke that ownership. Use `using`
unless intentionally retaining the frame. Do not Close/Dispose the camera from its callback.
Fatal CLR failures, process termination and general heap exhaustion are not recovery guarantees.

The native [delivery helper](../../../lang/cs/native/camera_callback.h) guards frame-handle creation:
bad_alloc reports ResourceExhausted, other C++ construction exceptions report Internal, with a NULL
frame and the worker's TLS diagnostic. It notifies once, then an internal marker exits through the
CameraSession worker's C++ callback-exception boundary. The marker does not cross the exported C ABI
or a managed boundary. Delivery termination alone does not stop native streaming: the owner must
Stop/join. A successful frame is already receiver-owned when its callback starts; that callback is
outside the allocation guard, preventing a thrown receiver exception from causing a second event.

## Partial transfer at the C boundary: CABI-TRANSFER-07

TCP/Serial Send and UDP SendTo zero a non-NULL count output before validating other arguments.
After the native operation returns, [publishTransferCount](../../../lang/cs/native/transfer_result.h)
publishes its completed count before mapping an error; it does not branch past the count on failure.
Known completed bytes can therefore coexist with TimedOut, Cancelled or another failure. A count is
not a peer acknowledgement. A cancelled pending serial write can affect the wire beyond completed
chunks; a C++ exception before a TransferResult returns leaves zero, which is not proof of no effects.
If diagnostic conversion itself fails after count publication, the outer guard changes the status
to ResourceExhausted/Internal while retaining the count. The adapter does not retry automatically.

`wse_capi_udp_client_receive_from` publishes an owner only on success.
`wse_capi_udp_client_receive_from_with_progress` clears a non-NULL output before
validation and returns an owned datagram on success or DatagramTruncated. On truncation it owns
exactly the stored prefix and original source endpoint; the consumed suffix cannot be read later.
Destroy the datagram even though status failed. Other errors leave NULL. Frame-holder or diagnostic
conversion allocation failure also leaves NULL; the consumed datagram is not replayed or recoverable.
An internal unique_ptr owns the result until both construction and status conversion succeed.
Copy the failing TLS diagnostic before calling payload/source getters, which clear it on success.
Both entry points use the documented public value layout and ABI version 1; use matched managed/native build artifacts.

C# `TcpClient.Send`, `SerialPort.Send` and `UdpClient.SendTo` throw `TransferException` for native
failure, preserving `BytesTransferred` as ulong without narrowing the native size_t on the error path.
The exception derives from WseException, so existing base catches remain valid. Failed UDP ReceiveFrom
uses `wse_capi_udp_client_receive_from_with_progress`: DatagramTruncated throws TransferException with detached `ReceivedData`
and `SourceEndpoint`, and BytesTransferred is the prefix length. The native datagram is disposed before
the throw completes; the exception has no native owner and needs no Dispose. Send failures have null
ReceivedData/SourceEndpoint. Ordinary UDP timeout/argument failures retain the existing WseException
shape and fabricate no payload. These rules do not turn pre-call argument/disposal or allocation
exceptions into TransferException. Failure while copying/adopting the prefix or building its exception
releases the pending native owner and propagates that failure; it does not guarantee recoverable bytes.

The current C++ TCP/Serial Receive paths return their first completed chunk on success and an empty
value on error, including failed/cancelled pending Windows reads. The adapters do not fabricate an
error prefix that the native result did not expose. Python, JavaScript and Java expose their progress/prefix through
the separate runtime adapters described by BIND-TRANSFER-05 in [Language Bindings](LanguageBindings.md).
The C ABI gates below remain scoped to their own conversion and ownership paths.

Reproduce with `wse.capi.transfer_contract` (27 production-helper cases),
`wse.capi.udp_progress_contract` (loaded C ABI and UDP loopback),
`wse.binding.dotnet_transfer_contract` (25 synthetic managed cases, including 5 rollback boundaries),
and `wse.binding.dotnet_contract` (real-shim UDP and TCP send/cancellation). Synthetic handles detect
leaks/double release and getters deliberately overwrite diagnostics. Positive progress within a
failed send is deterministic fixture evidence; socket send-buffer sizes are not used as a portable
way to force short writes. No serial hardware, whole-heap exhaustion, wire-level delivery or all
native I/O failures are certified by this matrix.

## Evidence and open limits

- `wse.capi.abi_snapshot` and `wse.capi.c_layout_snapshot` compile the complete value-layout snapshot
  as C++ and C. The C fixture also prints measured layouts for the managed test; no shim is loaded by these two tests.
- `wse.capi.runtime_contract` loads the built shim through public Core C calls and checks two-call
  strings/buffers, independent ownership, thread-local diagnostics and sticky cancellation.
- `wse.capi.allocation_contract` compiles the production common/Core bridge and facade sources into an executable and
  sweeps intercepted `operator new` failures in runtime creation, cancellation creation and frame copying.
  It verifies unpublished outputs on failure, recovery on the next allocation position, and diagnostics
  under persistent allocation failure, including long UTF-8 messages, unknown exceptions and TLS isolation.
  The isolated MSVC fixture disables debug iterator bookkeeping: that STL can allocate a container
  proxy inside a noexcept move and terminate before any C ABI guard can catch it. Production build
  flags are unchanged. MSVC debug-STL allocation exhaustion, aligned/native allocators and every
  component's resource rollback are outside this injection scope.
- `wse.binding.dotnet_contract` compares 16 C/C# layouts and 87 fields, checks last-lease release
  ordering and idempotence, and runs 64 Core/frame/cancellation Dispose races. A source inventory gate
  checks all 96 direct opaque input parameters that use SafeHandle. Component/HTTP checks run when enabled.
- `wse.binding.dotnet_ownership_contract` uses a synthetic C ABI provider with exact live-handle
  accounting and double-destroy rejection. It checks 53 injected rollback cases across constructors,
  returned owners, partial collections, metadata, HTTP exception creation, Projection result/pin
  registration, plus 8 native-worker reverse-P/Invoke cases. It verifies retained frames, late-event
  suppression, stop failure and registration reset. It does not load a physical camera or inject
  real CLR heap exhaustion; faults occur at named managed boundaries.
- `wse.capi.camera_delivery_contract` executes the production delivery helper/guard in 5 cases:
  owned delivery, native allocation failure, two construction failure categories and receiver throw.
  It checks a single error event and internal stop marker; CameraSession worker containment is
  separately covered by the existing camera start/stop contract.
- Complete native-I/O stress, native device allocation and device-worker shutdown races remain
  separate work. These no-device tests do not certify camera hardware or exhaustive concurrency/OOM.

To reproduce, enable `WSE_BUILD_DOTNET_BINDING=ON` with the host .NET SDK on an already bootstrapped
Windows or Linux preset, build it, then run `ctest --test-dir <build-directory> -C Debug
--output-on-failure -R "wse.capi.|wse.binding.dotnet.*contract"`. STATIC Core still uses a shared shim.
The normal full suite additionally checks the header inventory and documentation; keep physical opt-ins disabled.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
