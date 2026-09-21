# WSE Offline Kit tools

These tools implement a two-step release gate. A candidate is not an accepted offline kit.

Built-in checks cover package structure and invented test markers. Set `WSE_EXPORT_DENYLIST`
to the external private inspection policy for concrete content restrictions. A public export
requires that complete private scan plus human review; passing the standalone structural check
does not authorize publication. Keep the policy and its inspection evidence outside public output.

The candidate format is `wse-offline-kit-candidate-v2`; the accepted manifest uses
`schemaVersion: 2`. [OfflineKitManifest.schema.json](OfflineKitManifest.schema.json) is the
SDK-owned contract and is copied into every candidate with the standalone verifier.
`wse.repository` is `Engine`, with
the SDK commit and tree (the tree must match the installed `source_commit`). Application
identities, checkout layout, and application package recipes belong to the application.
The `--allow-dirty` switch is for local candidate development only; its commit/tree still
identify HEAD, so that output must not be used as release provenance.

Two classifications exist:

- **public** — a Core/XPT-only package. The candidate must pass the public export denylist.
- **controlled** — a Core/XPT/VPJ package built with the access-controlled extension overlay.
  The candidate is classified `private-internal`, records the overlay identity, and must never
  travel through a public channel. Its kit id must start with `wse-confidential-`.

## 1. Create a candidate on the staging PC

### Public kit

Run from the WSE source root after installing a Core/XPT-only package:

Use a fresh Release-only install prefix for a public Windows kit. Debug PDBs contain
source paths and diagnostic metadata and do not belong in a public kit; a prefix that
already contains a Debug install can retain them even after installing Release. The
public content scan must pass without removing or weakening its rules.

```bat
py -3 tools\offline\create_offline_kit.py ^
  --package-root build\install\windows-msvc-shared-xpt ^
  --output build\offline-kit\windows-x86_64-shared-xpt ^
  --kit-id wse-1.0.0-windows-x86_64-shared-xpt ^
  --toolchain-version 14.43
```

### Controlled kit

Install with an overlay preset first (`WSE_EXTENSION_ROOT` set, e.g.
`windows-msvc-shared-vpj-video`), then point the tool at both working copies. The extension
tree must be committed and must match the `extension_commit` the installed package recorded.

```bat
py -3 tools\offline\create_offline_kit.py ^
  --package-root build\install\windows-msvc-shared-vpj-video ^
  --output build\offline-kit\windows-x86_64-shared-vpj ^
  --kit-id wse-confidential-1.0.0-windows-x86_64-shared-vpj ^
  --classification controlled ^
  --extension-root ..\private-overlay ^
  --toolchain-version 14.43
```

A controlled candidate bundles the controlled consumer from
`<extension-root>/example/offline_consumer` instead of the public one. For a Linux kit add
`--target-os linux` (and `--target-arch arm64` for the Pi 4 target); the compiler then
defaults to GCC.

The normal command refuses an uncommitted tree in either repository. `--allow-dirty` is for
local development validation only and must not be used for a release candidate.

## 2. Accept on the clean offline PC

Copy the whole candidate directory to the target PC. Disable every physical and virtual network
adapter, then run from an elevated PowerShell or Command Prompt (Windows) or a root shell
(Linux):

```bat
py -3 verify_offline_kit.py ^
  --kit-root D:\wse-offline-kit\windows-x86_64-shared-xpt ^
  --output-manifest D:\wse-offline-kit\accepted-manifest.json ^
  --configuration Release ^
  --confirm-clean-host
```

The verifier runs on Windows and Linux hosts and accepts only a kit whose `target.os` matches
the host. It refuses missing confirmation, enabled adapters (`Get-NetAdapter` on Windows,
`/sys/class/net` operstate on Linux), checksum mismatch, unsafe archives, remote-access
directives, a private-internal candidate without its overlay identity, or a failing local
consumer build/test. Preserve the generated accepted manifest with the delivered kit; a
controlled kit's manifest carries the `privateOverlay` identity and stays under the
private-overlay handling rules.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
