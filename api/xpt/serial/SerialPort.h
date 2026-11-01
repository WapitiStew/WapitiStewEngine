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
//!   Nov-01, 2026   Retain Windows pending reads across polling deadlines and collect write progress.
//!   Oct-15, 2026   Accept positive int32 baud requests with native readback verification.
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
    //! @param [in] baud_rate_in 1..INT32_MAX (2147483647). OSへ要求する正整数Baud rate.
    //! @param [in] context_in 明示Timeout／Cancellation.
    //! @return 成否とStructured Error. 成功時だけ既存Portを置換する.
    //! @note \~japanese 設定拒否・読戻し失敗・Rate不一致はConfigurationFailed. 実際の信号精度は保証しない.
    //! @note \~english Configuration rejection, readback failure or rate mismatch returns ConfigurationFailed.
    //!     OS readback does not certify physical signal accuracy.
    TransportStatus open(
          const std::string&      device_name_in
        , const std::int32_t      baud_rate_in
        , const OperationContext& context_in
    );

    //! \~japanese Portを閉じる. 複数回呼出可能. \~english Closes the port and is idempotent.
    //! @details Windowsの保留Readを取消し・完了回収してからBuffer・Event・Portを解放する.
    //!     Windows cancels and collects a pending read before releasing its storage and port.
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
    //! @return 最後の保留Writeの回収分を含む送信済みSize [byte]とStructured Error.
    //! @details Completed native progress is counted once even when cancellation races completion; no automatic replay occurs.
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
    //! @return 受信DataとStructured Error. Errorとともに返る取得Prefixも一度だけ消費する.
    //! @details WindowsのPoll TimeoutではOS Read・BufferをPort内へ保持し、次のPollで同じ完了を回収する.
    //!     完了済みDataをDeadlineより先に確認し、返却上限が縮んだ場合の未返却Suffixも所有し続ける.
    //!     Cancellation／closeは保留I/Oを回収する. Native cleanupは名目Timeoutを超え得る.
    //!     On Windows a polling timeout retains the read, event and buffer in the port. A later call collects the
    //!     same completion; a smaller maximum returns a prefix and retains the suffix. Collected data can be returned
    //!     alongside an error. Cancellation and close collect native completion before releasing I/O storage.
    TransferResult<std::vector<std::uint8_t>> receive(
          const std::size_t       maximum_size_in
        , const OperationContext& context_in
    );
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_SERIAL_SERIALPORT_H
