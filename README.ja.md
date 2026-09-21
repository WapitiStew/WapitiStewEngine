# WonderStewEngine

> Canonical source: [English README](README.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-21

WonderStewEngine（WSE）は、複数Projectで再利用するC++17 Library群です。Coreは常にBuildし、通信、File、
Input、Renderer／Projection、Cameraおよび言語BindingをConsumerごとに選択します。

| Component | CMake Target | 用途 | 現在の公開Platform |
| --- | --- | --- | --- |
| Core | `WSE::Core` | Data、Logging、Utility、Binding Facade | Windows x86-64、Linux x86-64／ARM64 Build |
| XPT | `WSE::Xpt` | TCP、UDP、HTTP、Serial、Retry Policy | Windows／Linux |
| GEF | `WSE::Gef` | Generic Binary／CSV | Windows／Linux |
| IUI | `WSE::Iui` | Keyboard入力状態 | Windows、Linux evdev |
| OUI | `WSE::Oui` | Renderer、Display、Portable Projection | Windows D3D12、Linux Vulkan 1.2 |
| Tmr | `WSE::Tmr` | Backend非依存Camera制御 | Windows MF、Linux V4L2／libcamera |

Shared／Static Library、Install可能なCMake Packageと、Node-API／pybind11／JNI／平坦C ABIによる
JavaScript／Python／Java／C#（.NET 8）のOptional Bindingを提供します。Linux ARM64 Cross buildは検証済みです。
Raspberry Pi 4のCore／XPTと4言語Bindingには受理済みの`HARDWARE_SMOKE_PASS` Evidenceがありますが、
Camera／Display実機認証は未完了です。OUIのOffscreen検証と未実施のPi 5 Gateを含む正確な範囲は
[Hardware Validation設計](doc/design/ja/HardwareValidation.md)を参照してください。BuildやSmoke成功を実機認証済みと表示しません。

## Quick start

公開Sourceを取得します。

```sh
git clone https://github.com/WapitiStew/WapitiStewEngine.git
cd WapitiStewEngine
```

WSE source rootからWindows Core-onlyをBuildします。

```bat
cmake --preset windows-msvc-shared-core
cmake --build --preset windows-msvc-shared-core-release
ctest --test-dir build\windows-msvc-shared-core -C Release --output-on-failure
```

Linuxでは次を使用します。

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
```

配布可能なWindows全公開Componentには`windows-msvc-*-public`を使用します。Access-controlled統合Presetから
Public artifactを作成してはいけません。CMake ConfigureはDependencyをDownloadしないため、必要なOptional
Componentでは明示Bootstrapを先に実行します。

Buildしたい対象からMachineを準備する場合は、Package名ではなく言語とComponentを指定します。Bootstrapは
選択を固定Dependencyと検証用Toolchainへ解決し、Git管理外の`CMakeUserPresets.json`へConfigure Preset
`wse-local`を書き出します。

```bat
bootstrap.bat --language js --language python --component xpt --component tmr
cmake --preset wse-local
```

検出済みToolchainが常に優先され、取得したToolchainはBootstrap Cacheに留まり`vendor/`やInstall Package
には入りません。選択肢の全体は[Build／Install Guide](doc/ja/BuildGuide.md)を参照してください。

Install PackageのConsumerはComponent Targetを使用します。

```cmake
find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_application PRIVATE WSE::Core)
target_compile_features(my_application PRIVATE cxx_std_17)
```

CTestで検証するC++ Sourceは[example/cpp/core/quickstart.cpp](example/cpp/core/quickstart.cpp)です。

## 文書

- [Getting Started](doc/ja/GettingStarted.md)
- [Build／Install Guide](doc/ja/BuildGuide.md)
- [How-to](doc/ja/HOWTO.md)
- [公開API Reference](doc/ja/API_REFERENCE.md)
- [Architecture](doc/design/ja/Architecture.md)
- [文書Index](doc/README.md)
- [English README](README.md)

Internal reportは正式仕様ではありません。Access-controlled Extensionの名称、Source、Device profile、
Protocol詳細および識別MetadataをPublic package、Sanitized history、文書、LogまたはArtifactへ含めてはいけません。

## ライセンス

公開SDKには[Apache License 2.0](LICENSE)を適用します。その条件に従って商用利用、改変、再配布ができます。
帰属表示と依存物のLicense境界は[NOTICE](NOTICE)と[License Guide](doc/ja/LICENSE.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-21 | 公開SDKにApache-2.0を適用した。 |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
