# Installed WSE Consumer Test

このProjectはInstall済みWSE Packageだけを使い、Source treeへの暗黙依存を検出する。

検査内容:

- `WSE::Core`のCompile／Link／Runtime loadと、削除済み互換Alias `WonderStewEngine`が復活していないこと
- C++17 usage requirement
- All-components PackageのTarget、公開入口HeaderのCompileおよびDependency
- Public all-optional PackageのXPT／GEF／IUI／OUI／Tmr Targetと、VPJ／Private dependency非混入
- XPT-only Packageの`WSE::Xpt` Target、公開入口Headerおよび他Optional Target非混入
- GEF-only Packageの`WSE::Gef` Target、公開入口Header、Binary APIおよび他Optional Target非混入
- IUI-only Packageの`WSE::Iui` Target、公開入口Header、Backend状態および他Optional Target非混入
- OUI-only Packageの`WSE::Oui` Target、公開入口Headerおよび他Optional Target非混入
- OUI-only PackageのBackend非依存`Renderer` API、Texture upload／Mesh symbol、Lifecycle／Error契約およびNative Header非露出
- OUI-only PackageのPortable `ProjectionPipeline`、Layer descriptor、2倍Supersample ValidationおよびLifecycle Error symbol
- Tmr-only Packageの`WSE::Tmr` Target、Camera／Calibration公開Header、Session lifecycle Error symbolおよび他Optional Target非混入
- VPJ PackageのConsumerが`WSE::Vpj`だけをLinkし、`WSE::Xpt`／`WSE::Core`を推移的に解決すること
- VPJ Target／Package metadataへOUI、IUI、Tmr、OpenCVまたはPrivate JPEG Targetが漏れないこと。GEFは許可境界だが現行VPJでは非必須
- Static OUI consumerへのD3D12／DXGI／D3DCompiler／User32 System link要件の伝播
- Core-only PackageへのOptional Target、HeaderおよびDependency非混入
- Shared／Static metadataと`WSE_STATIC`の伝播

例:

```bat
cmake -S engine/wse/test/consumer -B build/wse-consumer -G "Visual Studio 17 2022" -A x64 ^
  -DWSE_PACKAGE_ROOT=C:/path/to/installed/wse ^
  -DWSE_EXPECT_CORE_ONLY=ON
cmake --build build/wse-consumer --config Debug
ctest --test-dir build/wse-consumer -C Debug --output-on-failure
```

Private全機能Packageでは`WSE_EXPECT_ALL_COMPONENTS=ON`、Public全Optional Packageでは
`WSE_EXPECT_PUBLIC_COMPONENTS=ON`、XPT-only Packageでは
`WSE_EXPECT_XPT_ONLY=ON`、GEF-only Packageでは`WSE_EXPECT_GEF_ONLY=ON`、
IUI-only Packageでは`WSE_EXPECT_IUI_ONLY=ON`、OUI-only Packageでは`WSE_EXPECT_OUI_ONLY=ON`を使用する。
Tmr-only Packageでは`WSE_EXPECT_TMR_ONLY=ON`を使用する。
VPJ Packageでは`WSE_EXPECT_VPJ_CONTROL=ON`を使用し、Application側のLink entryは`WSE::Vpj`だけにする。
