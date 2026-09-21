# Usage

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

This file explains the basic usage in C++ programming.

## Include the Header

```cpp
#include "wse/stew.h"
#include "tmr/stew.h"
```

## Function Calls

```cpp
wse::double_xy points(10, 10);
```

## Error Handling

* Functions may throw exceptions. Wrap calls in `try/catch` as needed.

```cpp
wse::registDefaultLog();
try
{
    // ...
} catch (const std::exception& e)
{
    WLog() << "Error: " << e.what();
}
```

### Example: Value Calculation Function

```cpp
wse::double_xy pt1(10, 10);
wse::double_xy pt2(50, 30);
double distance = pt1.distance(pt2);
```

### Example: Camera Device Operations

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

Select native and output formats separately. Check control `readable`/`writable` flags and modes,
and use only advertised extension-unit selectors whose payload schema is known. See the complete
[`example/cpp/tmr/webcamera.cpp`](../../../example/cpp/tmr/webcamera.cpp) and the
[WebCamera migration guide](../WebCameraMigration.md).

### Example: Keyboard

```cpp
bool isRunning = true;
wse::iui::Keyboard keyboard;
if (!keyboard.isAvailable())
{
    return;
}
while (isRunning)
{
    const int8_t key = keyboard.getASCII();

    if (key != wse::iui::ASCII_NULL)
    {
        switch (key)
        {
            case 'q':
            {
                WLog() << "Press Q key";
                isRunning = false;
            }
            break;
            case 'c':
            {
                WLog() << "Press C key";
            }
            break;
            default:
                break;
        }
    }
}
```

`isAvailable()` distinguishes an all-released physical state from an unavailable, denied, or
disconnected backend. Linux reads evdev without grabbing the device or changing permissions. Its
ASCII values are a US-layout compatibility projection, not localized text or IME input.

---

*For detailed API specifications, refer to the [API Reference](../API_REFERENCE.md).*

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
