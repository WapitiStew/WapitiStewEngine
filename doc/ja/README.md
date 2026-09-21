# WonderStewEngine SDK

> Canonical source: [English SDK overview](../en/README.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

WonderStewEngine（WSE）は、複数Projectで再利用する汎用C++ Library群です。CoreのData、Log、
Utilityを常に提供し、通信（XPT）、File（GEF）、Input（IUI）、Output／Projection（OUI）およびCamera
（Tmr）を選択して組み込めます。

## 現在の対応範囲

- Windows x86-64：Coreおよび全Optional component、Visual Studio 2022／MSVC
- Linux x86-64：Core、XPT TCP／UDP／HTTP／SerialPort／RetryPolicy、GEF、IUI evdev Keyboard、OUI Vulkan 1.2／Wayland／DRM-KMS、およびTmr V4L2／libcamera、GCCまたはClang
- C++17
- Shared libraryとStatic library
- CMake Install／Export package
- Node.js 18以降向けOptional Node-API JavaScript Binding
- CPython 3.11以降向けOptional pybind11 Python Binding
- Java 17以降向けOptional JNI Binding
- .NET 8上のC#向けOptional 平坦C ABI／P/Invoke Binding

Linux ARM64はCore／XPT／GEF／IUI／OUI／TmrとNode-API／Python／Java JNI Native moduleのGCC Cross buildを
検証済みです。Raspberry Pi 4のCore／XPTと4言語の公開Bindingには、受理済みの`HARDWARE_SMOKE_PASS`
Evidenceがあります。OUIのOffscreen検証と、残るCamera／DisplayおよびPi 5のGateの範囲は
[Hardware Validation設計](../design/ja/HardwareValidation.md)を参照してください。
Linux XPTはTCP／UDP／HTTP／Portable Serial／明示Retry Policyに対応します。
Linuxで未対応のOptional componentを有効化するとCMake Configureで
明示的に失敗します。
Linux IUIはKernel evdevの読取可能Keyboardを監視し、Device grabやPermission変更を行いません。
Snapshotを解釈する前に`Keyboard::accessState()`または`isAvailable()`を確認します。詳細は
[IUI Keyboard設計](../design/ja/IuiKeyboard.md)を参照してください。
JavaScript／Python／Java／C# BindingをNode-API／pybind11／JNI／平坦C ABIで利用でき、TmrとHandle-free OUI
Projectionを4言語から操作できます。言語別のGolden、Lifecycle、CancellationおよびRuntimeの検証範囲は
[多言語Binding設計](../design/ja/LanguageBindings.md)を参照してください。

## Component

| Component | CMake Target | Role |
| --- | --- | --- |
| Core | `WSE::Core` | Data、Log、Utility、License |
| XPT | `WSE::Xpt` | 通信機能 |
| GEF | `WSE::Gef` | Binary／CSV／設定File |
| IUI | `WSE::Iui` | Input device |
| OUI | `WSE::Oui` | Display、Renderer、Projection |
| Tmr | `WSE::Tmr` | Camera |

Consumerは`WSE::*`のComponent Targetを使用します。Consumer向け互換Alias `WonderStewEngine`は削除済みです。
`find_package`で指定するPackage名は`WonderStewEngine`のままです。
[Build／Packaging設計](../design/ja/BuildPackaging.md)を参照してください。
Access-controlled legacy extensionは公開Packageへ含めてはいけません。

## Quick start

```cpp
#include <wse/stew.h>

int main()
{
    wse::double_xy point(10.0, 50.0);
    return point.x == 10.0 ? 0 : 1;
}
```

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE WSE::Core)
```

## Documentation

- [Getting Started](GettingStarted.md)
- [Build／Install Guide](BuildGuide.md)
- [How-to](HOWTO.md)
- [公開API Reference](API_REFERENCE.md)
- [WebCamera移行Guide](WebCameraMigration.md)
- [非推奨API・移行Guide](DeprecationMigration.md)
- [Portable Core設計](../design/ja/PortableCore.md)
- [XPT Transport設計](../design/ja/XptTransport.md)
- [OUI Projection設計](../design/ja/OuiProjection.md)
- [Tmr Camera設計](../design/ja/TmrCamera.md)
- [多言語Binding設計](../design/ja/LanguageBindings.md)
- [Architecture](../design/ja/Architecture.md)
- [Thread／Ownership Model](../design/ja/ThreadOwnership.md)
- [Build／Packaging設計](../design/ja/BuildPackaging.md)
- [Version／Compatibility設計](../design/ja/VersionCompatibility.md)
- [Security／Privacy設計](../design/ja/SecurityPrivacy.md)
- [Hardware Validation設計](../design/ja/HardwareValidation.md)
- [旧SDK仕様書からの案内](SDK.md)

Licenseは確定したRepository Licenseと各第三者DependencyのLicenseを確認してください。公開、商用利用、
Private moduleの配布はOwner承認なしに行わないでください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
