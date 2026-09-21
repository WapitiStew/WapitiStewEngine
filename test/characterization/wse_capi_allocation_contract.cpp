// Fault injection into the production common/Core C bridge, without shipping test hooks.
#include "../../lang/cs/native/capi_internal.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

namespace
{
thread_local long fail_after = -1;
thread_local long attempts = 0;

void* allocate( std::size_t size_in )
{
    if ( fail_after >= 0 && attempts++ >= fail_after ) { throw std::bad_alloc(); }
    if ( void* allocation = std::malloc( size_in == 0U ? 1U : size_in ) ) { return allocation; }
    throw std::bad_alloc();
}

void require( bool condition_in, const char* message_in )
{
    if ( !condition_in ) { throw std::runtime_error( message_in ); }
}

struct FailureScope
{
    explicit FailureScope( long index_in ) { attempts = 0; fail_after = index_in; }
    ~FailureScope() { fail_after = -1; }
};

template <typename Handle, typename Create, typename Destroy>
void sweep( Create create_in, Destroy destroy_in )
{
    unsigned failures = 0;
    for ( long index = 0; index != 128; ++index )
    {
        // A real non-null sentinel verifies that failure does not publish an owner.
        int sentinel = 0;
        Handle handle = reinterpret_cast<Handle>( &sentinel );
        wse_capi_status status;
        {
            FailureScope failing( index );
            status = create_in( &handle );
        }
        if ( status.category == WSE_CAPI_ERROR_NONE )
        {
            require( handle != reinterpret_cast<Handle>( &sentinel ) && handle != nullptr,
                "success must publish an owner" );
            destroy_in( handle );
            require( failures != 0, "the injector must reach at least one allocation" );
            std::cout << "allocation sweep: " << failures << " failure positions\n";
            return;
        }
        require( status.category == WSE_CAPI_ERROR_RESOURCE_EXHAUSTED,
            "allocation failure must become ResourceExhausted" );
        require( handle == reinterpret_cast<Handle>( &sentinel ), "failure changed owner output" );
        require( std::strlen( wse_capi_last_error_message() ) != 0U, "failure lost diagnostic" );
        ++failures;
    }
    throw std::runtime_error( "allocation sweep did not reach success" );
}
}

void* operator new( std::size_t size_in ) { return allocate( size_in ); }
void* operator new[]( std::size_t size_in ) { return allocate( size_in ); }
void operator delete( void* pointer_in ) noexcept { std::free( pointer_in ); }
void operator delete[]( void* pointer_in ) noexcept { std::free( pointer_in ); }
void operator delete( void* pointer_in, std::size_t ) noexcept { std::free( pointer_in ); }
void operator delete[]( void* pointer_in, std::size_t ) noexcept { std::free( pointer_in ); }

int main()
{
    std::cout << std::unitbuf;
    try
    {
        const std::string long_message( 2048U, 'x' );
        const std::runtime_error exception( long_message );
        wse_capi_status status;
        long diagnostic_allocations = 0;
        {
            FailureScope failing( 0 );
            status = wse::capi::guard( [&exception]() -> wse_capi_status { throw exception; } );
            diagnostic_allocations = attempts;
        }
        require( status.category == WSE_CAPI_ERROR_INTERNAL && status.code == 0,
            "standard exception mapping" );
        require( diagnostic_allocations == 0 && std::strlen( wse_capi_last_error_message() ) == 1023U,
            "bounded diagnostic must not allocate" );
        {
            FailureScope failing( 0 );
            status = wse::capi::guard( []() -> wse_capi_status { throw 42; } );
            diagnostic_allocations = attempts;
        }
        require( status.category == WSE_CAPI_ERROR_INTERNAL && diagnostic_allocations == 0,
            "unknown exception mapping must not allocate" );
        const std::string utf8 = std::string( 1022U, 'a' ) + "\xe3\x81\x82";
        status = wse::capi::makeStatus( WSE_CAPI_ERROR_PROTOCOL, 17, utf8, 23 );
        require( status.code == 17 && status.native_code == 23 &&
            std::strlen( wse_capi_last_error_message() ) == 1022U, "UTF-8 truncation/status values" );
        std::thread other( [] { wse::capi::makeSuccess(); } );
        other.join();
        require( std::strlen( wse_capi_last_error_message() ) == 1022U, "thread-local isolation" );
        {
            FailureScope failing( 0 );
            status = wse::capi::makeSuccess();
            diagnostic_allocations = attempts;
        }
        require( status.category == WSE_CAPI_ERROR_NONE && diagnostic_allocations == 0 &&
            wse_capi_last_error_message()[0] == '\0', "success clears without allocating" );

        std::cout << "runtime creation\n";
        sweep<wse_capi_runtime>( wse_capi_runtime_create, wse_capi_runtime_destroy );
        std::cout << "cancellation creation\n";
        sweep<wse_capi_cancellation>( wse_capi_cancellation_create, wse_capi_cancellation_destroy );
        wse_capi_runtime runtime = nullptr;
        require( wse_capi_runtime_create( &runtime ).category == WSE_CAPI_ERROR_NONE, "runtime setup" );
        const std::uint8_t bytes[64] = { 1, 2, 3 };
        try
        {
            std::cout << "frame copy\n";
            sweep<wse_capi_frame_buffer>( [runtime, &bytes]( wse_capi_frame_buffer* output_in )
                { return wse_capi_runtime_copy_frame( runtime, output_in, bytes, sizeof( bytes ) ); },
                wse_capi_frame_buffer_destroy );
        }
        catch ( ... ) { wse_capi_runtime_destroy( runtime ); throw; }
        wse_capi_runtime_destroy( runtime );
        std::cout << "C ABI allocation and diagnostic contracts passed\n";
        return 0;
    }
    catch ( const std::exception& error_in )
    {
        std::cerr << error_in.what() << '\n';
        return 1;
    }
}
