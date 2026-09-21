# WSE Architecture

> Canonical source: [English Architecture](../en/Architecture.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象

WSEは汎用の再利用可能なC++17ライブラリです。公開API、Portable実装、OS Adapter、Optional Component、
言語Adapterを分離します。未対応操作は明示Errorにします。

## 依存方向

WSEはライブラリObjectを提供します。Main loop、Device選択、Scheduling、Componentの組合せはApplicationが
所有します。すべてのComponentを暗黙に起動するGlobal Engine Objectはありません。
以下の矢印は呼出し・利用関係であり、継承や別々の共有Libraryを表しません。

```text
C++ application                  JS / Python / Java             C#
       |                          Node-API / pybind11 / JNI      P/Invoke
       |                                    |                     |
       |                                    |                flat C ABI
       |                                    +----------+----------+
       |                                               |
       +---------------------+             public binding operations
                             |                         |
                    enabled public components <--------+
                  GEF    IUI    OUI    Tmr    XPT
                             |
                     Core domain API

component facades -> portable implementation -> selected OS/backend adapter
```

Bindingは文書化した共通Facadeを公開し、全C++型を公開するものではありません。例えばGEFや低水準OUI
Renderer Objectは直接の言語Binding対象ではありません。対応は[多言語Binding](LanguageBindings.md)に従います。

- 依存関係におけるCoreは`api/wse/`のDomain非依存APIです。Source directory名`core/`全体ではありません。
- Coreは常に存在し、Optional Componentへ依存しません。
- 公開Optional ComponentはCoreへ依存し、言語Runtimeへ依存しません。
- 言語Adapterは共通Binding Facadeと有効な公開Componentだけを利用します。
- OS／第三者型はPIMPL・Adapterの内側に保持し、公開Headerへ露出しません。
- Component間の循環依存を禁止します。Protocol／製品固有PolicyはConsumerが保持します。

## Source配置

| Path | 責務 |
| --- | --- |
| `api/wse/` | Binding宣言・平坦C ABIを含む公開Core Header |
| `api/{xpt,gef,iui,oui,tmr}/` | InstallされるComponent C++ Header |
| `core/` | Platform非依存実装とComponent Logic |
| `platform/` | Windows／Linux AdapterとHardware seam |
| `api/wse/binding/`、`core/wse/binding/` | 共通Binding値・Error・Buffer・Runtime実装 |
| `core/{xpt,iui,oui,tmr}/binding/` | Binding向けのComponent Error／Operation変換 |
| `lang/` | Node-API、pybind11、JNI、C# Adapter。`lang/cs/native/`はC ABI実装 |
| `cmake/` | Package定義と依存Target |
| `example/` | Compile／実行する利用例 |
| `test/` | Contract、境界、Golden、Package、Consumer Gate |
| `doc/design/` | 英語を正本とする正式設計 |

内部証跡・過去Reportは公開Source tree外に保持し、公開Headerや正式設計を上書きしません。

## Componentの責務と実装入口

| Component | 所有する責務 | 所有しない責務 | 実装・検証入口 |
| --- | --- | --- | --- |
| Core | Data値、数値・画像演算、Timer、Log、License値、共通Binding Runtime | Camera取得、通信Session、GPU表示 | [data](../../../api/wse/data/)、[runtime契約](../../../test/characterization/core_runtime_contract.cpp) |
| GEF | 型付きBIN Block、設定CSV表 | 機器固有の設定意味 | [BIN](../../../core/gef/bin/binController.cpp)、[CSV](../../../core/gef/csv/csvController.cpp) |
| XPT | 同期通信、明示Retry判定 | 製品Protocol、Commandの暗黙再実行 | [network](../../../platform/xpt/network/)、[HTTP](../../../platform/xpt/http/HttpClient.cpp) |
| IUI | Keyboard Snapshot、Access state、監視Worker | Text入力、IME、権限変更 | [Windows](../../../platform/iui/win/device/Keyboard.cpp)、[Linux](../../../platform/iui/linux/device/Keyboard.cpp) |
| OUI | 描画Backend、Surface、世代付き資源ID、Projection Pass | Camera取得、Calibration方針 | [Renderer](../../../core/oui/renderer/Renderer.cpp)、[backend境界](../../../core/oui/renderer/RendererBackend.h) |
| Tmr | Device Session、Capability、取得・制御、切り離されたFrame | ApplicationのFrame Queue、Projection Scheduling | [CameraSession](../../../core/tmr/camera/Camera.cpp)、[backend境界](../../../core/tmr/camera/CameraBackend.h) |

## Build時と実行時の構造

`WSE::Core`はNative Library Targetを指します。有効なComponent Sourceは1つの`WonderStewEngine`
BinaryへCompileされます。`WSE::Xpt`、`WSE::Gef`、`WSE::Iui`、`WSE::Oui`、`WSE::Tmr`はComponentと
依存関係のFacadeであり、選択が別Component DLLのLoadを意味するわけではありません。
Bindingは個別のNative AdapterをBuildします。理由、Static依存、Install Targetは
[Build／Packaging](BuildPackaging.md)を参照してください。

C++でCameraから表示までを組み合わせる場合も、Ownerは独立しています。

```text
application
  +-- WebCamera -> CameraSession -> selected camera backend -> OS capture buffers
  |                   |
  |                   +-- copies -> owned sCameraFrame
  |
  +-- Core Image <--- explicit frame conversion (owned copy)
  |
  +-- Renderer -> selected renderer backend -> GPU resources / surfaces / fences
         ^                 ^
         |                 +-- keeps submitted resources until GPU completion
    ProjectionPipeline
    (borrows Renderer; caller supplies texture/mesh identities)
```

変換・Upload・Submit・PresentのタイミングはApplicationが決めます。Camera取得が暗黙にOUIを呼ぶことは
ありません。CPU FrameはCapture buffer再利用後も有効であり、Submit済みGPU処理は呼出側が資源IDを
破棄した後も継続できます。同一Rendererの呼出しは利用側が直列化します。
各OwnerはGlobal Shutdownではなく自身のStop／Close／Shutdown契約に従います。

## 読む順序と再現の入口

1. 本書の後に[Core Data Model](CoreDataModel.md)で共通表現、
   [Core演算設計](CoreAlgorithms.md)で数値規則、
   [Core共通サービス設計](CoreServices.md)でTimer・Log・共有状態を確認します。
2. 実装対象の[GEF](GefFileFormats.md)、XPT、IUI、OUI、Tmrを読みます。
3. 境界ごとに[Thread／Ownership](ThreadOwnership.md)と[Result](ResultContract.md)を確認します。
4. Managed Runtimeを跨ぐ場合は[多言語Binding](LanguageBindings.md)へ進みます。
5. [Design Verification](DesignVerification.md)で実行可能な根拠と検証の限界を確認します。
6. [開発者向け実習](DeveloperWalkthrough.md)でOffscreen処理、Fake Cameraの復旧、入力／Memory／時間の
   計測境界を順に確認します。

記述粒度は[文書設計](DocumentationGeneration.md#ja-detailed-design-authoring)に従います。
Data配置・観測可能な契約は規範であり、内部Source対応表はAPI変更なしで変更しうる現在の実装構造です。

## 共通契約

- [公開API Policy](PublicApiPolicy.md): 命名、Error、Log、Header、第三者型境界。
- [Thread／Ownership](ThreadOwnership.md): RAII、Callback、Cancellation、Shutdown。
- [Build／Packaging](BuildPackaging.md): Component選択、Install、Offline依存。
- [Version／Compatibility](VersionCompatibility.md): WSE 1.0.0互換契約。
- [Security／Privacy](SecurityPrivacy.md): Data、依存、公開範囲。
- [Hardware Validation](HardwareValidation.md): Build、Contract、Smoke、Certificationの区別。

Component契約は[Portable Core](PortableCore.md)、[GEF](GefFileFormats.md)、[XPT](XptTransport.md)、
[IUI](IuiKeyboard.md)、[OUI Renderer](OuiRenderer.md)、[OUI Projection](OuiProjection.md)、
[Tmr Camera](TmrCamera.md)、[多言語Binding](LanguageBindings.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
