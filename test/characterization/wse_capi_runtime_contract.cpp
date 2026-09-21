// Public C calls only: layout snapshots do not establish runtime ownership or copy semantics.
#include <wse/capi/wse_capi_core.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace
{
int failures = 0;
void expect( const bool condition_in, const char* message_in )
{
    if( !condition_in ) { ++failures; std::cerr << message_in << '\n'; }
}
bool succeeded( const wse_capi_status status_in )
{
    return status_in.category == WSE_CAPI_ERROR_NONE;
}
}

int main()
{
    expect( wse_capi_abi_version() == WSE_CAPI_ABI_VERSION, "Loaded C ABI matches header" );
    wse_capi_runtime runtime = nullptr;
    expect( succeeded( wse_capi_runtime_create( &runtime ) ) && runtime != nullptr, "Create runtime" );
    if( runtime == nullptr ) return 1;

    // CABI-BUFFER-04: strings include NUL and never silently truncate.
    std::size_t required = 0U;
    expect( succeeded( wse_capi_runtime_version( runtime, nullptr, &required, 0U ) )
        && required == 6U, "Version query includes NUL" );
    std::array<char, 6> version { '?', '?', '?', '?', '?', '?' };
    auto status = wse_capi_runtime_version( runtime, version.data(), &required, 5U );
    expect( status.category == WSE_CAPI_ERROR_INVALID_ARGUMENT && required == 6U
        && version == std::array<char, 6>{ '?', '?', '?', '?', '?', '?' },
        "Short string destination remains untouched and reports required size" );
    expect( succeeded( wse_capi_runtime_version( runtime, version.data(), &required, version.size() ) )
        && std::memcmp( version.data(), "1.0.0", 6U ) == 0, "Exact string destination includes NUL" );

    // CABI-ERROR-03: capture diagnostics before any successful status call clears them.
    status = wse_capi_runtime_wait( runtime, -1, nullptr );
    const std::string diagnostic = wse_capi_last_error_message();
    expect( status.category == WSE_CAPI_ERROR_INVALID_ARGUMENT && !diagnostic.empty(), "Capture diagnostic" );
    bool worker_ok = false;
    std::thread worker( [ & ]()
    {
        const auto invalid = wse_capi_runtime_version( nullptr, nullptr, &required, 0U );
        const std::string worker_diagnostic = wse_capi_last_error_message();
        wse_capi_runtime other = nullptr;
        const auto created = wse_capi_runtime_create( &other );
        worker_ok = invalid.category == WSE_CAPI_ERROR_INVALID_ARGUMENT
            && !worker_diagnostic.empty() && succeeded( created )
            && std::strlen( wse_capi_last_error_message() ) == 0U;
        wse_capi_runtime_destroy( other );
    } );
    worker.join();
    expect( worker_ok && diagnostic == wse_capi_last_error_message(), "Diagnostics are thread-local" );
    expect( succeeded( wse_capi_runtime_wait( runtime, 0, nullptr ) )
        && std::strlen( wse_capi_last_error_message() ) == 0U, "Success clears current-thread diagnostic" );

    // Cancellation is sticky, including a zero-duration wait's final check.
    wse_capi_cancellation cancellation = nullptr;
    expect( succeeded( wse_capi_cancellation_create( &cancellation ) ), "Create cancellation" );
    expect( succeeded( wse_capi_cancellation_cancel( cancellation ) )
        && succeeded( wse_capi_cancellation_cancel( cancellation ) ), "Repeat cancellation" );
    expect( wse_capi_runtime_wait( runtime, 0, cancellation ).category == WSE_CAPI_ERROR_CANCELLATION,
        "Zero-duration wait observes cancellation" );
    wse_capi_cancellation_destroy( cancellation );

    // CABI-OWNER-01: the output owns bytes independently of source and creator Runtime.
    std::array<std::uint8_t, 4> source { 0x00, 0x7f, 0x80, 0xff };
    wse_capi_frame_buffer buffer = nullptr;
    expect( succeeded( wse_capi_runtime_copy_frame( runtime, &buffer, source.data(), source.size() ) ),
        "Copy owned frame" );
    source.fill( 0x33 );
    wse_capi_runtime_destroy( runtime );
    runtime = nullptr;
    expect( succeeded( wse_capi_frame_buffer_copy_to( buffer, nullptr, &required, 0U ) )
        && required == 4U, "Buffer outlives Runtime; payload excludes terminator" );
    std::array<std::uint8_t, 4> output { 0xaa, 0xaa, 0xaa, 0xaa };
    status = wse_capi_frame_buffer_copy_to( buffer, output.data(), &required, 3U );
    expect( status.category == WSE_CAPI_ERROR_INVALID_ARGUMENT && required == 0U
        && output == std::array<std::uint8_t, 4>{ 0xaa, 0xaa, 0xaa, 0xaa },
        "Short frame copy writes zero bytes and preserves destination" );
    expect( succeeded( wse_capi_frame_buffer_copy_to( buffer, output.data(), &required, output.size() ) )
        && required == 4U && output == std::array<std::uint8_t, 4>{ 0x00, 0x7f, 0x80, 0xff },
        "Frame retains literal input bytes" );
    wse_capi_frame_buffer_destroy( buffer );
    wse_capi_runtime_destroy( nullptr );
    wse_capi_frame_buffer_destroy( nullptr );
    wse_capi_cancellation_destroy( nullptr );
    return failures == 0 ? 0 : 1;
}
