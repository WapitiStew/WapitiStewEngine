# Changelog

## [1.1.0] - 2026-10-15

XPT serial accepts any positive int32 baud-rate request (1 through 2147483647).
Windows and Linux verify OS-reported speeds after configuration; rejection,
readback failure or mismatch preserves the existing owner. The original 15 rates
and arbitrary boundary values have virtual coverage. JavaScript rejects fractional,
non-finite and out-of-range values before narrowing. Physical serial validation
is skipped (`HARDWARE_NOT_RUN`); OS readback does not certify signal accuracy.

Source version, JavaScript/.NET package metadata and synchronized English/Japanese documentation
now identify WSE 1.1.0. Binding ABI and C ABI remain 1. Existing API and layout contracts are unchanged.

## [1.0.0] - 2026-09-20

Initial public baseline for WSE 1.0.0.

Supported configurations and current limitations are documented in the
[Support Matrix](doc/en/SupportMatrix.md). Version and compatibility follow the
[version policy](doc/design/en/VersionCompatibility.md).
