// Windows serial adapter regression: no physical port is opened.
// Compile a renamed private copy against Win32 I/O doubles, retaining the real
// deadline, cancellation, configuration, receive, and ownership implementation.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <xpt/error/TransportError.h>
#include <xpt/operation/OperationContext.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace
{
COMMTIMEOUTS configured = {};
DCB configured_state = {};
bool reject_state = false;
int state_reads = 0;
bool reject_readback = false;
bool round_readback = false;
bool reject_timeouts = false;
bool have_reply = true;

HANDLE WINAPI openPort( LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE )
{
    state_reads = 0;
    return CreateEventW( nullptr, TRUE, FALSE, nullptr );
}
BOOL WINAPI getState( HANDLE, LPDCB state_out )
{
    if ( ++state_reads == 2 && reject_readback )
    {
        SetLastError( ERROR_ACCESS_DENIED );
        return FALSE;
    }
    *state_out = configured_state;
    if ( state_reads == 2 && round_readback )
    {
        state_out->BaudRate = 9600;
    }
    return TRUE;
}
BOOL WINAPI setState( HANDLE, LPDCB p_state_in )
{
    if ( reject_state )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    configured_state = *p_state_in;
    return TRUE;
}
BOOL WINAPI setup( HANDLE, DWORD, DWORD ) { return TRUE; }
BOOL WINAPI purge( HANDLE, DWORD ) { return TRUE; }
BOOL WINAPI setTimeouts( HANDLE, LPCOMMTIMEOUTS timeouts_in )
{
    if ( reject_timeouts )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    configured = *timeouts_in;
    return TRUE;
}
BOOL WINAPI readPort( HANDLE, LPVOID data_out, DWORD size_in, LPDWORD count_out,
                      LPOVERLAPPED operation_inout )
{
    // Model documented first-byte completion; all-zero defaults wait for size_in.
    const bool first_byte = configured.ReadIntervalTimeout == MAXDWORD &&
        configured.ReadTotalTimeoutMultiplier == MAXDWORD &&
        configured.ReadTotalTimeoutConstant > 0 &&
        configured.ReadTotalTimeoutConstant < MAXDWORD;
    if ( first_byte && have_reply && size_in >= 3 )
    {
        const unsigned char reply[] = { 0x12, 0x00, 0x34 };
        std::memcpy( data_out, reply, sizeof( reply ) );
        *count_out = sizeof( reply );
        have_reply = false;
        return TRUE;
    }
    *count_out = 0;
    ResetEvent( operation_inout->hEvent );
    SetLastError( ERROR_IO_PENDING );
    return FALSE;
}
BOOL WINAPI cancelRead( HANDLE, LPOVERLAPPED operation_inout )
{
    return operation_inout == nullptr ? TRUE : SetEvent( operation_inout->hEvent );
}
}

// Dependencies retain their normal DLL imports. Only the isolated class is renamed.
#undef WSE_API
#define WSE_API
#define SerialPort WindowsSerialUnderTest
#define CreateFileW openPort
#define GetCommState getState
#define SetCommState setState
#define SetupComm setup
#define PurgeComm purge
#define SetCommTimeouts setTimeouts
#define ReadFile readPort
#define CancelIoEx cancelRead
#include "../../platform/xpt/serial/SerialPort.cpp"
#undef SerialPort
#undef CreateFileW
#undef GetCommState
#undef SetCommState
#undef SetupComm
#undef PurgeComm
#undef SetCommTimeouts
#undef ReadFile
#undef CancelIoEx

int main()
{
    using namespace wse::xpt;
    int failures = 0;
    const auto expect = [&failures]( const bool ok_in, const char* message_in )
    {
        if ( !ok_in )
        {
            std::cerr << message_in << '\n';
            ++failures;
        }
    };
    const OperationContext context( Timeout::milliseconds( 60 ) );
    const std::int32_t baud_rates[] = {
        110, 300, 600, 1200, 2400, 4800, 9600, 14400,
        19200, 38400, 57600, 115200, 230400, 460800, 921600,
        1, 12345, 250000, 1000000, 2147483647
    };
    WindowsSerialUnderTest rate_port;
    for ( const auto baud_rate : baud_rates )
    {
        expect( rate_port.open( "virtual-port", baud_rate, context ).succeeded() &&
                    rate_port.getBaudRate() == baud_rate &&
                    configured_state.BaudRate == static_cast<DWORD>( baud_rate ) &&
                    configured_state.ByteSize == 8 && configured_state.Parity == NOPARITY &&
                    configured_state.StopBits == ONESTOPBIT && !configured_state.fOutxCtsFlow &&
                    !configured_state.fOutxDsrFlow && !configured_state.fOutX && !configured_state.fInX,
                "every rate reaches the Windows adapter exactly with fixed 8N1 framing" );
        reject_state = true;
        const auto rejected = rate_port.open( "rejected-port", baud_rate, context );
        expect( !rejected.succeeded() && rejected.error().code() == eTransportErrorCode::ConfigurationFailed &&
                    rejected.error().nativeCode() == ERROR_INVALID_PARAMETER &&
                    rate_port.isOpen() && rate_port.getDeviceName() == "virtual-port" &&
                    rate_port.getBaudRate() == baud_rate,
                "driver rejection preserves the active port and reports the native error" );
        reject_state = false;
    }
    const auto unsupported = rate_port.open( "virtual-port", 0, context );
    expect( !unsupported.succeeded() && unsupported.error().code() == eTransportErrorCode::InvalidArgument &&
                rate_port.getBaudRate() == 2147483647,
            "unsupported rate preserves the active port" );
    round_readback = true;
    const auto rounded = rate_port.open( "rounded-port", 12345, context );
    expect( !rounded.succeeded() && rounded.error().code() == eTransportErrorCode::ConfigurationFailed &&
                rounded.error().nativeCode() == 0 && rate_port.getDeviceName() == "virtual-port" &&
                rate_port.getBaudRate() == 2147483647,
            "rounded readback fails without a stale native error or owner replacement" );
    round_readback = false;
    reject_readback = true;
    const auto unreadable = rate_port.open( "unreadable-port", 12345, context );
    expect( !unreadable.succeeded() && unreadable.error().code() == eTransportErrorCode::ConfigurationFailed &&
                unreadable.error().nativeCode() == ERROR_ACCESS_DENIED &&
                rate_port.getDeviceName() == "virtual-port" && rate_port.getBaudRate() == 2147483647,
            "readback failure preserves the port and reports the native error" );
    reject_readback = false;
    WindowsSerialUnderTest port;
    expect( port.open( "test-port", 9600, context ).succeeded(), "open succeeds" );
    const auto received = port.receive( 512, context );
    expect( received.succeeded() && received.value() == std::vector<std::uint8_t>({ 0x12, 0x00, 0x34 }),
            "a short reply is returned with driver-default timeouts at entry" );
    const auto start = std::chrono::steady_clock::now();
    const auto empty = port.receive( 512, context );
    expect( !empty.succeeded() && empty.error().category() == eTransportErrorCategory::Timeout,
            "idle receive times out" );
    expect( std::chrono::steady_clock::now() - start < std::chrono::seconds( 1 ),
            "idle wait is bounded" );
    expect( port.isOpen(), "timeout preserves the port" );
    reject_timeouts = true;
    const auto failed_open = port.open( "replacement-port", 9600, context );
    expect( !failed_open.succeeded() &&
            failed_open.error().code() == eTransportErrorCode::ConfigurationFailed &&
            failed_open.error().nativeCode() == ERROR_INVALID_PARAMETER,
            "timeout configuration failure is observable" );
    expect( port.isOpen() && port.getDeviceName() == "test-port",
            "failed configuration preserves the existing owner" );
    return failures == 0 ? 0 : 1;
}
