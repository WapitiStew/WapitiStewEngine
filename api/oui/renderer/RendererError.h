//*****************************************************************************************************************
//!
//! @file    RendererError.h
//! @brief   \~japanese OUI RendererのPortable ErrorとResultを定義する.
//! @brief   \~english  Defines portable OUI renderer errors and results.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#pragma once
#ifndef WONDERSTEWENGINE_OUI_RENDERER_RENDERERERROR_H
#define WONDERSTEWENGINE_OUI_RENDERER_RENDERERERROR_H

#include "../../dynamic.h"
#include "../../wse/utility/wse_Result.h"

#include <cstdint>
#include <string>
#include <utility>

namespace wse
{
namespace oui
{

//! \~japanese Renderer失敗のPortable分類. \~english Portable renderer failure categories.
enum class eRendererErrorCategory : std::uint8_t
{
      None        = 0U //!< Errorなし.
    , Validation       //!< Public contractの入力違反.
    , Lifecycle        //!< 初期化状態またはResource lifetime違反.
    , Resource         //!< GPU／Surface Resource操作失敗.
    , Execution        //!< Command実行または同期失敗.
    , Timeout          //!< 明示Timeout超過.
    , Unsupported      //!< Platform／Backend／Operation未対応.
    , Backend          //!< Native Backend失敗.
};

//! \~japanese Renderer失敗の安定Code. \~english Stable renderer failure codes.
enum class eRendererErrorCode : std::uint16_t
{
      None                 = 0U //!< Errorなし.
    , InvalidArgument           //!< 不正なPublic API引数.
    , InvalidDescription        //!< Descriptor内の値または組合せが不正.
    , NotInitialized            //!< 初期化前のOperation.
    , AlreadyInitialized        //!< 異なる設定による再初期化.
    , UnsupportedBackend        //!< 要求Backendが未対応.
    , UnsupportedFormat         //!< Pixel formatが未対応.
    , UnsupportedSurface        //!< Surface種類が未対応.
    , UnsupportedOperation      //!< Operationが未対応.
    , ResourceNotFound          //!< Handleに対応するResourceが存在しない.
    , ResourceInUse             //!< Resourceが別Resourceから参照中.
    , ResourceExhausted         //!< GPU／Memory／Handle等のResource不足.
    , BackendFailure            //!< Native Backend API失敗.
    , DeviceLost                //!< GPU Deviceが失われた.
    , TimedOut                  //!< Fence待機がTimeoutした.
};

//! \~japanese Portable Error識別子と診断用Native code.
//! \~english  Portable error identity with a diagnostic native code.
class WSE_API RendererError final
{
  private:
    eRendererErrorCategory m_category;
    eRendererErrorCode     m_code;
    std::string            m_message;
    std::int64_t           m_native_code;

  public:
    //! \~japanese Errorなしの値を生成する. \~english Creates a no-error value.
    RendererError() noexcept;

    //!
    //! @brief Renderer Errorを生成する.
    //! @param [in] category_in    Portable Error分類.
    //! @param [in] code_in        Portable Error Code.
    //! @param [in] message_in     診断Message.
    //! @param [in] native_code_in 診断用OS／Library code. Portableな分岐へ使用しない.
    //!
    RendererError(
          const eRendererErrorCategory category_in
        , const eRendererErrorCode     code_in
        , const std::string&           message_in
        , const std::int64_t           native_code_in = 0
    );

    //! @return \~japanese Errorなしの場合true. \~english True when no error is present.
    bool ok() const noexcept;

    //! @return \~japanese Portable Error分類. \~english Portable error category.
    eRendererErrorCategory category() const noexcept;

    //! @return \~japanese Portable Error Code. \~english Portable error code.
    eRendererErrorCode code() const noexcept;

    //! @return \~japanese 診断Message. \~english Diagnostic message.
    const std::string& message() const noexcept;

    //! @return \~japanese 診断用Native code. \~english Diagnostic native code.
    std::int64_t nativeCode() const noexcept;

};

//! \~japanese 正準Result契約 (doc/design/ja/ResultContract.md) をRenderer Errorで使う別名.
//! \~english  Alias applying the canonical result contract (doc/design/en/ResultContract.md)
//!            with the renderer error type.
template <typename T>
using RendererResult = wse::Result< T, RendererError >;

//! \~japanese 値なしOperationの成否. 無意味なbool Payloadは持たない.
//! \~english  Outcome of a value-less operation, without the meaningless bool payload.
using RendererStatus = wse::Status< RendererError >;

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_RENDERERERROR_H
