//*****************************************************************************************************************
//!
//! @file    UdpClient.h
//! @brief   \~japanese Portableな同期UDP Datagram Client契約を定義する.
//! @brief   \~english  Defines the portable synchronous UDP datagram client contract.
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

#ifndef WONDERSTEWENGINE_XPT_UDP_UDPCLIENT_H
#define WONDERSTEWENGINE_XPT_UDP_UDPCLIENT_H

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

//! \~japanese 受信した1 Datagramと送信元Endpoint.
//! \~english  One received datagram and its source endpoint.
class WSE_API UdpDatagram final
{
  private:
    Endpoint m_source;
    std::vector<std::uint8_t> m_payload;

  public:
    //! \~japanese 空Payloadと不正Endpointを持つ値を生成する.
    //! \~english  Creates an empty value with an invalid endpoint.
    UdpDatagram();

    //! @param [in] source_in Datagram送信元.
    //! @param [in] payload_in 受信Payloadまたは切り詰められた部分Payload.
    UdpDatagram( const Endpoint& source_in, std::vector<std::uint8_t> payload_in );

    //! @return \~japanese Datagram送信元. \~english The datagram source.
    const Endpoint& source() const noexcept;

    //! @return \~japanese Datagram Payload. \~english The datagram payload.
    const std::vector<std::uint8_t>& payload() const noexcept;

    //! @return \~japanese 変更可能なDatagram Payload. \~english The mutable datagram payload.
    std::vector<std::uint8_t>& payload() noexcept;
};

//! \~japanese Caller-confinedでMove-onlyのUDP Socket Owner.
//! \~english  Caller-confined, move-only UDP socket owner.
class WSE_API UdpClient final
{
  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    //! \~japanese 未Open Clientを生成する. \~english Creates a closed client.
    UdpClient();

    //! \~japanese Socketを閉じてResourceを解放する. \~english Closes the socket and releases resources.
    ~UdpClient();
    UdpClient( const UdpClient& ) = delete;
    UdpClient& operator=( const UdpClient& ) = delete;

    //! \~japanese Socket所有権を移動する. \~english Moves socket ownership.
    UdpClient( UdpClient&& other_inout ) noexcept;

    //! \~japanese Socket所有権を移動代入する. \~english Move-assigns socket ownership.
    UdpClient& operator=( UdpClient&& other_inout ) noexcept;

    //! @return \~japanese Portableに送受信可能な最大UDP Payload Size [byte].
    //! @return \~english The maximum portable UDP payload size in bytes.
    static std::size_t maximumDatagramSize() noexcept;

    //! \~japanese Local EndpointへSocketをBindする.
    //! \~english  Binds the socket to a local endpoint.
    //! @param [in] local_endpoint_in Local Host／Port. Port 0はOSによる自動割当を要求する.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 成否とStructured Error. 成功時は既存Socketを置換する.
    TransportStatus bind(
          const Endpoint&         local_endpoint_in
        , const OperationContext& context_in
    );

    //! \~japanese Socketを閉じる. 複数回呼出可能. \~english Closes the socket and is idempotent.
    void close() noexcept;

    //! @return \~japanese Local Socketを保持する場合true. \~english True while a local socket is held.
    bool isOpen() const noexcept;

    //! @return \~japanese 実際のLocal Endpoint. 未Open時は不正Endpoint.
    //! @return \~english The actual local endpoint or an invalid endpoint while closed.
    Endpoint getLocalEndpoint() const;

    //! \~japanese 1 Datagramを同期送信する.
    //! \~english  Sends one datagram synchronously.
    //! @param [in] remote_endpoint_in 送信先Host／Port. Port 0は不正.
    //! @param [in] data_in Payload. `size_in == 0`の場合nullptr可能.
    //! @param [in] size_in Payload Size [byte]. `maximumDatagramSize()`以下.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 原子的に送信されたSize [byte]とStructured Error.
    TransferResult<std::size_t> sendTo(
          const Endpoint&         remote_endpoint_in
        , const std::uint8_t*     data_in
        , const std::size_t       size_in
        , const OperationContext& context_in
    );

    //! \~japanese Vectorを1 Datagramとして同期送信する.
    //! \~english  Sends a vector as one datagram synchronously.
    TransferResult<std::size_t> sendTo(
          const Endpoint&                  remote_endpoint_in
        , const std::vector<std::uint8_t>& data_in
        , const OperationContext&          context_in
    );

    //! \~japanese 1 Datagramと送信元を同期受信する.
    //! \~english  Receives one datagram and its source synchronously.
    //! @param [in] maximum_size_in 保存可能な最大Payload Size [byte]. 1以上`maximumDatagramSize()`以下.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 受信値とStructured Error. Buffer不足では部分Payload付き`DatagramTruncated`.
    TransferResult<UdpDatagram> receiveFrom(
          const std::size_t       maximum_size_in
        , const OperationContext& context_in
    );
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_UDP_UDPCLIENT_H
