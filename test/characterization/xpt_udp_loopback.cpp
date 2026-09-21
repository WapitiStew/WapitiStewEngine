//*****************************************************************************************************************
//!
//! @file    xpt_udp_loopback.cpp
//! @brief   \~japanese Portable XPT UDP／Packet境界／Timeout／Cancellation契約を検証する.
//! @brief   \~english  Verifies portable XPT UDP packet, timeout, and cancellation contracts.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <xpt/stew.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

int failures = 0;

void expect( const bool condition_in, const char* const message_in )
{
    if ( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}

wse::xpt::OperationContext context( const std::int64_t milliseconds_in = 1000 )
{
    return wse::xpt::OperationContext(
        wse::xpt::Timeout::milliseconds( milliseconds_in ) );
}

} // namespace

int main()
{
    static_assert( !std::is_copy_constructible<wse::xpt::UdpClient>::value,
                   "UdpClient is not copyable" );
    static_assert( std::is_move_constructible<wse::xpt::UdpClient>::value,
                   "UdpClient is movable" );

    expect( wse::xpt::UdpClient::maximumDatagramSize() == 65507,
            "UDP exposes the portable IPv4/IPv6-safe payload boundary" );

    wse::xpt::UdpClient validation_client;
    const wse::xpt::Endpoint invalid_endpoint;
    const auto invalid_bind = validation_client.bind( invalid_endpoint, context() );
    expect( !invalid_bind.succeeded() &&
                invalid_bind.error().category() ==
                    wse::xpt::eTransportErrorCategory::Validation,
            "UDP bind rejects an empty local host" );

    const auto invalid_timeout = validation_client.bind(
        wse::xpt::Endpoint( "127.0.0.1", 0 ), context( -1 ) );
    expect( !invalid_timeout.succeeded() &&
                invalid_timeout.error().code() ==
                    wse::xpt::eTransportErrorCode::InvalidArgument,
            "UDP bind rejects a negative timeout" );

    const std::uint8_t one_byte = 1;
    const auto invalid_destination = validation_client.sendTo(
        invalid_endpoint, &one_byte, 1, context() );
    expect( !invalid_destination.succeeded() &&
                invalid_destination.error().code() ==
                    wse::xpt::eTransportErrorCode::InvalidArgument,
            "UDP send rejects an invalid destination" );

    const auto invalid_pointer = validation_client.sendTo(
        wse::xpt::Endpoint( "127.0.0.1", 9 ), nullptr, 1, context() );
    expect( !invalid_pointer.succeeded() &&
                invalid_pointer.error().code() ==
                    wse::xpt::eTransportErrorCode::InvalidArgument,
            "UDP send rejects a null non-empty payload" );

    const auto oversized_send = validation_client.sendTo(
        wse::xpt::Endpoint( "127.0.0.1", 9 ), &one_byte,
        wse::xpt::UdpClient::maximumDatagramSize() + 1, context() );
    expect( !oversized_send.succeeded() &&
                oversized_send.error().code() ==
                    wse::xpt::eTransportErrorCode::MessageTooLarge,
            "UDP send rejects a payload above the portable boundary before socket access" );

    const auto unopened_receive = validation_client.receiveFrom( 1, context() );
    expect( !unopened_receive.succeeded() &&
                unopened_receive.error().code() ==
                    wse::xpt::eTransportErrorCode::NotConnected,
            "UDP receive requires an open socket" );

    {
        wse::xpt::UdpClient receiver;
        const auto bound = receiver.bind(
            wse::xpt::Endpoint( "127.0.0.1", 0 ), context() );
        expect( bound.succeeded() && receiver.isOpen(),
                "UDP binds to an OS-assigned loopback port" );
        expect( receiver.getLocalEndpoint().isValid() &&
                    receiver.getLocalEndpoint().host() == "127.0.0.1",
                "UDP exposes the actual bound local endpoint" );

        const auto invalid_receive = receiver.receiveFrom( 0, context() );
        expect( !invalid_receive.succeeded() &&
                    invalid_receive.error().code() ==
                        wse::xpt::eTransportErrorCode::InvalidArgument,
                "UDP receive rejects a zero-capacity buffer" );

        wse::xpt::UdpClient sender;
        const std::vector<std::uint8_t> payload { 'W', 'S', 'E', '-', 'U', 'D', 'P' };
        const auto sent = sender.sendTo( receiver.getLocalEndpoint(), payload, context() );
        expect( sent.succeeded() && sent.value() == payload.size() && sender.isOpen(),
                "UDP sends one complete loopback datagram" );
        expect( sender.getLocalEndpoint().isValid(),
                "UDP lazy open exposes its OS-assigned local endpoint" );

        const auto received = receiver.receiveFrom( 64, context() );
        expect( received.succeeded() && received.value().payload() == payload,
                "UDP receives the complete loopback datagram" );
        expect( received.value().source().port() == sender.getLocalEndpoint().port(),
                "UDP receive exposes the portable source endpoint" );

        const auto empty_sent = sender.sendTo(
            receiver.getLocalEndpoint(), nullptr, 0, context() );
        expect( empty_sent.succeeded() && empty_sent.value() == 0,
                "UDP preserves a zero-length datagram instead of treating it as a no-op" );
        const auto empty_received = receiver.receiveFrom( 1, context() );
        expect( empty_received.succeeded() && empty_received.value().payload().empty(),
                "UDP receives a zero-length datagram as a successful packet" );

        receiver.close();
        expect( !receiver.isOpen() && !receiver.getLocalEndpoint().isValid(),
                "UDP close is observable and clears the local endpoint" );
        receiver.close();
    }

    {
        wse::xpt::UdpClient client;
        wse::xpt::UdpClient occupied;
        expect( client.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded() &&
                    occupied.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded(),
                "UDP transactional-bind fixture binds two sockets" );
        const wse::xpt::Endpoint original = client.getLocalEndpoint();
        const auto failed_rebind = client.bind( occupied.getLocalEndpoint(), context() );
        expect( !failed_rebind.succeeded() &&
                    failed_rebind.error().code() ==
                        wse::xpt::eTransportErrorCode::BindFailed,
                "UDP rebind reports an occupied local endpoint" );
        expect( client.isOpen() &&
                    client.getLocalEndpoint().port() == original.port(),
                "UDP failed rebind preserves the existing socket" );
    }

    {
        wse::xpt::UdpClient receiver;
        expect( receiver.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded(),
                "UDP timeout fixture binds" );
        const auto started = std::chrono::steady_clock::now();
        const auto timed_out = receiver.receiveFrom( 16, context( 50 ) );
        const auto elapsed = std::chrono::steady_clock::now() - started;
        expect( !timed_out.succeeded() &&
                    timed_out.error().code() ==
                        wse::xpt::eTransportErrorCode::TimedOut,
                "UDP receive timeout has a distinguishable error" );
        expect( elapsed < std::chrono::milliseconds( 500 ),
                "UDP receive timeout is bounded by a monotonic deadline" );
        expect( receiver.isOpen(), "UDP timeout does not discard the socket" );
    }

    {
        wse::xpt::UdpClient receiver;
        expect( receiver.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded(),
                "UDP cancellation fixture binds" );
        wse::xpt::CancellationSource cancellation;
        std::thread canceller( [&cancellation]() {
            std::this_thread::sleep_for( std::chrono::milliseconds( 40 ) );
            cancellation.cancel();
        } );
        const auto cancelled = receiver.receiveFrom(
            16, wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ),
                                            cancellation.token() ) );
        canceller.join();
        expect( !cancelled.succeeded() &&
                    cancelled.error().code() ==
                        wse::xpt::eTransportErrorCode::Cancelled,
                "Cross-thread cancellation interrupts UDP receive" );
        expect( receiver.isOpen(), "UDP cancellation does not discard the socket" );
    }

    {
        wse::xpt::UdpClient receiver;
        wse::xpt::UdpClient sender;
        expect( receiver.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded(),
                "UDP truncation fixture binds" );
        std::vector<std::uint8_t> payload( 32 );
        for ( std::size_t index = 0; index < payload.size(); ++index )
        {
            payload[ index ] = static_cast<std::uint8_t>( index );
        }
        expect( sender.sendTo( receiver.getLocalEndpoint(), payload, context() ).succeeded(),
                "UDP truncation fixture sends" );
        const auto truncated = receiver.receiveFrom( 8, context() );
        expect( !truncated.succeeded() &&
                    truncated.error().code() ==
                        wse::xpt::eTransportErrorCode::DatagramTruncated,
                "UDP buffer shortage has a distinguishable error" );
        expect( truncated.value().payload().size() == 8,
                "UDP truncation preserves the received prefix" );
        expect( truncated.value().source().port() == sender.getLocalEndpoint().port(),
                "UDP truncation preserves the source endpoint" );
        bool prefix_matches = truncated.value().payload().size() == 8;
        for ( std::size_t index = 0; prefix_matches && index < 8; ++index )
        {
            prefix_matches = truncated.value().payload()[ index ] == payload[ index ];
        }
        expect( prefix_matches, "UDP truncation preserves the payload prefix" );
    }

    {
        wse::xpt::UdpClient receiver;
        wse::xpt::UdpClient sender;
        expect( receiver.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context() ).succeeded(),
                "UDP maximum-size fixture binds" );
        std::vector<std::uint8_t> maximum_payload(
            wse::xpt::UdpClient::maximumDatagramSize(), 0x5A );
        const auto sent = sender.sendTo(
            receiver.getLocalEndpoint(), maximum_payload, context( 3000 ) );
        expect( sent.succeeded() && sent.value() == maximum_payload.size(),
                "UDP sends the maximum portable datagram" );
        const auto received = receiver.receiveFrom(
            wse::xpt::UdpClient::maximumDatagramSize(), context( 3000 ) );
        expect( received.succeeded() && received.value().payload() == maximum_payload,
                "UDP receives the maximum portable datagram without truncation" );
    }

    return failures == 0 ? 0 : 1;
}
