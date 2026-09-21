// @file WebCameraInternal.h
// @brief \~japanese WebCamera facadeとそのSoftware契約Testが使う注入可能な内部境界.
//        \~english  Internal injectable boundary used by the WebCamera facade and its software contract.

#ifndef WONDERSTEWENGINE_CORE_TMR_DEVICE_WEBCAMERAINTERNAL_H
#define WONDERSTEWENGINE_CORE_TMR_DEVICE_WEBCAMERAINTERNAL_H

#include <tmr/device/WebCamera.h>

#include <memory>

namespace wse
{
namespace tmr
{
namespace detail
{

// Mirror of CameraSession's public surface. The facade talks only to this, so the software
// contract test can script every session behavior — including failures a real device cannot
// produce on demand — without hardware.
class IWebCameraSession
{
  public:
    virtual ~IWebCameraSession() = default;
    virtual CameraStatus open(
          const sCameraDeviceInfo&          device_in
        , const sCameraStreamConfiguration& configuration_in ) = 0;
    virtual void close() noexcept = 0;
    virtual CameraStatus start() = 0;
    virtual CameraStatus start( const CameraFrameCallback& callback_in ) = 0;
    virtual CameraStatus stop() = 0;
    virtual CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in ) = 0;
    virtual CameraResult< sCameraControlValue > getControl( eCameraControl control_in ) = 0;
    virtual CameraStatus setControl( const sCameraControlValue& value_in ) = 0;
    virtual CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in ) = 0;
    virtual CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in ) = 0;
    virtual bool isOpen() const noexcept = 0;
    virtual bool isStreaming() const noexcept = 0;
};

class IWebCameraRuntime
{
  public:
    virtual ~IWebCameraRuntime() = default;
    virtual CameraResult< std::vector< sCameraDeviceInfo > > enumerate(
        eCameraBackend backend_in ) = 0;
    virtual CameraResult< sCameraCapability > capabilities(
        const sCameraDeviceInfo& device_in ) = 0;
    virtual std::unique_ptr< IWebCameraSession > createSession() = 0;
};

// The runtime that reaches the real backend. Every camera facade in this component shares it, so
// the interface above is named after its first caller rather than after only that caller's device.
WSE_API std::shared_ptr< IWebCameraRuntime > platformCameraRuntime();

// Builds a WebCamera over an injected runtime. This is the only way to construct one that does
// not use the platform runtime, and it exists solely for the software contract test.
struct WSE_API WebCameraTestAccess final
{
    static std::unique_ptr< WebCamera > create(
        std::shared_ptr< IWebCameraRuntime > runtime_in );
};

} // namespace detail
} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_CORE_TMR_DEVICE_WEBCAMERAINTERNAL_H
