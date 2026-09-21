// @file udp_loopback.cpp
// @brief Portable XPT UDP send/receive over the loopback interface with explicit deadlines.
//
// Hardware-free by design: both endpoints are UDP sockets bound to 127.0.0.1 in this process,
// so the sample runs anywhere XPT builds. Build with WSE_BUILD_XPT=ON and link WSE::Xpt.
//
// XPT deliberately has no default timeout: every operation takes an explicit OperationContext so
// a caller can never wait forever by accident.
//
// This is the one XPT sample whose interesting path always runs, which is why it is also the one
// that returns a non-zero code for a transport failure: with both endpoints inside this process
// there is no absent device or missing server to excuse one. The idle receive at the end is the
// exception, where a Timeout is the result being demonstrated rather than a failure.

#include <xpt/stew.h>
#include <wse/stew.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

//! Logs a structured transport failure without inventing a category of its own.
void reportFailure( const std::string& operation_in, const wse::xpt::TransportError& error_in )
{
    wse::WLog() << "ERROR:" << operation_in << "failed."
                << "category=" << static_cast< int >( error_in.category() )
                << "code=" << static_cast< int >( error_in.code() )
                << "nativeCode=" << error_in.nativeCode()
                << "message=" << error_in.message();
}

} // namespace

int main()
{
    wse::registDefaultLog();

    // One explicit deadline is reused for every operation in this sample.
    // Reuse is safe because a Timeout carries a duration, not an absolute moment, so each call
    // gets its own second. A second is far more than a loopback datagram needs; it is sized so
    // that a loaded machine cannot make a working exchange look like a failure.
    const wse::xpt::OperationContext context(
        wse::xpt::Timeout( std::chrono::milliseconds( 1000 ) ) );

    // Port zero asks the operating system to assign a free port to each socket.
    // That is what keeps the sample from colliding with anything already running, and it is the
    // one place a zero port is meaningful: as a remote endpoint it would be rejected. The
    // receiver is bound first because a datagram sent to a port nobody holds is simply dropped.
    wse::xpt::UdpClient receiver;
    const wse::xpt::TransportStatus bound_receiver =
        receiver.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context );
    if ( !bound_receiver.succeeded() )
    {
        reportFailure( "receiver bind", bound_receiver.error() );
        return 1;
    }

    wse::xpt::UdpClient sender;
    const wse::xpt::TransportStatus bound_sender =
        sender.bind( wse::xpt::Endpoint( "127.0.0.1", 0 ), context );
    if ( !bound_sender.succeeded() )
    {
        reportFailure( "sender bind", bound_sender.error() );
        return 1;
    }

    // The port the operating system chose is only knowable by reading it back; this is the
    // address the sender then aims at. On a closed client the same call returns an invalid
    // endpoint rather than failing.
    const wse::xpt::Endpoint receiver_endpoint = receiver.getLocalEndpoint();
    wse::WLog() << "receiver endpoint:" << receiver_endpoint.host() << ":"
                << receiver_endpoint.port();
    wse::WLog() << "maximum datagram size:" << wse::xpt::UdpClient::maximumDatagramSize();

    // maximumDatagramSize() is the portable ceiling XPT will carry, deliberately below what a
    // particular stack might allow, so a payload that fits is sendable everywhere. A larger one
    // is refused as MessageTooLarge rather than fragmented.
    // A datagram is sent whole or not at all, so the byte count below is either the payload
    // size or nothing; there is no partial send to resume the way a stream has.
    const std::string message = "wse-xpt-udp";
    const std::vector<std::uint8_t> payload( message.begin(), message.end() );
    const wse::xpt::TransferResult<std::size_t> sent =
        sender.sendTo( receiver_endpoint, payload, context );
    if ( !sent.succeeded() )
    {
        reportFailure( "sendTo", sent.error() );
        return 1;
    }
    wse::WLog() << "sent bytes:" << sent.value();

    // Asking for the full ceiling is what makes truncation impossible here. A smaller argument
    // would not wait for a smaller datagram; it would report DatagramTruncated and hand back
    // the leading bytes, with the rest lost, because the remainder of a datagram is discarded
    // rather than queued the way a stream's would be.
    const wse::xpt::TransferResult<wse::xpt::UdpDatagram> received =
        receiver.receiveFrom( wse::xpt::UdpClient::maximumDatagramSize(), context );
    if ( !received.succeeded() )
    {
        reportFailure( "receiveFrom", received.error() );
        return 1;
    }

    // The datagram owns its payload and carries the source endpoint with it, which is the only
    // way an unconnected UDP socket learns who sent what. It is a value held by the result, so
    // it stays valid after the receiver is closed.
    const wse::xpt::UdpDatagram& datagram = received.value();
    const std::string text( datagram.payload().begin(), datagram.payload().end() );
    wse::WLog() << "received from:" << datagram.source().host() << ":"
                << datagram.source().port();
    wse::WLog() << "received payload:" << text;

    // A short deadline with no traffic must report Timeout rather than blocking.
    // Fifty milliseconds is chosen to be plainly shorter than a human notices while still being
    // long enough that a scheduling hiccup cannot make a delivered datagram look late. Nothing
    // is in flight at this point, so the wait is expected to expire; that is the demonstration.
    const wse::xpt::OperationContext short_context(
        wse::xpt::Timeout( std::chrono::milliseconds( 50 ) ) );
    const wse::xpt::TransferResult<wse::xpt::UdpDatagram> idle =
        receiver.receiveFrom( wse::xpt::UdpClient::maximumDatagramSize(), short_context );
    wse::WLog() << "idle receive timed out:"
                << ( !idle.succeeded()
                     && idle.error().code() == wse::xpt::eTransportErrorCode::TimedOut );

    // close() is idempotent; the destructor would do the same. It is not terminal for the
    // object: after it isOpen() is false and getLocalEndpoint() is invalid, but the same client
    // can be bound again.
    receiver.close();
    sender.close();
    // The payload is compared rather than assumed. On loopback within one process a datagram is
    // not realistically lost or reordered, which is what lets this sample assert equality at
    // all; over a real network UDP promises neither, and a protocol would need its own sequence
    // numbers and retries.
    return ( text == message ) ? 0 : 1;
}
