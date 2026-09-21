# WSE Raspberry Pi 4 Tools

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

These tools support the explicit Raspberry Pi 4 hardware gate. They do not convert cross-build,
mock, or inventory results into hardware certification.

## Environment inventory

Run the read-only inventory on Raspberry Pi OS 64-bit before configuring WSE:

```sh
sh tools/pi/inspect_pi4_environment.sh
```

The script requires an ARM64 operating system and emits stable `key=value` records. It reports the
OS and kernel versions, build/runtime tool availability, display session type, DRM and camera node
counts, Vulkan API version, default-route and SSH-listener presence, memory, storage, temperature,
and throttling status where available.

It deliberately omits the host name, network addresses, account name, hardware serials, device
paths, and device identifiers. It performs no package installation, network connection, device
open, display-mode change, or camera capture. Store raw output only in the approved evidence area;
public reports should retain only the fields needed for the stated acceptance result.

`platform.pi_generation=4` confirms the expected board family. A value of `unknown`, `other`, or
`5` is not a Pi 4 acceptance result. `display.vulkan_api_version=unavailable` means the Vulkan gate
has not passed; it is not evidence that the hardware lacks Vulkan support.

## Runtime smoke

After producing matching ARM64 Debug artifacts, copy the artifact directory and
`run_pi4_smoke.sh` to the Pi and run:

```sh
sh run_pi4_smoke.sh <directory-containing-arm64-debug-artifacts>
```

The smoke tool checks the ARM64 architecture, required files, dynamic dependencies, and the Core,
TCP, UDP, retry, HTTP, and portable serial contracts. A successful smoke is
`HARDWARE_SMOKE_PASS`; the broader native package, display, endurance, and restoration gates remain
separate.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
