# WSE Core Services

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope and ownership map

Core services provide waiting, periodic execution, logging, license loading and platform
queries. They do not form a global engine scheduler. The application decides startup order,
retains callback dependencies, and stops owned work before releasing those dependencies.

```text
application thread -> Wait -> standard-library sleep
application owns Timer -> Impl -> WorkerController -> one worker
                            +-> interval + owned callback, protected by settings mutex
caller's logging thread -> shared registry snapshot -> output adapter -> LogSink callbacks
application startup -> License::load -> file/decode/verify -> one shared license state
application query -> pickupDeviceInfo -> selected OS adapter -> owned result values
```

Arrows mean invocation or containment as labelled; they do not introduce component link
dependencies. Shared logging/license state belongs to the linked WSE library instance.
Separate copies of a static library in different modules are not a cross-module singleton.

| Service | Public declaration | Implementation / test |
| --- | --- | --- |
| Wait | [Wait](../../../api/wse/utility/wse_Wait.h) | [runtime contract](../../../test/characterization/core_runtime_contract.cpp) |
| Timer | [Timer](../../../api/wse/utility/wse_Timer.h) | [Timer implementation](../../../core/wse/utility/wse_Timer.cpp), [worker](../../../core/wse/utility/wse_WorkerController.cpp), runtime contract |
| Logging | [Log](../../../api/wse/utility/wse_Log.h) | [Log implementation](../../../core/wse/utility/wse_Log.cpp), runtime contract |
| License | [License](../../../api/wse/license/wse_License.h), [key](../../../api/wse/license/wse_LicenseKey.h), [writer](../../../api/wse/license/wse_LicenseWriter.h) | [loader](../../../core/wse/license/wse_License.cpp), [state](../../../core/wse/license/wse_LicenceAdmin.cpp), [license contract](../../../test/characterization/license_contract.cpp) |
| Device queries | [Portable Core](PortableCore.md) | Platform adapters and Linux runtime-contract branch |

## Waiting and scheduling: CORE-SVC-01

`Wait::sec/msec/usec/nsec` block the calling thread using the corresponding chrono duration
and `sleep_for`. They do not pump a GUI loop or provide cancellation. Scheduler delay can
extend the wait; the runtime test checks unit scale, not a real-time upper-bound guarantee.

A Timer starts stopped with a 1 ms stored interval. A successful `start(interval, callback)`
retains the callable and launches one worker. Each iteration is:

```text
lock settings -> snapshot interval -> unlock
waitFor(interval): stop request wakes this wait
if stopped: exit
lock settings -> acquire shared callback -> unlock
invoke callback on worker; catch any exception and exit
repeat from interval snapshot
on exit: release stored callback; worker records not-running
```

The first callback follows the first wait. The interval is a delay between callback completion
and the next dispatch, not a fixed-rate schedule measured from the previous callback start.
A 10 ms interval with a 7 ms callback gives roughly 17 ms between starts, plus scheduling
overhead. No missed-tick queue, parallel callback or catch-up loop exists.

`setInterval` changes the next interval snapshot; it does not wake or shorten a wait that
already captured the previous interval. Both interval setters reject non-positive durations
with `std::invalid_argument`. `start` also rejects an empty callable before testing running
state. Timer acquisition can throw on allocation/thread creation; no structured TimerStatus
or general rollback guarantee exists for those failures.

## Timer lifecycle and cancellation: CORE-SVC-02

The following are conceptual states, not a new public enumeration:

| State / call | Result and next state |
| --- | --- |
| Stopped + valid start | `true`; Running with owned captures |
| Running + valid start | `false`; existing interval/callback unchanged |
| Any + invalid start/setInterval | `std::invalid_argument`; rejected arguments do not replace settings |
| Running + owner-thread stop | Request stop, wake wait, join outside worker lock; Stopped |
| Running + callback stop | Request stop and return to current callback; Stopping |
| Callback returns after self-stop | Release captures; worker exits; Stopped with thread to reap |
| Callback throws | Contain exception, release captures, exit; owner later reaps |
| Stopped + stop | Safe to repeat; joins a finished worker if still owned |
| Stopped + start after self-stop/throw | Reap prior worker before launching next |
| Destruction on owner thread | Request stop and join before releasing Impl |
| Move construction/assignment | Stop source (and assignment destination), copy stored interval; destination remains stopped |

Owner-thread stop may wait for an already acquired callback; it does not forcibly interrupt
user code. When that stop returns, its joined worker can no longer invoke a callback.
Self-stop returns while the current invocation still owns its captures. `isRunning` is a
snapshot of worker state and may remain true until the callback and cleanup finish.

A callback may call `stop` or change its interval. It must not destroy, move or assign its
own Timer. Keep the owner alive and let an external thread reap it. Caller-side lifecycle
operations should be serialized; internal mutexes are not a promise that arbitrary concurrent
start/move/destruction is safe. A callback that never returns can block owner shutdown.
See [Thread Ownership](ThreadOwnership.md) for the shared worker and component exceptions.

## Log record construction and dispatch: CORE-SVC-03

Logging is synchronous on the thread that writes the record. Registry/configuration data
uses a state mutex; console/file output uses a separate output mutex. Custom sinks are called
outside both. There is no implicit logging worker, delivery queue or backpressure buffer.

```text
writeLog(level, tag, source, message, details)
  -> copy profile and construct record (UTC timestamp, severity, strings)
  -> under state lock: filter level and snapshot shared_ptr sinks
  -> under output lock: configured console/file output
  -> without either lock: call each snapshotted sink, contain sink exceptions
  -> return to writer
```

The default minimum level is Info. Off suppresses delivery; records below the threshold
are also suppressed. This is a delivery filter, not a guarantee of zero formatting/allocation
cost. An absent profile means no console/file output but does not suppress registered sinks.
Sink order follows an unordered registry and is unspecified.

| Entry | Profile used | Record behavior |
| --- | --- | --- |
| Five-argument `writeLog` including source | Supplied tag; empty tag becomes WSE | Empty source becomes effective tag |
| Short `writeLog` without source | WSE profile | Supplied record tag (or WSE); source equals record tag |
| `formatLogMessage` | Supplied configuration, without registration | Returns formatted text only; no sink/output dispatch |
| `Logger<Tag>` / ILogger path | Registered tag profile | Formats caller filename/line, then dispatches; missing profile suppresses that path |

The standard formatter strips the directory from its supplied filename. Arbitrary source,
message and details strings are caller data, not automatically redacted. Formatting's local
date/time prefix and the record's UTC timestamp serve different purposes.

<a id="application-owned-tags"></a>
### Application-owned tags

The SDK defines only its `WSE_TAG`/`WLog` and `WSE_DEV_TAG`/`DLog` built-ins.
Applications own any other tag and `Logger<Tag>` alias. Define an external-linkage character
array once in an application C++ source file, declare it in an application header, and use that array
as the template argument. C++17 inline character arrays are another valid single-identity form.
Register the tag profile before using the stream logger; `writeLog(level, tag, source, message,
details)` is the generic structured entry point. Application types stay out of SDK exports.
The installed-package consumer tests a custom tag across translation units and sink delivery
with both SHARED and STATIC linkage.

## Log sink lifetime, re-entry and flush: CORE-SVC-04

`registerLogSink(shared_ptr)` returns a nonzero handle and retains the sink. A null pointer
returns 0. `unregisterLogSink` removes that handle; zero or absent handles are harmless.
Removal prevents future registry snapshots, **not** calls from a snapshot already acquired.
The snapshot's shared_ptr keeps its sink alive until dispatch finishes.

Two writer threads can invoke the same sink concurrently: the sink must synchronize its own
state. It may unregister itself. A same-thread nested write can still reach console/file
output, but thread-local re-entry suppression prevents nested delivery to every custom sink.
Exceptions thrown by one sink are caught so dispatch can continue to others. This does not
make every allocation or platform output operation no-throw.

`flushLog` locks output and flushes stdout/stderr. It is not a custom-sink drain, a callback
barrier or a durable file-storage guarantee. Applications needing final custom-sink delivery
must stop/join their producers and coordinate completion with the sink before destroying
external resources borrowed by it.

The runtime contract checks filtering, formatting without dispatch, record fields, normal
unregistration, sink self-removal, nested-write suppression and exception containment.
It does not exhaustively verify every cross-thread registry race or a storage-failure policy.

## License loading and state: CORE-SVC-05

License loading is an application-startup operation. Serialize it before concurrent feature
use; the license state implementation does not provide a concurrent reload protocol.
The key loader and license loader intentionally have different missing-file behavior:

| Operation | Result |
| --- | --- |
| `Licensekey::load` cannot open key | Failed `LicenseResult`: `Io/FileOpenFailed` |
| Key read fails | `Io/ReadFailed` |
| `License::load` has no license bytes | Success, committing Free/Alpha version 0.0.0 |
| Date-limited license outside range | `Verification/LicenseExpired`; no state commit |
| Device identity absent or mismatched | `Verification/UnlicensedDevice`; no state commit |
| Valid load | Commit type/version once |
| Later load reaches another commit | `std::logic_error`; no replacement |
| Writer receives invalid license-kind enum | `std::invalid_argument` |

The load sequence is device identity -> file bytes -> decode with supplied key -> analyze
temporary values -> commit. For a date license, compare the local calendar date as
`year*10000 + month*100 + day` with inclusive start/end values. It is not a monotonic-time
lease and does not prevent wall-clock changes. A successful Free fallback also consumes
the one successful-load slot; restarting/reloading requires an application-level decision.

The current file helper conflates absent/unreadable license bytes with empty input.
Nonempty malformed license data and an empty decoding key are not comprehensively validated.
Use a valid nonempty key and well-formed files; do not infer robust arbitrary-input validation
or a cryptographic authenticity guarantee from LicenseStatus. Writer success is not an
atomic-replacement or late-storage-failure guarantee. Hardening or changing these behaviors
is outside this contract.

The existing license test uses isolated scratch files and one successful load per process.
It covers key round trip, invalid writer type, expiry, valid load and repeated commit.
Free fallback is documented from the public header and source review; full malformed-file
and device-identity acceptance are not established by that test.
Deployment terms are described separately in the [license guide](../../en/LICENSE.md);
device identity limitations are in [Portable Core](PortableCore.md).

## Device queries and platform boundary: CORE-SVC-06

Device enumeration is synchronous, returns owned information values, and does not monitor
hotplug after the call. It neither grants permissions nor establishes that a device can be
opened. Linux network/serial enumeration and explicitly unsupported query kinds follow
[Portable Core](PortableCore.md). Windows selects its own adapter through the build.
Do not infer identical ordering, counts or stable hardware identity across platforms.

No lifecycle state from a query is shared with Timer or a camera session. Tests that enumerate
metadata do not certify physical input, capture or display operations.

## Verification and maintenance

Use `wse.core.runtime_contract`, `wse.core.worker_controller` and `wse.core.license_contract`.
The license case needs its own process because it commits shared state. Timer tests exercise
periodic dispatch, captured lifetime, self-stop/owner-stop interaction, restart and exceptions;
their timeouts detect hangs rather than specify real-time scheduling.

[Design Verification](DesignVerification.md) maps the claims to evidence. Adding queueing,
changing removal barriers, date semantics or callback scheduling changes observable behavior
and requires corresponding API/design/test review.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
