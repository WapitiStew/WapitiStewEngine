cmake_minimum_required(VERSION 3.24)

project(WsePiGefNative LANGUAGES CXX)

set(WSE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH
    "Root of the WSE source snapshot")
set(WSE_LIBRARY_TYPE "SHARED" CACHE STRING "SHARED or STATIC")
set_property(CACHE WSE_LIBRARY_TYPE PROPERTY STRINGS SHARED STATIC)

if(NOT WSE_LIBRARY_TYPE MATCHES "^(SHARED|STATIC)$")
    message(FATAL_ERROR "WSE_LIBRARY_TYPE must be SHARED or STATIC.")
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

set(wse_gef_sources
    core/gef/bin/binController.cpp
    core/gef/csv/csvController.cpp
    core/gef/error/GefError.cpp)

list(TRANSFORM wse_core_sources PREPEND "${WSE_SOURCE_ROOT}/")
list(TRANSFORM wse_gef_sources PREPEND "${WSE_SOURCE_ROOT}/")

foreach(wse_source IN LISTS wse_core_sources wse_gef_sources)
    if(NOT EXISTS "${wse_source}")
        message(FATAL_ERROR "Required public source is missing: ${wse_source}")
    endif()
endforeach()

find_package(Threads REQUIRED)

add_library(WonderStewEngine ${WSE_LIBRARY_TYPE}
    ${wse_core_sources}
    ${wse_gef_sources})
target_compile_features(WonderStewEngine PUBLIC cxx_std_17)
target_include_directories(WonderStewEngine PUBLIC
    "${WSE_SOURCE_ROOT}/api"
    "${WSE_SOURCE_ROOT}/..")
target_link_libraries(WonderStewEngine PUBLIC Threads::Threads)

if(WSE_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(WonderStewEngine PRIVATE __WSE_EXPORTS__)
else()
    target_compile_definitions(WonderStewEngine PUBLIC WSE_STATIC)
endif()

add_executable(wse_pi_gef_native_contract
    "${WSE_SOURCE_ROOT}/test/characterization/gef_bin_contract.cpp")
target_compile_features(wse_pi_gef_native_contract PRIVATE cxx_std_17)
target_link_libraries(wse_pi_gef_native_contract PRIVATE WonderStewEngine)

enable_testing()
add_test(NAME wse_pi_gef_native_contract
    COMMAND wse_pi_gef_native_contract
        "${CMAKE_CURRENT_BINARY_DIR}/wse-pi-gef-native-contract.bin")
