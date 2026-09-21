# WSE XPT Transport Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose and scope

This document is the normative design for the portable communication boundary owned by
`WSE::Xpt`. It defines endpoints, explicit timeouts, cooperative
cancellation, structured errors, synchronous TCP, UDP, and serial clients, and an explicit retry
policy shared by Windows and Linux. It also defines the synchronous HTTP client, pinned libcurl,
ARM64 package, and offline-kit boundaries.
Model-specific protocols, commands, credentials, and URIs must not enter this module.

## Support matrix

| Target | TCP | UDP | HTTP | `SerialPort` | Library forms |
| --- | --- | --- | --- | --- | --- |
| Windows x86-64 / MSVC | Supported | Supported | Supported | Supported | Shared, static |
| Linux x86-64 / GCC or Clang | Supported | Supported | Supported | Supported | Shared, static |
| Linux ARM64 / GCC cross | Cross-built | Cross-built | Cross-built | Cross-built | Shared, static |

Linux ARM64 shared/static cross builds and packages are supported. Pi 4 Core/XPT runtime smoke
has been accepted only within the scope recorded by [Hardware Validation](HardwareValidation.md).
Neither cross compilation nor that bounded smoke certifies every physical network/serial device.
Every XPT public header is packaged on every supported target.

## Public contract

| Type | Responsibility |
| --- | --- |
| `Endpoint` | Holds a host name or numeric address and port; remote ports are non-zero, while UDP bind accepts zero for automatic assignment |
| `Timeout` | Holds a finite duration measured with `steady_clock`; negative values are invalid |
| `CancellationSource` | Requests cancellation in a thread-safe way |
| `CancellationToken` | Shares observable cancellation state across copies |
| `OperationContext` | Carries the required timeout and optional cancellation token |
| `TransportError` | Carries stable category/code, a message, and a diagnostic native code |
| `TransportResult<T>` | Strict success value or failure error, following ResultContract |
| `TransferResult<T>` | Keeps transfer progress or a received prefix meaningful alongside an error |
| `HttpResult` | Carries a response together with the HTTP outcome, including a 4xx/5xx response |
| `TcpClient` | Owns a move-only connection and provides connect, send, receive, and disconnect |
| `UdpDatagram` | Holds one received payload and its source endpoint |
| `UdpClient` | Owns a move-only socket and provides bind, send-to, receive-from, and close |
| `SerialPort` | Owns a move-only port and provides 8N1 open, send, receive, and close |
| `RetryPolicy` | Holds a total attempt limit and fixed or exponential backoff and returns a fail-safe retry decision |
| `RetryDecision` | Holds permission and delay for the next attempt, or a reason to stop |
| `HttpAuthentication` | Move-only runtime username/secret plus an explicit server-negotiated authentication policy; clears retained text on destruction |
| `HttpRequest` | Holds method, URL, headers and body |
| `HttpExecutionOptions` | Holds response-body limit, retry policy, operation safety and transient-failure opt-in; separate from the required OperationContext |
| `HttpResponse` | Holds HTTP status, headers, body and attempt count |
| `HttpClient` | Executes synchronous HTTP and an explicit retry lifecycle without exposing libcurl types |

Public headers do not expose WinSock `SOCKET`, POSIX file descriptors, `sockaddr`, or platform
values as primary error identities. `nativeCode()` is diagnostic only; portable decisions use
`category()` and `code()`.

## Structure and operation context: XPT-OP-01

| Owner/value | Implementation and lifetime |
| --- | --- |
| TCP | [TcpClient.cpp](../../../platform/xpt/network/TcpClient.cpp): unique PIMPL owns socket, remote endpoint and effective local endpoint |
| UDP | [UdpClient.cpp](../../../platform/xpt/network/UdpClient.cpp): unique PIMPL owns socket, its family and local endpoint |
| Serial | [SerialPort.cpp](../../../platform/xpt/serial/SerialPort.cpp): unique PIMPL owns native port; operation-local overlapped state on Windows |
| HTTP | [HttpClient.cpp](../../../platform/xpt/http/HttpClient.cpp): process curl runtime plus per-attempt handles/header list/response accumulator; no public native handle |
| Cancellation | [Cancellation.cpp](../../../core/xpt/operation/Cancellation.cpp): source and tokens share one atomic boolean |
| Retry decision | [RetryPolicy.cpp](../../../core/xpt/retry/RetryPolicy.cpp): pure policy calculation, no worker or resource owner |

Public declarations are under [api/xpt/stew.h](../../../api/xpt/stew.h). Native owners are move-only.
Move assignment releases the destination's previous resource and transfers the PIMPL; a moved-from
transport has no native resource and can be destroyed or assigned a replacement. It is not an
automatically reinitialized connection. Consumers serialize calls and lifetime changes on each
owner. There is no background transport worker, public completion callback or asynchronous close.

The caller supplies `OperationContext(timeout, token)` separately for each operation; HTTP also
takes `HttpExecutionOptions`. There is no implicit timeout. Negative durations fail validation;
zero is valid but ordinarily leaves no budget for I/O. Validation and no-op paths may return before
a cancellation/deadline check, so zero duration is not a promise that every call returns TimedOut.

After initial validation, an operation establishes one steady-clock deadline. Large additions
saturate at the clock's maximum. Address candidates, interrupted/would-block waits and HTTP
backoff consume the remaining budget rather than restarting it. Socket waits, HTTP polling and
serial waits use slices of at most 20 ms. This is a wait-slice bound, not an end-to-end cancellation
latency guarantee: OS scheduling, synchronous name resolution and native cleanup can take longer.

A default token never requests cancellation. `CancellationSource::cancel()` stores true in shared
atomic state; tokens observe it with acquire/release ordering. Requests are idempotent and cannot
be reset. Tokens retain that state after source destruction; destruction alone does not cancel.
Cancellation is cooperative and does not undo bytes already sent or peer-side actions. Only the
cancellation source is intended for concurrent use with an operation on a caller-confined owner.

## TCP operation semantics

- `connect`, `send`, and `receive` are synchronous and require an `OperationContext`.
- A timeout becomes one `std::chrono::steady_clock` deadline at operation entry. Retried OS waits
  do not extend it.
- Cancellation is cooperative. Blocked socket waits use at most 20 ms slices and return
  `eTransportErrorCode::Cancelled` when cancellation is observed; see XPT-OP-01 for latency limits.
- `send` continues until the complete buffer is sent. On failure, `value()` is the byte count sent.
- `receive` returns one read up to the requested maximum. An orderly peer close is `RemoteClosed`,
  not an empty success.
- Send/receive timeout and cancellation preserve an established connection. A reset from native
  I/O and peer-close errors close local connection state. Reconnect has the replacement rules below.
- `disconnect` and destruction shut down and close the socket. `disconnect` is idempotent.
- A successful TCP connect records the OS-selected numeric local endpoint. `getLocalEndpoint()`
  returns it until disconnect, allowing an upper protocol to construct its own address field without
  exposing a native socket.
- `checkPeerConnection()` performs an immediate non-destructive `MSG_PEEK`. Pending payload remains
  unread. An OS-observable orderly close returns `RemoteClosed`; reset and receive failures use their
  structured connection/input-output error and close local state. Would-block means only that no
  close is currently observable and is not an end-to-end liveness proof.

`TcpClient` is caller-confined; consumers must not call methods concurrently on one instance.
Only `CancellationSource::cancel()` may be called from a different thread than the operation.
There are no public callbacks here. Asynchronous callback lifetime is not part of this contract.

### TCP state and processing: XPT-TCP-02

| Operation/path | Before | After and retained result |
| --- | --- | --- |
| Connect rejects arguments or an already-cancelled context | Closed or connected | Preserve old state; no replacement started |
| Validated connect starts | Closed or connected | Close old socket before resolving; try nonblocking candidates using one deadline |
| Candidate succeeds | Connecting internally | Store socket/endpoints; connected |
| Resolution, timeout, cancellation or all candidates fail after replacement starts | Connecting internally | Close candidates; closed, with no rollback to the old socket |
| Send/receive timeout or cancellation | Connected | Keep socket; return completed progress or error |
| Native receive returns zero | Connected | Close and return RemoteClosed |
| Disconnect / owner destruction | Any | Shutdown/close owned socket; clear endpoints |

Connect validates first, closes the old socket, resolves the host, and iterates addresses. Each
candidate is nonblocking; a pending connect waits for writability and checks `SO_ERROR`. A failed
candidate closes before the next is tried. A successfully resolved zero-budget reconnect therefore
loses its former connection even though it cannot establish a new one.

Send advances an offset over completed native writes, using chunks no larger than `INT_MAX`.
Would-block and interrupted operations reuse the deadline. A failed `TransferResult<size_t>` keeps
the known completed count: it is not application acknowledgement, nor permission to replay the buffer.
An empty send on a valid connection/context succeeds with zero before checking cancellation.
Receive requires a positive maximum at most `INT_MAX`, allocates its buffer, and returns the first
nonempty read; it neither fills the requested size nor frames application messages. Native I/O
connection errors close the owner, while a wait-helper failure is returned through its own path;
do not infer closure from every error category. Query owner state before choosing recovery.

## UDP operation semantics

- `bind`, `sendTo`, and `receiveFrom` are synchronous and use the same `OperationContext` as TCP.
- `bind` requires a host and accepts port zero for OS-assigned ephemeral binding. The effective
  endpoint is available from `getLocalEndpoint()` after success. Rebind replaces an existing
  socket only after success and preserves it on failure.
- `sendTo` on an unbound client lazily opens a socket for the destination address family and keeps
  its OS-assigned local endpoint.
- One `sendTo` is exactly one datagram. Partial transmission is not success, and a zero-byte
  datagram is transmitted rather than treated as a no-op.
- The portable payload boundary is 65,507 bytes. Larger input returns `MessageTooLarge` before
  socket access.
- `receiveFrom` returns one datagram and source. If its buffer is too small, the result preserves
  the stored prefix and source while returning `DatagramTruncated`.
- Timeout and cancellation preserve the socket and local endpoint. `close` is idempotent.

`UdpClient` is also caller-confined and must not be operated concurrently from multiple threads.

### UDP state and processing: XPT-UDP-03

| Operation/path | Resource effect |
| --- | --- |
| Bind/rebind validation, resolution, candidate bind or endpoint-query failure | Close candidate only; retain any previous socket/endpoint |
| Bind/rebind success | Commit candidate, close previous socket and publish new effective endpoint |
| Send on an unbound owner | Resolve and create a candidate; retain it only after a successful send |
| Send on a bound owner | Use the socket's existing address family; no implicit family replacement |
| Receive timeout/cancellation | Keep socket and endpoint |
| Truncated receive | Consume the entire datagram; return stored prefix/source plus DatagramTruncated |
| Close / destruction | Release socket and endpoint; repeated close is harmless |

A truncated datagram's discarded suffix cannot be read in a later call. For example, a five-byte
datagram received with capacity three returns three bytes and an error; the next receive waits for
a new datagram. A zero-byte datagram is a real message and does not mean peer closure. UDP has no
connection handshake, delivery acknowledgement or automatic replay.

## Serial operation semantics

- `SerialPort::open`, `send`, and `receive` are synchronous and use the same `OperationContext`
  as TCP and UDP.
- Portable framing is fixed at eight data bits, no parity, one stop bit, and no flow
  control. Supported baud rates are 1200, 2400, 4800, 9600, 19200, 38400, 57600, and 115200.
- Device names are forms such as `COM3` on Windows and paths such as `/dev/ttyUSB0` or
  `/dev/ttyACM0` on Linux.
- `open` replaces an existing port only after opening and configuring the candidate succeeds.
- Windows `open` initializes native read timeouts so short replies complete without filling the
  requested buffer, independently of driver defaults or previous port users. Idle reads remain
  bounded by the operation deadline; native timeout configuration failure fails the open.
- `send` continues until the complete buffer has entered OS I/O. On failure, `value()` is the
  transmitted byte count. `receive` returns one read up to the requested maximum.
- Timeout and cancellation preserve the port. Waits use at most 20 ms slices, subject to native
  completion cleanup below, and `close` is idempotent.

`SerialPort` is caller-confined; only `CancellationSource::cancel()` may be called from another
thread. The Linux PTY test verifies the backend contract but is not USB-UART, GPIO UART, or target
device certification.

XPT deliberately accepts an explicit device path and does not own device discovery. Core
`pickupDeviceInfo::Serials()` may return an opaque `stable_id`: a Windows device-instance identity
or a Linux `/dev/serial/by-id` identity when present. A consumer may use that value to resolve the
current path before reopening a failed logical session. The stable ID is not an XPT endpoint, is not
portable across operating systems, and must never cause fallback to a different enumerated device.

`SerialPort` is the public serial transport API.

RAII owners hold the PIMPLs of `TcpClient`, `UdpClient`, and `SerialPort`. Socket and port release
during move assignment and destruction is centralized in the implementation destructor.

### Serial state and processing: XPT-SERIAL-04

Open validates path/baud/context, opens a candidate, configures raw 8N1/no flow control, then checks
cancellation/deadline before committing it. Until commit, the existing handle remains owned and
usable after failure. Success closes the former handle and installs the candidate. This differs
from TCP reconnect, which closes its former socket before resolution.

Linux uses a nonblocking descriptor and poll against the operation deadline. Windows owns an
event and OVERLAPPED state per pending transfer. On timeout/cancellation Windows calls `CancelIoEx`
and waits for native completion before releasing the event, OVERLAPPED or buffer. That final wait
uses `INFINITE`; a driver that delays cancellation can exceed the nominal deadline. Keeping native
I/O memory alive takes precedence over returning while it is still in use.

Send reports completed chunks; a pending cancelled write may already have affected the wire, so
the returned count is not a rollback or peer-acceptance certificate. Receive returns an available
prefix up to the maximum, including short replies. Native read/write failure paths can close the
port, while timeout/cancellation preserve it; inspect the result and `isOpen()` before recovery.
Raw transport operations never reopen a device or resend a command automatically.

## Retry policy semantics

- A default `RetryPolicy` is valid but allows only the initial attempt, so retry is disabled.
  Maximum attempts always include the initial attempt.
- `RetryPolicy::evaluate()` only makes a decision. It never sleeps, calls a transport, reconnects,
  or re-executes an operation.
- The caller explicitly supplies `Idempotent` or `NonIdempotent` and `Retryable` or `DoNotRetry`
  for every failed attempt.
- Retry is eligible only when both `Idempotent` and `Retryable` are explicit. Non-idempotent
  operations always stop.
- Success, cancellation, validation, protocol, security, and `Unsupported` failures stop even if
  the caller supplies `Retryable`. Resolution, connection, input/output, timeout, and HTTP
  categories are eligible only when the caller explicitly classifies that occurrence as transient.
- Fixed backoff retains the configured delay. Exponential backoff starts with the initial delay,
  doubles after each failed attempt, and saturates at the maximum delay without overflow. There is
  currently no jitter.
- Zero maximum attempts, negative delays, a maximum below the initial delay, and unknown strategies
  are invalid. Invalid policies and attempt zero fail closed with an observable stop reason.

TCP, UDP, and serial operations do not consume `RetryPolicy` implicitly. The transport layer must
not automatically repeat a partially sent buffer, UDP datagram, serial command, or device-open
operation. A higher-level operation using the policy owns its overall deadline, cancellation while
waiting, reconnection sequence, and re-execution lifecycle.

### Retry calculation: XPT-RETRY-05

For completed failed attempt count `n >= 1`, exponential delay is
`min(maximum_delay, initial_delay * 2^(n-1))`, computed with saturating arithmetic. With maximum
attempts 4, initial delay 10 ms and cap 25 ms, failures 1/2/3 permit delays 10/20/25 ms; failure 4
stops. Fixed delay always uses the initial value. There is no jitter or hidden sleep in evaluate.

Decision precedence is invalid policy, invalid attempt zero, no failure, cancellation,
non-idempotent operation, failure classification/category rejection, attempt limit, then retry.
This ordering determines `stop_reason()` when multiple stopping conditions apply.

## HTTP operation and dependency semantics

- `HttpClient::execute` and `executeAuthenticated` are synchronous and treat the separately supplied
  `OperationContext` as one overall deadline.
- Authenticated execution keeps credentials out of `HttpRequest` headers and asks the backend to
  answer only a server-advertised authentication challenge. Redirects remain disabled, so runtime
  credentials are scoped to the configured request origin. The move-only authentication value
  clears its retained username and secret during move replacement and destruction.
- Server negotiation can select Basic authentication when a server advertises it. Because Basic
  does not protect a credential on plaintext HTTP, deployments use HTTPS or an approved isolated
  device network. WSE does not log the authentication header or credential material.
- Exceeding the response-body limit produces a structured error instead of partial success.
- HTTP 4xx/5xx returns `HttpStatusError` together with the received response. A status alone does not authorize retry.
- Retry runs only when `RetryPolicy`, request idempotency, and failure classification all permit it.
  Cancellation and the overall deadline remain active during backoff.
- Public headers, CMake targets, and consumers do not expose curl types or headers.

### HTTP attempt sequence: XPT-HTTP-06

1. Validate request/options/context and authentication when present. GET/HEAD cannot carry a body;
   unsupported methods, malformed headers/URLs and invalid options fail before an attempt.
2. Check cancellation, acquire the process curl runtime and establish the overall deadline.
3. Increment the attempt count. Create fresh easy/multi handles, request-header list and response
   accumulator. Set the remaining timeout, disable redirects, and install internal body/header
   callbacks. These are backend callbacks, not user callbacks.
4. Perform/poll until completion, cancellation or deadline. Body accumulation checks its configured
   byte limit and allocation failure; headers are accumulated separately. The body limit is not a
   total header/memory budget. The callbacks catch `std::bad_alloc` from accumulation and mark failure.
5. Release attempt resources and construct HttpResponse with status, headers, accepted body and
   attempt count. Outcome precedence is cancellation, timeout, body-limit error, allocation error,
   other curl error, HTTP status >= 400, then success.
6. On failure, classify it and ask RetryPolicy. If permitted, wait with the same overall deadline
   and cancellation token, then start a fresh attempt. Responses from attempts are not concatenated.

Defaults permit only one attempt, classify the operation as NonIdempotent and disable transient
retries. Explicit opt-in plus Idempotent and a permitting policy are all required. The HTTP
classifier treats 408, 425, 429, 500, 502, 503 and 504 as transient candidates, along with eligible
resolution/connection/I/O/timeout failures; ResponseTooLarge and ResourceExhausted are excluded.
Classification alone never authorizes repetition.

Failed HttpResult values can retain the received response, including an accepted body prefix on
limit/transfer failure. Do not parse that prefix as a complete success. If backoff is cancelled or
times out, the result retains the previous attempt's response with the new wait error. An attempt
count describes WSE attempts, not each exchange involved in server-negotiated authentication.

### Dependency and packaging boundary

- Libcurl 8.21.0 source, SHA-256, official signature sidecar, signer fingerprint, and curl license
  are pinned in the standalone Bootstrap manifest.
- Bootstrap uses only Python standard-library archive handling. It rejects traversal, symbolic and
  hard links, device entries, incomplete targets, and platform or architecture metadata mismatch.
- The source archive can be transported once, while extracted trees and Debug/Release static
  libraries are separated by target OS. Windows uses Schannel; Linux uses OpenSSL.
- `wse-dependency.json` records the exact source identity, target OS/architecture, linkage, license,
  system dependencies, and Bootstrap recipe hash for each provisioned target.
- Windows uses Schannel, while Linux x86-64/ARM64 uses OpenSSL. ARM64 installed packages bundle
  the pinned OpenSSL tree as a package-internal dependency.

## Error contract

Categories distinguish validation, resolution, connection, input/output, timeout, cancellation,
protocol, security, and HTTP failures. TCP, UDP, and serial can return at least InvalidArgument, HostNotFound,
AddressUnavailable, ConnectionRefused, ConnectionReset, NetworkUnreachable, NotConnected,
RemoteClosed, TimedOut, Cancelled, BindFailed, SendFailed, ReceiveFailed, MessageTooLarge,
DatagramTruncated, ResourceExhausted, and Unknown. Serial open and configuration failures use
OpenFailed and ConfigurationFailed. Transport-specific codes are not forced onto
operations where they do not apply.

The OS error number is retained in `nativeCode()` but is not stable across Windows and Linux.
Messages are diagnostic text, not machine-readable or localization keys.

## Current limitations

- System name resolution through `getaddrinfo` is synchronous and cannot be interrupted while the
  resolver call is executing. Its elapsed time counts against the same deadline. After successful
  resolution the candidate loop checks timeout/cancellation; resolver failure can report HostNotFound
  directly instead. A timeout does not bound all allocation, resolution or cleanup work.
- XPT does not own serial enumeration or hot-plug notifications. Core exposes snapshot enumeration;
  a consumer performs its own bounded re-enumeration after an observed session failure. Event-driven
  hot-plug is not implemented.
- Custom serial framing and flow control are not implemented.
- There is no implicit transport retry. In particular, non-idempotent operations must not be retried
  automatically.
- TCP/UDP/HTTP loopback and Linux PTY serial are contract evidence, not physical LAN, Raspberry Pi, or
  device certification.

## Verification contract

| Contract | Evidence | Remaining limit |
| --- | --- | --- |
| XPT-OP-01 | TCP/UDP/Serial/HTTP deadline and cancellation tests | No hard scheduling/resolver/driver cancellation bound |
| XPT-TCP-02 | [TCP loopback](../../../test/characterization/xpt_tcp_loopback.cpp): invalid/pre-cancelled reconnect retains socket; zero-budget replacement closes it; peer close/peek | No physical network failure matrix |
| XPT-UDP-03 | [UDP loopback](../../../test/characterization/xpt_udp_loopback.cpp): rebind, payload limits, truncation, zero datagram | No delivery/reliability guarantee |
| XPT-SERIAL-04 | [Serial contract](../../../test/characterization/xpt_serial_port_contract.cpp): input errors and Linux PTY reopen/I/O | Windows pending physical-driver cancellation is not covered by PTY |
| XPT-RETRY-05 | [Retry contract](../../../test/characterization/xpt_retry_policy_contract.cpp): policy precedence, attempt counts and saturation | Caller must establish actual operation idempotency |
| XPT-HTTP-06 | [HTTP contract](../../../test/characterization/xpt_http_contract.cpp): local server, response limits, cancellation, retry and auth | No general Internet/TLS/device certification |

TCP/UDP loopback and portable serial contract tests run for Windows MSVC and Linux GCC/Clang in
shared and static forms. UDP covers ordinary, zero-byte, 65,507-byte, truncation, timeout, and
cross-thread cancellation behavior. Linux serial uses a PTY for bidirectional I/O, timeout,
cancellation, and transactional reopen. Windows verifies the hardware-independent error contract.
Installed-tree consumers link `WSE::Core` and `WSE::Xpt`
without using source-tree or platform headers.
The retry contract test covers the default no-retry state, attempt limits, fixed and exponential
backoff, saturating arithmetic, non-idempotent operations, explicit failure classification, and
fail-safe handling of cancellation, validation, protocol, and security failures in every platform
and library form.
HTTP contract tests use a local fake server to verify methods, headers, bodies, status, response
limits, timeout, cancellation, retry lifecycle, challenge-driven authentication, and rejection of
incomplete authentication values. ARM64 gates cross-compile the test executables; Pi 4 execution is
the separate runtime gate.

Raw transport binding failures preserve send progress and UDP truncated prefix/source across
Python, JavaScript, Java and C#. See BIND-TRANSFER-05 in [Language Bindings](LanguageBindings.md)
for each exception shape, ownership, compatibility and evidence limits. No binding retries a failed send automatically.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
