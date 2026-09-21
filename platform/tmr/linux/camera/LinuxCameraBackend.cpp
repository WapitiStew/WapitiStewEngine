//*****************************************************************************************************************
//! @file    LinuxCameraBackend.cpp
//! @brief   \~japanese Tmr利用者へNative APIを見せずにLinux Camera backendを選ぶ.
//! @brief   \~english  Selects a Linux camera backend without exposing native APIs to Tmr users.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include "LinuxCameraBackends.h"

namespace wse
{
namespace tmr
{
namespace detail
{
namespace
{

CameraError unsupportedLinuxBackendError( const char* const message_in )
{
    return makeCameraError(
          eCameraErrorCategory::Unsupported
        , eCameraErrorCode::UnsupportedBackend
        , message_in );
}

} // namespace

std::unique_ptr< ICameraBackend > createPlatformCameraBackend(
    const eCameraBackend backend_in )
{
    if( backend_in == eCameraBackend::Video4Linux2 )
        return createV4L2CameraBackend();
    if( backend_in == eCameraBackend::Libcamera )
        return createLibcameraCameraBackend();
    return nullptr;
}

CameraResult< std::vector< sCameraDeviceInfo > > enumeratePlatformCameras(
    const eCameraBackend backend_in )
{
    // TmrCamera設計の定めるAutomaticの規則: 実Adapterとしてのlibcameraが実際にDeviceを列挙できた
    // ときはそれを選び、そうでなければV4L2へ続く。空成功をDeviceの存在と読み違えないよう、
    // 「成功かつ一件以上」を条件にしている.
    if( backend_in == eCameraBackend::Automatic )
    {
        if( libcameraCameraBackendAvailable() )
        {
            CameraResult< std::vector< sCameraDeviceInfo > > libcamera =
                enumerateLibcameraCameraDevices();
            if( libcamera.succeeded() && !libcamera.value().empty() )
                return libcamera;
        }
        return enumerateV4L2CameraDevices();
    }
    if( backend_in == eCameraBackend::Video4Linux2 )
        return enumerateV4L2CameraDevices();
    if( backend_in == eCameraBackend::Libcamera )
        return enumerateLibcameraCameraDevices();
    return CameraResult< std::vector< sCameraDeviceInfo > >::failure(
        unsupportedLinuxBackendError( "Requested camera backend is unavailable on Linux." ) );
}

CameraResult< sCameraCapability > queryPlatformCameraCapabilities(
    const sCameraDeviceInfo& device_in )
{
    if( device_in.backend == eCameraBackend::Video4Linux2 )
        return queryV4L2CameraCapabilities( device_in );
    if( device_in.backend == eCameraBackend::Libcamera )
        return queryLibcameraCameraCapabilities( device_in );
    return CameraResult< sCameraCapability >::failure(
        unsupportedLinuxBackendError( "Requested camera backend is unavailable on Linux." ) );
}

} // namespace detail
} // namespace tmr
} // namespace wse
