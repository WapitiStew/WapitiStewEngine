# WSE Build／Packaging設計

> Canonical source: [English Build and Packaging Design](../en/BuildPackaging.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-21

## Build model

Coreは常にBuildします。`WSE_BUILD_XPT`、`WSE_BUILD_GEF`、`WSE_BUILD_IUI`、`WSE_BUILD_OUI`、
`WSE_BUILD_TMR`で公開Componentを選択します。
JavaScript／Python／Java／C# Bindingは個別Optionです。
`WSE_LIBRARY_TYPE=SHARED|STATIC`でNative Library形式を選びます。未対応Platform／Component組合せは
Configureで失敗します。

この契約でCoreは`api/wse`配下のAPIを意味します。`WSE::Xpt`等のComponent Targetは機能と依存の
Facadeであり、独立Binaryではありません。選択した全Native sourceは単一の`WonderStewEngine` Libraryへ
Buildされ、各FacadeはFeature macroとLink edgeだけを運びながらConsumer graphを強制します。これは先送り
ではなく意図した決定です - Package／Consumer検証は単一Libraryを前提に成立しており、分割が要求する
Symbol・Install・ABIの変動を正当化する実測上の必要（Size・Deployment・Link時間）は現在ありません。
Facadeが実Binaryになるのは次のいずれかが現れたときだけです: Core-only Packageの実測過大、独立出荷が
必要なDeployment単位、独立更新が必要なBinding、Runtimeから完全に外すべきOptional dependency、実測に
現れるLink時間。分割しても依存Ruleは緩和されません。

Presetは便利な構成であり、Capability認証ではありません。Windows公開Artifactは`*-public`または公開Optionを
明示し、Access-controlled Optionを無効化した構成から作ります。LinuxはComponent別Presetを使用し、GEFと
IUI evdev KeyboardはPortable対応済みです。ARM64 PresetはCross buildだけを意味し、物理Keyboard成功を
示しません。

## Dependency／Offline

CMake Configure／BuildはDependencyをDownloadしません。`bootstrap.py`／`bootstrap.bat`は分離した明示Provisioning
で、固定Version、Source identity／Hash、License、Target Platform、Architectureを検査します。Strict Offlineは
Cache missで失敗します。生成Dependencyは`vendor/`または外部`WSE_DEPENDENCY_ROOT`へ置き、Git管理しません。

## Package契約

Standalone buildは公開Header、選択したNative Target、Package Config、構成に必要なDependency Runtime／License、
Optional Binding PackageをInstallします。Example／利用文書はSource配布に保持します。Consumerは`find_package(WonderStewEngine CONFIG REQUIRED COMPONENTS ...)`と`WSE::*`を使用します。
`WonderStewEngine`はIn-tree Build Targetの名前です。Install Packageが公開するLink Targetは
`WSE::*`であり、`WonderStewEngine`というLink Targetは定義しません。

Package GateはPrivate Header／Metadata、Component Target欠落、Dependency漏出、Library形式不一致および異なる
Platform／Architectureの再利用を拒否します。外部Consumerは現行`WSE::*` TargetでShared／Staticの両方を検証し、
Packageが`WonderStewEngine`互換Aliasを定義しないことも確認します。
Commandは[Build／Install Guide](../../ja/BuildGuide.md)を参照してください。

## Build graphとSource対応: PKG-GRAPH-01

| 段階 | Source | 入力／出力 |
| --- | --- | --- |
| 選択・Provisioning | [bootstrap.py](../../../bootstrap.py)、[依存Manifest](../../../bootstrap-manifest.json) | 言語・Component・Platform・Architecture・検証済みCache -> 依存Root、Host検証Tool、生成User preset |
| Option・依存 | [WseOptions.cmake](../../../cmake/WseOptions.cmake)、[WseDependencies.cmake](../../../cmake/WseDependencies.cmake) | Optionと明示Root -> 検証済みLocal依存。Downloadしない |
| Native graph | [WseCore.cmake](../../../cmake/WseCore.cmake) | 選択Source -> 単一Native LibraryとComponent INTERFACE Target |
| 言語Graph | [WseBindings.cmake](../../../cmake/WseBindings.cmake) | Core／ComponentとSDK Header／Tool -> Optional moduleとManaged package |
| Install／Export | [WseInstall.cmake](../../../cmake/WseInstall.cmake)、[Package config](../../../cmake/WonderStewEngineConfig.cmake.in) | Artifact -> 選択Header／Library、WSETargets、移動可能な依存探索 |
| 識別情報 | [wse-package.json.in](../../../cmake/wse-package.json.in) | Version、Library形式、選択Component、Source tree識別情報、依存Catalog |
| 外部検証 | [Consumer](../../../test/consumer/CMakeLists.txt)、[Package verifier](../../../test/package/verify_public_package.py) | Install先だけを入力にCompile／Link／LoadとPackage構造を確認 |

```text
selection -> bootstrap cache / dependency root -> configure -> native library
                                                   +------> language modules
native library -- exported as --> WSE::Core
selected public facade -- transitive link --> WSE::Core
build -> CTest -> fresh install prefix -> package scan -> external consumer -> runtime test
```

選択したNative ComponentごとにEngine Binaryは増えません。Python／Node／JNI moduleとC ABI shimは追加の
Load可能なBinaryです。STATICはEngineのLink形式を意味し、 `.node` 、Python拡張、JNI Library、P/Invoke shimを
静的な言語Packageに変えません。LinuxのModuleへ使うStatic Engine Objectは位置独立である必要があります。
C#単独を含む4 Bindingのいずれかを選ぶと、Static Engine targetの`POSITION_INDEPENDENT_CODE`を有効にします。

## Provisioningの順序と失敗: PKG-PROVISION-02

1. `--language`／`--component`からNative Option、依存、検証Toolchainを解決します。別Architecture向けでも
   検証ToolはHost上で実行するものを選びます。
2. 使用可能な導入済み検証Toolを先に検出し、無ければ検証済みCache／Provisioningを使います。
   取得したNode／JDK／CPython／.NETはBootstrap cacheだけに置き、Testに必要という理由でVendor、
   Install依存、SBOMへ含めません。
3. 依存のVersion、License、Hash／Source identity、Target Platform／Architectureを検査します。
   Offlineは必要なCache欠落で失敗します。`--no-fetch-toolchain`は不足Toolの取得を禁止し、
   `--check`はProvisioningせず既存Materialを検査します。
4. 無効化されていなければUser presetを生成します。生成物を手修正せず選択・Manifest・Generatorを変えます。
   PresetはRoot／Optionの記録であり、Packageや実機結果ではありません。
5. ConfigureはLocal入力だけを使い、Build／CTestは選択Runtimeを使います。Cross buildではTarget用Python／JNI
   Headerを別途供給します。ARM64 Compile成功でもx86-64 InterpreterからModuleは実行できません。
   Cross buildで生成を省略したJava／C# Managed Artifactは、言語Package完成とする前に別途用意します。

互換性のないTarget間で依存・Build Directoryを再利用しません。Configure失敗をCMake内Downloadや
要求Componentの黙示無効化で回避しません。依存ArchiveはLinkを拒否し、Toolchain Archiveは検証済みの
内部相対Linkだけを保持します。Network pin検証はOpt-inであり、既定のOffline Gateに含めません。

## Install配置とLoad: PKG-LAYOUT-03

| PrefixからのPath | 内容／所有者 |
| --- | --- |
| include/wse/api/wse | Binding／C ABI宣言を含むCore公開Header |
| include/wse/api/{xpt,gef,iui,oui,tmr} | 選択公開ComponentだけのHeader。OS／Private実装Headerは除外 |
| bin | Windows Engine／Runtime DLL、対応Platformで選択したNode／JNI／C ABI共有Module |
| lib | Native Static Library、Windows Import Library、Linux共有Engine Library |
| cmake | WonderStewEngineConfigとWSETargets Import |
| lang/python/wse | Python拡張、Loader、型宣言、py.typed |
| lang/js | JS Loader、TypeScript宣言、Package metadata |
| lang/java | Buildで生成したJava 17互換wse.jar |
| lang/cs | 生成したnet8.0 Managed assemblyとXML文書 |
| vendor | 選択依存のPayload、License、識別情報。検証Toolchainを含めない |
| wse-package.json | Package識別情報と選択Component／Binding Catalog |

Headerの実体は一つです。`WSE::Core`は`include`と`include/wse/api`の両探索Rootを公開し、選択したNative
ComponentのFacadeはCoreへのLinkを通じて継承します。STATICでは最終Link時にPrivate Backendの
依存も必要になり得ます。XPTは同梱curl Targetを再構成し、Linux OUIはVulkan／Wayland／DRMを探索し、
libcameraは有効なPackage方針に従います。公開APIにThird-party型を出すこととは別のLink要件です。
SHARED Consumerでは同じStatic Backendを再構成する必要はありません。

**PKG-BINDLOAD-06:** LinuxのLoad可能Bindingは、SHARED／STATICの双方で自身のInstall位置から同梱libcameraを探索します。
Pythonは自身から3階層上のvendor/libcamera/lib、Node／JNI／C ABI Shimは1階層上を相対探索します。
Static Engine archiveのRuntime探索Pathは、それを取り込むModuleへ自動継承されません。
wse.binding.installed_libcameraは新規PrefixへInstallし、Build時のLoader上書きを除き、
有効なPython／Node PackageをImport、JNI／C ABI BinaryをLoadします。実際にMapされたlibcameraが
そのPrefix内か確認します。JNI／C ABIのLoadだけではManaged操作や実Cameraを検証しません。
[Installed Load Test](../../../test/package/installed_libcamera_runtime_contract.py)は、libcameraを同梱し、
Binding実行環境のあるNative Linux Standalone buildにだけ登録します。

`WSE::Dotnet`はImportされたC ABI共有Shimであり、Native ComponentのFacadeではありません。現行Targetは
Include探索Rootと`WSE::Core`への推移Linkを公開しません。C/C++から直接利用するC ABI Clientは
`WSE::Dotnet`をLinkし、`#include <wse/capi/wse_capi_core.h>`用にInstall先の`include/wse/api`を明示追加します。
呼出元が`WSE_PACKAGE_PREFIX`をInstall先Prefixに設定し、`cabi_client`を定義済みの場合は次の形です。

```cmake
find_package(WonderStewEngine CONFIG REQUIRED
    PATHS "${WSE_PACKAGE_PREFIX}/cmake" NO_DEFAULT_PATH)
if(NOT TARGET WSE::Dotnet)
    message(FATAL_ERROR "This consumer requires the C ABI shared shim")
endif()
target_include_directories(cabi_client PRIVATE "${WSE_PACKAGE_PREFIX}/include/wse/api")
target_link_libraries(cabi_client PRIVATE WSE::Dotnet)
```

Include先は同じInstall済みPackageから指定します。公開C宣言の探索設定であり、Runtime Loaderの探索変更や
Source／Build treeへのFallbackではありません。

Linux共有Node／JNI／C ABI ModuleのEngine探索は`$ORIGIN/../lib`、Pythonは`$ORIGIN/../../../lib`です。
Windows PythonはInstall先binをDLL探索へ追加します。JSはLoaderからの相対位置または`WSE_NODE_ADDON`、
C#は明示Path、`WSE_CAPI_LIBRARY`、既定探索の順に解決します。最上位ModuleのPath指定だけではすべての
依存DLLを発見できる保証はありません。Source／Build Directoryの外から、意図したRuntime探索Rootだけで検証します。

## Consumer選択と識別情報の限界: PKG-CONSUME-04

現行Configは選択TargetをImportしますが、要求ごとの`*_FIND_COMPONENTS`検証を実装していません。
`find_package(... COMPONENTS ...)`成功だけでComponentの存在を証明できないため、必要Targetを明示検査してLinkします。

```cmake
find_package(WonderStewEngine CONFIG REQUIRED)
if(NOT TARGET WSE::Tmr)
    message(FATAL_ERROR "This consumer requires a package built with Tmr")
endif()
target_link_libraries(camera_app PRIVATE WSE::Tmr)
```

`camera_app`はConsumer側で定義済みのExecutableです。Install先cmakeをWonderStewEngine_DIR、または
PrefixをCMAKE_PREFIX_PATHへ渡します。Source／Build tree HeaderへFallbackしません。
Install PackageはWonderStewEngine互換Targetを提供しません。

wse-package.jsonの`source_commit`は現在HEADから取得するGit Tree識別情報であり、作業Tree全ByteのHashではなく、
未Commit変更は反映しません。依存Catalogには未選択の既知依存も存在し得るため、Component／Bundled Flagと
実際の内容を使います。Manifestだけで鮮度、残存Fileの不在、Binary ABI互換性、実機実行を証明できません。
Installは以前の無効Componentを削除しないので、選択構成ごとに新しいPrefixを使います。
Build構成と検証結果をArtifactとともに保持し、Field名から強い識別保証を推測しません。

## 検証と再現: PKG-VERIFY-05

具体的Commandは[Build Guide](../../ja/BuildGuide.md)と `test/consumer` の外部Consumer Projectを使い、
新しいPrefixを使います。Configure／Build、CTest、Install、正確な選択Component／Bindingを指定した
verify_public_package、外部ConsumerのConfigure／BuildとCTestの順です。Package変更ではSHARED／STATICを検証します。
Binding TestはHostで有効・対応する場合だけ実行し、Cross Compileだけの結果は区別します。

| 証拠 | 確認できること | 確認できないこと |
| --- | --- | --- |
| Bootstrap／Offline-kit契約 | Resolver、識別検証、Offline失敗、展開Case | Opt-in Network検証なしでの全Remote Archive取得可能性 |
| 公開Header／API／非推奨Gate | 選択Source／Header方針 | Install Directory内容そのもの |
| Package scan | 要求Header、Target、Binding File、除外Metadata | Clean Processでの全Module Load |
| 外部Consumer Build／CTest | その構成のInstall Target／Header／Link／Runtime経路 | 他OS／Architecture／Library形式 |
| 5言語Quickstart | 有効なBuild Runtime入口 | Install先を明示しない場合のInstall済みLoad |

最初の再現課題ではInstall済み公開Header／TargetだけでCore Consumerを作り、不在Componentを意図的に要求して
上のTarget検査で診断します。準備済みPackageは入力にできますが、実装SourceやIn-tree BuildへのFallbackは使いません。
[設計と検証](DesignVerification.md)で既存Example実行と区別します。

## SDK Offline Kitの識別

Offline Tool（Checkout内の`tools/offline/README.md`）は`wse-offline-kit-candidate-v2`を生成し、
Candidateに同梱するSchemaに対応した`schemaVersion: 2`の受入Manifestを作成します。
`wse.repository`はSDKの論理名`Engine`です。`commit`と`tree`はSDK Checkoutを指し、
`tree`はInstall済みPackageの`source_commit`と一致する必要があります。Checkout先の
Directory名や利用Applicationの識別情報はSDKのMetadataに含めません。Applicationの
配布Toolは自身のRevisionを管理し、ChecksumによってSDK Candidateへ対応付けます。
`bootstrap-manifest.json`はSDK依存と検証Toolを定義します。Application用Packageの
配置先とBuild Recipeは利用側Repositoryが所有します。

## PackageのLicense

すべてのInstall packageは公開SDKのApache-2.0 `LICENSE`と帰属表示`NOTICE`を保持します。
`wse-package.json`の`license`は選択したSDKとExtensionの組合せのLicense expression、
`license_files`は同梱するRoot直下の条件・表示File一覧です。依存物の条件は別に維持します。
SBOM生成はこの明示されたExpressionを読み、License metadataやFileの不足時に失敗します。
古いPackageに新しい条件を暗黙適用してはいけません。公開Package検査はSDKのLicenseとNoticeの
Byte一致も検査します。InstallするNode packageのMetadataも同じ組合せのExpressionを使います。

OverlayはTarget hookで`WSE_EXTENSION_LICENSE_IDENTIFIER`と`WSE_EXTENSION_LICENSE_FILE`を供給します。
選択したOverlayにこれらの条件がなければConfigureに失敗します。公開LICENSEとNOTICEを保持したまま、
ExtensionのLicenseを`LICENSE.extension`としてInstallします。PackageとSBOMには
`Apache-2.0 AND <extension-license-identifier>`を記録します。これは別Licenseの部分を含むことを示し、
ExtensionへのApache許諾を意味しません。Extensionの識別子と条件はOverlayだけが供給します。
`test/ci/test_licensing.py`のOffline fixtureが公開・併用Install、依存物の帰属表示、
条件の不足や誤った限定の拒否を検証します。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-21 | 公開SDKのApache表示と、公開・併用Packageの明示的なLicense metadataを追加した。 |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
