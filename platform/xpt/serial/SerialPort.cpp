//*****************************************************************************************************************
//!
//! @file    SerialPort.cpp
//! @brief   \~japanese Win32／POSIX APIを用いてPortable Serial Portを実装する.
//! @brief   \~english  Implements the portable serial port with Win32 and POSIX APIs.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-15, 2026   Initialize Windows read timeouts independently of previous port users.
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

#include "xpt/serial/SerialPort.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <new>
#include <utility>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace wse
{
namespace xpt
{
namespace
{

//--------------------------------------------------
// Common Serial operation helpers.
//--------------------------------------------------

using Clock = std::chrono::steady_clock;

enum class eSerialWait : std::uint8_t
{
      Read = 0
    , Write
};

TransportError validationError( const char* const message_in )
{
    return TransportError( eTransportErrorCategory::Validation,
                           eTransportErrorCode::InvalidArgument, message_in );
}

TransportError timeoutError()
{
    return TransportError( eTransportErrorCategory::Timeout,
                           eTransportErrorCode::TimedOut,
                           "The serial operation timed out." );
}

TransportError cancellationError()
{
    return TransportError( eTransportErrorCategory::Cancellation,
                           eTransportErrorCode::Cancelled,
                           "The serial operation was cancelled." );
}

TransportError serialError(
      const eTransportErrorCode code_in
    , const char* const         message_in
    , const std::int64_t        native_code_in
)
{
    const eTransportErrorCategory category =
        code_in == eTransportErrorCode::OpenFailed
            ? eTransportErrorCategory::Connection
            : eTransportErrorCategory::InputOutput;
    return TransportError( category, code_in, message_in, native_code_in );
}

bool isSupportedBaudRate( const std::int32_t baud_rate_in ) noexcept
{
    switch ( baud_rate_in )
    {
        case 1200:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            return true;
        default:
            return false;
    }
}

Clock::time_point operationDeadline( const OperationContext& context_in )
{
    const auto duration = context_in.timeout().duration();
    const Clock::time_point now = Clock::now();
    const auto maximum = std::chrono::duration_cast<std::chrono::milliseconds>(
        ( Clock::time_point::max )() - now );
    return duration >= maximum ? ( Clock::time_point::max )() : now + duration;
}

#if defined( _WIN32 )

//--------------------------------------------------
// Windows Serial Adapter helpers.
//--------------------------------------------------

std::wstring convertDeviceName( const std::string& device_name_in )
{
    if ( device_name_in.empty() )
    {
        return {};
    }

    const int required_size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, device_name_in.data(),
        static_cast<int>( device_name_in.size() ), nullptr, 0 );
    if ( required_size <= 0 )
    {
        return {};
    }

    std::wstring converted( static_cast<std::size_t>( required_size ), L'\0' );
    if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, device_name_in.data(),
                              static_cast<int>( device_name_in.size() ),
                              &converted[ 0 ], required_size ) <= 0 )
    {
        return {};
    }

    if ( converted.size() >= 3 &&
         ( converted[ 0 ] == L'C' || converted[ 0 ] == L'c' ) &&
         ( converted[ 1 ] == L'O' || converted[ 1 ] == L'o' ) &&
         ( converted[ 2 ] == L'M' || converted[ 2 ] == L'm' ) )
    {
        converted.insert( 0, L"\\\\.\\" );
    }
    return converted;
}

class NativeEvent final
{
  private:
    HANDLE m_handle;

  public:
    NativeEvent()
        : m_handle ( CreateEventW( nullptr, TRUE, FALSE, nullptr ) )
    {
    }

    ~NativeEvent()
    {
        if ( this->m_handle != nullptr )
        {
            CloseHandle( this->m_handle );
        }
    }

    NativeEvent( const NativeEvent& ) = delete;
    NativeEvent& operator=( const NativeEvent& ) = delete;

    HANDLE get() const noexcept
    {
        return this->m_handle;
    }
};

TransportError waitForOverlapped(
          OVERLAPPED*             p_overlapped_inout
    , const HANDLE                handle_in
    , const Clock::time_point     deadline_in
    , const CancellationToken&    cancellation_in
)
{
    for ( ;; )
    {
        if ( cancellation_in.isCancellationRequested() )
        {
            CancelIoEx( handle_in, p_overlapped_inout );
            WaitForSingleObject( p_overlapped_inout->hEvent, INFINITE );
            return cancellationError();
        }

        const Clock::time_point now = Clock::now();
        if ( now >= deadline_in )
        {
            CancelIoEx( handle_in, p_overlapped_inout );
            WaitForSingleObject( p_overlapped_inout->hEvent, INFINITE );
            return timeoutError();
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline_in - now );
        const DWORD wait_ms = static_cast<DWORD>( ( std::max )( std::int64_t( 1 ),
            ( std::min )( std::int64_t( 20 ), remaining.count() ) ) );
        const DWORD wait_result = WaitForSingleObject( p_overlapped_inout->hEvent, wait_ms );
        if ( wait_result == WAIT_OBJECT_0 )
        {
            return TransportError();
        }
        if ( wait_result != WAIT_TIMEOUT )
        {
            const DWORD wait_code = GetLastError();
            CancelIoEx( handle_in, p_overlapped_inout );
            WaitForSingleObject( p_overlapped_inout->hEvent, INFINITE );
            return serialError( eTransportErrorCode::Unknown,
                                "Waiting for serial I/O failed.", wait_code );
        }
    }
}

#else

//--------------------------------------------------
// POSIX Serial Adapter helpers.
//--------------------------------------------------

speed_t convertBaudRate( const std::int32_t baud_rate_in ) noexcept
{
    switch ( baud_rate_in )
    {
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return static_cast<speed_t>( 0 );
    }
}

TransportError waitForDescriptor(
      const int                  descriptor_in
    , const eSerialWait          operation_in
    , const Clock::time_point    deadline_in
    , const CancellationToken&   cancellation_in
)
{
    for ( ;; )
    {
        if ( cancellation_in.isCancellationRequested() )
        {
            return cancellationError();
        }

        const Clock::time_point now = Clock::now();
        if ( now >= deadline_in )
        {
            return timeoutError();
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline_in - now );
        const int wait_ms = static_cast<int>( ( std::max )( std::int64_t( 1 ),
            ( std::min )( std::int64_t( 20 ), remaining.count() ) ) );
        pollfd descriptor = {};
        descriptor.fd = descriptor_in;
        descriptor.events = operation_in == eSerialWait::Read ? POLLIN : POLLOUT;
        const int result = poll( &descriptor, 1, wait_ms );
        if ( result > 0 )
        {
            if ( ( descriptor.revents & ( POLLERR | POLLHUP | POLLNVAL ) ) != 0 )
            {
                return serialError( operation_in == eSerialWait::Read
                                        ? eTransportErrorCode::ReceiveFailed
                                        : eTransportErrorCode::SendFailed,
                                    "The serial device was disconnected.", EIO );
            }
            if ( ( descriptor.revents & descriptor.events ) != 0 )
            {
                return TransportError();
            }
            continue;
        }
        if ( result == 0 || errno == EINTR )
        {
            continue;
        }
        return serialError( eTransportErrorCode::Unknown,
                            "Waiting for serial I/O failed.", errno );
    }
}

#endif

} // namespace

//--------------------------------------------------
// Native Serial resource ownership.
//--------------------------------------------------

class SerialPort::Impl
{
    //! @brief Construct all members with explicit defaults.
public:
    Impl()
#if defined( _WIN32 )
        : handle      ( INVALID_HANDLE_VALUE )
#else
        : descriptor  ( -1 )
#endif
        , device_name ( {} )
        , baud_rate   ( 0 )
    {
    }
private:

  public:
#if defined( _WIN32 )
    HANDLE handle;
#else
    int descriptor;
#endif
    std::string device_name;
    std::int32_t baud_rate;

    bool isOpen() const noexcept
    {
#if defined( _WIN32 )
        return this->handle != INVALID_HANDLE_VALUE;
#else
        return this->descriptor >= 0;
#endif
    }

    void close() noexcept
    {
#if defined( _WIN32 )
        if ( this->handle != INVALID_HANDLE_VALUE )
        {
            CancelIoEx( this->handle, nullptr );
            CloseHandle( this->handle );
            this->handle = INVALID_HANDLE_VALUE;
        }
#else
        if ( this->descriptor >= 0 )
        {
            ::close( this->descriptor );
            this->descriptor = -1;
        }
#endif
        this->device_name.clear();
        this->baud_rate = 0;
    }

    ~Impl()
    {
        this->close();
    }
};

//--------------------------------------------------
// Portable SerialPort API.
//--------------------------------------------------

SerialPort::SerialPort()
    : m_impl ( std::make_unique<Impl>() )
{
}

SerialPort::~SerialPort() = default;

SerialPort::SerialPort( SerialPort&& other_inout ) noexcept = default;

SerialPort& SerialPort::operator=( SerialPort&& other_inout ) noexcept = default;

TransportStatus SerialPort::open(
      const std::string&      device_name_in
    , const std::int32_t      baud_rate_in
    , const OperationContext& context_in
)
{
    // Validate the public contract before touching a device.
    if ( this->m_impl == nullptr )
    {
        return TransportStatus::failure( validationError( "The serial port was moved from." ) );
    }
    if ( device_name_in.empty() || device_name_in.size() > static_cast<std::size_t>( INT_MAX ) ||
         device_name_in.find( '\0' ) != std::string::npos || !isSupportedBaudRate( baud_rate_in ) ||
         !context_in.isValid() )
    {
        return TransportStatus::failure( validationError(
            "A device, supported baud rate, and non-negative timeout are required." ) );
    }
    if ( context_in.cancellation().isCancellationRequested() )
    {
        return TransportStatus::failure( cancellationError() );
    }

    const Clock::time_point deadline = operationDeadline( context_in );
    if ( Clock::now() >= deadline )
    {
        return TransportStatus::failure( timeoutError() );
    }

#if defined( _WIN32 )
    // Open and configure a candidate without disturbing the active port.
    const std::wstring native_name = convertDeviceName( device_name_in );
    if ( native_name.empty() )
    {
        return TransportStatus::failure( validationError( "The device name must be valid UTF-8." ) );
    }

    const HANDLE candidate = CreateFileW(
          native_name.c_str()                // Device path.
        , GENERIC_READ | GENERIC_WRITE       // Access mode.
        , 0                                  // Exclusive access.
        , nullptr                            // Security attributes.
        , OPEN_EXISTING                      // Existing device only.
        , FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED // Bounded I/O.
        , nullptr                            // Template file.
    );
    if ( candidate == INVALID_HANDLE_VALUE )
    {
        return TransportStatus::failure( serialError( eTransportErrorCode::OpenFailed,
                                                     "Opening the serial device failed.",
                                                     GetLastError() ) );
    }

    DCB settings = {};
    settings.DCBlength = sizeof( settings );
    if ( GetCommState( candidate, &settings ) == FALSE )
    {
        const DWORD code = GetLastError();
        CloseHandle( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Reading serial settings failed.", code ) );
    }
    settings.BaudRate = static_cast<DWORD>( baud_rate_in );
    settings.ByteSize = 8;
    settings.Parity = NOPARITY;
    settings.StopBits = ONESTOPBIT;
    settings.fBinary = TRUE;
    settings.fParity = FALSE;
    settings.fOutxCtsFlow = FALSE;
    settings.fOutxDsrFlow = FALSE;
    settings.fDsrSensitivity = FALSE;
    settings.fOutX = FALSE;
    settings.fInX = FALSE;
    settings.fRtsControl = RTS_CONTROL_DISABLE;
    settings.fDtrControl = DTR_CONTROL_ENABLE;
    if ( SetCommState( candidate, &settings ) == FALSE ||
         SetupComm( candidate, 4096, 4096 ) == FALSE )
    {
        const DWORD code = GetLastError();
        CloseHandle( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Configuring the serial device failed.", code ) );
    }
    // Return buffered bytes or the first arriving byte, not a full requested buffer.
    // Driver defaults (or a previous user's settings) can otherwise hold short replies
    // until our operation deadline cancels the read and discards the received bytes.
    // A bounded native wait avoids spinning when idle; OperationContext owns the deadline.
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 20;
    if ( SetCommTimeouts( candidate, &timeouts ) == FALSE )
    {
        const DWORD code = GetLastError();
        CloseHandle( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Configuring serial timeouts failed.", code ) );
    }
    if ( PurgeComm( candidate, PURGE_RXABORT | PURGE_RXCLEAR |
                               PURGE_TXABORT | PURGE_TXCLEAR ) == FALSE )
    {
        const DWORD code = GetLastError();
        CloseHandle( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Clearing serial buffers failed.", code ) );
    }

    if ( context_in.cancellation().isCancellationRequested() || Clock::now() >= deadline )
    {
        CloseHandle( candidate );
        return TransportStatus::failure(
            context_in.cancellation().isCancellationRequested()
                ? cancellationError() : timeoutError() );
    }

    // Commit the candidate only after the complete configuration succeeds.
    this->m_impl->close();
    this->m_impl->handle = candidate;
#else
    // Open and configure a candidate without disturbing the active port.
    const int candidate = ::open( device_name_in.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC );
    if ( candidate < 0 )
    {
        return TransportStatus::failure( serialError( eTransportErrorCode::OpenFailed,
                                                     "Opening the serial device failed.", errno ) );
    }

    termios settings = {};
    if ( tcgetattr( candidate, &settings ) != 0 )
    {
        const int code = errno;
        ::close( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Reading serial settings failed.", code ) );
    }
    settings.c_iflag &= static_cast<tcflag_t>( ~( IGNBRK | BRKINT | PARMRK | ISTRIP |
                                                  INLCR | IGNCR | ICRNL | IXON ) );
    settings.c_oflag &= static_cast<tcflag_t>( ~OPOST );
    settings.c_lflag &= static_cast<tcflag_t>( ~( ECHO | ECHONL | ICANON | ISIG | IEXTEN ) );
    settings.c_cflag &= static_cast<tcflag_t>( ~( CSIZE | PARENB | CSTOPB | CRTSCTS ) );
    settings.c_cflag |= static_cast<tcflag_t>( CS8 | CLOCAL | CREAD );
    settings.c_cc[ VMIN ] = 0;
    settings.c_cc[ VTIME ] = 0;
    const speed_t native_baud_rate = convertBaudRate( baud_rate_in );
    if ( cfsetispeed( &settings, native_baud_rate ) != 0 ||
         cfsetospeed( &settings, native_baud_rate ) != 0 ||
         tcsetattr( candidate, TCSANOW, &settings ) != 0 )
    {
        const int code = errno;
        ::close( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Configuring the serial device failed.", code ) );
    }
    if ( tcflush( candidate, TCIOFLUSH ) != 0 )
    {
        const int code = errno;
        ::close( candidate );
        return TransportStatus::failure( serialError( eTransportErrorCode::ConfigurationFailed,
                                                     "Clearing serial buffers failed.", code ) );
    }

    if ( context_in.cancellation().isCancellationRequested() || Clock::now() >= deadline )
    {
        ::close( candidate );
        return TransportStatus::failure(
            context_in.cancellation().isCancellationRequested()
                ? cancellationError() : timeoutError() );
    }

    // Commit the candidate only after the complete configuration succeeds.
    this->m_impl->close();
    this->m_impl->descriptor = candidate;
#endif
    this->m_impl->device_name = device_name_in;
    this->m_impl->baud_rate = baud_rate_in;
    return TransportStatus::success();
}

void SerialPort::close() noexcept
{
    if ( this->m_impl != nullptr )
    {
        this->m_impl->close();
    }
}

bool SerialPort::isOpen() const noexcept
{
    return this->m_impl != nullptr && this->m_impl->isOpen();
}

std::string SerialPort::getDeviceName() const
{
    return this->m_impl == nullptr ? std::string() : this->m_impl->device_name;
}

std::int32_t SerialPort::getBaudRate() const noexcept
{
    return this->m_impl == nullptr ? 0 : this->m_impl->baud_rate;
}

TransferResult<std::size_t> SerialPort::send(
      const std::uint8_t*     data_in
    , const std::size_t       size_in
    , const OperationContext& context_in
)
{
    // Validate state and input before starting a bounded operation.
    if ( this->m_impl == nullptr || !this->isOpen() )
    {
        return TransferResult<std::size_t>(
            0, TransportError( eTransportErrorCategory::Connection,
                               eTransportErrorCode::NotConnected,
                               "The serial port is not open." ) );
    }
    if ( ( data_in == nullptr && size_in != 0 ) || size_in > static_cast<std::size_t>( INT_MAX ) ||
         !context_in.isValid() )
    {
        return TransferResult<std::size_t>(
            0, validationError( "Send data, size, and timeout must be valid." ) );
    }
    if ( size_in == 0 )
    {
        return TransferResult<std::size_t>( 0 );
    }

    // Preserve partial progress while sending the complete buffer.
    const Clock::time_point deadline = operationDeadline( context_in );
    std::size_t sent_total = 0;
    while ( sent_total < size_in )
    {
        if ( context_in.cancellation().isCancellationRequested() )
        {
            return TransferResult<std::size_t>( sent_total, cancellationError() );
        }
        if ( Clock::now() >= deadline )
        {
            return TransferResult<std::size_t>( sent_total, timeoutError() );
        }
#if defined( _WIN32 )
        NativeEvent event;
        if ( event.get() == nullptr )
        {
            return TransferResult<std::size_t>( sent_total,
                serialError( eTransportErrorCode::ResourceExhausted,
                             "Creating a serial I/O event failed.", GetLastError() ) );
        }
        OVERLAPPED operation = {};
        operation.hEvent = event.get();
        DWORD sent = 0;
        const DWORD requested = static_cast<DWORD>( size_in - sent_total );
        const BOOL started = WriteFile( this->m_impl->handle, data_in + sent_total,
                                        requested, &sent, &operation );
        if ( started == FALSE )
        {
            const DWORD code = GetLastError();
            if ( code != ERROR_IO_PENDING )
            {
                this->m_impl->close();
                return TransferResult<std::size_t>( sent_total,
                    serialError( eTransportErrorCode::SendFailed,
                                 "Sending serial data failed.", code ) );
            }
            const TransportError wait_error = waitForOverlapped(
                &operation, this->m_impl->handle, deadline, context_in.cancellation() );
            if ( !wait_error.ok() )
            {
                return TransferResult<std::size_t>( sent_total, wait_error );
            }
            if ( GetOverlappedResult( this->m_impl->handle, &operation, &sent, FALSE ) == FALSE )
            {
                const DWORD result_code = GetLastError();
                this->m_impl->close();
                return TransferResult<std::size_t>( sent_total,
                    serialError( eTransportErrorCode::SendFailed,
                                 "Completing serial send failed.", result_code ) );
            }
        }
        sent_total += static_cast<std::size_t>( sent );
#else
        const TransportError wait_error = waitForDescriptor(
            this->m_impl->descriptor, eSerialWait::Write, deadline,
            context_in.cancellation() );
        if ( !wait_error.ok() )
        {
            if ( wait_error.category() == eTransportErrorCategory::InputOutput )
            {
                this->m_impl->close();
            }
            return TransferResult<std::size_t>( sent_total, wait_error );
        }
        const ssize_t sent = ::write( this->m_impl->descriptor, data_in + sent_total,
                                      size_in - sent_total );
        if ( sent < 0 )
        {
            if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR )
            {
                continue;
            }
            const int code = errno;
            this->m_impl->close();
            return TransferResult<std::size_t>( sent_total,
                serialError( eTransportErrorCode::SendFailed,
                             "Sending serial data failed.", code ) );
        }
        sent_total += static_cast<std::size_t>( sent );
#endif
    }
    return TransferResult<std::size_t>( sent_total );
}

TransferResult<std::size_t> SerialPort::send(
      const std::vector<std::uint8_t>& data_in
    , const OperationContext&          context_in
)
{
    return this->send( data_in.data(), data_in.size(), context_in );
}

TransferResult<std::vector<std::uint8_t>> SerialPort::receive(
      const std::size_t       maximum_size_in
    , const OperationContext& context_in
)
{
    // Validate state and allocate the complete caller-requested capacity.
    if ( this->m_impl == nullptr || !this->isOpen() )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, TransportError( eTransportErrorCategory::Connection,
                                eTransportErrorCode::NotConnected,
                                "The serial port is not open." ) );
    }
    if ( maximum_size_in == 0 || maximum_size_in > static_cast<std::size_t>( INT_MAX ) ||
         !context_in.isValid() )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, validationError( "Receive size and timeout must be valid." ) );
    }

    std::vector<std::uint8_t> received;
    try
    {
        received.resize( maximum_size_in );
    }
    catch ( const std::bad_alloc& )
    {
        return TransferResult<std::vector<std::uint8_t>>(
            {}, serialError( eTransportErrorCode::ResourceExhausted,
                             "Allocating the serial receive buffer failed.", 0 ) );
    }

    // Return the first non-empty read while preserving the port on timeout or cancellation.
    const Clock::time_point deadline = operationDeadline( context_in );
    for ( ;; )
    {
        if ( context_in.cancellation().isCancellationRequested() )
        {
            return TransferResult<std::vector<std::uint8_t>>( {}, cancellationError() );
        }
        if ( Clock::now() >= deadline )
        {
            return TransferResult<std::vector<std::uint8_t>>( {}, timeoutError() );
        }
#if defined( _WIN32 )
        NativeEvent event;
        if ( event.get() == nullptr )
        {
            return TransferResult<std::vector<std::uint8_t>>(
                {}, serialError( eTransportErrorCode::ResourceExhausted,
                                 "Creating a serial I/O event failed.", GetLastError() ) );
        }
        OVERLAPPED operation = {};
        operation.hEvent = event.get();
        DWORD received_size = 0;
        const BOOL started = ReadFile( this->m_impl->handle, received.data(),
                                       static_cast<DWORD>( maximum_size_in ),
                                       &received_size, &operation );
        if ( started == FALSE )
        {
            const DWORD code = GetLastError();
            if ( code != ERROR_IO_PENDING )
            {
                this->m_impl->close();
                return TransferResult<std::vector<std::uint8_t>>(
                    {}, serialError( eTransportErrorCode::ReceiveFailed,
                                     "Receiving serial data failed.", code ) );
            }
            const TransportError wait_error = waitForOverlapped(
                &operation, this->m_impl->handle, deadline, context_in.cancellation() );
            if ( !wait_error.ok() )
            {
                return TransferResult<std::vector<std::uint8_t>>( {}, wait_error );
            }
            if ( GetOverlappedResult( this->m_impl->handle, &operation,
                                      &received_size, FALSE ) == FALSE )
            {
                const DWORD result_code = GetLastError();
                this->m_impl->close();
                return TransferResult<std::vector<std::uint8_t>>(
                    {}, serialError( eTransportErrorCode::ReceiveFailed,
                                     "Completing serial receive failed.", result_code ) );
            }
        }
        if ( received_size == 0 )
        {
            continue;
        }
        received.resize( static_cast<std::size_t>( received_size ) );
#else
        const TransportError wait_error = waitForDescriptor(
            this->m_impl->descriptor, eSerialWait::Read, deadline,
            context_in.cancellation() );
        if ( !wait_error.ok() )
        {
            if ( wait_error.category() == eTransportErrorCategory::InputOutput )
            {
                this->m_impl->close();
            }
            return TransferResult<std::vector<std::uint8_t>>( {}, wait_error );
        }
        const ssize_t received_size = ::read( this->m_impl->descriptor,
                                              received.data(), maximum_size_in );
        if ( received_size < 0 )
        {
            if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR )
            {
                continue;
            }
            const int code = errno;
            this->m_impl->close();
            return TransferResult<std::vector<std::uint8_t>>(
                {}, serialError( eTransportErrorCode::ReceiveFailed,
                                 "Receiving serial data failed.", code ) );
        }
        if ( received_size == 0 )
        {
            continue;
        }
        received.resize( static_cast<std::size_t>( received_size ) );
#endif
        return TransferResult<std::vector<std::uint8_t>>( std::move( received ) );
    }
}

} // namespace xpt
} // namespace wse
