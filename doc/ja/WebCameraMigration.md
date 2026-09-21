# WebCamera移行Guide

> Canonical source: [English WebCamera Migration Guide](../en/WebCameraMigration.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

本書は、旧C++ `WebCamera` MethodとPhase 6までのManaged `CameraSession` Bindingを、正規`WebCamera` APIへ
移行するためのGuideです。通常のUSB/UVC CameraではWindowsのMedia Foundation／DirectShow、Linuxの
V4L2／libcameraを利用側で選択しません。

## C++移行表

| 旧操作 | 正規操作 | 補足 |
| --- | --- | --- |
| `connect(index)` | `WebCamera::enumerate()`でDeviceを選択 | 永続IDとTransport／USB情報を保持できる |
| `setStreamSize()`／`setStreamFPS()` | `capabilities().stream_profiles`からNative profileを選択 | Resolution、FPS、Native pixel formatを一体で選ぶ |
| `open()` | `open(device, configuration)` | Native形式とOutput形式を分離する |
| `play()`／`pause()` | `start()`／`stop()` | `stop()`後は同じOpen sessionを再Startできる |
| `getStreamFrame()` | `readFrame(timeout_ms)`。CoreのImageが必要なら`wse::tmr::readImage(camera, timeout_ms, image)` | 返却Frame／ImageはByte列を所有する |
| `getStreamFrame(average_count)` | `wse::tmr::readAveragedFrame(camera, count, timeout_ms)`。CoreのImageなら`readAveragedImage()` | 指定枚数を読んで平均する。Streaming Callbackからは`CameraFrameAccumulator`で同じことができる |
| `setStreamCollback()` | `start(CameraFrameCallback)` | Raw object pointerを新規Codeで使用しない |
| `setExposure()`／`setGain()` | `controlCapability()`後に`setControl()`、または`writeExposure()`／`writeGain()`／`writeFocus()` | 対応、Mode、Range、Step、Unitを検査する |
| `getExposureTime()`／`getGain()` | `getControl()`、または`readExposure()`／`readGain()`／`readFocus()` | Convenience Accessorは動詞とControl名で構成する |
| `setStreamDOR()`／`getStreamDOR()` | `wse::tmr::applyOrientation(frame, orientation)` | 旧来の180度と反転に加え、90度回転を追加した |
| RGB `img3c08_t` Frame | `wse::tmr::toImage(frame, image)`または`readImage()` | `Bgr8`はBGRのままのImage、`Bgra8`は4 Channelのままとなり、いずれもRGBへ潰れない |
| OpenCV変換 | `readFrame()`に対するConsumer adapter、またはCoreのImageを`toImage()`で取得 | Device controlから分離する |
| `showSettingsWindow()` | 直接のPortable代替なし | Capability APIで必要な設定UIを利用側が構成する |
| `disconnect()` | `close()` | 冪等な終端操作。再Openには新しいOwnerを作る |

WSE 1.0.0で旧Methodを削除しました。Compiler警告はもう存在しないため、1.0.0より前のReleaseに対して書かれた
Codeは警告ではなく上表を頼りに移行します。1.0.0が削除した他の面は
[非推奨API・移行Guide](DeprecationMigration.md)を参照してください。

## Frame演算

向き補正と平均合成は通常の画像演算であるため、Device Methodではなく自由関数として提供します。
`WebCamera`はDeviceと会話する責務だけを持ち続けます。

演算そのものはCoreが持ち、Cameraを一切知りません。

```cpp
const wse::img3c08_t corrected =
    wse::applyOrientation( image, wse::eImageOrientation::Rotate90CW );
const wse::img3c08_t averaged = wse::averageImages< wse::ePixFormat::CH3D8 >( images );
```

TmrはCamera Frameをその演算へ写し、元のPixel Formatを復元します。

```cpp
const auto corrected = wse::tmr::applyOrientation(
    frame, wse::eImageOrientation::Rotate180 );
const auto averaged = wse::tmr::readAveragedFrame( camera, 8U, 1000U );
```

対応Formatは`Gray8`、`Rgb8`、`Bgr8`、`Bgra8`です。いずれの演算もChannelの意味を解釈しないため、RGBと
BGRを個別に扱う必要はありません。`Yuyv422`、`Uyvy422`、`Nv12`はSubsampling、`Mjpeg`はCompressionを伴い、画素単位の
移動や加算の意味が定義できないため、暗黙に誤処理せず`UnsupportedFormat`で拒否します。

`CameraFrameAccumulator`が保持するのは累積Buffer 1つだけで枚数に依存しません。したがって64枚平均でも
画像1枚分の4倍のメモリで済み、`readFrame()`だけでなくStreaming Callbackからも加算できます。

## Managed言語

Python、JavaScriptおよびJavaの旧`CameraSession`と`CameraOpenDescription`は7-A5で削除済みです。
置換先は各言語で一つです。

| 言語 | 生成／所有 | Frame読出し | 終了 |
| --- | --- | --- | --- |
| JavaScript | `new wse.WebCamera()` | `await camera.readFrame(timeout)` | `camera.close()` |
| Python | `wse.WebCamera()` | `camera.read_frame(timeout)` | `with`または`camera.close()` |
| Java | `new WebCamera()` | `camera.readFrame(timeout)` | try-with-resourcesまたは`close()` |

公開型はDevice、USB identity、Native／Output profile、Control capability/value、Extension Unit selector/value、
所有Frameです。Native pointer、COM object、ioctl構造体およびBackend handleは移行先へ持ち込みません。

## FormatとControl

`Uyvy422`（値15）は変換無効時にU0 Y0 V0 Y1順のNative byteを保持します。
`Yuyv422`とは異なるため、読み替えではなく列挙されたNative profileを選択します。


- `native_format`はCameraが広告したCapture形式です。
- `output_format`はWSEから受け取る形式です。変換が不要なら同じ形式を選びます。
- `allow_conversion=false`では対応する変換が必要な要求を成功扱いしません。
- Controlは列挙されたCapabilityだけを使用し、書込み前に`writable`、Mode、Range、StepおよびUnitを検査します。
- 実機試験でControlを書き換えた場合は、元の値とModeを必ず復元します。
- Extension Unitは既知の機器Schema、広告済みSelector、Access modeおよびPayload長が一致する場合だけ使用します。
  不明なSelectorへ探索的に書き込みません。

Windowsでは広告された奇数高さのUYVY／YUYV Profileも、`allow_conversion=true`で`Bgra8`を出力できます。
内部の取得経路はWSEが選択します。Native Profileは変更せず、OutputにBGRAを指定します。
変換はAlphaを255とし、行順序とFrame Metadataを保持します。色の校正は行いません。
要求Frame rateはReadbackで一致する必要があり（100 nsの1 tickの丸めを許可）、未対応の近似値への置換はOpenで失敗します。
Media Foundationの終端Read Error後はClose／Reopenしてから再開します。一般的な`ReadFailed`だけで抜去とは判断しません。
詳細と契約Testは[Tmr Camera](../design/ja/TmrCamera.md#ja-packed-uyvy-frames)を参照してください。

## CameraSessionの位置付け

C++の`CameraSession`はCSI、Virtual Camera、明示Backend選択および`WebCamera`内部実装に使う低水準Portable基盤
として残します。通常のUSB/UVC Camera、JavaScript、PythonおよびJavaでは`WebCamera`を選択します。

## Rollback

旧実装削除前のSDK SourceはEngine Tag
`wse-webcamera-legacy-baseline-20260829`で保全されています。RollbackはTagから専用Branchを作り、旧実装を
復元するCommitとして行います。現作業BranchをHard resetしません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
