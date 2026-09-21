# WSE Thread and Ownership Model

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Ownership

- Every resource has one RAII owner. Public handles and frame buffers are either owned values or
  explicitly non-owning identifiers.
- PIMPL owns operating-system handles, COM objects, file descriptors, threads, and callback state.
- Returned frame and binding buffers own their bytes. They do not reference a backend capture buffer
  after the call or callback returns.
- A raw pointer is never an implicit ownership transfer. The public API exposes no raw `void*`.

## Thread safety

- Value types and immutable descriptions may be copied between threads.
- Stateful device, transport, renderer, and runtime owners are not generally safe for concurrent
  mutation unless their component contract states otherwise.
- Synchronous operations define timeout/cancellation through their component context. Cancellation
  does not silently report success.
- Stop and close operations are idempotent. Destruction requests shutdown and joins owned work.
  Finite polling waits do not bound user callback execution or every OS call; a callback that
  never returns can prevent an owner-thread join from completing.

## Callbacks

Each callback API documents the execution thread, retained state, permitted re-entry, unsubscribe
behavior, and exception boundary. WSE retains an owned callable, not a borrowed stack callback.
Removal, stop and destruction have different synchronization guarantees, as listed below.
Component-specific contracts take precedence over a general rule. Exceptions must not cross C, OS
or language-runtime boundaries. Native operations translate supported exceptions, but callback
containment differs by adapter; the C# camera bridge retains callback exceptions in `CallbackException`.
See [Language Bindings](LanguageBindings.md) and [C ABI](CAbiContract.md) for limits.

| Operation | Callback already acquired/running | Return / re-entry contract |
| --- | --- | --- |
| Timer owner-thread `stop()` | Waits by joining | No later invocation starts; owner can restart |
| Timer callback `stop()` | Current invocation continues | Requests stop; owner-side destruction or next start reaps the worker |
| Keyboard `clearCallback()` | An acquired callable can still run | Prevents later acquisition; not an in-flight barrier; snapshot observers and clear are allowed inside callback |
| Keyboard owner-thread destruction | Waits for worker/callback | Callback must not destroy or assign the same Keyboard |
| Log sink unregistration | An acquired registry snapshot can still invoke it | No completion barrier; shared snapshot retains the sink; producer threads may call concurrently |
| CameraSession owner-thread `stop()`/`close()` | Waits for current read, serializes native stop, then joins outside backend lock | Successful stop retains open session; failed native cleanup may close it; close ends session |
| Camera callback `stop()` | Current invocation continues; serializes native stop with owner | Thread-local self identification defers join; next owner-thread stop/start/close reaps worker |
| Managed terminal release | Runtime-specific | Follow LanguageBindings; session close and terminal release are distinct |

A Timer callback must not destroy its own Timer. CameraSession documents a compatibility-safe
self-destruction path, but consumers should retain an external owner and use owner-thread reaping.
No generic self-destruction guarantee is inferred from an API allowing self-stop.

CameraSession prepares callback state before native start and publishes thread ownership before the
worker proceeds. Launch failure stops the backend, or closes it if rollback fails; the original C++
exception propagates. Read exceptions become one terminal callback result. See TMR-CALLBACK-03
in [Tmr Camera](TmrCamera.md) for the exact state and exception contract.

## Worker implementation and locking

The internal [WorkerController](../../../core/wse/utility/wse_WorkerController.h) owns a thread,
mutex, condition variable, running flag and stop-request flag. It does not own a public task
queue. Timer and Keyboard compose it; CameraSession has its own callback-worker state.

```text
owner start -> reap previously finished thread outside lock -> mark running -> launch body
owner stop  -> set stop flag -> notify wait -> take thread under lock -> join outside lock
self stop   -> set stop flag -> notify wait -> return to callback -> body exits
next owner stop/start/destructor -> join finished thread
```

Worker-body exceptions are caught before the thread exits. Joins must not hold the mutex a
worker needs to finish. Component state locks protect snapshots or callback acquisition;
the user callback executes outside the Keyboard state mutex. This does not make concurrent
mutation of every owner safe. XPT and Renderer are caller-confined; cross-thread cancellation
is an explicit separate facility.

The [worker contract](../../../test/characterization/wse_worker_controller_contract.cpp) covers
repeat start/stop, self-stop, wakeup and exception containment. The [Keyboard lifecycle test](../../../test/characterization/iui_keyboard_lifecycle.cpp)
covers owned callback captures and owner replacement, not a physical key event or all acquisition races.

## Binding runtimes

Runtime close is runtime-specific: Node owns worker cleanup; Python Core Runtime close requests
cancellation without joining active calls; a C# Runtime is a synchronous handle owner. Cancellation
does not universally remove queued callbacks. Serialize owner destruction with operations unless
the individual adapter provides an explicit guarantee. GC/finalizers are a fallback; examples use
explicit close, context managers, or `AutoCloseable`.

C# exposes synchronous runtime operations through the flat C ABI. Its owners implement
`IDisposable` with a single `SafeHandle`; examples use `using` or explicit `Dispose()`.
Disposal is idempotent and terminal, and subsequent operations raise `ObjectDisposedException`.
For `WebCamera`, `Close()` ends only the device session and permits reopening the same object;
`Dispose()` releases the owner. See the [Language Bindings](LanguageBindings.md) contract for
the corresponding session-close and terminal-release operations in each language.

Detailed Timer scheduling and Log sink re-entry are defined in [Core Services](CoreServices.md).
Camera callback delivery and backend streaming are separate states: a terminal read error or a
throwing callback can end delivery while `isStreaming()` remains true. Owner-side stop/reap is
required before restart. Renderer handles carry a backend-lifetime generation: move transfers it,
while shutdown/reinitialization and other instances use different generations. Generation reservation
is atomic, but callers must still serialize operations on one renderer. Vulkan currently waits
inside submission; its shutdown wait is not finitely bounded. The Tmr/OUI designs
detail these limits rather than treating a stopped worker or returned fence as universal completion.
Keyboard copy shares the callable between independent workers; access state and snapshot are
separate observations. See [IUI Keyboard](IuiKeyboard.md) for publication order and platform differences.
Component-specific rules are normative in [XPT](XptTransport.md),
[OUI Renderer](OuiRenderer.md), [Tmr Camera](TmrCamera.md), and
[Language Bindings](LanguageBindings.md).

The libcamera completion callback uses a preallocated request ring; metadata processing belongs to
the reader. Failed stop retains pending request owners, and disconnected-pipeline close has no
finite wait guarantee. See TMR-LIBCAMERA-06 in [Tmr Camera](TmrCamera.md).

Windows MF uses task-memory owners for enumeration/name results and a source owner that attempts
Shutdown before reference release. A dedicated native thread constructs, operates and destroys
the backend, so public callback self-stop or later close from another thread cannot move COM
cleanup to the caller. The public callback worker is separate. TMR-MF-STOP-08 in
[Tmr Camera](TmrCamera.md) defines synchronous stack dispatch, asynchronous Flush completion,
the 2,000 ms notification wait and close/reopen after failure; native call duration remains unbounded.
libcamera enumeration and sessions share a manager lease within one WSE runtime; construction and
last-lease destruction are serialized, while the disconnected pending-request limit remains.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
