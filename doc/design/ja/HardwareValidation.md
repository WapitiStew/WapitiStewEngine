# WSE Hardware Validation設計

> Canonical source: [English Hardware Validation Design](../en/HardwareValidation.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 状態語彙

| Status | 意味 |
| --- | --- |
| `BUILD_VERIFIED` | 指定Toolchain／ArchitectureでCompile／Link済み |
| `CONTRACT_VERIFIED` | Unit、Fake、Replay、Golden Contract合格。物理Device認証ではない |
| `HARDWARE_SMOKE_PASS` | 記録したDevice／構成で限定Smoke合格 |
| `HARDWARE_CERTIFIED` | 記録Commitの承認済み全Hardware Matrix合格 |
| `HARDWARE_NOT_RUN` | 物理Test未実施 |
| `HARDWARE_ON_HOLD` | 実機受入保留。WSE成功／失敗の判定ではない |
| `UNSUPPORTED` | 意図的な非対応で、明示Errorを返す |

## 証跡

Hardware証跡はWSE Commit、日付、Operator／Environment、OS／Kernel、Architecture、Device Model／安定Identity、
Firmware／Driver、Backend、Build Option、Dependency Identity、Test一覧、結果、Log／Artifact Hashおよび制約を
記録します。結果は記録構成だけに適用し、Cross build、Mock、OS列挙をHardware状態へ昇格しません。

物理Keyboard／Camera／Display Testは既定無効で、明示Optionと承認済み機材が必要です。成功、失敗、Timeout、中断の全経路で
Display modeを復元し、Streamを停止し、HandleをCloseしてDevice状態を保持します。記録Media初期化等の破壊操作は
別のOwner承認とTarget実体確認を要求します。

## Windows実機Testの明示実行

公開Presetで承認済みのGateだけを有効にします。`WSE_ENABLE_CAMERA_HARDWARE_TESTS=ON`は
`wse.tmr.windows_camera_smoke`を登録します。複数Cameraがある場合、`WSE_CAMERA_TEST_NAME`に
承認済みの列挙名を完全一致で指定します。指定名は1台だけに一致する必要があり、能力照会・Openで
別DeviceへFallbackしません。BGRA変換可能な広告済みNative Profileから選択し、Webcamの解像度を仮定しません。
Control検証を分けて記録し、Control操作の一巡が成立しなくてもFrame取得を試みます。全体合格には両方が必要で、
Frameだけの成功はControl認証ではありません。FrameはMemory内で扱い、画像File保存は不要です。
同期取得、再開、Callback、WebCameraの各Readで要求寸法と実際のBGRA8形式を確認します。
広告された変換能力は選択の手掛かりであり、取得成功だけがそのProfileの限定的なFrame経路の根拠です。
Controlの範囲が固定の場合、現在値を読めても変更／Readback／復元の一巡は証明できません。

Control TestはManual値を実際に異なる値へ変更し、書込み前に元値・Modeを保持して、復元後も値・Modeを読戻します。
途中失敗・例外でもScope Ownerが復元を試み、復元失敗はGate失敗です。Frame Smokeはさらに短いRead待機後の
Stop／Restartを5周、Callback内Self-stop後のOwner回収、別ThreadからのCloseを検査します。
各結果は選択したNative Profileに限定し、抜去／再接続復旧や全Driverの動作保証へ拡張しません。

`WSE_ENABLE_IUI_HARDWARE_TESTS=ON`は`wse.iui.keyboard_hardware_smoke`を登録します。
Prompt後60秒以内に、操作者が物理Qキーを約1秒押して離します。PollingとCallbackの両方で押下・解放を
観測し、自動入力は実機証跡にしません。CallbackのCaptureはKeyboard Ownerより長く生存し、CTestの上限は
起動・解放観測を含め90秒です。

`WSE_ENABLE_DISPLAY_MODE_TESTS=ON`、`WSE_DISPLAY_TEST_NUMBER`、`WSE_DISPLAY_TEST_HOLD_MS`で
承認済みWindows出力と表示維持時間を指定します。`wse.oui.d3d12_display_mode_restoration`はWindowと
DirectDisplayについてProjection、Readback、Present、Event処理、破棄、復元を分けて確認します。
Current-mode／BorderlessへのFallbackを記録します。いずれも別Modeへの変更を証明せず、Borderlessでは
DirectDisplayを実施しません。実行前後の正確なNative ModeとDesktop配置を比較し、外部Timeout監視は
Process中断時に指定出力を復元します。OS／Device識別情報、失敗Log、Source差分Hashは公開成果物へ入れず、
管理された実行証跡とともに保持します。

ProjectionのReadbackではAlphaを除き、4領域のRGBを比較します。2 x 2のSourceは左上 (255,96,0)、
右上 (0,255,96)、左下 (0,96,255)、右下 (255,255,255) です。整数除算で求める
X座標 width/5 または width-width/5、Y座標 height/5 または height-height/5 の各中心周囲3 x 3、合計36画素を検査し、
RGB各ChannelでByte値2までの差を許可します。RGBA8／BGRA8とRow pitchを区別します。
この位置はEdge blend外のClampされた隅領域です。
[ProjectionReadbackCheck.h](../../../test/support/ProjectionReadbackCheck.h)が判定を実装し、
`wse.oui.frame_contract`で不透明な黒、赤青交換、上下反転、短いDataを拒否し、Padding付きRGBA／BGRAを受理します。
Alphaの変動だけで成功することはありません。ReadbackとPresent成功は実際のScan-out、光学的な色精度、人間の視認を
測定しません。それらの認証には別途観察・計測が必要です。

## 現在の境界

受理済み実機Statusは、非公開の証跡Recordに記録する正確な機器・Profile・Build・Test一覧に限定します。
Windows CameraのSmoke証跡は別機器やProfileへの対応を証明せず、OS Access失敗は実機合格ではありません。

| 対象／範囲 | 受理済みStatusと制限 |
| --- | --- |
| Raspberry Pi 4、64-bit Debian 13のCore／XPT Runtimeと公開Node-API／Python／Java-JNI／C# Binding Smoke | HW-3・HW-10が指定するCommit／構成について`HARDWARE_SMOKE_PASS` |
| 同BoardのAccess-controlled高水準Facade | `HARDWARE_NOT_RUN` |
| 同BoardのOUI Shared／Static | `BUILD_VERIFIED`。GPUによるOffscreen Vulkan Projection証跡はOffscreen描画だけに適用 |
| 同BoardのHDMI／Window表示／DRM-KMS | `HARDWARE_NOT_RUN`。Offscreen描画からScan-outやWindow表示を保証しない |
| Linux物理UVC、Generic Extension Unit、Pi Display、CSI認証 | 完了した認証なし。Pi 5／CSI機材は利用不可 |

Wayland表示ContractはCompositorへ到達できない場合にSkipします。Skipは表示合格ではありません。
Access-controlled DeviceのRecordは管理環境内だけで扱います。Software互換性GateやOwnerの免除は
実機認証の根拠にはなりません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
