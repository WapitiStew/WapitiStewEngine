//*****************************************************************************************************************
//!
//! @file    TransportError.h
//! @brief   \~japanese 安定したXPT Transport Error分類と結果値を定義する.
//! @brief   \~english  Defines stable XPT transport error categories and result values.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_XPT_ERROR_TRANSPORTERROR_H
#define WONDERSTEWENGINE_XPT_ERROR_TRANSPORTERROR_H

#include "../../dynamic.h"
#include "../../wse/utility/wse_Result.h"

#include <cstdint>
#include <string>
#include <utility>

namespace wse
{
namespace xpt
{

//! \~japanese Transport失敗のPortable分類. \~english Portable transport failure categories.
enum class eTransportErrorCategory : std::uint8_t
{
      None         = 0  //!< Errorなし.
    , Validation        //!< Public contractの入力違反.
    , Resolution        //!< Host／Service解決失敗.
    , Connection        //!< 接続または接続状態の失敗.
    , InputOutput       //!< Transport I/O失敗.
    , Timeout           //!< 明示Deadline超過.
    , Cancellation      //!< 協調的Cancellation.
    , Protocol          //!< Transport上位Protocol違反.
    , Security          //!< TLS／認証等のSecurity失敗.
    , Http              //!< HTTP status／HTTP contract失敗.
};

//! \~japanese Transport失敗の安定Code. \~english Stable transport failure codes.
enum class eTransportErrorCode : std::uint16_t
{
      None               = 0  //!< Errorなし.
    , InvalidArgument         //!< 不正なPublic API引数.
    , HostNotFound            //!< Hostを解決できない.
    , AddressUnavailable      //!< 利用可能なAddressがない.
    , ConnectionRefused       //!< Remoteが接続を拒否した.
    , ConnectionReset         //!< 接続がReset／Abortされた.
    , NetworkUnreachable      //!< NetworkまたはHostへ到達できない.
    , NotConnected            //!< 接続を必要とするOperationが未接続.
    , RemoteClosed            //!< Remoteが正常に接続を閉じた.
    , TimedOut                //!< 明示Deadlineを超過した.
    , Cancelled               //!< Cancellationが要求された.
    , BindFailed              //!< Local EndpointへのBindに失敗した.
    , SendFailed              //!< 送信に失敗した.
    , ReceiveFailed           //!< 受信に失敗した.
    , MessageTooLarge         //!< 送信MessageがTransport上限を超えた.
    , DatagramTruncated       //!< UDP Datagramが受信Bufferを超えて切り詰められた.
    , ResourceExhausted       //!< Handle／Socket／Memory等のResource不足.
    , Unsupported             //!< PlatformまたはBackendが未対応.
    , Unknown                 //!< 上記へ分類できない失敗.
    , OpenFailed              //!< Device／Transport ResourceのOpen失敗.
    , ConfigurationFailed     //!< Device／Transport Resourceの設定失敗.
    , HttpStatusError         //!< HTTP 4xx／5xx Status.
    , ResponseTooLarge        //!< HTTP Response Bodyが呼出側上限を超えた.
    , SecurityFailed          //!< TLS検証／Security処理失敗.
};

//! \~japanese Portable Error識別子と任意の診断用OS／Library code.
//! \~english  Portable error identity plus an optional diagnostic OS/library code.
class WSE_API TransportError final
{
  private:
    eTransportErrorCategory m_category;
    eTransportErrorCode m_code;
    std::string m_message;
    std::int64_t m_native_code;

  public:
    //! \~japanese Errorなしの値を生成する. \~english Creates a no-error value.
    TransportError() noexcept;

    //! \~japanese Error値を生成する.
    //! \~english  Creates an error value.
    //! @param [in] category_in Portable Error分類.
    //! @param [in] code_in Portable Error Code.
    //! @param [in] message_in 診断Message.
    //! @param [in] native_code_in 任意のOS／Library code. Portableな分岐へ使用しない.
    TransportError(
          const eTransportErrorCategory category_in
        , const eTransportErrorCode     code_in
        , const std::string&            message_in
        , const std::int64_t            native_code_in = 0
    );

    //! @return \~japanese Errorなしの場合true. \~english True when no error is present.
    bool ok() const noexcept;

    //! @return \~japanese Portable Error分類. \~english The portable error category.
    eTransportErrorCategory category() const noexcept;

    //! @return \~japanese Portable Error Code. \~english The portable error code.
    eTransportErrorCode code() const noexcept;

    //! @return \~japanese 診断Message. \~english The diagnostic message.
    const std::string &message() const noexcept;

    //! @return \~japanese 診断用OS／Library code. \~english The diagnostic OS/library code.
    std::int64_t nativeCode() const noexcept;

};

//! \~japanese 正準Result契約 (doc/design/ja/ResultContract.md) をTransport Errorで使う別名.
//!            成功は値だけ、失敗はErrorだけを運ぶ. 部分進捗はTransferResultを使う.
//! \~english  Alias applying the canonical result contract (doc/design/en/ResultContract.md)
//!            with the transport error type. Partial progress uses TransferResult instead.
template <typename T>
using TransportResult = wse::Result< T, TransportError >;

//! \~japanese 値なしOperationの成否. 無意味なbool Payloadは持たない.
//! \~english  Outcome of a value-less operation, without the meaningless bool payload.
using TransportStatus = wse::Status< TransportError >;

//! \~japanese 部分進捗Operation専用の契約. Byteを送った後の失敗でも進捗値が意味を持つ.
//! \~english  Dedicated partial-progress contract: the progress value stays meaningful even
//!            when the operation fails after moving bytes.
template <typename T>
using TransferResult = wse::PartialResult< T, TransportError >;

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_ERROR_TRANSPORTERROR_H
