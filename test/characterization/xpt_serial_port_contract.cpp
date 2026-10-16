//*****************************************************************************************************************
//!
//! @file    xpt_serial_port_contract.cpp
//! @brief   \~japanese Portable XPT Serial／PTY／Timeout／Cancellation契約を検証する.
//! @brief   \~english  Verifies portable XPT serial, PTY, timeout, and cancellation contracts.
//!
//! @details
//!   \~japanese
//!   この契約が破れると、SerialPortを使うApplicationは次を失う.
//!   引数不正がDeviceへ触れる前に構造化Errorになること、Timeoutが単調Deadlineで
//!   必ず戻ること、他ThreadからのCancellationが受信中のCallを解けること、
//!   そして失敗したOpen／Timeout／Cancellationが既存のPortを壊さないこと.
//!   実Serial Hardwareは使わない。LinuxではIn-process PTY対を使い、Windowsでは
//!   存在し得ないDevice名でOpen失敗だけを踏む。したがってFlow control、Parity／Frame Error、
//!   Cable抜去、および実Deviceの遅い応答は再現しない.
//!   \~english
//!   Failing this test means a SerialPort consumer loses argument validation before
//!   device access, a bounded monotonic receive deadline, cross-thread cancellation of a
//!   blocking receive, and the guarantee that a failed open, timeout, or cancellation
//!   leaves the already-open port intact.
//!   No physical serial hardware is used. Linux drives a complete round trip over an
//!   in-process PTY pair; Windows only exercises a deterministic open failure. Flow
//!   control, parity and framing errors, cable removal, and slow real devices are
//!   therefore out of scope.
//!
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Oct-15, 2026   Verify representative and arbitrary baud rates with virtual terminals.
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <xpt/stew.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if !defined( _WIN32 )
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

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

#if !defined( _WIN32 )

class PseudoTerminal final
{
  private:
    int m_master;
    std::string m_slave_name;

  public:
    PseudoTerminal()
        : m_master     ( -1 )
        , m_slave_name ()
    {
        this->m_master = posix_openpt( O_RDWR | O_NOCTTY | O_NONBLOCK );
        if ( this->m_master < 0 || grantpt( this->m_master ) != 0 ||
             unlockpt( this->m_master ) != 0 )
        {
            this->close();
            return;
        }
        const char* const p_slave_name = ptsname( this->m_master );
        if ( p_slave_name == nullptr )
        {
            this->close();
            return;
        }
        this->m_slave_name = p_slave_name;
    }

    ~PseudoTerminal()
    {
        this->close();
    }

    PseudoTerminal( const PseudoTerminal& ) = delete;
    PseudoTerminal& operator=( const PseudoTerminal& ) = delete;

    bool isOpen() const noexcept
    {
        return this->m_master >= 0 && !this->m_slave_name.empty();
    }

    int getMaster() const noexcept
    {
        return this->m_master;
    }

    const std::string& getSlaveName() const noexcept
    {
        return this->m_slave_name;
    }

    void close() noexcept
    {
        if ( this->m_master >= 0 )
        {
            ::close( this->m_master );
            this->m_master = -1;
        }
        this->m_slave_name.clear();
    }
};

bool readMaster(
          std::vector<std::uint8_t>* p_received_out
    , const int                       master_in
    , const std::size_t               expected_size_in
)
{
    std::vector<std::uint8_t>& received_out = *p_received_out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 1 );
    while ( received_out.size() < expected_size_in &&
            std::chrono::steady_clock::now() < deadline )
    {
        pollfd descriptor = {};
        descriptor.fd = master_in;
        descriptor.events = POLLIN;
        if ( poll( &descriptor, 1, 20 ) <= 0 )
        {
            continue;
        }
        std::uint8_t buffer[ 64 ] = {};
        const ssize_t size = ::read( master_in, buffer, sizeof( buffer ) );
        if ( size > 0 )
        {
            received_out.insert( received_out.end(), buffer, buffer + size );
        }
    }
    return received_out.size() == expected_size_in;
}

#endif

} // namespace

int main()
{
    // Verify the platform-independent type and validation contract.
    static_assert( !std::is_copy_constructible<wse::xpt::SerialPort>::value,
                   "SerialPort is not copyable" );
    static_assert( std::is_move_constructible<wse::xpt::SerialPort>::value,
                   "SerialPort is movable" );

    wse::xpt::SerialPort validation_port;
    expect( !validation_port.isOpen() && validation_port.getDeviceName().empty() &&
                validation_port.getBaudRate() == 0,
            "SerialPort starts closed" );

    const auto empty_device = validation_port.open( "", 9600, context() );
    expect( !empty_device.succeeded() && empty_device.error().code() ==
                wse::xpt::eTransportErrorCode::InvalidArgument,
            "Serial open rejects an empty device" );
    const std::int32_t unsupported_rates[] = { -2147483647 - 1, -1, 0 };
    for ( const auto baud_rate : unsupported_rates )
    {
        const auto invalid_baud = validation_port.open( "invalid", baud_rate, context() );
        expect( !invalid_baud.succeeded() && invalid_baud.error().code() ==
                    wse::xpt::eTransportErrorCode::InvalidArgument,
                "Serial open rejects unsupported baud rates before device access" );
    }
    const auto invalid_timeout = validation_port.open( "invalid", 9600, context( -1 ) );
    expect( !invalid_timeout.succeeded() && invalid_timeout.error().code() ==
                wse::xpt::eTransportErrorCode::InvalidArgument,
            "Serial open rejects a negative timeout" );

    wse::xpt::CancellationSource already_cancelled;
    already_cancelled.cancel();
    const auto cancelled_open = validation_port.open(
        "invalid", 9600,
        wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ),
                                    already_cancelled.token() ) );
    expect( !cancelled_open.succeeded() && cancelled_open.error().code() ==
                wse::xpt::eTransportErrorCode::Cancelled,
            "Serial open observes prior cancellation before device access" );

    const std::uint8_t byte = 1;
    const auto closed_send = validation_port.send( &byte, 1, context() );
    const auto closed_receive = validation_port.receive( 1, context() );
    expect( !closed_send.succeeded() && closed_send.error().code() ==
                wse::xpt::eTransportErrorCode::NotConnected,
            "Serial send requires an open port" );
    expect( !closed_receive.succeeded() && closed_receive.error().code() ==
                wse::xpt::eTransportErrorCode::NotConnected,
            "Serial receive requires an open port" );

#if defined( _WIN32 )
    // Exercise a deterministic Windows open failure without physical hardware.
    const auto unavailable = validation_port.open(
        "WSE_PHASE2C_INVALID_SERIAL_PORT_5A29B4C1", 9600, context() );
    expect( !unavailable.succeeded() && unavailable.error().code() ==
                wse::xpt::eTransportErrorCode::OpenFailed,
            "Windows reports an unavailable serial device without hardware" );
#else
    // Exercise the complete Linux backend against an in-process PTY pair.
    PseudoTerminal pseudo_terminal;
    expect( pseudo_terminal.isOpen(), "Linux creates a PTY fixture" );
    if ( pseudo_terminal.isOpen() )
    {
        // Retain the fifteen representative rates and exercise arbitrary values and boundaries.
        const std::int32_t baud_rates[] = {
            110, 300, 600, 1200, 2400, 4800, 9600, 14400,
            19200, 38400, 57600, 115200, 230400, 460800, 921600,
            1, 12345, 250000, 1000000, 2147483647
        };
        wse::xpt::SerialPort rate_port;
        for ( const auto baud_rate : baud_rates )
        {
            const auto configured = rate_port.open( pseudo_terminal.getSlaveName(), baud_rate, context() );
            expect( configured.succeeded() && rate_port.getBaudRate() == baud_rate,
                    "Every supported baud rate opens and reopens a PTY" );
            if ( !configured.succeeded() )
            {
                continue;
            }
            const int observer = ::open( pseudo_terminal.getSlaveName().c_str(), O_RDWR | O_NOCTTY );
            termios2 settings = {};
            expect( observer >= 0 && ::ioctl( observer, TCGETS2, &settings ) == 0 &&
                        settings.c_ispeed == static_cast<speed_t>( baud_rate ) &&
                        settings.c_ospeed == static_cast<speed_t>( baud_rate ) &&
                        ( settings.c_cflag & ( CSIZE | PARENB | CSTOPB | CRTSCTS ) ) == CS8,
                    "Kernel settings retain the exact rate and 8N1 without hardware flow control" );
            if ( observer >= 0 )
            {
                ::close( observer );
            }
            const std::vector<std::uint8_t> payload { 0x00, 0x11, 0x13, 0xff };
            const auto sent_at_rate = rate_port.send( payload, context() );
            std::vector<std::uint8_t> observed;
            expect( sent_at_rate.succeeded() && sent_at_rate.value() == payload.size() &&
                        readMaster( &observed, pseudo_terminal.getMaster(), payload.size() ) &&
                        observed == payload,
                    "Every rate sends binary data through the PTY" );
            const auto written = ::write( pseudo_terminal.getMaster(), payload.data(), payload.size() );
            const auto reply = rate_port.receive( 64, context() );
            expect( written == static_cast<ssize_t>( payload.size() ) && reply.succeeded() &&
                        reply.value() == payload,
                    "Every rate receives binary data through the PTY" );
            const auto rejected = rate_port.open( pseudo_terminal.getSlaveName(), 0, context() );
            expect( !rejected.succeeded() && rejected.error().code() ==
                        wse::xpt::eTransportErrorCode::InvalidArgument && rate_port.isOpen() &&
                        rate_port.getBaudRate() == baud_rate,
                    "Unsupported rates preserve the active configuration" );
        }
        const auto non_terminal = rate_port.open( "/dev/null", 14400, context() );
        expect( !non_terminal.succeeded() && non_terminal.error().code() ==
                    wse::xpt::eTransportErrorCode::ConfigurationFailed &&
                    non_terminal.error().nativeCode() != 0 && rate_port.isOpen() &&
                    rate_port.getBaudRate() == 2147483647,
                "Native configuration failure preserves the existing PTY and reports the error" );
        rate_port.close();
        wse::xpt::SerialPort port;
        const auto opened = port.open( pseudo_terminal.getSlaveName(), 115200, context() );
        expect( opened.succeeded() && port.isOpen(), "Linux opens a PTY slave as SerialPort" );
        expect( port.getDeviceName() == pseudo_terminal.getSlaveName() &&
                    port.getBaudRate() == 115200,
                "SerialPort exposes its active configuration" );

        const std::vector<std::uint8_t> outbound { 'W', 'S', 'E', '-', 'T', 'X' };
        const auto sent = port.send( outbound, context() );
        std::vector<std::uint8_t> master_received;
        expect( sent.succeeded() && sent.value() == outbound.size() &&
                    readMaster( &master_received, pseudo_terminal.getMaster(), outbound.size() ) &&
                    master_received == outbound,
                "SerialPort sends a complete payload to the PTY master" );

        const std::vector<std::uint8_t> inbound { 'W', 'S', 'E', '-', 'R', 'X' };
        const ssize_t master_sent = ::write( pseudo_terminal.getMaster(),
                                             inbound.data(), inbound.size() );
        const auto received = port.receive( 64, context() );
        expect( master_sent == static_cast<ssize_t>( inbound.size() ) &&
                    received.succeeded() && received.value() == inbound,
                "SerialPort receives a complete PTY payload" );

        const auto invalid_receive = port.receive( 0, context() );
        expect( !invalid_receive.succeeded() && invalid_receive.error().code() ==
                    wse::xpt::eTransportErrorCode::InvalidArgument,
                "Serial receive rejects a zero-capacity buffer" );

        // Verify bounded receive and cross-thread cancellation without losing the port.
        const auto started = std::chrono::steady_clock::now();
        const auto timed_out = port.receive( 16, context( 50 ) );
        const auto elapsed = std::chrono::steady_clock::now() - started;
        expect( !timed_out.succeeded() && timed_out.error().code() ==
                    wse::xpt::eTransportErrorCode::TimedOut &&
                    elapsed < std::chrono::milliseconds( 500 ) && port.isOpen(),
                "Serial receive timeout is bounded and preserves the port" );

        wse::xpt::CancellationSource cancellation;
        std::thread canceller( [&cancellation]() {
            std::this_thread::sleep_for( std::chrono::milliseconds( 40 ) );
            cancellation.cancel();
        } );
        const auto cancelled = port.receive(
            16, wse::xpt::OperationContext( wse::xpt::Timeout::milliseconds( 1000 ),
                                            cancellation.token() ) );
        canceller.join();
        expect( !cancelled.succeeded() && cancelled.error().code() ==
                    wse::xpt::eTransportErrorCode::Cancelled && port.isOpen(),
                "Cross-thread cancellation interrupts receive and preserves the port" );

        // Verify transactional reopen, move ownership, and idempotent close.
        const std::string original_device = port.getDeviceName();
        const auto failed_reopen = port.open(
            "/dev/wse-phase2c-missing-5a29b4c1", 9600, context() );
        expect( !failed_reopen.succeeded() && failed_reopen.error().code() ==
                    wse::xpt::eTransportErrorCode::OpenFailed && port.isOpen() &&
                    port.getDeviceName() == original_device,
                "Failed serial reopen preserves the existing port" );

        wse::xpt::SerialPort moved( std::move( port ) );
        expect( moved.isOpen() && !port.isOpen(), "SerialPort ownership is movable" );
        moved.close();
        expect( !moved.isOpen() && moved.getDeviceName().empty() &&
                    moved.getBaudRate() == 0,
                "Serial close clears observable configuration" );
        moved.close();
    }
#endif

    return failures == 0 ? 0 : 1;
}
