//*****************************************************************************************************************
//!
//! @file    Camera.h
//! @brief   \~japanese Portable Tmr Camera Sessionを定義する.
//! @brief   \~english  Defines the portable Tmr camera session.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_TMR_CAMERA_CAMERA_H
#define WONDERSTEWENGINE_TMR_CAMERA_CAMERA_H

#include "CameraError.h"
#include "CameraTypes.h"
#include "../../dynamic.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace wse
{
namespace tmr
{

namespace detail { struct CameraSessionTestAccess; }

//! \~japanese CallbackはCameraSession所有Worker thread上で逐次呼ばれる.
//! \~english  Callbacks are serialized on a worker thread owned by CameraSession.
using CameraFrameCallback = std::function< void( const CameraResult< sCameraFrame >& ) >;

//! \~japanese Move不可・Copy不可のCamera Session owner. \~english Non-movable, non-copyable camera-session owner.
class WSE_API CameraSession final
{
  private:
    class Impl;
    std::unique_ptr< Impl > m_impl;

    friend struct detail::CameraSessionTestAccess;

  public:
    CameraSession();
    ~CameraSession();
    CameraSession( const CameraSession& ) = delete;
    CameraSession& operator=( const CameraSession& ) = delete;
    CameraSession( CameraSession&& ) = delete;
    CameraSession& operator=( CameraSession&& ) = delete;

    static CameraResult< std::vector< sCameraDeviceInfo > > enumerate(
        eCameraBackend backend_in = eCameraBackend::Automatic );

    static CameraResult< sCameraCapability > capabilities(
        const sCameraDeviceInfo& device_in );

    CameraStatus open( const sCameraOpenDescription& description_in );
    CameraStatus open(
          const sCameraDeviceInfo&          device_in
        , const sCameraStreamConfiguration& configuration_in );
    void close() noexcept;
    CameraStatus start();
    CameraStatus start( const CameraFrameCallback& callback_in );
    CameraStatus stop();

    //! \~japanese timeoutは1 ms以上のFrame待機予算。Copy・Native後始末・停止の実時間上限ではない.
    //! \~english  Timeout is a frame-wait budget of at least 1 ms, not a wall-time bound for copy, native cleanup or stop.
    CameraResult< sCameraFrame > readFrame( std::uint32_t timeout_ms_in );
    CameraResult< sCameraControlValue > getControl( eCameraControl control_in );
    CameraStatus setControl( const sCameraControlValue& value_in );
    CameraResult< sCameraExtensionUnitValue > getExtensionUnit(
        const sCameraExtensionUnitSelector& selector_in );
    CameraStatus setExtensionUnit( const sCameraExtensionUnitValue& value_in );

    bool isOpen() const noexcept;
    bool isStreaming() const noexcept;
};

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_CAMERA_CAMERA_H
