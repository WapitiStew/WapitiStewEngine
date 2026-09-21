# WSE Sample

> Canonical source: [English Samples](README.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

Sampleは`example/<言語>/<Component>/`の構成で配置します。各SampleのHeaderに別途記載が無い限り、
Sampleは実機を必要としません。物理Deviceが無い環境では、失敗ではなく読取可否StateまたはStructured
Errorとして報告します。

## 配置

| Directory | 内容 |
| --- | --- |
| `example/<言語>/core/` | Runtime情報、所有Frame Buffer、待機、Cancellation |
| `example/<言語>/xpt/` | 明示Deadline付きのPortable Transport |
| `example/<言語>/tmr/` | Camera列挙、Profile選択、所有Frame 1枚の取得 |
| `example/<言語>/iui/` | Keyboardの読取可否と一貫したKey Snapshot |
| `example/<言語>/oui/` | Handle-freeなProjection描画 |
| `example/offline_consumer/` | Install済みPackageを利用する外部CMake Consumer |

## 対応状況

全Componentが全言語でSampleを持ち、TransportとCameraのSample集合はC++と1対1で一致します。

| Sample | C++ | C# | Java | JavaScript | Python |
| --- | --- | --- | --- | --- | --- |
| Core Quick start | 対応 | 対応 | 対応 | 対応 | 対応 |
| XPT UDP Loopback | 対応 | 対応 | 対応 | 対応 | 対応 |
| XPT TCP Client | 対応 | 対応 | 対応 | 対応 | 対応 |
| XPT Serial 送受信 | 対応 | 対応 | 対応 | 対応 | 対応 |
| XPT HTTP GET | 対応 | 対応 | 対応 | 対応 | 対応 |
| Tmr Frame 1枚 | 対応 | 対応 | 対応 | 対応 | 対応 |
| Tmr 露出変更 | 対応 | 対応 | 対応 | 対応 | 対応 |
| Tmr 解像度変更 | 対応 | 対応 | 対応 | 対応 | 対応 |
| Tmr 10 Frame Stream | 対応 | 対応 | 対応 | 対応 | 対応 |
| IUI Keyboard Snapshot | 対応 | 対応 | 対応 | 対応 | 対応 |
| OUI Projection描画 | 対応 | 対応 | 対応 | 対応 | 対応 |

C# Bindingは`api/wse/capi`の平坦C ABI経由でNative facadeへ到達し、他の3言語は各言語固有のNative層を
使います。いずれのBindingも現行APIのみを公開し、非推奨のCamera Method、Callback方式のLegacy Serial
Connector、Legacyな Projector 制御面は含めません。

Access-controlled ComponentはBuild Optionで追加のGateが掛かり、有効化したBuildにのみ存在します。

## 追加のC++ Sample

これらがC++のみである理由は、示す対象をBindingが公開していないためです。Coreのデータクラスと
Log機能はBindingに含まれず、Window付きRendererも同様です。Bindingが到達できるものは上の対応状況の
とおり全言語にSampleがあります。C++ Sampleは「1使用例＝1 File」で構成しています。C++ Sampleの出力は
すべてWSEのStream Logger（`wse::registDefaultLog()` の後に `wse::WLog() << ...`）で行い、
`std::cout`は使いません。構造化 `wse::writeLog()` 層はLogging Sampleのみが追加で示します。
データクラスの型名は `double_*` より `float64_*` の別名を優先します。実機に依存するSampleは
Device名や接続先AddressをFile冒頭のSource内定数として持ちます。

| Sample | 内容 |
| --- | --- |
| `example/cpp/core/size.cpp` | `wse::Size_` データクラス：構築、Accessor、型変換 |
| `example/cpp/core/range2.cpp` | `wse::Range2_` 矩形：境界と包含判定 |
| `example/cpp/core/matrix.cpp` | `wse::Matrix_`：行列式、転置、逆行列のRound trip |
| `example/cpp/core/image.cpp` | `wse::Image_`：Stride付きBufferの取り込み、型が運ぶChannel順序、向き補正、書き戻し |
| `example/cpp/core/logging.cpp` | Log機能：構造化Record、Level、Sink、Stream Logger、File出力 |
| `example/cpp/tmr/single_frame.cpp` | 先頭Cameraから所有Frame 1枚を取得しBMP保存 |
| `example/cpp/tmr/camera_image.cpp` | CameraのChannel順序を保ったままCoreの`wse::Image_`として取得し、明示的に変換する |
| `example/cpp/oui/windowed_projection.cpp` | 生成した30 FrameをSource→Screen→Projection→Windowで描画 |
| `example/cpp/oui/borderless_fullscreen.cpp` | Primary DisplayへのBorderless Fullscreen遷移とWindowed復帰 |

画像を扱うSampleはすべて、生Byte列ではなく`wse::Image_`として扱います。Camera SampleはImageとして
読み出し、OUI SampleはImageへ描いてUploadし、Projector SampleはImageをSubmitします。Image型が
Channel数とChannel順序を運ぶため、順序やChannel数の変換は常にSample側が明示的に書く呼び出しになります。

tmrのSampleは`example/cpp/tmr/example_camera_utility.h`を共有します。先頭に列挙されたCameraの選択、
FrameからRGB Imageへの変換、およびImageを受け取る最小限のBMP Writerを持つHeaderで、Sample専用であり
公開APIには含まれません。

## Sampleの実行

各Sampleが必要とするBuild Optionは、そのSampleのHeader Commentに記載しています。CTestが対象とする
のはQuick start Sampleです。

```text
ctest --test-dir <build-directory> [-C <configuration>] -R "wse.documentation.quickstart" --output-on-failure
```

残りのSampleは参照Codeです。C++ Sampleは対象Componentを有効化したBuildでCompileされます。他言語の
Sampleは、Buildが生成したArtifactに対して実行します。

```bat
rem C#：Sample ProjectはBuild済みBinding Assemblyを参照します。
dotnet run --project example\cs\core\QuickStart.csproj

rem Python：Build済みExtension Moduleを渡します。
python example\python\core\quickstart.py <build-directory>\x64\Release\wse.pyd

rem JavaScript：Install済み lang\js Package、または開発時はBuild済みAddonを渡します。
node example\js\core\quickstart.js <build-directory>\x64\Release\wse.node

rem Java：Binding ClassへCompileし、Engine DirectoryをPATHへ追加して実行します。
javac --release 17 -cp <build-directory>\java\classes -d <output> example\java\core\QuickStart.java
set PATH=<build-directory>\x64\Release;%PATH%
java "-Dwse.runtime.path=<build-directory>\x64\Release\WonderStewEngine.dll" ^
  -cp "<build-directory>\java\classes;<output>" QuickStart
```

JavaScript SampleはInstall済み`lang/js` DirectoryとBuild済み`wse.node`のどちらでも受け付けます。Addonを
渡した場合もPackage経由でLoadするため、Sampleは常にApplicationと同じ公開面を使用します。Java Sampleは
`WonderStewEngine`を絶対Pathで読み込みますが、Windowsではその方式でDLL自身のDirectoryが依存探索Pathへ
追加されません。したがって独自Runtime Libraryを持つComponentを有効化したBuildでは、Engine Directoryを
`PATH`へ追加する必要があります。

[Build and Install Guide](../doc/ja/BuildGuide.md)、[How-to](../doc/ja/HOWTO.md)および
[多言語Binding設計](../doc/design/ja/LanguageBindings.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
