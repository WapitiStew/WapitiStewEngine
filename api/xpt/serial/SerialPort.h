//*****************************************************************************************************************
//!
//! @file    SerialPort.h
//! @brief   \~japanese Portableな同期Serial Port契約を定義する.
//! @brief   \~english  Defines the portable synchronous serial-port contract.
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

#pragma once

#ifndef WONDERSTEWENGINE_XPT_SERIAL_SERIALPORT_H
#define WONDERSTEWENGINE_XPT_SERIAL_SERIALPORT_H

#include "../error/TransportError.h"
#include "../operation/OperationContext.h"
#include "../../dynamic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wse
{
namespace xpt
{

//! \~japanese Caller-confinedでMove-onlyのSerial Port Owner.
//! \~english  Caller-confined, move-only serial-port owner.
//! @details 8 data bits、no parity、1 stop bit、no flow controlの固定契約を使用する.
class WSE_API SerialPort final
{
  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    //! \~japanese Close状態のPortを生成する. \~english Creates a closed port.
    SerialPort();

    //! \~japanese Portを閉じてResourceを解放する. \~english Closes the port and releases resources.
    ~SerialPort();
    SerialPort( const SerialPort& ) = delete;
    SerialPort& operator=( const SerialPort& ) = delete;

    //! \~japanese Port所有権を移動する. \~english Moves port ownership.
    SerialPort( SerialPort&& other_inout ) noexcept;

    //! \~japanese Port所有権を移動代入する. \~english Move-assigns port ownership.
    SerialPort& operator=( SerialPort&& other_inout ) noexcept;

    //! \~japanese Deviceを8N1で同期Openする.
    //! \~english  Opens a device synchronously with 8N1 framing.
    //! @param [in] device_name_in Windowsの`COM3`またはLinuxの`/dev/ttyUSB0`等.
    //! @param [in] baud_rate_in 1200、2400、4800、9600、19200、38400、57600、115200のいずれか.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 成否とStructured Error. 成功時だけ既存Portを置換する.
    TransportStatus open(
          const std::string&      device_name_in
        , const std::int32_t      baud_rate_in
        , const OperationContext& context_in
    );

    //! \~japanese Portを閉じる. 複数回呼出可能. \~english Closes the port and is idempotent.
    void close() noexcept;

    //! @return \~japanese Portを保持する場合true. \~english True while a port is held.
    bool isOpen() const noexcept;

    //! @return \~japanese Open中のDevice名. Close中は空文字列. \~english The open device name or an empty string.
    std::string getDeviceName() const;

    //! @return \~japanese Open中のBaud rate. Close中は0. \~english The active baud rate or zero.
    std::int32_t getBaudRate() const noexcept;

    //! \~japanese Buffer全体を同期送信する. \~english Sends the complete buffer synchronously.
    //! @param [in] data_in 送信Buffer. `size_in == 0`の場合nullptr可能.
    //! @param [in] size_in 送信Size [byte].
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 送信済みSize [byte]とStructured Error.
    TransferResult<std::size_t> send(
          const std::uint8_t*     data_in
        , const std::size_t       size_in
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
    //! @return 受信DataとStructured Error.
    TransferResult<std::vector<std::uint8_t>> receive(
          const std::size_t       maximum_size_in
        , const OperationContext& context_in
    );
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_SERIAL_SERIALPORT_H
