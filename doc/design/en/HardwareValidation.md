# WSE Hardware Validation Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Status vocabulary

| Status | Meaning |
| --- | --- |
| `BUILD_VERIFIED` | Compiled and linked for the named toolchain/architecture |
| `CONTRACT_VERIFIED` | Unit, fake, replay, or golden contract passed without a physical-device claim |
| `HARDWARE_SMOKE_PASS` | A bounded smoke test passed on the recorded device/configuration |
| `HARDWARE_CERTIFIED` | The complete approved hardware matrix passed for the recorded commit |
| `HARDWARE_NOT_RUN` | No physical test was run |
| `HARDWARE_ON_HOLD` | Physical acceptance is paused; this is not a WSE pass or failure |
| `UNSUPPORTED` | Deliberately unsupported and required to return an explicit error |

## Evidence

Hardware evidence records the exact WSE commit, date, operator/environment, OS/kernel, architecture,
device model and stable identity, firmware/driver, backend, build options, dependency identities,
test list, result, logs/artifact hashes, and known limitations. A result applies only to the recorded
configuration. Cross-build, mock, and OS enumeration results cannot be promoted to hardware status.

Physical keyboard/camera/display tests are disabled by default and require an explicit option and approved
equipment. They must restore display modes, stop streams, close handles, and preserve device state on
success, failure, timeout, and interruption. Destructive media initialization requires separate owner
authorization and exact-target verification.

## Opt-in Windows execution

Enable only the approved gates on a public preset. `WSE_ENABLE_CAMERA_HARDWARE_TESTS=ON`
registers `wse.tmr.windows_camera_smoke`; set `WSE_CAMERA_TEST_NAME` to the exact approved
enumerated name when several cameras are connected. A supplied name must match exactly one
device, and capability queries and open attempts never fall back to a different device. Stream
selection uses advertised BGRA-convertible native profiles, without assuming a webcam resolution.
The smoke records control verification separately and still attempts frame acquisition when the
control cycle cannot pass. Its overall pass requires both; a frame-only result is not certification
of controls. Frames stay in memory; no image file is required.
The frame check requires the requested dimensions and actual BGRA8 format on synchronous,
restart, callback and WebCamera reads. Advertised conversion capability remains a selection hint;
only successful acquisition proves that named profile's bounded frame path. A fixed-range control
cannot establish a change/readback/restore cycle, even when its current value is readable.

The control test requires a distinct manual value, retains the original value/mode before writing,
and reads back the restored value/mode before reporting success. A scoped restoration attempt also
runs on early failure or exception; a failed restoration is a failed gate. The frame smoke additionally
performs five short-read stop/restart cycles, callback self-stop followed by owner reaping, and close
from another thread. Each result is scoped to the chosen native profile. Neither timeout recovery
nor these bounded runs establish removal/replug recovery or all-driver behavior.

`WSE_ENABLE_IUI_HARDWARE_TESTS=ON` registers `wse.iui.keyboard_hardware_smoke`.
After its prompt the operator physically presses Q for about one second and releases it, within
60 seconds. Both polling and callback must observe press and release; synthetic input is not
physical evidence. The callback capture outlives the Keyboard owner and the CTest deadline is
90 seconds, including startup and release observation.

`WSE_ENABLE_DISPLAY_MODE_TESTS=ON`, `WSE_DISPLAY_TEST_NUMBER` and
`WSE_DISPLAY_TEST_HOLD_MS` select the approved Windows output and visible hold interval.
`wse.oui.d3d12_display_mode_restoration` checks projection, readback, presentation, event handling,
destruction and restoration separately for Window and DirectDisplay. Record any current-mode or
borderless fallback: neither proves an alternate mode change, and borderless skips DirectDisplay.
Compare the exact native mode and desktop placement before and after execution; an outer timeout
guard must restore the selected output if the process is interrupted. Keep OS/device identities,
failure logs and source-diff hashes with controlled execution evidence, outside public artifacts.

Projection readback compares RGB in four spatial regions, ignoring alpha. The 2 x 2 source is
top-left `(255,96,0)`, top-right `(0,255,96)`, bottom-left `(0,96,255)`, bottom-right `(255,255,255)`.
Sample a 3 x 3 patch around each `(width/5 or width-width/5, height/5 or height-height/5)` center,
using integer division: 36 pixels total, tolerance two byte values per RGB channel. Honor RGBA8
versus BGRA8 and row pitch. These points lie in the clamped corner regions outside edge blending.
[ProjectionReadbackCheck.h](../../../test/support/ProjectionReadbackCheck.h) implements the check;
`wse.oui.frame_contract` rejects opaque black, red/blue swaps, vertical reversal and truncated data
while accepting padded RGBA/BGRA storage. This prevents alpha variation alone from passing.
Readback plus a successful Present does not measure physical scan-out, optical color accuracy or
human visibility; those require separate observation/instrumentation for certification.

## Current boundary

Accepted hardware status is scoped to the exact device, profile, build and test list in the controlled
evidence record. Windows camera smoke evidence does not establish support for other devices or profiles;
OS access failure is not a hardware pass.

| Target / scope | Accepted status and limitation |
| --- | --- |
| Raspberry Pi 4, 64-bit Debian 13: Core/XPT runtime and public Node-API, Python, Java/JNI, C# binding smoke | `HARDWARE_SMOKE_PASS` for the exact commits/configurations in HW-3 and HW-10 |
| Same board: access-controlled high-level facade | `HARDWARE_NOT_RUN` |
| Same board: OUI shared/static | `BUILD_VERIFIED`; GPU-backed offscreen Vulkan projection evidence applies only to offscreen rendering |
| Same board: HDMI, window presentation and DRM/KMS | `HARDWARE_NOT_RUN`; no scan-out or window claim follows from offscreen rendering |
| Linux physical UVC, generic extension-unit, Pi display and CSI certification | No completed certification; Pi 5/CSI equipment is unavailable |

The Wayland presentation contract skips when no compositor is reachable. A skip is not a presentation
pass. Access-controlled device records belong only in their controlled environment. A software
compatibility gate or owner waiver does not establish hardware certification.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
