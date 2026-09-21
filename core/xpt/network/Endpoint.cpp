//*****************************************************************************************************************
//!
//! @file    Endpoint.cpp
//! @brief   \~japanese PortableなNetwork Endpoint値を実装する.
//! @brief   \~english  Implements the portable network endpoint value.
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

#include "xpt/network/Endpoint.h"

namespace wse
{
namespace xpt
{

Endpoint::Endpoint()
    : m_host ()
    , m_port ( 0 )
{
}

Endpoint::Endpoint( const std::string& host_in, const std::uint16_t port_in )
    : m_host ( host_in )
    , m_port ( port_in )
{
}

const std::string &Endpoint::host() const noexcept
{
    return this->m_host;
}

std::uint16_t Endpoint::port() const noexcept
{
    return this->m_port;
}

bool Endpoint::isValid() const noexcept
{
    return !this->m_host.empty() && this->m_port != 0;
}

} // namespace xpt
} // namespace wse
