//*****************************************************************************************************************
//!
//! @file    CameraBackend.h
//! @brief   \~japanese 内部のPortable Camera backend境界.
//! @brief   \~english  Internal portable camera backend boundary.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_CORE_TMR_CAMERA_CAMERABACKEND_H
#define WONDERSTEWENGINE_CORE_TMR_CAMERA_CAMERABACKEND_H

#include <tmr/camera/CameraError.h>
#include <tmr/camera/CameraTypes.h>
#include <dynamic.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace wse
{
namespace tmr
{

class CameraSession;

namespace detail
{

CameraError makeCameraError(
      eCameraErrorCategory category_in
    , eCameraErrorCode     code_in
    , const char*          message_in
    , std::int64_t         native_code_in = 0 );

//! \~japanese OS別Camera adapterが実装する内部境界. Extension unitだけは既定実装が未対応Errorを
//!            返すので、UVCを持たないBackendは二つの純粋仮想を書かずに済む.
//! \~english  The internal boundary each per-OS camera adapter implements. Only the extension-unit
//!            pair has a default returning an unsupported error, so a backend without UVC does not
//!            have to write them.
class ICameraBackend
{
  public:
    virtual ~ICameraBackend() = default;
    virtual CameraStatus open( const sCameraOpenDescription& description_in ) = 0;
    virtual void close() noexcept = 0;
    virtual CameraStatus start() = 0;
    virtual CameraStatus stop() = 0;
    virtual CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in ) = 0;
    virtual CameraResult< sCameraControlValue > getControl( eCameraControl control_in ) = 0;
    virtual CameraStatus setControl( const sCameraControlValue& value_in ) = 0;
    virtual CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& )
    {
        return CameraResult< sCameraExtensionUnitValue >::failure( CameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedExtensionUnit
            , "Camera backend does not expose UVC extension units." ) );
    }
    virtual CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& )
    {
        return CameraStatus::failure( CameraError(
              eCameraErrorCategory::Unsupported
            , eCameraErrorCode::UnsupportedExtensionUnit
            , "Camera backend does not expose UVC extension units." ) );
    }
    virtual bool isOpen() const noexcept = 0;
    virtual bool isStreaming() const noexcept = 0;
};

//! Internal characterization seam. Production backends are still selected only by CameraSession::open().
struct WSE_API CameraSessionTestAccess
{
    using ThreadLauncher = std::thread (*)( const std::function< void() >& task_in );
    static void installBackend(
          CameraSession*                     p_session_inout
        , std::shared_ptr< ICameraBackend >  backend_in );
    static void setThreadLauncher(
          CameraSession* p_session_inout
        , ThreadLauncher launcher_in );
};

std::unique_ptr< ICameraBackend > createPlatformCameraBackend( eCameraBackend backend_in );
CameraResult< std::vector< sCameraDeviceInfo > > enumeratePlatformCameras( eCameraBackend backend_in );
CameraResult< sCameraCapability > queryPlatformCameraCapabilities( const sCameraDeviceInfo& device_in );

} // namespace detail
} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_CORE_TMR_CAMERA_CAMERABACKEND_H
