# WSE Build／Install Guide（Windows／Linux）

> Canonical source: [English Build and Install Guide](../en/BuildGuide.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Compilerと文書の品質Gate

`WSE_WARNING_SWEEP=ON`はNative LibraryにMSVCの`/W4`、GCC／Clangの
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`を有効にします。
`WSE_WARNINGS_AS_ERRORS=ON`は同じ警告設定に加え、警告をBuild Errorとして扱います。
両Optionは既定`OFF`で、ConsumerのCompile Flagや第三者Targetには適用しません。
Core GCC CIは警告Error化と空のBaselineを使います。Windows Core CIは既存DLL Interface警告の
承認済みFile別Baselineを使い、警告の追加・増加を拒否します。これらのCore Gateは、
全Optional Componentや各言語Compilerの警告ゼロを認証するものではありません。

```sh
cmake --preset linux-gcc-shared-core -DWSE_WARNINGS_AS_ERRORS=ON
cmake --build build/linux-gcc-shared-core --parallel 4
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
python3 doxy/RunDoxygen.py --preset user-public --check
python3 doxy/RunDoxygen.py --preset developer-public --check
```

Windowsでは`cmake --preset windows-msvc-shared-core -DWSE_WARNING_SWEEP=ON`の後、
`cmake --build build/windows-msvc-shared-core --config Debug`と
`ctest --test-dir build/windows-msvc-shared-core -C Debug --output-on-failure`を使います。
[Doxygen入口](../../doxy/README.md)はDoxygen 1.13.2以降で日英両方を検査します。
`--check`を外すとHTML・LaTeX・図を生成し、Graphvizの`dot`も必要になります。
警告や生成失敗があればCommandは失敗し、診断用の警告Logを保持します。
Tool準備はConfigure／Buildから分離します。CIは公開User／Developerの両方を日英で完全生成します。
User／Developer Profile、Confidential Overlayの選択、起動Fileと分離した出力先は
[文書生成設計](../design/ja/DocumentationGeneration.md)を参照してください。

## Prerequisite

- C++17
- CMake 3.24以上（Preset利用時は3.25以上）
- Windows x86-64：Visual Studio 2022 Desktop C++ workload。Coreおよび全Optional component対応
- Linux x86-64：GCCまたはClang。Core、GEF、IUI evdev Keyboard、XPT TCP／UDP／HTTP／SerialPort／RetryPolicy、Vulkan 1.2 OUIおよびTmr V4L2／libcamera対応
- Linux ARM64：GCC Cross buildとRaspberry Pi 4／5向けNative Core／GEF／IUI／XPT／Vulkan 1.2 OUI／Tmr buildを受理するが、実機認証は未完了
- Python 3.8以上：Dependency Bootstrapで使用。固定SourceをCheckout／Bundleから準備するDependencyではGitも必要

## 言語／Component選択によるBootstrap

`bootstrap.py`はPackage名ではなく、Buildしたい言語とComponentからMachineを準備できます。選択を固定
Dependency、検証用Toolchain、CMake Optionへ解決し、User-local Configure Presetを生成します。

```bat
bootstrap.bat --language js --language python --component xpt --component tmr
cmake --preset wse-local
cmake --build build\wse-local --config Release
ctest --test-dir build\wse-local -C Release --output-on-failure
```

言語は`cpp`、`cs`、`java`、`js`、`python`です。Componentは`xpt`、`gef`、`iui`、`oui`、`tmr`です。
Componentは必要なComponentを含意し、`CMakeLists.txt`の自動有効化規則と一致します。

明示的な選択が優先されます。選択が必要とするPackageのみを準備し、Vendor Dependencyを必要としない言語
では何も準備しません。`--language`と`--component`を指定しない場合の動作は従来どおりで、既定Package
一式を準備します。

### 検証用Toolchain

言語Bindingには、WSEがLinkもせず配布もしないToolが必要です。Node.js Runtime、JDK、CPython Interpreter、
.NET SDKが該当します。Bootstrapは次の順序で解決します。

1. Install済みのTool。`WSE_NODE_HOME`、`WSE_JAVA_HOME`または`JAVA_HOME`、`WSE_PYTHON_HOME`、
   `WSE_DOTNET_ROOT`または`DOTNET_ROOT`、続いて`PATH`から検出します。固定最小Versionを満たすものが
   常に優先されるため、既存Toolchainを持つMachineはそれを使い続けます。
2. `.bootstrap-cache/toolchains/`に準備済みのTool。
3. 当該Host向けの固定Archive。Manifest記載のSHA-256で検証します。

ToolchainはDependencyではなく検証用Toolのため、取得したToolchainは `.bootstrap-cache/toolchains/` にのみ
展開します。`vendor/`、Install Package、再配布対象には入りません。したがってToolchainを準備してもWSEの
配布物とSBOMは変化しません。

Node.js 24.19.0、Eclipse Temurin JDK 21.0.12.1+1および.NET SDK 8.0.424は、Windows／Linuxのx86-64と
ARM64について固定Archiveを持ちます。CPython 3.13.15はWindows x86-64／ARM64向けに固定します。Bindingの
Buildに必要な開発Headerとimport Libraryを含む再配布可能Packageを使用します。Linuxでは Interpreter と
Headerをsystem Package Managerから導入するため、Bootstrapは検出のみを行い、見つからない場合は
`apt install python3-dev`を案内します。`--no-fetch-toolchain`は取得を完全に禁止します。自前Toolchainを
持つCI ImageではこのOptionを使用します。

固定Archiveは、いずれもVendorが公開するDigestを記録します。Node.jsの`SHASUMS256.txt`とAdoptium Release
MetadataはSHA-256、.NET Release MetadataとNuGet CatalogはSHA-512です。BootstrapはEntryが固定する全
Digestを検証し、Digestを1つも持たないEntryはDownload前に拒否します。またToolchainはCross build時でも
Host PlatformとHost Architectureで選択します。検証用ToolはBuildを実行するMachine上で動作する必要がある
ためです。`--target-architecture`はWSEがLinkするDependency Artifactのみを選択します。

検証用Toolchainは、内部の相対Linkを保持して展開します。Node.jsは`bin/npm`を`lib/node_modules`へ、JDKは
Module別のLicense Fileを共有実体へ向けるためです。展開Treeの外へ解決されるLinkと絶対PathのLinkは拒否し
ます。Dependency Archiveは従来どおり厳格に展開し、Linkを一切許可しません。内容を`vendor/`へ複製して
再配布するためです。

Windows x86-64とLinux x86-64は、Toolchain準備からTest Suite全体まで通しで実行して確認しています。
Linux ARM64は64-bit Debian 13のRaspberry Pi 4で確認しています。固定したNode.js／Temurin JDK／.NET SDKの
Archiveを取得して実行し、WSE本体も4言語Binding込みでNative buildし、Test Suiteが通ります。Windows ARM64の
ArchiveはVerify済みですが、当該Host上での実行は未実施です。

`tools/bootstrap/verify_toolchain_pins.py`は、任意のHostから全固定Archiveを検証します。各DigestをVendorが
現在公開している値と照合し、Archive自身のMember表から`extracted_root`と全`required_files`の存在を確認し、
実際の展開も実行します。Bootstrapが拒否するMemberを、準備先のHostで初めて踏む前に検出するためです。

```bat
python tools\bootstrap\verify_toolchain_pins.py
python tools\bootstrap\verify_toolchain_pins.py --identity-only
python tools\bootstrap\verify_toolchain_pins.py --toolchain nodejs --keep
```

Vendorへ接続するため既定のGateには含めません。`WSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION`が、
`network;bootstrap` Label付きのOpt-in Test 2件として登録します。固定Versionを変更した際に実行してください。

```bat
cmake --preset windows-msvc-shared-core -DWSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION=ON
ctest --test-dir build\windows-msvc-shared-core -C Release -L network --output-on-failure
```

`wse.bootstrap.toolchain_pin_identity`は公開Digestの再読込のみで数秒で終わります。
`wse.bootstrap.toolchain_pin_contents`は全Archiveを取得するため約2GBを要します。

### Cross build

`--target-architecture`はWSEがLinkするArtifactを選択します。生成PresetはToolchain Fileを持つ公式Cross
Presetを継承し、当該TargetでCMakeが推論できないDependency Rootを固定します。したがってLinux ARM64のXPTを
選択すると、libcurlがLinkする固定OpenSSLも併せて準備します。

```sh
python3 bootstrap.py --target-architecture arm64 --component xpt
cmake --preset wse-local
```

検証用ToolchainはこのときもHost Nativeのままです。JDKやNode.js RuntimeはBuild対象のMachineではなく、
Buildを実行するMachine上で動作する必要があるためです。

### 生成されるPreset

解決した選択は`CMakeUserPresets.json`にConfigure Preset `wse-local`として書き出されます。Host既定のCore
Presetを継承し、解決したToolchain PathとComponent Optionを追加します。`CMakeUserPresets.json`はCMake
標準のUser-local機構でありGit管理外のため、Machine固有PathがRepositoryに入りません。生成File自体を編集
せず、選択を変更してBootstrapを再実行してください。`--preset-name`、`--base-preset`、`--user-presets`、
`--no-write-presets`で名称、継承元、出力先の変更および出力抑止ができます。

選択解決もOffline安全です。`--offline`はToolchain取得を含む全Remote操作を拒否し、不足しているCache
ArchiveをName／Version付きで報告します。`--check`は選択した各ToolchainのInstall状況のみを報告します。

## Dependency Bootstrap

WSE source rootの`bootstrap-manifest.json`はlibjpeg-turbo 3.0.4、
libcurl 8.21.0、libcamera 0.7.2、nlohmann/json 3.12.0、pybind11 3.1.0およびOptional Node-API Binding用
Node 24.19.0 Headerを固定します。CMake ConfigureはNetwork取得を行いません。

Networkへ接続できるStaging PCでは、WSE source rootから次を実行します。`--cache-dir`にはOffline PCへ
搬送するDirectoryを指定します。

```bat
bootstrap.bat --cache-dir C:\wse-offline-cache
bootstrap.bat --verify-cache --offline --cache-dir C:\wse-offline-cache
```

Offline PCでは、搬送済みCacheだけからDependencyを復元します。`--offline`はCache不足時もRemoteへ
Fallbackせず、Dependency名、Version、期待CommitまたはSHA-256を表示して失敗します。

```bat
bootstrap.bat --offline --verify-cache --cache-dir D:\wse-offline-cache
bootstrap.bat --offline --cache-dir D:\wse-offline-cache --vendor-root D:\wse-dependencies
bootstrap.bat --check --vendor-root D:\wse-dependencies
```

既定の配置先はWSE source内の`vendor`です。Source外へ配置した場合は、Configureで同じRootを指定します。

```bat
cmake --preset windows-msvc-shared-public -DWSE_DEPENDENCY_ROOT=D:/wse-dependencies
```

既存targetは必要Fileをすべて検査してから再利用します。中断等で不完全なtargetを検出した場合は自動削除せず
停止します。Bootstrap管理対象を置換してよいことを確認した場合だけ`--force`を使用してください。
旧`--skip-existing`は互換受理しますが、検査を省略しません。

展開Cacheには生成時のBuild Recipeを記録します。ManifestのBuild Commandを変更した場合は
以前の出力を再利用せずBuildをやり直します。旧Recipeで生成済みのTargetは黙って再利用せず
その旨を報告します。置換には`--force`が必要です。

Linux Core-only buildはこれらをLinkしません。Core＋XPT buildは固定libcurlを内部実装としてLinkし、
ARM64では固定OpenSSLも必要です。CMake ConfigureはNetwork取得を行いません。

Portable cacheに必要なSource identityは次のとおりです。

- `3.0.4.zip`（SHA-256
  `0c58853494f31a65329e567569d8614f35a74c1251bdcca10bb3d01689b35035`）
- `curl-8.21.0.tar.xz`（SHA-256
  `aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6`）と、公式の
  `curl-8.21.0.tar.xz.asc`。ManifestにはRelease manager fingerprint
  `27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2`を記録する。BootstrapはASCII Armor形式の
  Sidecar存在とArchive SHA-256を検査し、OpenPGP暗号検証はRelease stagingで別途実施する
- `libcamera-0.7.2/`のGit checkout（Commit
  `191e202178f02430b5942397c70d215cdd2056fa`）、または同Commitを含む
  `libcamera-0.7.2.bundle`
- `nlohmann-json-3.12.0.tar.xz`（SHA-256
  `42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa`）

XPT HTTP向けlibcurlだけを準備する場合は次を実行します。

```bat
bootstrap.bat --package libcurl --cache-dir C:\wse-offline-cache
bootstrap.bat --package libcurl --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package libcurl --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
bootstrap.bat --package libcurl --check --vendor-root C:\wse-dependencies
```

Linuxでは同じOptionを`python3 bootstrap.py`へ渡します。

```sh
python3 bootstrap.py --package libcurl --offline \
  --cache-dir /media/wse-offline-cache --vendor-root /opt/wse-dependencies
python3 bootstrap.py --package libcurl --check --vendor-root /opt/wse-dependencies
```

原本Archiveは両OSで共用できますが、展開Sourceと生成LibraryはWindows／Linux別のCache directoryへ
配置します。WindowsはSchannelを使うStatic Debug／Release、LinuxはOpenSSLを使うStatic Debug／Releaseを
生成し、LinuxではOpenSSL development package（Ubuntu／Raspberry Pi OSでは`libssl-dev`）が必要です。
配置先の`wse-dependency.json`はPlatform／Architectureを記録し、異なる対象の誤再利用を拒否します。
libcurlはXPTがPrivate Linkし、公開Header／CMake Targetへcurl型やHeaderを露出しません。

Linux x86-64 Tmrでlibcameraを準備する場合は、Build toolとSystem development packageを先に
導入し、固定SourceをBootstrapします。

```sh
sudo apt install meson ninja-build pkg-config python3-ply python3-yaml python3-jinja2 \
  libyaml-dev libudev-dev libssl-dev
python3 bootstrap.py --package libcamera --cache-dir /media/wse-offline-cache
python3 bootstrap.py --package libcamera --offline --verify-cache \
  --cache-dir /media/wse-offline-cache
python3 bootstrap.py --package libcamera --offline \
  --cache-dir /media/wse-offline-cache --vendor-root /opt/wse-dependencies
```

Source外へ配置した場合は`WSE_LIBCAMERA_ROOT=/opt/wse-dependencies/libcamera`をConfigureへ渡します。
Linux x86-64 Tmrでは`WSE_ENABLE_LIBCAMERA=ON`が既定で、固定Rootが不足または不整合ならConfigureを
失敗させます。無効化する場合だけ`-DWSE_ENABLE_LIBCAMERA=OFF`を指定します。Install Packageは
libcamera Runtime、IPA、Licenseおよびmetadataを同梱しますが、libudev、OpenSSLおよびYAML Runtimeは
OS側で必要です。libcamera 0.7.2はC++20を要求しますが、WSEは内部Adapter sourceだけをC++20でCompileし、
公開TargetとConsumerはC++17のままです。

Optional JavaScript Bindingでは、固定Node-API HeaderとMIT LicenseだけをBootstrapします。Header archiveは
Platform共通ですが、Addon Test／Application実行にはNode.js 18以降が別途必要です。

```bat
bootstrap.bat --package node-api-headers --cache-dir C:\wse-offline-cache
bootstrap.bat --package node-api-headers --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package node-api-headers --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
```

Cacheは`node-v24.19.0-headers.tar.gz`、SHA-256
`54f14a297d47ea0794fe272363703d9dc419c96ac68f20d890f98b63754a3e4c`および個別に固定した公式
`LICENSE`から構成します。厳格Offline検証は両方を検査します。`node-api`を`WSE_DEPENDENCY_ROOT`外へ
配置する場合は`WSE_NODE_API_ROOT`を指定します。

Optional Python Bindingではpybind11を固定Bootstrapし、CPython 3.11以降のInterpreterとDevelopment Headerを
用意します。初期Support matrixはCPython 3.11～3.14です。

```bat
bootstrap.bat --package pybind11 --cache-dir C:\wse-offline-cache
bootstrap.bat --package pybind11 --offline --verify-cache --cache-dir C:\wse-offline-cache
bootstrap.bat --package pybind11 --offline --cache-dir C:\wse-offline-cache --vendor-root C:\wse-dependencies
```

固定Sourceは`pybind11-3.1.0.tar.gz`、SHA-256
`a1cc06b524ab3edca51f8ad3895f9c4fa20b8b19283173dff4ae781449dc9639`、LicenseはBSD-3-Clauseです。
`WSE_DEPENDENCY_ROOT`外へ配置する場合は`WSE_PYBIND11_ROOT`を指定します。

Linux Core-only BuildはこれらのDependencyへLinkしません。Core-plus-XPTは固定libcurl実装へLinkし、
`WSE_LIBCURL_ROOT`を必要とします。ARM64では`WSE_OPENSSL_ROOT`も必要です。
CMake ConfigureはどちらのDependencyもDownloadしません。

## Preset

WSE source rootで実行します。

```bat
cmake --list-presets
```

Windowsの主要なConfigure Presetは次のとおりです。

- 公開可能な全Optional: `windows-msvc-shared-public`、`windows-msvc-static-public`
- `windows-msvc-shared-all`
- `windows-msvc-static-all`
- `windows-msvc-shared-core`
- `windows-msvc-static-core`
- `windows-msvc-shared-xpt`
- `windows-msvc-static-xpt`
- `windows-msvc-shared-oui`
- `windows-msvc-static-oui`
- `windows-msvc-shared-tmr`
- `windows-msvc-static-tmr`

`*-public`はXPT／GEF／IUI／OUI／Tmrを有効化し、すべてのAccess-controlled Extensionを
明示的に無効化します。配布候補の全Optional PackageではこのPresetを使用します。`*-all`はアクセス制御された
Private統合検証用であり、Public PackageやSanitized exportの生成には使用しません。

例:

```bat
cmake --preset windows-msvc-shared-core
cmake --build --preset windows-msvc-shared-core-debug
cmake --install build\windows-msvc-shared-core --config Debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
```

Node-API Addonを共有CoreとBuildする場合は、Node Headerを準備したうえでCore PresetへOptionを追加します。
`node.exe`が`PATH`にない場合は`WSE_NODE_EXECUTABLE`を指定します。

```bat
bootstrap.bat --package node-api-headers
cmake --preset windows-msvc-shared-core -DWSE_BUILD_NODE_BINDING=ON -DWSE_NODE_EXECUTABLE=C:/tools/node/node.exe
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
node -e "const wse=require('C:/wse-sdk/lang/js'); console.log(wse.runtimeInfo())"
```

CoreをAddonへ静的Linkする場合は`windows-msvc-static-core`を使用します。どちらもAddon自体はLoad可能な
`.node` Moduleです。

Windows Python Moduleは次のようにBuild／Installします。

```bat
bootstrap.bat --package pybind11
cmake --preset windows-msvc-shared-core -DWSE_BUILD_PYTHON_BINDING=ON -DWSE_PYTHON_EXECUTABLE=C:/Python314/python.exe
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
set PYTHONPATH=C:\wse-sdk\lang\python
C:\Python314\python.exe -c "import wse; print(wse.runtime_info())"
```

CoreをPython Moduleへ静的Linkする場合は`windows-msvc-static-core`を使用します。

Windows Java JNI ModuleとJava 17互換JARは次のようにBuild／Installします。JDKはWSEへ同梱しません。

```bat
cmake --preset windows-msvc-shared-core -DWSE_BUILD_JAVA_BINDING=ON -DWSE_JAVA_HOME=C:/tools/jdk
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
java -Djava.library.path=C:\wse-sdk\bin -cp C:\wse-sdk\lang\java\wse.jar com.example.Main
```

`windows-msvc-static-core`ではCoreをLoad可能な`wse_jni`へ静的Linkします。Shared Packageでは同じ`bin`の
WSE RuntimeをLoadします。

Java 17互換JARを複数Runtimeで検証する場合は、Configure時に各実行Fileを指定します。指定したRuntimeだけが
CTestへ登録されます。JDKは検証用Toolであり、VendorやInstall packageへCopyしません。

```bat
cmake -S . -B build\java-matrix ^
  -DWSE_BUILD_JAVA_BINDING=ON ^
  -DWSE_JAVA_17_EXECUTABLE=C:/tools/jdk-17/bin/java.exe ^
  -DWSE_JAVA_21_EXECUTABLE=C:/tools/jdk-21/bin/java.exe ^
  -DWSE_JAVA_25_EXECUTABLE=C:/tools/jdk-25/bin/java.exe
cmake --build build\java-matrix --config Debug
ctest --test-dir build\java-matrix -C Debug -R "wse.binding.java" --output-on-failure
```

Java 25のCTestは`--enable-native-access=ALL-UNNAMED`を自動指定します。ApplicationからJava 25で
`wse.jar`を実行する場合も同じOptionを指定してください。

C# Bindingは次のようにBuild／Installします。.NET SDKはWSEへ同梱しません。平坦C ABI上の`net8.0`
Assemblyであるため、Buildは`wse_capi`と`WapitiStew.Wse.dll`の両方を生成します。`dotnet`が`PATH`に
無い場合は`WSE_DOTNET_EXECUTABLE`を指定します。

```bat
cmake --preset windows-msvc-shared-core -DWSE_BUILD_DOTNET_BINDING=ON
cmake --build --preset windows-msvc-shared-core-debug
ctest --test-dir build\windows-msvc-shared-core -C Debug --output-on-failure
cmake --install build\windows-msvc-shared-core --config Debug --prefix C:\wse-sdk
```

`windows-msvc-static-core`ではCoreを`wse_capi`共有Libraryへ静的Linkします。P/Invokeは共有Libraryしか
解決できないため、C ABI自体は常に共有Libraryとして生成します。Applicationは
`C:\wse-sdk\lang\cs\WapitiStew.Wse.dll`を参照し、`wse_capi`を実行File隣へ配置するか、
`WseRuntime.SetNativeLibraryPath`または環境変数`WSE_CAPI_LIBRARY`でFileを明示します。

Install先は`build\install\<configure-preset>`です。`build/`はGit管理対象外です。

## Manual options

Presetを使用しない場合は`WSE_LIBRARY_TYPE=SHARED|STATIC`と、次のBoolean optionを指定できます。

- `WSE_BUILD_XPT`
- `WSE_BUILD_GEF`
- `WSE_BUILD_IUI`
- `WSE_BUILD_OUI`
- `WSE_BUILD_TMR`
- `WSE_BUILD_NODE_BINDING`（既定OFF）
- `WSE_BUILD_PYTHON_BINDING`（既定OFF）
- `WSE_BUILD_JAVA_BINDING`（既定OFF）
- `WSE_BUILD_DOTNET_BINDING`（既定OFF）
- `WSE_BUILD_TESTING`
- `WSE_ENABLE_CAMERA_HARDWARE_TESTS`（既定OFF。接続済みCameraへアクセスする明示Gate）
- `WSE_ENABLE_IUI_HARDWARE_TESTS`（既定OFF。接続済み物理Keyboardの対話Smoke Gate）
- `WSE_ENABLE_LIBCAMERA`（Linux x86-64 Tmrで既定ON。ARM64では固定DependencyをPi 4 Native／実機Gateで受理するまで既定OFF）

統制対象のComponentはこのRepositoryにありません。Extension Overlayが供給し、
`WSE_EXTENSION_ROOT`がその位置を指します。

```
cmake --preset windows-msvc-shared-all -DWSE_EXTENSION_ROOT=<Overlayのpath>
```

該当ComponentをONにするPresetはいずれも環境変数`WSE_EXTENSION_ROOT`も読むため、
一度exportしておけば毎回渡す必要はありません。

```
set WSE_EXTENSION_ROOT=<Overlayのpath>
cmake --preset windows-msvc-shared-all
```

Overlayは自分が持つComponentの実装、Test、install規則を供給し、選択されていないComponentへは
何も足しません。このRepositoryだけでは組めないComponentをOverlayなしで選ぶと、Configure時に
拒否します。Componentが在ると答えるのに何も出来ないLibraryを作らないためです。

Windowsでは既存互換のためOptional componentは既定ONです。Linuxでは既定OFFです。
Standalone BuildではCharacterization testが既定ONです。
Linuxで`WSE_ENABLE_CAMERA_HARDWARE_TESTS=ON`を指定すると、公開`WebCamera`だけで1 FrameをMemory上へ取得する
`wse.tmr.webcamera_hardware_smoke`を追加します。Device identityを出力せず、Camera control値を変更しません。

## Linux Core／XPT／GEF／IUI／OUI／Tmr Preset

Linux上のWSE source rootから実行します。GCC Shared Coreの最短手順は次のとおりです。

```sh
cmake --preset linux-gcc-shared-core
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core
```

ほかに`linux-gcc-static-core`、`linux-clang-shared-core`、`linux-clang-static-core`があり、
対応するBuild Preset名には`-debug`を付けます。XPT TCP／UDP／HTTP／SerialPort／RetryPolicyを含める場合は`core`を`xpt`へ置き換えた
`linux-gcc-shared-xpt`、`linux-gcc-static-xpt`、`linux-clang-shared-xpt`または
`linux-clang-static-xpt`を使用します。GEF Binary／CSVは`linux-gcc-shared-gef`、
`linux-gcc-static-gef`、`linux-clang-shared-gef`または`linux-clang-static-gef`を使用し、第三者Dependencyを
追加しません。読取専用evdev Keyboardには`linux-gcc-shared-iui`、`linux-gcc-static-iui`、
`linux-clang-shared-iui`または`linux-clang-static-iui`を使用します。IUIは第三者Dependencyや
`/dev/input` Permission変更を追加しません。Userは事前に読取権限を持ち、Applicationは状態を解釈する前に
`Keyboard::isAvailable()`を確認します。OUIは`linux-gcc-shared-oui`または
`linux-gcc-static-oui`を使用します。Tmrは`linux-gcc-shared-tmr`、
`linux-gcc-static-tmr`、`linux-clang-shared-tmr`または`linux-clang-static-tmr`を使用します。
LinuxではOptional componentが既定OFFです。未対応の
Optional componentを有効にするとConfigureで明示的に失敗します。
Linux XPTはTCP／UDP／Portable `SerialPort`／`RetryPolicy`を含みます。

Linux Node-API Addonは次のようにBuild／Installします。

```sh
python3 bootstrap.py --package node-api-headers
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_NODE_BINDING=ON -DWSE_NODE_EXECUTABLE=/opt/node/bin/node
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
/opt/node/bin/node -e "const wse=require('/opt/wse-sdk/lang/js'); console.log(wse.runtimeInfo())"
```

Install済みShared AddonはPackage内のWSEを相対Runpathで解決するため、上の配置では
`LD_LIBRARY_PATH`を必要としません。`WSE_NODE_ADDON`は別の開発／Test Addonを明示的に読む場合だけ使用します。

Linux Python Moduleは次のようにBuild／Installします。

```sh
sudo apt install python3-dev
python3 bootstrap.py --package pybind11
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_PYTHON_BINDING=ON -DWSE_PYTHON_EXECUTABLE=/usr/bin/python3
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
PYTHONPATH=/opt/wse-sdk/lang/python python3 -c "import wse; print(wse.runtime_info())"
```

Install済みModuleは`$ORIGIN/../../../lib`でShared WSE Runtimeを解決します。

Linux Java JNI ModuleとJARは次のようにBuild／Installします。

```sh
cmake --preset linux-gcc-shared-core \
  -DWSE_BUILD_JAVA_BINDING=ON -DWSE_JAVA_HOME=/opt/jdk
cmake --build --preset linux-gcc-shared-core-debug
ctest --test-dir build/linux-gcc-shared-core --output-on-failure
cmake --install build/linux-gcc-shared-core --prefix /opt/wse-sdk
java -Djava.library.path=/opt/wse-sdk/bin \
  -cp /opt/wse-sdk/lang/java/wse.jar com.example.Main
```

Ubuntu 24.04でOUIをBuildする場合はVulkan Loader／Header、Wayland client／protocol、libdrm、
Shader compilerおよびpkg-configが必要です。

```sh
sudo apt install libvulkan-dev mesa-vulkan-drivers vulkan-tools \
  libwayland-dev wayland-protocols libdrm-dev glslang-tools pkg-config
cmake --preset linux-gcc-shared-oui
cmake --build --preset linux-gcc-shared-oui-debug
ctest --test-dir build/linux-gcc-shared-oui --output-on-failure
```

Wayland Testは利用可能なCompositor上でWindow／Fullscreen／Resize／Presentを実行します。DRM／KMSの
直接表示・復元TestはDisplay modeを変更するため既定無効であり、専用Consoleと復元手段を確保した場合だけ
`WSE_ENABLE_DISPLAY_MODE_TESTS=ON`で有効化します。

64-bit Raspberry Pi OS上ではGCCまたはClangのCore／GEF／XPT PresetをNative実行します。CMakeのProcessorが
`aarch64`または`arm64`の場合、成果物は`linux-arm64`配下へ出力されます。Pi 4／5実機Gateを通すまでは
認証済みと記録してはいけません。

Configure前に`sh tools/pi/inspect_pi4_environment.sh`を実行し、Privacy filter済みInventoryを実機Evidenceと
ともに保存します。ScriptはHost名、Address、Account名およびDevice identifierを出力せず、Display／Cameraも
Openしません。出力と受入境界は[Pi 4 Tool Guide](../../tools/pi/README.ja.md)を参照してください。

GCC AArch64 Cross buildでは、固定ARM64 OpenSSLとlibcurlを先に準備します。

```sh
python3 bootstrap.py --package openssl --target-architecture arm64 \
  --cache-dir /media/wse-cache --vendor-root vendor
python3 bootstrap.py --package libcurl --target-architecture arm64 \
  --cache-dir /media/wse-cache --vendor-root vendor
cmake --preset linux-arm64-gcc-shared-xpt
cmake --build --preset linux-arm64-gcc-shared-xpt-debug
cmake --install build/linux-arm64-gcc-shared-xpt
```

Static版は`linux-arm64-gcc-static-xpt`を使用します。x86-64 HostではCross buildしたTestを実行できません。
`linux-arm64/Debug`成果物と`tools/pi/run_pi4_smoke.sh`を64-bit Pi 4へ搬送し、
`sh run_pi4_smoke.sh <成果物Directory>`で実機Gateを実施します。

Offline ARM64環境へ搬送する前に、Remote fallbackなしでDependency cache全体を検証します。

```sh
python3 bootstrap.py --offline --verify-cache --target-architecture arm64 \
  --package openssl --package libcurl --package nlohmann-json \
  --cache-dir /media/wse-cache
```

ArchiveまたはSignature不足は失敗であり、Strict Offline modeは取得を行いません。Access-controlled Extensionの
Package／ProfileはPublic Offline KitおよびPublic Engine exportへ含めません。

GEFは外部Dependencyを必要としません。`linux-arm64-gcc-shared-gef`または
`linux-arm64-gcc-static-gef`でBinary／CSV境界をCross buildし、Pi Native CTestを受入Gateとします。

IUIは外部Dependencyを必要としません。`linux-arm64-gcc-shared-iui`または
`linux-arm64-gcc-static-iui`でevdev境界をCross buildします。Native実機Evidenceは
`WSE_ENABLE_IUI_HARDWARE_TESTS=ON`でConfigureし、物理Keyboard接続中に
`wse.iui.keyboard_hardware_smoke`だけを実行して、Prompt後にQを押して離します。Lifecycle Testや
CEC Remote sourceを物理Keyboard Evidenceとして扱いません。

Core ARM64 Presetへ`-DWSE_BUILD_NODE_BINDING=ON -DWSE_BUILD_TESTING=OFF`を追加するとAArch64
`wse.node`をCross buildできます。これはNode Runtimeを実行せず、Pi 4／5でのAddon Load／Unload成功を
認証しません。Native Runtime検証は実施中のPi 4実機Gateの受入項目です。

ARM64 Python Cross buildでは対象CPython 3.11以降のDevelopment Headerを別途用意し、
`WSE_PYTHON_TARGET_INCLUDE_DIR`と`WSE_PYTHON_EXTENSION_SUFFIX`を対象ABIに合わせます。
`WSE_BUILD_PYTHON_BINDING=ON -DWSE_BUILD_TESTING=OFF`で生成したAArch64 ELFはBuild境界だけを検証します。
Import、Lifecycle、GILおよびCancellationはPi 4 Native Gateの受入項目です。

ARM64 Java Cross buildではHost JDKの`javac`／`jar`と対象Linux JNI Headerを用意し、
`WSE_BUILD_JAVA_BINDING=ON -DWSE_BUILD_TESTING=OFF`、`WSE_JAVA_HOME`および
`WSE_JNI_TARGET_INCLUDE_DIRS`を指定します。生成したAArch64 `wse_jni`とJava 17互換`wse.jar`はBuild境界だけを
検証し、PiでのLoad、Callback attach／detachおよびLifecycleはPi 4 Native Gateの受入項目です。

ARM64 OUIはHost toolの`wayland-scanner`／`glslangValidator`とARM64側のVulkan／Wayland／DRM development
packageを用意し、`linux-arm64-gcc-shared-oui`または`linux-arm64-gcc-static-oui`を使用します。
Cross build成功はPi 4／5での描画成功を意味しません。

ARM64 Tmrは`linux-arm64-gcc-shared-tmr`または`linux-arm64-gcc-static-tmr`を使用します。
Cross buildはV4L2 Header／ABI境界を検証しますが、Camera Frame、Control、抜去／再接続およびPerformanceの
実機認証ではありません。固定ARM64 DependencyをPi 4 Native／実機Gateで受理するまでは
`WSE_ENABLE_LIBCAMERA=OFF`がARM64の既定です。libcameraを有効化する場合は、対象ARM64向けに固定・構築した
Dependencyを`WSE_LIBCAMERA_ROOT`で明示し、Native／実機Gateを別途通します。
Pi CameraのGateは`HARDWARE_NOT_RUN`のままで、Pi 5およびPi CSI CameraのTestは未実施です。
受理済みのPi 4 Core／XPT、4言語BindingのRuntime SmokeおよびOUI Offscreen結果は別の範囲です。
正確な範囲は[Hardware Validation設計](../design/ja/HardwareValidation.md)を参照してください。

## Offline Kit受入

`tools/offline/create_offline_kit.py`はInstall済みWindows Core／XPT Packageから不変Candidateを作成します。
`tools/offline/verify_offline_kit.py`は全Network adapterを無効化したWindowsで、同梱ConsumerのConfigure／Build／
CTestまで成功した場合だけAccepted manifestを生成します。詳細は`tools/offline/README.md`を参照してください。
Accepted manifestのないCandidateはRelease artifactではありません。

## Consume installed package

```cmake
find_package(WonderStewEngine CONFIG REQUIRED
    PATHS "C:/path/to/wse/cmake"
    NO_DEFAULT_PATH
)

target_link_libraries(my_application PRIVATE WSE::Core WSE::Oui)
```

Componentを無効化したPackageには、そのTarget、Headerおよび固有Dependencyは入りません。
Packageが公開するのは`WSE::*` Component Targetだけです。Consumer向け互換Alias `WonderStewEngine`は
Legacy削除Programにより2026-09-13に削除しました。旧名が必要なProjectは自身のLocal Aliasを
`WSE::Core`上に定義してください。
## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
