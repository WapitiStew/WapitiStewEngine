# WSE Support Matrix

> Canonical source: [English WSE Support Matrix](../en/SupportMatrix.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-09

本ページは`tools/release/make_support_matrix.py`がBuild自身のMetadata - Version source、
ABI Header、Preset目録、Bootstrap manifest - から生成し、`wse.documentation.support_matrix`
Gateが本文とそれらの乖離を拒否します。編集は生成器に対して行い、本ファイルは編集しません。

## VersionとABI

- WSE version: **1.0.0**
- Flat C ABI version（`WSE_CAPI_ABI_VERSION`）: **1**
- Binding ABI version（Node／Python／Java）: **1**

## PlatformとComponent

セルは、維持されているCMake PresetがそのPlatformでComponentをBuildすることを示します。
Hardwareの主張は別勘定です: 実機検証の状態は
[Hardware Validation設計](../design/ja/HardwareValidation.md)にあり、実機Evidenceが
受理されるまで`HARDWARE_NOT_RUN`のままです。

| Platform | Core | XPT | GEF | IUI | OUI | TMR |
| --- | --- | --- | --- | --- | --- | --- |
| Linux ARM64 (GCC cross) | 対応 | 対応 | 対応 | 対応 | 対応 | 対応 |
| Linux x64 (Clang) | 対応 | 対応 | 対応 | 対応 | - | 対応 |
| Linux x64 (GCC) | 対応 | 対応 | 対応 | 対応 | 対応 | 対応 |
| Windows x64 (MSVC) | 対応 | 対応 | 対応 | 対応 | 対応 | 対応 |

Access-controlledなVPJ Componentと各言語Bindingは、これらのPresetに重ねるConfigure時
Optionです。Release Archiveの正確な内容は`wse-package.json`が記録します。

## Pinned third-party dependencies

| Dependency | Pinned version | License |
| --- | --- | --- |
| libcamera | 0.7.2 | LGPL-2.1-or-later |
| libcurl | 8.21.0 | curl |
| libjpeg-turbo | 3.0.4 | IJG, BSD-3-Clause and Zlib |
| nlohmann-json | 3.12.0 | MIT |
| node-api-headers | 24.19.0 | MIT |
| openssl | 3.5.8 | Apache-2.0 |
| pybind11 | 3.1.0 | BSD-3-Clause |
