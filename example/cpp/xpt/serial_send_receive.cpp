// @file serial_send_receive.cpp
// @brief Portable XPT serial port: open with fixed 8N1 framing, send, receive, close.
//
// Requires WSE_BUILD_XPT=ON. The device name below is a source constant; without the
// physical device the sample logs the structured OpenFailed error and exits cleanly,
// which is itself the documented no-device behavior.
//
// The framing is not a parameter: the contract is fixed at eight data bits, no parity, one stop
// bit, and no flow control. Only the device name and the baud rate are open to the caller, so a
// peer that needs seven data bits, a parity bit, or RTS/CTS cannot be reached through this API
// at all. That is a deliberate narrowing, not an omission this sample works around.
//
// A Timeout on the receive below is an expected outcome rather than a failure, because a port
// with nothing attached to answer is the normal case when this sample is run.

#include <xpt/stew.h>
#include <wse/stew.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

//! Windows uses "COM3"-style names; Linux uses "/dev/ttyUSB0"-style paths.
// The name is passed to the platform unchanged, so a machine without this port answers with an
// OpenFailed error and the sample exits. A COM port above COM9 needs the \\.\COM10 form on
// Windows, which is a platform rule rather than an XPT one.
constexpr const char DEVICE_NAME[] = "COM3";
//! One of the supported rates: 1200/2400/4800/9600/19200/38400/57600/115200.
// Anything outside that list is rejected as an invalid argument before the device is touched.
// The rate has to match what the peer is configured for; a mismatch does not fail, it delivers
// bytes that are silently wrong. 115200 is chosen only because it is the common default of a
// USB-to-serial adapter.
constexpr std::int32_t BAUD_RATE = 115200;

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

    // XPT deliberately has no default timeout: every operation takes an explicit deadline.
    // One context serves the open, the send, and the receive, and each of them gets the full
    // second, because a Timeout carries a duration rather than an absolute moment. A second is
    // generous for opening a port and for writing fifteen bytes at 115200 baud, and it is the
    // receive it really sizes: shorten it and an answering device that thinks for longer than
    // that is reported as TimedOut, lengthen it and a sample with no device attached takes that
    // much longer to finish. A context also carries a cancellation token; the single-argument
    // constructor supplies one that is never signalled.
    const wse::xpt::OperationContext context(
        wse::xpt::Timeout( std::chrono::milliseconds( 1000 ) ) );

    // The port owns the handle and is move-only, so it cannot be shared by accident. open()
    // replaces an existing port only when it succeeds, so a failed reopen leaves the previous
    // one usable.
    wse::xpt::SerialPort port;
    const auto opened = port.open( DEVICE_NAME, BAUD_RATE, context );
    if ( !opened.succeeded() )
    {
        // No device on this machine is a reportable state, not a sample defect.
        logTransportError( std::string( "open " ) + DEVICE_NAME, opened.error() );
        wse::WLog() << "adjust the DEVICE_NAME constant to a port that exists on this machine";
        return 0;
    }
    wse::WLog() << "opened" << port.getDeviceName() << "at" << port.getBaudRate()
                << "baud (8N1)";

    // The trailing carriage return is there because a line-oriented device usually needs a
    // terminator before it acts. XPT sends the bytes as given and adds no framing of its own.
    const std::string message = "WSE-XPT-SERIAL\r";
    const std::vector< std::uint8_t > payload( message.begin(), message.end() );
    // send() writes the whole buffer or fails; it does not return early with a short count on
    // success. On failure the result still carries the number of bytes that did get out, which
    // is why the value travels with the error instead of being replaced by it.
    const auto sent = port.send( payload, context );
    if ( !sent.succeeded() )
    {
        logTransportError( "send", sent.error() );
        port.close();
        return 1;
    }
    wse::WLog() << "sent" << sent.value() << "bytes";

    // One receive returns whatever the peer produced within the deadline. Without a
    // loopback plug or an answering device, a Timeout is the expected structured result.
    // The 256 is a ceiling on this one read, not a message length: a serial stream has no
    // record boundaries, so a longer reply simply leaves its remainder for the next receive,
    // and a shorter one returns early rather than waiting for the buffer to fill. Any real
    // protocol loops here until its own terminator arrives instead of reading once.
    const auto received = port.receive( 256U, context );
    if ( received.succeeded() )
    {
        const std::string text( received.value().begin(), received.value().end() );
        wse::WLog() << "received" << received.value().size() << "bytes:" << text;
    }
    // The code, not the message, is what a decision is made on. A timeout is treated as a
    // normal outcome here and keeps the exit code at zero; every other code is a real failure.
    else if ( received.error().code() == wse::xpt::eTransportErrorCode::TimedOut )
    {
        wse::WLog() << "no reply within the deadline (expected without an answering device)";
    }
    else
    {
        logTransportError( "receive", received.error() );
        port.close();
        return 2;
    }

    // close() is idempotent; the destructor would do the same.
    // The explicit close on each failure path above is therefore redundant with the destructor
    // as well; it is written out so the release point is visible. After the call isOpen() is
    // false, getDeviceName() is empty, and getBaudRate() is zero, and the same object can be
    // opened again.
    port.close();
    wse::WLog() << "closed";
    return 0;
}
