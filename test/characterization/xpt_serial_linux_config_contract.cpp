// Linux serial configuration failures: real PTYs, intercepted ioctl readback.
// No physical device is opened; the real adapter and owner implementation run.
#include <xpt/error/TransportError.h>
#include <xpt/operation/OperationContext.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
enum class eReadbackMode { Exact, InputMismatch, OutputMismatch, ReadFailure, SetFailure };
eReadbackMode mode = eReadbackMode::Exact;
int state_reads = 0;
int candidate_descriptor = -1;
}

int serialTestIoctl( const int descriptor_in, const unsigned long request_in, termios2* p_settings_inout )
{
    termios2& settings_inout = *p_settings_inout;
    if ( request_in == TCGETS2 )
    {
        candidate_descriptor = descriptor_in;
        if ( ++state_reads == 2 && mode == eReadbackMode::ReadFailure )
        {
            errno = EIO;
            return -1;
        }
    }
    if ( request_in == TCSETS2 && mode == eReadbackMode::SetFailure )
    {
        errno = EINVAL;
        return -1;
    }
    const int result = ::ioctl( descriptor_in, request_in, &settings_inout );
    if ( result == 0 && request_in == TCGETS2 && state_reads == 2 )
    {
        if ( mode == eReadbackMode::InputMismatch ) settings_inout.c_ispeed = 9600;
        if ( mode == eReadbackMode::OutputMismatch ) settings_inout.c_ospeed = 9600;
    }
    return result;
}

int serialTestIoctl( const int descriptor_in, const unsigned long request_in, const int argument_in )
{
    return ::ioctl( descriptor_in, request_in, argument_in );
}

#undef WSE_API
#define WSE_API
#define SerialPort LinuxSerialUnderTest
#define ioctl serialTestIoctl
#include "../../platform/xpt/serial/SerialPort.cpp"
#undef SerialPort
#undef ioctl

namespace
{
class Terminal final
{
public:
    int master = -1;
    std::string slave;

    Terminal()
    {
        master = posix_openpt( O_RDWR | O_NOCTTY | O_NONBLOCK );
        if ( master >= 0 && grantpt( master ) == 0 && unlockpt( master ) == 0 )
        {
            const char* const p_name = ptsname( master );
            if ( p_name != nullptr ) slave = p_name;
        }
    }
    ~Terminal() { if ( master >= 0 ) ::close( master ); }
    Terminal( const Terminal& ) = delete;
    Terminal& operator=( const Terminal& ) = delete;
};
}

int main()
{
    using namespace wse::xpt;
    int failures = 0;
    const auto expect = [&failures]( const bool ok_in, const char* const message_in )
    {
        if ( !ok_in ) { std::cerr << message_in << '\n'; ++failures; }
    };
    Terminal active;
    Terminal replacement;
    if ( active.slave.empty() || replacement.slave.empty() ) return 1;
    const OperationContext context( Timeout::milliseconds( 1000 ) );
    LinuxSerialUnderTest port;
    expect( port.open( active.slave, 250000, context ).succeeded(), "arbitrary rate opens exactly" );
    for ( const auto failure_mode : { eReadbackMode::InputMismatch, eReadbackMode::OutputMismatch,
                                     eReadbackMode::ReadFailure, eReadbackMode::SetFailure } )
    {
        for ( const bool initially_open : { false, true } )
        {
            LinuxSerialUnderTest closed_port;
            LinuxSerialUnderTest& target = initially_open ? port : closed_port;
            mode = failure_mode;
            state_reads = 0;
            errno = EBUSY; // A mismatch must not report an unrelated stale errno.
            const auto result = target.open( replacement.slave, 12345, context );
            const int expected_code = mode == eReadbackMode::ReadFailure ? EIO :
                ( mode == eReadbackMode::SetFailure ? EINVAL : 0 );
            expect( !result.succeeded() && result.error().code() == eTransportErrorCode::ConfigurationFailed &&
                        result.error().nativeCode() == expected_code,
                    "configuration rejection/readback failure/mismatch reports the expected error" );
            expect( ::fcntl( candidate_descriptor, F_GETFD ) == -1 && errno == EBADF,
                    "failed candidate descriptor is closed" );
            expect( target.isOpen() == initially_open &&
                        target.getBaudRate() == ( initially_open ? 250000 : 0 ) &&
                        target.getDeviceName() == ( initially_open ? active.slave : std::string() ),
                    "configuration failure preserves the prior owner state" );
            const unsigned char byte = 0x5a;
            expect( ::write( active.master, &byte, 1 ) == 1, "active peer remains writable" );
            const auto received = port.receive( 1, context );
            expect( received.succeeded() && received.value() == std::vector<std::uint8_t>({ byte }),
                    "active port remains usable after failed replacement" );
        }
    }
    mode = eReadbackMode::Exact;
    state_reads = 0;
    expect( port.open( replacement.slave, 12345, context ).succeeded() &&
                port.getBaudRate() == 12345 && port.getDeviceName() == replacement.slave,
            "exact arbitrary readback commits the replacement" );
    return failures == 0 ? 0 : 1;
}
