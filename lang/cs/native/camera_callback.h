#ifndef WSE_CAPI_INTERNAL_CAMERA_CALLBACK_H
#define WSE_CAPI_INTERNAL_CAMERA_CALLBACK_H
#include "capi_internal.h"
#include <wse/capi/wse_capi_tmr.h>
#include <memory>

namespace wse::capi
{
// Consumed by CameraSession's callback-exception boundary on the camera worker.
// It never escapes an exported C function or calls through a foreign runtime.
struct CameraCallbackDeliveryStopped {};

template<class Handle, class Frame, class Callback>
void deliverCameraFrame( const Frame& frame_in, const Callback& callback_in,
    const wse_capi_user_data user_data_in )
{
    std::unique_ptr<Handle> frame;
    const auto status = guard([&]()
    {
        frame = std::make_unique<Handle>(frame_in);
        return makeSuccess();
    });
    // Receiver owns a successful frame before entry. Keep the receiver outside
    // the allocation guard so its exception cannot cause a second notification.
    callback_in(frame.release(), status, user_data_in);
    if(status.category != WSE_CAPI_ERROR_NONE) throw CameraCallbackDeliveryStopped{};
}
}
#endif
