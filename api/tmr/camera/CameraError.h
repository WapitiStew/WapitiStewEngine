//*****************************************************************************************************************
//!
//! @file    CameraError.h
//! @brief   \~japanese Tmr CameraのPortable ErrorとResultを定義する.
//! @brief   \~english  Defines portable Tmr camera errors and results.
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

#ifndef WONDERSTEWENGINE_TMR_CAMERA_CAMERAERROR_H
#define WONDERSTEWENGINE_TMR_CAMERA_CAMERAERROR_H

#include "../../dynamic.h"
#include "../../wse/utility/wse_Result.h"

#include <cstdint>
#include <string>
#include <utility>

namespace wse
{
namespace tmr
{

//! \~japanese Camera失敗のPortable分類. \~english Portable camera failure categories.
enum class eCameraErrorCategory : std::uint8_t
{
      None        = 0U
    , Validation
    , Lifecycle
    , Device
    , InputOutput
    , Timeout
    , Unsupported
    , Backend
};

//! \~japanese Camera失敗の安定Code. \~english Stable camera failure codes.
enum class eCameraErrorCode : std::uint16_t
{
      None              = 0U
    , InvalidArgument
    , NotOpen
    , AlreadyOpen
    , NotStreaming
    , AlreadyStreaming
    , ConcurrentRead
    , DeviceNotFound
    , DeviceDisconnected
    , UnsupportedBackend
    , UnsupportedFormat
    , UnsupportedControl
    , TimedOut
    , OpenFailed
    , ConfigurationFailed
    , ReadFailed
    , ResourceExhausted
    , BackendFailure
    , ControlReadFailed
    , ControlWriteFailed
    , UnsupportedExtensionUnit
    , ExtensionUnitReadFailed
    , ExtensionUnitWriteFailed
    , PayloadSizeMismatch
    , AccessDenied
};

//! \~japanese Portable Error識別子と診断用Native code.
//! \~english  Portable error identity with a diagnostic native code.
class WSE_API CameraError final
{
  private:
    eCameraErrorCategory m_category;
    eCameraErrorCode     m_code;
    std::string          m_message;
    std::int64_t         m_native_code;

  public:
    CameraError() noexcept;
    CameraError(
          eCameraErrorCategory category_in
        , eCameraErrorCode     code_in
        , const std::string&   message_in
        , std::int64_t         native_code_in = 0
    );

    bool ok() const noexcept;
    eCameraErrorCategory category() const noexcept;
    eCameraErrorCode code() const noexcept;
    const std::string& message() const noexcept;
    std::int64_t nativeCode() const noexcept;
};

//! \~japanese 正準Result契約 (doc/design/ja/ResultContract.md) をCamera Errorで使う別名.
//! \~english  Alias applying the canonical result contract (doc/design/en/ResultContract.md)
//!            with the camera error type.
template <typename T>
using CameraResult = wse::Result< T, CameraError >;

//! \~japanese 値なしOperationの成否. 無意味なbool Payloadは持たない.
//! \~english  Outcome of a value-less operation, without the meaningless bool payload.
using CameraStatus = wse::Status< CameraError >;

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_CAMERA_CAMERAERROR_H
