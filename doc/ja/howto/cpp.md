# 使い方

> Canonical source: [English C++ How-to](../../en/howto/cpp.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

C++プログラミングでの基本的な使い方を説明します。

## ヘッダのインクルード

```cpp
#include "wse/stew.h"
#include "tmr/stew.h"
```

## 関数の呼び出し

```cpp
 wse::double_xy points(10, 10);
```

## エラーハンドリング

* 関数は例外を投げる場合があります。必要に応じて `try/catch` で囲んでください。

```cpp
wse::registDefaultLog();
try
{
    /********/
} catch ( const std::exception& e )
{
    WLog() << "エラー: " << e.what();
}
```

### 例: 値を計算する関数

```cpp
 wse::double_xy pt1(10, 10);
 wse::double_xy pt2(50, 30);
double distance = pt1.distance( pt2 );
```

### 例: カメラデバイス操作

```cpp
using namespace wse::tmr;

const auto devices = WebCamera::enumerate();
if (!devices.succeeded() || devices.value().empty())
    return;
const auto capability = WebCamera::capabilities(devices.value().front());
if (!capability.succeeded() || capability.value().stream_profiles.empty())
    return;
const auto& profile = capability.value().stream_profiles.front();
if (profile.output_formats.empty()) return;

WebCamera camera;
const sCameraStreamConfiguration configuration{
    profile.native_format, profile.output_formats.front(), true};
if (!camera.open(devices.value().front(), configuration).succeeded())
    return;
if (!camera.start().succeeded())
    return;
const auto frame = camera.readFrame(1000U);
camera.stop();
camera.close();
```

Native形式とOutput形式を別々に選びます。ControlはCapabilityで`readable`／`writable`と対応Modeを確認し、
Extension Unitは広告されたSelectorと既知のPayload Schemaだけを使用してください。完全な例は
[`example/cpp/tmr/webcamera.cpp`](../../../example/cpp/tmr/webcamera.cpp)、旧Methodからの移行は
[WebCamera移行Guide](../WebCameraMigration.md)を参照してください。

### 例: キーボード

```cpp
bool is_runnable = true;
wse::iui::Keyboard   keyboard;
if( !keyboard.isAvailable() )
{
    return;
}
while( is_runnable )
{
    const int8_t key = keyboard.getASCII();

    if( key != wse::iui::ASCII_NULL )
    {
        switch( key )
        {
            case 'q' :
            {
                WLog() << "Press Q key";
                is_runnable = false;
            }break;
            case 'c' :
            {
                WLog() << "Press C key";
            } break;
            default  : { } break;
        }
    }
}
```

`isAvailable()`により、全Key Release状態とBackend利用不可／権限拒否／切断を区別します。LinuxはDeviceを
GrabせずPermissionも変更せずにevdevを読みます。ASCII値はUS Layout互換Projectionであり、Localize済み
Text／IME入力ではありません。


---

*詳細なAPI仕様は[API Reference](../API_REFERENCE.md)をご参照ください。*

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
