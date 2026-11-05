# WSE Asynchronous Logging

> Canonical language: English
> Documentation version: WSE 1.1.0
> Last synchronized: 2026-11-05

## Scope and public surface: CORE-LOG-01

The C++17 Core service provides an explicitly owned asynchronous output pipeline. The
application creates [AsyncLogRuntime](../../../api/wse/utility/AsyncLogRuntime.h), supplies
settings and retains the owner until all producers have stopped. Construction does not start
a worker. A successful `configure()` creates one writer; later reconfiguration reuses it.
`stop()` drains and joins it, and a later successful configure can restart the same owner.

This service is independent of the synchronous `Logger<Tag>`, `writeLog()`, registered
`LogSink` objects and their global minimum level. Those existing APIs keep their synchronous
dispatch, profile and removal rules in [Core Services](CoreServices.md). Creating an async
runtime does not redirect existing library logs or create a process-global logging thread.

The application owns tag names, verbosity meanings, message catalogs, presentation and
startup/shutdown order. WSE owns bounded delivery, writer execution, output resources and
portable logging diagnostics. Application enums and application error/message identifiers
are not added to the public SDK.

| Public type / operation | Responsibility |
| --- | --- |
| `sAsyncLogRecord`, `sLogPart` | Owned timestamp, severity, tag, source, location, verbosity, body parts and attributes |
| [LogMessageBuilder](../../../api/wse/utility/LogMessageBuilder.h) | Value conversion, separators and bounded typed body construction |
| `AsyncLogLine` | Runtime-specific RAII stream frontend; destruction submits one complete record |
| `sAsyncLogSettings` | Per-runtime filters, limits, output, formatter, console callback and custom sinks |
| `AsyncLogRuntime::tryAppend()` | Filter, validate and copy a complete record into the finite queue |
| `AsyncLogSink` | Writer-thread `write(record, text)` and `flush()` callbacks |
| [LogError / LogStatus](../../../api/wse/error/LogError.h) | Portable category/code and diagnostic message/native code |
| `sAsyncLogStatus` | Running/admission state, queue depth, cumulative counts and last error |

## Typed body and conversion: CORE-LOG-02

`LogMessageBuilder` is producer-confined. Each ordinary append prepends the selected WSE
separator, including the first value and empty text. A separator is not inserted only between
values. Strings, UTF-8-converted wide strings, arithmetic values, booleans and enums use WSE
conversion conventions: arithmetic uses `std::to_string`, booleans use `true`/`false`, and
ordinary enums use the hexadecimal representation for their underlying width.

The dedicated non-template `append(eLogControl)` and `operator<<` overloads recognize
`eLogControl::NewLine` before the ordinary enum template. They append a typed `NewLine` part without a separator.
The next ordinary value still receives its normal leading separator. NewLine does not split
the record, submit it, flush a sink or reset the separator convention.

```cpp
wse::LogMessageBuilder body( wse::eLogSeparate::SPACE );
body << "first" << wse::eLogControl::NewLine << "second";
// Body rendering: leading space + first + platform newline + leading space + second.
```

Body parts retain their type through the queue and formatter callback. `renderLogParts()`
renders `Text` CR/LF/NUL as the visible sequences `\r`, `\n` and `\0`; only a typed NewLine
produces `END_LINE()`. It adds no prefix or suffix. A magic text delimiter is not used, so
arbitrary application text cannot accidentally become an explicit formatting control.
The default formatter applies the configured suffix once to the whole record, not once per
displayed line. It uses the record's captured timestamp and standard tag/file/line layout.
A custom formatter returns the complete output text and owns its prefix, suffix and typed
body rendering; it may call `renderLogParts()` rather than flattening controls into text first.

The existing synchronous stream logger keeps its ordinary string behavior. Its conversion
or newline handling is not changed merely by using an async runtime elsewhere in the process.

`AsyncLogLine` binds a runtime, tag, verbosity, source location and severity, then submits one
record on destruction. `setLevel()` changes that record's severity. The runtime must outlive
the line. Its dedicated NewLine overload preserves the typed control rather than formatting
the control enum as a number.
The line snapshots separator and builder limits at construction. If configuration changes
before destruction, final submission uses the current runtime filter and record limits;
the accepting configuration supplies the formatter, sinks and suffix. Constructing a line
does not reserve the old configuration for its entire lifetime.

## Acceptance, filtering and capacity: CORE-LOG-03

The record owns its strings, parts and attributes. Acceptance copies those values; the writer
does not borrow a producer stack, input buffer, feature frame or temporary string. Consumers
must not modify the same input record concurrently with `tryAppend()`.

Filters belong to one runtime. Severity must reach `minimum_level`; zero verbosity and
`LogLevel::Off` are disabled. A tag-specific verbosity threshold overrides `default_verbosity`.
Verbosity is an application-defined unsigned scale, separate from severity. Changing this
runtime's filter never changes the synchronous WSE global filter or another runtime.

`queue_capacity`, `record_byte_limit`, `record_part_limit` and `metadata_byte_limit` bound the
accepted queue, body and metadata. Defaults are 1024 queued records, 16 KiB body, 1024 parts
and 16 KiB metadata. Body bytes count raw Text bytes plus `END_LINE().size()` for each typed
NewLine; attributes are also limited in count by `record_part_limit`. Excess input
is rejected as a whole rather than truncated or split into extra queue records.

`tryAppend()` is `noexcept` and performs no formatter or sink I/O on the producer thread.
Its result distinguishes `Accepted`, `Filtered`, `NotRunning`, `QueueFull`, `RecordTooLarge`
and `Failed`. Acceptance means ownership has entered the queue, not that output succeeded.
It uses synchronization and copies; it is not a lock-free or real-time guarantee.

## Writer, callbacks and failures: CORE-LOG-04

```text
producer -> runtime filter and limits -> owned finite queue -> one writer
                                                      -> formatter(record)
                                                      -> console/file output
                                                      -> custom sink write(record, text)
owner -> flush barrier / configure drain / stop and join
```

The writer performs formatting and output outside the queue mutex. The optional formatter,
console writer and custom sinks are retained by the active configuration and invoked on that
writer. Captured external resources must outlive stop completion. A shared sink used by
multiple runtimes needs its own synchronization; one runtime's writer does not serialize
another runtime's callbacks.

Console output can use built-in WSE output or an application `console_writer`. A frontend
that also writes prompts may serialize both through its own mutex inside that callback.
Do not hold that frontend mutex while calling a runtime control operation that waits for
the writer. WSE does not assume ownership of the application's UI lock.
The console callback has no separate flush hook; it must flush any buffer it owns when its
application requires that behavior. The runtime barrier waits for completed callback writes.

File output uses the configured UTF-8 `file_path`, opens the candidate resource before
committing reconfiguration and reports open/write/flush failures. It does not require a
fixed application file name. Custom sinks return `LogStatus`; formatter/callback failures
are contained and become logging diagnostics rather than exceptions from a producer append.
Output to multiple destinations is not an atomic transaction: a failure cannot retract bytes
already written to another destination.

`getStatus()` snapshots the queue/state, accepted/completed/dropped/filtered counts and
`last_error`. Error category/code are branching contracts; message and native code are
diagnostic. A logging failure is separate from an application's operation result. Explicit
successful reconfiguration clears the retained error and permits recovery.

`LogError` uses the logging-specific categories `None`, `Configuration`, `Capacity`, `Io`,
`Callback`, `Lifecycle` and `Internal`. Codes distinguish invalid settings, stopped admission,
queue/record limits, open/write/flush failure, callback/re-entry failure and allocation/thread
failure. Its taxonomy is independent of application errors and transport/device error enums.

Callbacks may observe `isEnabled()` and `getStatus()`. They must not invoke `configure`,
`flush`, `stop`, submit another record to or destroy their own runtime. Control
re-entry is reported as `ReentrantControl` rather than waiting for the writer to join or drain
itself; append re-entry is rejected as `Failed` to prevent delivery cycles. A callback that never returns can still prevent external flush/stop from completing;
the service does not forcibly interrupt application code or promise a finite I/O deadline.

## Reconfiguration and completion: CORE-LOG-05

`configure()` validates and prepares a candidate before closing admission or replacing
active settings. Candidate preparation failure keeps the old settings and output resource.
A successful switch drains records accepted under the old configuration before installing
the new formatter, sinks, output and filters. It keeps one writer instead of starting a
writer for every record or setting change.

`output=NONE` with no custom sink filters all output. With custom sinks it can provide
sink-only delivery; disabling native output does not force sink-only records to be dropped.

`flush()` is a barrier for records accepted before that call. Later concurrent producers do
not postpone that barrier indefinitely. It also flushes the relevant output resources and
custom sinks. It is not a producer-stop operation, a durable-storage/fsync guarantee or a
barrier for independent synchronous WSE logging.
`flush_each_record` defaults to false. When enabled, the writer flushes its output resources
and sinks before counting each record as complete; explicit barriers and stop still apply.

Normal shutdown is application-controlled: stop/join producers, retain their final records,
call `stop()`, then release callback dependencies and unload owning modules. `stop()` closes
admission, drains accepted work, flushes and joins the writer. On return no further callback
from that runtime can execute, and formatter/console/sink captures have been released.
Old reconfiguration captures and stop captures are destroyed outside the queue/control locks.
Repeated stop is safe. Destruction performs owner-side
cleanup; explicit stop remains the way to inspect an operational error before destruction.

## Verification and compatibility

The runtime and concurrency contracts cover owned records, per-runtime filters, writer-thread
output, bounds, typed newline construction, candidate failure, output/callback failure,
continuous-producer barriers, parallel producer order, capture release and shutdown.
These are software contracts, not hardware or durable-storage certification. Public-header,
package and external-consumer gates cover C++17 SHARED/STATIC use. See
[Design Verification](DesignVerification.md) for executable entry points.

The synchronous API remains source-compatible. Applications rebuilding against new public
headers use the matching installed WSE library; this design does not promise interchangeable
binary layouts across independently built SDK/application artifacts.

## Change history

| Date | Change |
| --- | --- |
| 2026-11-05 | Define explicitly owned async delivery, typed newline bodies, independent filters, diagnostics and completion barriers. |
