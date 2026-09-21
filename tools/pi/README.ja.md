# WSE Raspberry Pi 4 Tool

> Canonical source: [English Raspberry Pi 4 Tools](../pi/README.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

これらのToolは明示的なRaspberry Pi 4実機Gateを支援します。Cross build、MockまたはInventory結果を
実機認証へ読み替えてはいけません。

## 環境Inventory

WSEをConfigureする前にRaspberry Pi OS 64-bit上で読み取り専用Inventoryを実行します。

```sh
sh tools/pi/inspect_pi4_environment.sh
```

ScriptはARM64 OSを必須とし、安定した`key=value`形式で結果を出力します。OS／Kernel version、Build／
Runtime toolの有無、Display session種別、DRM／Camera node数、Vulkan API version、Default route／SSH listenerの
有無、Memory、Storage、取得可能な場合は温度とThrottling状態を記録します。

Host名、Network address、Account名、Hardware serial、Device pathおよびDevice identifierは意図的に出力しません。
Package導入、Network接続、Device open、Display mode変更およびCamera captureも行いません。Raw出力は承認済み
Evidence保管先だけへ保存し、公開Reportには対象の受入結果に必要なFieldだけを残します。

`platform.pi_generation=4`は期待するBoard familyを示します。`unknown`、`other`または`5`はPi 4受入結果に
なりません。`display.vulkan_api_version=unavailable`はVulkan Gate未通過を表し、HardwareがVulkan非対応で
あることを意味しません。

## Runtime Smoke

対応するARM64 Debug成果物を生成後、Artifact directoryと`run_pi4_smoke.sh`をPiへCopyして実行します。

```sh
sh run_pi4_smoke.sh <ARM64-Debug成果物Directory>
```

Smoke ToolはARM64 Architecture、必須File、動的Dependency、Core、TCP、UDP、Retry、HTTPおよびPortable
Serial Contractを検査します。成功時の状態は`HARDWARE_SMOKE_PASS`であり、Native Package、Display、
連続運転および復元Gateは別途必要です。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
