//*****************************************************************************************************************
//!
//! @file    WebCamera.h
//! @brief   \~japanese UVC Web Cameraの高水準Portable Facadeを定義する.
//! @brief   \~english  Defines the high-level portable facade for UVC web cameras.
//! @author  WapitiStew.
//! @date    Aug-29, 2026   Modernize.
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_H
#define WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_H

#include "../camera/Camera.h"
#include "../data/Parameter.h"
#include "../../wse/data/wse_Image.h"
#include "../../wse/data/wse_Size.h"
#include "../../dynamic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wse
{
namespace tmr
{

namespace detail { struct WebCameraTestAccess; }

//! \~japanese USB Web CameraをOS API非依存で操作するFacade.
//! \~english  Operating-system-independent facade for USB web cameras.
class WSE_API WebCamera final
{
  private:
    class Impl;
    std::unique_ptr< Impl > m_impl;

    friend struct detail::WebCameraTestAccess;

  public:
    //! \~japanese Deviceを開いていないCameraを作る.
    //! \~english  Creates a camera that has not opened a device.
    WebCamera();

    //! \~japanese 列挙順のIndexで作る. Deviceの取り違えを避けるためEnumerate結果の利用を推奨する.
    //! \~english  Creates from an enumeration index. Prefer an enumerated device, which cannot slide.
    explicit WebCamera( std::int64_t index_in );

    //! \~japanese Streamを止めDeviceを閉じる.
    //! \~english  Stops the stream and closes the device.
    ~WebCamera();

    // A camera owns one device session, so it moves but does not copy.
    WebCamera( const WebCamera& ) = delete;
    WebCamera& operator=( const WebCamera& ) = delete;
    WebCamera( WebCamera&& other_inout ) noexcept;
    WebCamera& operator=( WebCamera&& other_inout ) noexcept;

    //! \~japanese Web Camera候補を列挙する. CSI、NetworkおよびVirtualは除外する.
    //! \~english Enumerates web-camera candidates, excluding CSI, network, and virtual transports.
    static CameraResult< std::vector< sCameraDeviceInfo > > enumerate(
        eCameraBackend backend_in = eCameraBackend::Automatic );

    //! \~japanese 開かずにDeviceの能力を読む. Format、Profile、制御およびExtension Unitを含む.
    //! \~english  Reads what a device supports without opening it: formats, profiles, controls,
    //!            and extension units.
    static CameraResult< sCameraCapability > capabilities(
        const sCameraDeviceInfo& device_in );

    //! \~japanese Capabilityの最初の有効ProfileでOpenする.
    //! \~english Opens the first valid profile advertised by the device.
    CameraStatus open( const sCameraDeviceInfo& device_in );

    //! \~japanese Stream構成を指定してOpenする. 解像度はSession中は固定である.
    //! \~english  Opens with a chosen stream configuration. The resolution is fixed for the
    //!            session, so changing it means closing and opening again.
    CameraStatus open(
          const sCameraDeviceInfo&          device_in
        , const sCameraStreamConfiguration& configuration_in );

    //! \~japanese Session を終える. Objectは残り、再びOpenできる. 何度呼んでも安全である.
    //! \~english  Ends the session. The object survives and opens again. Safe to call repeatedly.
    void close() noexcept;

    //! \~japanese Streamingを開始する. Frameは`readFrame()`で引き取る.
    //! \~english  Starts streaming. Frames are collected with `readFrame()`.
    CameraStatus start();

    //! \~japanese Callback付きでStreamingを開始する. Callbackは撮影Threadから呼ばれるため、
    //!            呼出元が自身のThread安全性に責任を持つ.
    //! \~english  Starts streaming with a callback. It runs on the capture thread, so the caller
    //!            owns its own thread safety.
    CameraStatus start( const CameraFrameCallback& callback_in );

    //! \~japanese Streamingを停止する. Deviceは開いたままである.
    //! \~english  Stops streaming. The device stays open.
    CameraStatus stop();

    //! \~japanese Frameを1枚読む. 期限内に届かない場合はTimeoutを報告する.
    //! \~english  Reads one frame, reporting a timeout when none arrives within the deadline.
    //! \~japanese timeoutは1 ms以上。Copy・Native後始末・停止の実時間上限ではない.
    //! \~english  Timeout must be at least 1 ms; it does not bound copy, native cleanup or stop time.
    CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in );

    //! \~japanese 開いているDeviceの能力を読む. 静的な`capabilities()`と異なりSessionを対象とする.
    //! \~english  Reads the capabilities of the open device, as opposed to the static
    //!            `capabilities()` which inspects a device it does not own.
    CameraResult< sCameraCapability > currentCapabilities() const;

    //! \~japanese 1つの制御の範囲、既定値、単位および可否を読む.
    //! \~english  Reads one control's range, default, unit, and whether it can be read or written.
    CameraResult< sCameraControlCapability > controlCapability(
        eCameraControl control_in ) const;

    //! \~japanese 現在値を読む. Driverへ問い合わせるためconstではない.
    //! \~english  Reads the current value. It talks to the driver, so it is not `const`.
    CameraResult< sCameraControlValue > getControl( eCameraControl control_in );

    //! \~japanese 値を書く. 範囲外はDriverへ渡す前にValidation Errorとなる.
    //! \~english  Writes a value. A value outside the advertised range is a validation error
    //!            before the driver ever sees it.
    CameraStatus setControl( const sCameraControlValue& value_in );

    //! \~japanese Vendor固有のExtension Unitを読む. Payloadの意味はVendorが決める.
    //! \~english  Reads a vendor extension unit. The payload's meaning belongs to the vendor.
    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in );

    //! \~japanese Vendor固有のExtension Unitへ書く.
    //! \~english  Writes a vendor extension unit.
    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in );

    // Named shortcuts over getControl/setControl for the three controls a caller reaches for most.
    // They carry no behaviour of their own beyond naming the control.

    //! \~japanese 露光を読む. \~english Reads the exposure.
    CameraResult< sCameraControlValue > readExposure();

    //! \~japanese 露光を書く. \~english Writes the exposure.
    CameraStatus writeExposure(
          std::int64_t value_in
        , eCameraControlMode mode_in = eCameraControlMode::Manual );

    //! \~japanese Gainを読む. \~english Reads the gain.
    CameraResult< sCameraControlValue > readGain();

    //! \~japanese Gainを書く. \~english Writes the gain.
    CameraStatus writeGain(
          std::int64_t value_in
        , eCameraControlMode mode_in = eCameraControlMode::Manual );

    //! \~japanese Focusを読む. \~english Reads the focus.
    CameraResult< sCameraControlValue > readFocus();

    //! \~japanese Focusを書く. \~english Writes the focus.
    CameraStatus writeFocus(
          std::int64_t value_in
        , eCameraControlMode mode_in = eCameraControlMode::Manual );

    //! \~japanese Deviceを開いているか. \~english Whether a device is open.
    bool isOpen() const noexcept;

    //! \~japanese Streaming中か. \~english Whether the stream is running.
    bool isStreaming() const noexcept;

    //! \~japanese 直近の失敗を返す. 戻り値でErrorを受け取る呼出しでは不要である.
    //! \~english  Returns the most recent failure. A caller that takes the error from the return
    //!            value does not need it.
    CameraError lastError() const;
};

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_H
