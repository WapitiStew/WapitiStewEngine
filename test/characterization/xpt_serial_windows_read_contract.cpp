// Windows serial adapter regression: no physical port is opened.
// Compile a renamed private copy against Win32 I/O doubles, retaining the real
// deadline, cancellation, configuration, receive, and ownership implementation.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <xpt/error/TransportError.h>
#include <xpt/operation/OperationContext.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

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
struct sPendingOperation
{
    unsigned char* p_read_buffer;
    const unsigned char* p_write_buffer;
    DWORD requested;
    DWORD transferred;
    bool is_complete;
    bool is_aborted;
};
std::map< LPOVERLAPPED, sPendingOperation > pending;
LPOVERLAPPED p_pending_read = nullptr;
wse::xpt::CancellationSource* p_cancel_on_write = nullptr;
bool complete_write_on_cancel = false;
DWORD cancelled_write_progress = 0;
DWORD maximum_write_chunk = MAXDWORD;
std::vector< unsigned char > written;
unsigned int reads_started = 0U;
unsigned int writes_started = 0U;

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
    ++reads_started;
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
    pending[ operation_inout ] = { static_cast< unsigned char* >( data_out ), nullptr,
                                  size_in, 0U, false, false };
    p_pending_read = operation_inout;
    ResetEvent( operation_inout->hEvent );
    SetLastError( ERROR_IO_PENDING );
    return FALSE;
}
BOOL WINAPI cancelPortOperation( HANDLE, LPOVERLAPPED operation_inout )
{
    const auto found = pending.find( operation_inout );
    if ( found == pending.end() )
    {
        SetLastError( ERROR_NOT_FOUND );
        return FALSE;
    }
    auto& operation = found->second;
    if ( operation.p_write_buffer != nullptr )
    {
        operation.transferred = complete_write_on_cancel ? operation.requested :
            ( std::min )( cancelled_write_progress, operation.requested );
        written.insert( written.end(), operation.p_write_buffer,
                        operation.p_write_buffer + operation.transferred );
    }
    operation.is_complete = true;
    operation.is_aborted = operation.p_write_buffer == nullptr || !complete_write_on_cancel;
    return SetEvent( operation_inout->hEvent );
}

BOOL WINAPI collectOperation( HANDLE, LPOVERLAPPED operation_inout,
                              LPDWORD count_out, BOOL )
{
    const auto found = pending.find( operation_inout );
    if ( found == pending.end() )
    {
        *count_out = 0;
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if ( !found->second.is_complete )
    {
        *count_out = 0;
        SetLastError( ERROR_IO_INCOMPLETE );
        return FALSE;
    }
    *count_out = found->second.transferred;
    const bool is_aborted = found->second.is_aborted;
    pending.erase( found );
    if ( p_pending_read == operation_inout )
    {
        p_pending_read = nullptr;
    }
    SetLastError( is_aborted ? ERROR_OPERATION_ABORTED : ERROR_SUCCESS );
    return is_aborted ? FALSE : TRUE;
}

BOOL WINAPI writePort( HANDLE, LPCVOID data_in, DWORD size_in, LPDWORD count_out,
                       LPOVERLAPPED operation_inout )
{
    ++writes_started;
    const auto* p_data = static_cast< const unsigned char* >( data_in );
    if ( p_cancel_on_write == nullptr )
    {
        *count_out = ( std::min )( size_in, maximum_write_chunk );
        written.insert( written.end(), p_data, p_data + *count_out );
        return TRUE;
    }
    *count_out = 0;
    pending[ operation_inout ] = { nullptr, p_data, size_in, 0U, false, false };
    ResetEvent( operation_inout->hEvent );
    p_cancel_on_write->cancel();
    SetLastError( ERROR_IO_PENDING );
    return FALSE;
}

void completeRead( const std::vector< unsigned char >& bytes_in )
{
    auto& operation = pending.at( p_pending_read );
    operation.transferred = ( std::min )( static_cast< DWORD >( bytes_in.size() ), operation.requested );
    std::memcpy( operation.p_read_buffer, bytes_in.data(), operation.transferred );
    operation.is_complete = true;
    SetEvent( p_pending_read->hEvent );
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
#define WriteFile writePort
#define GetOverlappedResult collectOperation
#define CancelIoEx cancelPortOperation
#include "../../platform/xpt/serial/SerialPort.cpp"
#undef SerialPort
#undef CreateFileW
#undef GetCommState
#undef SetCommState
#undef SetupComm
#undef PurgeComm
#undef SetCommTimeouts
#undef ReadFile
#undef WriteFile
#undef GetOverlappedResult
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
    const auto previous_reads = reads_started;
    completeRead( { 'A', 'B', 'C', 'D' } );
    const OperationContext no_wait( Timeout::milliseconds( 0 ) );
    expect( port.receive( 1, no_wait ).value() == std::vector<std::uint8_t>( { 'A' } ) &&
                port.receive( 2, no_wait ).value() == std::vector<std::uint8_t>( { 'B', 'C' } ) &&
                port.receive( 1, no_wait ).value() == std::vector<std::uint8_t>( { 'D' } ) &&
                reads_started == previous_reads,
            "completion wins an expired poll and smaller capacities retain every suffix once" );
    expect( !port.receive( 256, OperationContext( Timeout::milliseconds( 1 ) ) ).succeeded(),
            "a new pending read can outlive its poll" );
    WindowsSerialUnderTest moved( std::move( port ) );
    completeRead( { 'M' } );
    expect( !port.isOpen() && moved.receive( 1, no_wait ).value() == std::vector<std::uint8_t>( { 'M' } ),
            "moving the owner preserves pending I/O storage" );
    port = std::move( moved );

    const std::vector<std::uint8_t> payload{ 1U, 2U, 3U, 4U, 5U };
    written.clear();
    maximum_write_chunk = 2U;
    const auto sent = port.send( payload, context );
    expect( sent.succeeded() && sent.value() == payload.size() && written == payload,
            "partial native writes advance the buffer without loss or duplication" );
    maximum_write_chunk = MAXDWORD;
    CancellationSource cancelled_write;
    p_cancel_on_write = &cancelled_write;
    cancelled_write_progress = 2U;
    written.clear();
    const auto partial = port.send( payload, OperationContext( Timeout::milliseconds( 60 ), cancelled_write.token() ) );
    p_cancel_on_write = nullptr;
    expect( !partial.succeeded() && partial.error().code() == eTransportErrorCode::Cancelled &&
                partial.value() == 2U && written == std::vector<std::uint8_t>( { 1U, 2U } ) && port.isOpen(),
            "cancelled pending write progress is collected exactly once" );
    CancellationSource completed_write;
    p_cancel_on_write = &completed_write;
    complete_write_on_cancel = true;
    written.clear();
    const auto completed = port.send( payload, OperationContext( Timeout::milliseconds( 60 ), completed_write.token() ) );
    p_cancel_on_write = nullptr;
    complete_write_on_cancel = false;
    expect( completed.succeeded() && completed.value() == payload.size() && written == payload,
            "a fully completed write racing cancellation is not reported as unsent" );
    reject_timeouts = true;
    const auto failed_open = port.open( "replacement-port", 9600, context );
    expect( !failed_open.succeeded() &&
            failed_open.error().code() == eTransportErrorCode::ConfigurationFailed &&
            failed_open.error().nativeCode() == ERROR_INVALID_PARAMETER,
            "timeout configuration failure is observable" );
    expect( port.isOpen() && port.getDeviceName() == "test-port",
            "failed configuration preserves the existing owner" );
    reject_timeouts = false;
    {
        WindowsSerialUnderTest closing;
        expect( closing.open( "closing-port", 9600, context ).succeeded(), "a closing owner opens" );
        expect( !closing.receive( 256, OperationContext( Timeout::milliseconds( 1 ) ) ).succeeded(),
                "close fixture owns a pending read" );
    }
    expect( pending.empty(), "destruction cancels and collects pending reads before storage release" );
    return failures == 0 ? 0 : 1;
}
