# WSE IUI Keyboard Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope and structure

`wse::iui::Keyboard` monitors fixed-width key state through `WSE::Iui`. It is not a Unicode
text-input, IME, compositor event-routing, shortcut-registration or exclusive-input API.
The public header is [Keyboard.h](../../../api/iui/device/Keyboard.h); the adapters are
[Windows](../../../platform/iui/win/device/Keyboard.cpp) and
[Linux](../../../platform/iui/linux/device/Keyboard.cpp). Shared worker-stop behavior is in
[Thread Ownership](ThreadOwnership.md). This design describes current APIs and implementation
constraints; it does not require consumers to use internal worker or platform types.

```text
Caller -- calls --> Keyboard owner
                     +-- owns --> state arrays / mutex
                     +-- owns --> atomic access state
                     +-- retains --> shared callable
                     +-- owns --> monitor worker
                                    +-- uses --> private platform adapter
                                                  +-- uses --> Linux descriptors / Win32 polling
worker -- publishes under mutex --> state arrays
worker -- invokes outside mutex --> shared callable
```

There is one worker per monitoring owner. Snapshot arrays, access state, callback registration
and the scan-completion counter are distinct state. Native resources never leave the adapter.

## State and observation: IUI-STATE-01

`Keyboard::accessState()` returns the following enum. Platform-specific transitions matter:

On narrow screens, scroll this table horizontally.

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| State | Linux meaning and next transition | Windows behavior |
| --- | --- | --- |
| <span style="white-space:nowrap;">`Starting`</span> | Initial value until first probe/capture publishes an access result; immediate capture failure can already report Disconnected | Constructor initializes Ready directly |
| <span style="white-space:nowrap;">`Ready`</span> | At least one source was readable; loss of all readable sources can produce Disconnected | Asynchronous key polling is enabled; physical-device/access availability is not probed |
| <span style="white-space:nowrap;">`Unavailable`</span> | No readable source and no observed permission denial; later scan can recover | Not produced by this adapter |
| <span style="white-space:nowrap;">`PermissionDenied`</span> | No readable source and scan or key capture observed EACCES/EPERM; later scan can recover | Not produced by this adapter |
| <span style="white-space:nowrap;">`Disconnected`</span> | Capture failed for all previously readable sources without a permission denial; next scheduled scan re-evaluates access | Not produced by this adapter |

</div></div>

On Linux a readable keyboard wins over an inaccessible candidate: Ready applies if any usable
source remains. Denied candidates cannot always be capability-tested, so PermissionDenied is an
access diagnostic, not proof of a specific physical keyboard. HDMI-CEC sources are excluded.
Windows Ready is not proof that a physical keyboard is present or input is accessible.
The public `isAvailable()` contract defines this platform distinction explicitly;
applications must not treat Ready as a physical-presence probe.

`isAvailable()` compares its own access-state read with Ready. Separate calls to `isAvailable()`,
`accessState()` and `snapshot()` are not one atomic observation. Linux publishes access state before
locking and replacing the arrays; a caller can briefly observe a new status with an older snapshot.
After a non-ready scan publishes, its snapshot is all-released, not evidence of physical release.
An access-state change alone does not invoke the key callback if the arrays are unchanged.

## Snapshot representation and mapping: IUI-DATA-02

| Group | Width | Interpretation |
| --- | --- | --- |
| `ascii` | 128 (`ASCII_NUM`) | ASCII compatibility positions, not a text queue |
| `function` | 24 (`FANCTION_NUM`, retained spelling) | F1 through F24 |
| `arrow` | 4 | Arrow keys |
| `lock` | 3 | Lock indicators, not momentary key presses |
| `command` | 9 | Command-key positions defined in the public header |

`snapshot()` copies all five arrays under one mutex and returns an owned value. No array view
borrows worker memory. `getASCII()` returns the lowest active ASCII index, or zero if none; it is
not the last character typed. Key presses and releases between polls can be missed.

Windows uses virtual-key state. Linux maps evdev positions to a US ASCII compatibility layout:
letter case uses Caps Lock XOR Shift, shifted number keys map to punctuation, and keypad digits
depend on Num Lock. Multiple readable sources are OR-combined, including available lock LEDs.
For localized text, dead keys, compose, IME, Unicode or focus-aware Wayland input, use the window
toolkit rather than inferring text from these arrays.

`isReleasedAllKey()` waits for a scan-completion counter change for up to 100 ms, then examines
the published ASCII/function/arrow/command arrays. Lock indicators are excluded. On timeout it
still returns a snapshot-based boolean; it is neither a backend health check nor physical-release
confirmation. Calling it from the callback stalls the same worker until that wait expires.

## Polling sequence and backend resources: IUI-POLL-03

Both adapters use a 5 ms interruptible worker wait followed by polling and callback work. This is
a delay between iterations, not a real-time sampling period. Linux uses a 200-tick rescan countdown,
approximately one second without additional work; callback/OS latency extends it.

1. Wait, checking worker stop. Linux scans initially and when its countdown expires.
2. Linux captures into a local zero-initialized `KeyboardState` outside the state mutex, only when
   Ready. Failure of all sources sets PermissionDenied if any key query reported EACCES/EPERM,
   otherwise Disconnected, and schedules the next rescan. Windows instead
   holds the state mutex while saving the previous arrays, resetting and sampling the key groups.
3. Linux publishes access state, then under the state mutex saves previous arrays and installs the
   current arrays. Windows copies its newly sampled arrays to the local current value. Both acquire
   a shared reference to the registered callable under that mutex; Windows access state stays Ready.
4. Release the mutex and compare arrays. If they changed, a callable exists and stop has not been observed, invoke
   the callable with the local current state. Catch exceptions and clear the callback registration.
5. Increment the scan-completion counter under the mutex and notify waiting observers. Repeat.

The callback argument is a const reference valid for that invocation. Retain a copy, not its address.
A long callback delays the next poll, rescan and scan-completion notification. Stop does not preempt
user code and can race after the final pre-invocation stop check.

| Platform | Resource and capture algorithm |
| --- | --- |
| Windows | Win32 asynchronous state reads; no hook, injection or device-handle ownership |
| Linux | Worker-local RAII descriptor collection; read-only, nonblocking `/dev/input/event*` opens, capability filtering, then `EVIOCGKEY`/`EVIOCGLED` state queries |

Linux scans close the previous descriptor collection and build a fresh one. Candidates must expose
keyboard capabilities including A, Z, Enter and Space; HDMI-CEC (`BUS_CEC`) is excluded. Capture
queries current state with ioctl, not an evdev event-stream queue. Interrupted ioctl calls (EINTR)
retry before classifying the result. Other failed key queries close/remove
the affected descriptor; successes are OR-combined. Failed LED queries contribute no LED bits.
Losing one source while another remains readable does not by itself produce Disconnected.

The private [device collection](../../../platform/iui/linux/device/LinuxKeyboardDevices.h) owns
the directory stream and each candidate descriptor immediately, before path/vector work can throw.
Identity/capability queries and directory enumeration also contribute permission-denial evidence.
Directory-open denial yields PermissionDenied; another directory-open error yields Unavailable.
A readable candidate wins even if enumeration ends in an error. An allocation failure discards the
partial collection, yields Unavailable and allows the worker to rescan later; no stale pressed keys
are published. Close is attempted once, without retrying close after EINTR (the descriptor may already
be closed). Repeated ioctl interruptions have no real-time bound.

The device-free `wse.iui.keyboard_devices_contract` exercises denial at directory/open/identity/
capability/key-query boundaries; device filtering; mixed readable/denied sources; OR aggregation;
partial and total disconnection; denial after Ready; removal/reappearance; LED failure; EINTR retry;
and allocation-failure rollback/recovery. Every accepted, rejected and unwound descriptor and every
opened directory must have exactly one close. This tests the production resource owner with fake
operations, not kernel hotplug timing, OS ACL changes or physical keys.

The adapter uses kernel UAPI and the C++ runtime, without libinput, X11, Wayland or udev. It does
not require root, but the process must already have read permission through OS device policy.
WSE never changes groups, ACLs, device ownership or permissions, and never uses `EVIOCGRAB`.
Descriptors close when replaced, rejected, disconnected or when the worker-local collection dies.

## Owner and callback lifecycle: IUI-LIFE-04

On narrow screens, scroll this table horizontally.

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:44rem;">

| Operation | Worker and resource effect | Snapshot and callback effect |
| --- | --- | --- |
| Construct | Start one new worker | Initialize arrays; platform-specific initial access state |
| Copy construct | Source keeps running; destination starts a separate worker/backend | Copy arrays under source lock; share ownership of the same callable |
| Copy assign | Stop/join destination, then start its replacement worker | Replace from source; source keeps running |
| Move construct | Stop/join source, then start a new destination worker | Move callback ownership and copy/move state; no native worker/descriptor transfer |
| Move assign | Stop/join both affected workers, then start destination | Replace destination state/callback; source no longer monitors |
| Destroy | Request stop, wake wait and join; release backend resources | Release registration and stored state after worker completion |

</div></div>

Linux restarts access state at Starting on copy/move; Windows retains the source's access value.
This does not make access state and the copied arrays one atomic observation. Self-assignment is a no-op.
The moved-from object can be destroyed or assigned again. Its retained snapshot/access value is
not live monitoring evidence. There is no public stop/start method; owner lifetime and assignment
control monitoring. Destruction waits for an in-flight callback: arbitrary user code prevents a
universal bounded-stop guarantee.

`setCallback()` owns the callable through `shared_ptr<const KeyCallback>`; an empty callable clears
the registration. Copies of Keyboard share that callable rather than cloning its captured state.
Independent workers may therefore invoke the same captured state concurrently; the application
must synchronize mutable captures.

`clearCallback()` removes the owner's registration under the mutex. It prevents subsequent
acquisition but neither waits for nor revokes a callable already acquired by a worker or retained
by another Keyboard copy. It is not a callback-completion barrier. Captured objects are released
when all such references end.

Callbacks run on the monitor worker outside the state mutex. Snapshot/value observers and
`clearCallback()` are permitted. A callback must not destroy, move-assign or copy-assign the same
owner. Exceptions are caught at the worker boundary; the registration is cleared and monitoring
continues. This clear can also remove a replacement installed concurrently; there is no
generation-specific exception recovery guarantee. Ordinary owner lifecycle operations remain
externally synchronized; callback-safe observers do not authorize concurrent destruction.

## Validation and remaining limits

On narrow screens, scroll this table horizontally.

<div class="wse-iui-wide" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| Contract | Evidence | Limit |
| --- | --- | --- |
| IUI-STATE-01 | Lifecycle startup plus `wse.iui.keyboard_devices_contract` injected access/hotplug matrix | Kernel/physical transitions are not injected; observer calls are not atomic together |
| IUI-DATA-02 | Lifecycle compile-time widths and snapshot shape | Key mapping requires the opt-in physical gate |
| IUI-POLL-03 | Device collection fault matrix, allocation sweep and adapter/worker review | No real-time latency or exhaustive worker/callback race guarantee |
| IUI-LIFE-04 | [Lifecycle test](../../../test/characterization/iui_keyboard_lifecycle.cpp): capture retention through copy, eventual release after clear, copy/move/assignment/destruction | No physical keypress or forced in-flight callback; no general leak proof |

</div></div>

Shared/static and installed IUI-only consumers validate the supported build/target boundary;
Linux cross builds do not prove runtime input. Physical keyboard validation is a separate opt-in
gate described in [Hardware Validation](HardwareValidation.md). An unavailable source or a
successful lifecycle test never becomes hardware certification. See [Design Verification](DesignVerification.md).

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
