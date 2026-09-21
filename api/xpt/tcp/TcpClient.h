//*****************************************************************************************************************
//!
//! @file    TcpClient.h
//! @brief   \~japanese Portableな同期TCP Client契約を定義する.
//! @brief   \~english  Defines the portable synchronous TCP client contract.
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

#ifndef WONDERSTEWENGINE_XPT_TCP_TCPCLIENT_H
#define WONDERSTEWENGINE_XPT_TCP_TCPCLIENT_H

#include "../error/TransportError.h"
#include "../network/Endpoint.h"
#include "../operation/OperationContext.h"
#include "../../dynamic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wse
{
namespace xpt
{

//! \~japanese Caller-confinedでMove-onlyのTCP接続Owner.
//! \~english  Caller-confined, move-only TCP connection owner.
class WSE_API TcpClient final
{
  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    //! \~japanese 未接続Clientを生成する. \~english Creates a disconnected client.
    TcpClient();

    //! \~japanese 接続を閉じてResourceを解放する. \~english Closes the connection and releases resources.
    ~TcpClient();
    TcpClient( const TcpClient & ) = delete;
    TcpClient &operator=( const TcpClient & ) = delete;
    //! \~japanese 接続所有権を移動する. \~english Moves connection ownership.
    TcpClient( TcpClient&& other_inout ) noexcept;

    //! \~japanese 接続所有権を移動代入する. \~english Move-assigns connection ownership.
    TcpClient& operator=( TcpClient&& other_inout ) noexcept;

    //! \~japanese Endpointへ同期接続する.
    //! \~english  Connects synchronously to an endpoint.
    //! @param [in] endpoint_in 接続先Host／Port.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 成否とStructured Error.
    TransportStatus connect(
          const Endpoint&         endpoint_in
        , const OperationContext& context_in
    );

    //! \~japanese 接続を閉じる. 複数回呼出可能. \~english Closes the connection and is idempotent.
    void disconnect() noexcept;

    //! @return \~japanese Local側が接続を保持する場合true. \~english True while local connection state is held.
    bool isConnected() const noexcept;

    //! \~japanese PeerのFIN／RSTが現在OSで観測可能か、受信Dataを消費せず即時確認する.
    //! \~english  Immediately checks for an OS-observable peer FIN/reset without consuming pending data.
    //! @return \~japanese Close／Reset／未接続をStructured Errorで返す. SuccessはEnd-to-end生存証明ではない.
    //! @return \~english  A structured close/reset/not-connected error. Success is not an end-to-end liveness proof.
    TransportStatus checkPeerConnection();

    //! @return \~japanese 接続中のRemote Endpoint. 未接続時は不正Endpoint. \~english The remote endpoint or an invalid endpoint.
    Endpoint getRemoteEndpoint() const;

    //! @return \~japanese OSが選択したLocal Endpoint. 未接続時は不正Endpoint.
    //! @return \~english The OS-selected local endpoint or an invalid endpoint.
    Endpoint getLocalEndpoint() const;

    //! \~japanese Buffer全体を同期送信する.
    //! \~english  Sends the complete buffer synchronously.
    //! @param [in] data_in 送信Buffer. `size_in == 0`の場合nullptr可能.
    //! @param [in] size_in 送信Size [byte].
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 送信済みSize [byte]とStructured Error.
    TransferResult<std::size_t> send(
          const std::uint8_t*    data_in
        , const std::size_t      size_in
        , const OperationContext& context_in
    );

    //! \~japanese Vector全体を同期送信する. \~english Sends the complete vector synchronously.
    //! @param [in] data_in 送信Data.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 送信済みSize [byte]とStructured Error.
    TransferResult<std::size_t> send(
          const std::vector<std::uint8_t>& data_in
        , const OperationContext&          context_in
    );

    //! \~japanese 最大Sizeまでの一回分を同期受信する.
    //! \~english  Receives one chunk up to the maximum size synchronously.
    //! @param [in] maximum_size_in 最大受信Size [byte]. 1以上`INT_MAX`以下.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 受信DataとStructured Error. Peer closeは`RemoteClosed`.
    TransferResult<std::vector<std::uint8_t>> receive(
          const std::size_t       maximum_size_in
        , const OperationContext& context_in
    );
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_TCP_TCPCLIENT_H
