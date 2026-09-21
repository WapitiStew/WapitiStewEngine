// Internal Windows native-stream adapter. No platform type crosses the public facade.
#pragma once
#include "../../../../core/tmr/camera/CameraBackend.h"
#include <dshow.h>
#include <memory>

namespace wse { namespace tmr { namespace detail {

// The caller owns COM initialization. The graph copies samples on its delivery thread;
// readFrame waits on owned storage, and stop wakes readers before stopping the graph.
class DirectShowRawCapture final
{
    class Impl;
    std::unique_ptr< Impl > m_impl;
  public:
    DirectShowRawCapture();
    ~DirectShowRawCapture();
    CameraStatus open( const sCameraOpenDescription& description_in );
    CameraStatus start();
    CameraStatus stop();
    CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in );
    IBaseFilter* source() const noexcept;
};

} } }
