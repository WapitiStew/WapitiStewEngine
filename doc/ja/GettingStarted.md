# WSE Getting Started

> Canonical source: [English Getting Started](../en/GettingStarted.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

新規ConsumerがSourceから検証済みInstall Packageを作る最短手順です。CommandはWSE source rootから
実行します。CMake ConfigureはDependencyをDownloadしません。

## 1. 構成を選ぶ

最初はCore-onlyを使用し、Applicationが必要とする公開Componentだけを追加します。

| 目的 | Windows Preset | Linux Preset |
| --- | --- | --- |
| 最小Shared Package | `windows-msvc-shared-core` | `linux-gcc-shared-core` |
| 最小Static Package | `windows-msvc-static-core` | `linux-gcc-static-core` |
| XPT | `windows-msvc-shared-xpt` | `linux-gcc-shared-xpt` |
| GEF | Core Presetへ`WSE_BUILD_GEF=ON`を追加 | `linux-gcc-shared-gef` |
| IUI | Core Presetへ`WSE_BUILD_IUI=ON`を追加 | `linux-gcc-shared-iui` |
| OUI | `windows-msvc-shared-oui` | `linux-gcc-shared-oui` |
| Tmr | `windows-msvc-shared-tmr` | `linux-gcc-shared-tmr` |
| Windows全公開Module | `windows-msvc-shared-public` | 単一Presetなし。対応Moduleを明示指定 |

GEFとIUIはWindows／Linuxに対応します。Linux IUIは読取専用evdev Keyboard状態APIであり、状態取得前に
Availabilityを確認します。Raspberry Pi ARM64 PresetはCross build Gateであり、Runtimeまたは実機認証ではありません。

## 2. Configure、Build、Test

Windows Shared Core／Release:

```bat
cmake --preset windows-msvc-shared-core
cmake --build --preset windows-msvc-shared-core-release
ctest --test-dir build\windows-msvc-shared-core -C Release --output-on-failure
cmake --install build\windows-msvc-shared-core --config Release --prefix C:\wse-sdk
```

Linux Shared Core／Debug:

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
```

XPT、OUI、Tmrまたは言語Bindingでは、[Build／Install Guide](BuildGuide.md)の明示Bootstrapで固定Dependency
Cacheを準備します。Offline modeはNetworkへFallbackしません。

## 3. Consumerを作る

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(wse_consumer LANGUAGES CXX)

find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS Core)
add_executable(wse_consumer main.cpp)
target_compile_features(wse_consumer PRIVATE cxx_std_17)
target_link_libraries(wse_consumer PRIVATE WSE::Core)
```

`main.cpp`は検証対象の[C++ Quick start](../../example/cpp/core/quickstart.cpp)です。Consumer Configureでは
Windowsに`-DCMAKE_PREFIX_PATH=C:/wse-sdk`、Linuxに`/opt/wse-sdk`を指定します。

## 4. Optional言語Binding

- JavaScript: Node.js 18+とNode-API Package。[example/js/core/quickstart.js](../../example/js/core/quickstart.js)へ
  Install package Directoryを渡します。
- Python: CPython 3.11+とpybind11。[example/python/core/quickstart.py](../../example/python/core/quickstart.py)へ
  Install済みNative moduleを渡します。
- Java: JDK 17+とJNI。[example/java/core/QuickStart.java](../../example/java/core/QuickStart.java)を`wse.jar`と
  Install済みNative moduleに対してCompile／実行します。

Quick startはRuntime／Component情報だけを読み、HardwareをOpenしません。Camera取得は[How-to](HOWTO.md)を
参照してください。

## 5. 次に読む文書

- [Build／Install Guide](BuildGuide.md): Bootstrap、全Option、Package、ARM64、Troubleshooting
- [How-to](HOWTO.md): XPT、OUI、Tmr、言語別操作
- [API Reference](API_REFERENCE.md): Installされる公開面
- [Architecture](../design/ja/Architecture.md): Module／Platform境界

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
