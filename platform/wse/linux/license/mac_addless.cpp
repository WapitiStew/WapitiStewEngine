//*****************************************************************************************************************
//!
//! @file    mac_addless.cpp
//! @brief   \~japanese Linux向けDevice license識別子を取得する.
//! @brief   \~english  Acquires the Linux device-license identity.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create the Linux platform adapter.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "mac_addless.h"

#include <cstdio>
#include <ifaddrs.h>
#include <memory>

#include <net/if.h>
#include <netpacket/packet.h>

namespace wse
{
    namespace license
    {
        //! @brief Linux network interfaceからhardware addressを取得する.
        //! @author WapitiStew.
        std::string mac_address( void )
        {
            ifaddrs* p_interface_list = nullptr;
            if( getifaddrs( &p_interface_list ) != 0 )
            {
                return "";
            }

            const std::unique_ptr< ifaddrs, decltype( &freeifaddrs ) > interfaces(
                  p_interface_list
                , &freeifaddrs
            );

            for( const ifaddrs* p_interface = interfaces.get();
                 p_interface != nullptr;
                 p_interface = p_interface->ifa_next )
            {
                if( p_interface->ifa_addr == nullptr
                    || p_interface->ifa_addr->sa_family != AF_PACKET
                    || ( p_interface->ifa_flags & IFF_LOOPBACK ) != 0 )
                {
                    continue;
                }

                const sockaddr_ll* p_address = reinterpret_cast< const sockaddr_ll* >(
                    p_interface->ifa_addr
                );
                if( p_address->sll_halen != 6U )
                {
                    continue;
                }

                bool is_nonzero = false;
                for( unsigned int index = 0U; index < p_address->sll_halen; ++index )
                {
                    is_nonzero = is_nonzero || p_address->sll_addr[index] != 0U;
                }
                if( !is_nonzero )
                {
                    continue;
                }

                char address_text[18]{};
                std::snprintf(
                      address_text
                    , sizeof( address_text )
                    , "%02x:%02x:%02x:%02x:%02x:%02x"
                    , static_cast< unsigned int >( p_address->sll_addr[0] )
                    , static_cast< unsigned int >( p_address->sll_addr[1] )
                    , static_cast< unsigned int >( p_address->sll_addr[2] )
                    , static_cast< unsigned int >( p_address->sll_addr[3] )
                    , static_cast< unsigned int >( p_address->sll_addr[4] )
                    , static_cast< unsigned int >( p_address->sll_addr[5] )
                );
                return std::string( address_text );
            }

            return "";
        }
    }
}
