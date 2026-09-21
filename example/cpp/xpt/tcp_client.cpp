// @file tcp_client.cpp
// @brief Portable XPT TCP client: connect, send, receive, peer check, disconnect.
//
// Requires WSE_BUILD_XPT=ON. XPT provides no listener API, so the peer endpoint below is
// a source constant (point it at any TCP echo service). Without a listening server the
// sample logs the structured connection error and exits cleanly.
//
// Being client-only is the shape of the API, not a shortcut taken here: XPT exposes no way to
// accept an inbound connection, so a sample cannot stand up its own peer the way udp_loopback
// does. Something else has to be listening for the interesting path to run.
//
// A refused connection and a receive that times out are both expected outcomes on a machine
// with no echo service, and neither changes the exit code.

#include <xpt/stew.h>
#include <wse/stew.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

//! Loopback keeps the sample off the network. A host name is accepted here as well and is
//! resolved during connect(), where a name that does not resolve reports HostNotFound.
constexpr const char HOST[] = "127.0.0.1";
//! TCP port 7 is the conventional echo service port.
// It is almost never enabled on a modern machine, so the usual result is ConnectionRefused and
// the sample stops after the first block. Point HOST and PORT at any listening TCP service to
// see the send and receive path run. Port zero is not usable as a remote endpoint.
constexpr std::uint16_t PORT = 7U;

void logTransportError( const std::string& operation_in, const wse::xpt::TransportError& error_in )
{
    wse::WLog() << "ERROR:" << operation_in << "failed."
                << "category=" << static_cast< int >( error_in.category() )
                << "code=" << static_cast< int >( error_in.code() )
                << "native=" << error_in.nativeCode()
                << "message=" << error_in.message();
}

} // namespace

int main()
{
    wse::registDefaultLog();

    // XPT has no default timeout, so the deadline is explicit. One context serves the connect,
    // the send, and the receive, and each gets the full second because a Timeout holds a
    // duration rather than an absolute moment. A second is ample on loopback; against a remote
    // host it is the connect that would want more, since name resolution and the handshake both
    // have to fit inside it.
    const wse::xpt::OperationContext context(
        wse::xpt::Timeout( std::chrono::milliseconds( 1000 ) ) );
    const wse::xpt::Endpoint endpoint( HOST, PORT );

    // The client owns the socket and is move-only, so a connection cannot be copied into a
    // second owner.
    wse::xpt::TcpClient client;
    const auto connected = client.connect( endpoint, context );
    if ( !connected.succeeded() )
    {
        // No server on this machine is a reportable state, not a sample defect.
        logTransportError( "connect " + std::string( HOST ) + ":" + std::to_string( PORT ),
                           connected.error() );
        wse::WLog() << "adjust the HOST/PORT constants to a reachable TCP service";
        return 0;
    }
    // Both endpoints are only meaningful once the connect has succeeded; on an unconnected
    // client they return an invalid endpoint rather than failing. The local one is chosen by
    // the operating system, which is why it is read back instead of being known in advance.
    wse::WLog() << "connected:" << client.getLocalEndpoint().host() << ":"
                << client.getLocalEndpoint().port() << "->"
                << client.getRemoteEndpoint().host() << ":"
                << client.getRemoteEndpoint().port();

    // send() loops until the complete buffer is written or the deadline expires.
    // A short write is therefore not a success case here. When it does fail the result still
    // carries how many bytes reached the peer, which for a stream is the only way to know where
    // to resume. TCP adds no message boundary either, so the peer may see these eleven bytes in
    // any number of pieces; a protocol that needs framing supplies its own.
    const std::string message = "WSE-XPT-TCP";
    const std::vector< std::uint8_t > payload( message.begin(), message.end() );
    const auto sent = client.send( payload, context );
    if ( !sent.succeeded() )
    {
        logTransportError( "send", sent.error() );
        client.disconnect();
        return 1;
    }
    wse::WLog() << "sent" << sent.value() << "bytes";

    // One receive returns a single chunk; an orderly peer close is RemoteClosed.
    // The 256 is the ceiling for this one read, not an expected length: a longer reply leaves
    // its remainder for the next call and a shorter one returns as soon as anything arrives.
    // RemoteClosed is how an end of stream is reported, so it is a distinct outcome from a
    // reset and from an empty read, and a reader loops until it sees it.
    const auto received = client.receive( 256U, context );
    if ( received.succeeded() )
    {
        const std::string text( received.value().begin(), received.value().end() );
        wse::WLog() << "received" << received.value().size() << "bytes:" << text;
    }
    else if ( received.error().code() == wse::xpt::eTransportErrorCode::TimedOut )
    {
        wse::WLog() << "no reply within the deadline (the peer is not an echo service)";
    }
    // Unlike the send above, a failed receive only logs: the connection may still be good, and
    // the peer check below is worth running either way.
    else
    {
        logTransportError( "receive", received.error() );
    }

    // A non-destructive peek for an OS-observable peer close or reset.
    // It returns at once and leaves any pending data where it is, so it can be called between
    // reads. Success only means the local stack has seen no FIN or reset yet; it is not proof
    // that the peer is alive, because a silently dropped connection looks identical until
    // something is actually sent. Anything that needs real liveness sends a heartbeat.
    const auto peer = client.checkPeerConnection();
    if ( peer.succeeded() )
    {
        wse::WLog() << "peer check: no close or reset observed";
    }
    else
    {
        wse::WLog() << "peer check: peer reported code="
                    << static_cast< int >( peer.error().code() );
    }

    // disconnect() is idempotent; the destructor would do the same.
    // The explicit call on the send-failure path above is redundant with the destructor for the
    // same reason; both are written out so the release point is visible. Afterwards
    // isConnected() is false and the endpoints read back as invalid, and the same object can
    // connect again.
    client.disconnect();
    wse::WLog() << "disconnected";
    return 0;
}
