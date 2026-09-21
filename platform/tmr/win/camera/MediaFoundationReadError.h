// Internal HRESULT classification: a failed sample is not proof of unplugging.
#pragma once
#include <tmr/camera/CameraError.h>
#include <windows.h>
#include <mferror.h>

namespace wse::tmr::detail
{
inline CameraError mediaFoundationReadError( const HRESULT result_in )
{
    const bool disconnected = result_in == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED
        || result_in == HRESULT_FROM_WIN32( ERROR_DEVICE_NOT_CONNECTED );
    return CameraError( disconnected ? eCameraErrorCategory::Device : eCameraErrorCategory::Backend,
        disconnected ? eCameraErrorCode::DeviceDisconnected : eCameraErrorCode::ReadFailed,
        "Media Foundation frame acquisition ended; close and reopen the camera.", result_in );
}
} // namespace wse::tmr::detail
