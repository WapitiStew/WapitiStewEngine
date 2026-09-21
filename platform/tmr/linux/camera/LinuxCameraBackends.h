//*****************************************************************************************************************
//! @file    LinuxCameraBackends.h
//! @brief   \~japanese Linux Camera backend選択が呼ぶ内部宣言. V4L2とlibcameraを同じ形で並べる.
//! @brief   \~english  Internal declarations the Linux camera-backend selection calls, presenting V4L2 and
//!                     libcamera in the same shape.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_PLATFORM_TMR_LINUX_CAMERA_LINUXCAMERABACKENDS_H
#define WONDERSTEWENGINE_PLATFORM_TMR_LINUX_CAMERA_LINUXCAMERABACKENDS_H

#include "../../../../core/tmr/camera/CameraBackend.h"

namespace wse
{
namespace tmr
{
namespace detail
{

std::unique_ptr< ICameraBackend > createV4L2CameraBackend();
CameraResult< std::vector< sCameraDeviceInfo > > enumerateV4L2CameraDevices();
CameraResult< sCameraCapability > queryV4L2CameraCapabilities(
    const sCameraDeviceInfo& device_in );

bool libcameraCameraBackendAvailable() noexcept;
std::unique_ptr< ICameraBackend > createLibcameraCameraBackend();
CameraResult< std::vector< sCameraDeviceInfo > > enumerateLibcameraCameraDevices();
CameraResult< sCameraCapability > queryLibcameraCameraCapabilities(
    const sCameraDeviceInfo& device_in );

} // namespace detail
} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_PLATFORM_TMR_LINUX_CAMERA_LINUXCAMERABACKENDS_H
