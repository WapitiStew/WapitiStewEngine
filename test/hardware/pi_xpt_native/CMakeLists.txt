cmake_minimum_required(VERSION 3.24)

project(WsePiXptNative LANGUAGES CXX)

set(WSE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH
    "Root of the privacy-filtered WSE source snapshot")
set(WSE_LIBCURL_ROOT "" CACHE PATH
    "Root of the bootstrap-managed ARM64 libcurl target")
set(WSE_OPENSSL_ROOT "" CACHE PATH
    "Root of the bootstrap-managed ARM64 OpenSSL target")
set(WSE_LIBRARY_TYPE "SHARED" CACHE STRING "SHARED or STATIC")
set_property(CACHE WSE_LIBRARY_TYPE PROPERTY STRINGS SHARED STATIC)

if(NOT WSE_LIBRARY_TYPE MATCHES "^(SHARED|STATIC)$")
    message(FATAL_ERROR "WSE_LIBRARY_TYPE must be SHARED or STATIC.")
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Debug or Release.")
endif()

set(wse_core_sources
    core/wse/binding/Cancellation.cpp
    core/wse/binding/Error.cpp
    core/wse/binding/FrameBuffer.cpp
    core/wse/binding/Runtime.cpp
    core/wse/data/wse_Cell.cpp
    core/wse/data/wse_Homography.cpp
    core/wse/data/wse_Image.cpp
    core/wse/data/wse_Map.cpp
    core/wse/data/wse_Matrix.cpp
    core/wse/data/wse_Mesh.cpp
    core/wse/data/wse_Pixel.cpp
    core/wse/data/wse_Point2D.cpp
    core/wse/data/wse_Point3D.cpp
    core/wse/data/wse_Point4D.cpp
    core/wse/data/wse_Range1D.cpp
    core/wse/data/wse_Range2D.cpp
    core/wse/data/wse_Range3D.cpp
    core/wse/data/wse_Size.cpp
    core/wse/data/wse_TiePoint.cpp
    core/wse/error/CoreError.cpp
    core/wse/error/LicenseError.cpp
    core/wse/license/wse_LicenceAdmin.cpp
    core/wse/license/wse_License.cpp
    core/wse/license/wse_LicenseKey.cpp
    core/wse/license/wse_LicenseWriter.cpp
    core/wse/utility/wse_DataCast.cpp
    core/wse/utility/wse_Log.cpp
    core/wse/utility/wse_Timer.cpp
    core/wse/utility/wse_Wait.cpp
    core/wse/utility/wse_WorkerController.cpp
    platform/wse/linux/license/mac_addless.cpp
    platform/wse/linux/utility/wse_DevicePickup.cpp)

set(wse_xpt_sources
    core/xpt/binding/XptErrorAdapter.cpp
    core/xpt/error/TransportError.cpp
    core/xpt/network/Endpoint.cpp
    core/xpt/operation/Cancellation.cpp
    core/xpt/operation/OperationContext.cpp
    core/xpt/retry/RetryPolicy.cpp
    platform/xpt/http/HttpClient.cpp
    platform/xpt/network/TcpClient.cpp
    platform/xpt/network/UdpClient.cpp
    platform/xpt/serial/SerialPort.cpp)

list(TRANSFORM wse_core_sources PREPEND "${WSE_SOURCE_ROOT}/")
list(TRANSFORM wse_xpt_sources PREPEND "${WSE_SOURCE_ROOT}/")

set(wse_dependency_files
    "${WSE_LIBCURL_ROOT}/include/curl/curl.h"
    "${WSE_LIBCURL_ROOT}/lib/debug/libcurl-d.a"
    "${WSE_LIBCURL_ROOT}/lib/release/libcurl.a"
    "${WSE_OPENSSL_ROOT}/include/openssl/ssl.h"
    "${WSE_OPENSSL_ROOT}/lib/libssl.a"
    "${WSE_OPENSSL_ROOT}/lib/libcrypto.a")

foreach(wse_source IN LISTS wse_core_sources wse_xpt_sources wse_dependency_files)
    if(NOT EXISTS "${wse_source}")
        message(FATAL_ERROR "Required public source or dependency is missing: ${wse_source}")
    endif()
endforeach()

find_package(Threads REQUIRED)

set(OPENSSL_ROOT_DIR "${WSE_OPENSSL_ROOT}")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_INCLUDE_DIR "${WSE_OPENSSL_ROOT}/include")
set(OPENSSL_SSL_LIBRARY "${WSE_OPENSSL_ROOT}/lib/libssl.a")
set(OPENSSL_CRYPTO_LIBRARY "${WSE_OPENSSL_ROOT}/lib/libcrypto.a")
find_package(OpenSSL REQUIRED)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(wse_libcurl_library "${WSE_LIBCURL_ROOT}/lib/debug/libcurl-d.a")
else()
    set(wse_libcurl_library "${WSE_LIBCURL_ROOT}/lib/release/libcurl.a")
endif()

add_library(WsePiCurlBackend STATIC IMPORTED GLOBAL)
set_target_properties(WsePiCurlBackend PROPERTIES
    IMPORTED_LOCATION "${wse_libcurl_library}"
    INTERFACE_COMPILE_DEFINITIONS CURL_STATICLIB
    INTERFACE_INCLUDE_DIRECTORIES "${WSE_LIBCURL_ROOT}/include"
    INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto")

add_library(WonderStewEngine ${WSE_LIBRARY_TYPE}
    ${wse_core_sources}
    ${wse_xpt_sources})
target_compile_features(WonderStewEngine PUBLIC cxx_std_17)
target_include_directories(WonderStewEngine PUBLIC
    "${WSE_SOURCE_ROOT}/api"
    "${WSE_SOURCE_ROOT}/..")
target_compile_definitions(WonderStewEngine PUBLIC
    WSE_HAS_XPT=1
    WSE_HAS_XPT_TCP=1
    WSE_HAS_XPT_UDP=1
    WSE_HAS_XPT_SERIAL=1
    WSE_HAS_XPT_RETRY=1
    WSE_HAS_XPT_HTTP=1)
target_link_libraries(WonderStewEngine PRIVATE
    Threads::Threads
    WsePiCurlBackend)

if(WSE_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(WonderStewEngine PRIVATE __WSE_EXPORTS__)
else()
    target_compile_definitions(WonderStewEngine PUBLIC WSE_STATIC)
endif()

function(wse_add_pi_contract target_name source_name timeout_seconds)
    add_executable(${target_name} "${WSE_SOURCE_ROOT}/test/characterization/${source_name}")
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    target_link_libraries(${target_name} PRIVATE WonderStewEngine)
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES TIMEOUT ${timeout_seconds})
endfunction()

enable_testing()
wse_add_pi_contract(wse_pi_core_characterization core_characterization.cpp 10)
wse_add_pi_contract(wse_pi_core_runtime core_runtime_contract.cpp 10)
wse_add_pi_contract(wse_pi_xpt_tcp xpt_tcp_loopback.cpp 10)
wse_add_pi_contract(wse_pi_xpt_udp xpt_udp_loopback.cpp 10)
wse_add_pi_contract(wse_pi_xpt_retry xpt_retry_policy_contract.cpp 10)
wse_add_pi_contract(wse_pi_xpt_http xpt_http_contract.cpp 20)
wse_add_pi_contract(wse_pi_xpt_serial_pty xpt_serial_port_contract.cpp 10)
