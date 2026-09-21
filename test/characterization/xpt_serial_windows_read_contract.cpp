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
bool reject_timeouts = false;
bool have_reply = true;

HANDLE WINAPI openPort( LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE )
{
    return CreateEventW( nullptr, TRUE, FALSE, nullptr );
}
BOOL WINAPI getState( HANDLE, LPDCB state_out )
{
    *state_out = {};
    return TRUE;
}
BOOL WINAPI setState( HANDLE, LPDCB ) { return TRUE; }
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
