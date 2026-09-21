//*****************************************************************************************************************
//!
//! @file    Endpoint.h
//! @brief   \~japanese PortableなNetwork Endpoint値を定義する.
//! @brief   \~english  Defines a portable network endpoint value.
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

#ifndef WONDERSTEWENGINE_XPT_NETWORK_ENDPOINT_H
#define WONDERSTEWENGINE_XPT_NETWORK_ENDPOINT_H

#include "../../dynamic.h"

#include <cstdint>
#include <string>

namespace wse
{
namespace xpt
{

//! \~japanese Host名または数値AddressとTCP／UDP Portを組み合わせた値.
//! \~english  Host name or numeric address paired with a TCP/UDP port.
class WSE_API Endpoint final
{
  private:
    std::string m_host;
    std::uint16_t m_port;

  public:
    //! \~japanese 不正状態の空Endpointを生成する. \~english Creates an empty invalid endpoint.
    Endpoint();

    //! \~japanese HostとPortを保持するEndpointを生成する.
    //! \~english  Creates an endpoint holding a host and port.
    //! @param [in] host_in Host名、IPv4 AddressまたはIPv6 Address.
    //! @param [in] port_in Port番号. ZeroはRemote Endpointとして不正だが、UDP Bindでは自動割当を表す.
    Endpoint( const std::string& host_in, const std::uint16_t port_in );

    //! @return \~japanese 保持するHost. \~english The stored host.
    const std::string &host() const noexcept;

    //! @return \~japanese 保持するPort. \~english The stored port.
    std::uint16_t port() const noexcept;

    //! @return \~japanese Remote EndpointとしてHostが空でなくPortが非Zeroの場合true.
    //! @return \~english True for a non-empty host and non-zero port usable as a remote endpoint.
    bool isValid() const noexcept;
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_NETWORK_ENDPOINT_H
