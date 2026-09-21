# WSE Security and Privacy Design

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Trust boundaries

Network payloads, serial input, camera metadata/frames, file contents, environment paths, and binding
arguments are untrusted. Public adapters validate size, range, encoding, timeout, and state before
passing data to a backend. Errors retain a portable category/code and a diagnostic native code
without silently retrying unsafe operations.

XPT verifies TLS by default, limits response size, and owns request lifetime. Camera extension-unit
traffic requires explicit control identifiers, bounded buffers, and capability checks. Frame and
binding buffers are owned copies so backend reuse cannot mutate consumer data.

## Dependency integrity

Bootstrap verifies pinned source identity or SHA-256 and records license/platform/architecture
metadata. Offline mode does not access the network. Release staging performs independent signature,
notice, SBOM, provenance, and archive inspection gates.

## Public/private separation

Extension names and compatibility sources, device profiles, protocol details, and identifying metadata
may be access-controlled. They are excluded from public headers, language bindings, packages,
documentation, logs, and sanitized history. Public export requires a private denylist scan plus human
review. Repository visibility and public release are owner-only decisions.

Public inspection tools contain structural rules and invented test markers. Concrete private
identifiers are supplied through `WSE_EXPORT_DENYLIST` from outside the public checkout.
A standalone package/candidate check without that policy is a structural check only. The full
private content scan and human review are required before publication; their policy and evidence
remain outside the public source and artifacts.

WSE does not claim that a camera/display build or mock test certifies a physical device. Hardware
evidence follows [Hardware Validation](HardwareValidation.md).

## Change history

| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline, including the external publication inspection boundary. |
