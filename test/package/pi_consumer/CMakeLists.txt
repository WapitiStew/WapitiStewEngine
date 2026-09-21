cmake_minimum_required(VERSION 3.24)

project(WsePiPackageConsumer LANGUAGES CXX)

if(NOT DEFINED WSE_PACKAGE_ROOT OR WSE_PACKAGE_ROOT STREQUAL "")
    message(FATAL_ERROR "WSE_PACKAGE_ROOT must point to an installed WSE package.")
endif()

set(WSE_CONSUMER_SHAPE "" CACHE STRING
    "Package shape to exercise: xpt, gef, iui, tmr, or public-device")
set_property(CACHE WSE_CONSUMER_SHAPE PROPERTY STRINGS
    xpt gef iui tmr public-device)

if(NOT WSE_CONSUMER_SHAPE MATCHES "^(xpt|gef|iui|tmr|public-device)$")
    message(FATAL_ERROR
        "WSE_CONSUMER_SHAPE must be xpt, gef, iui, tmr, or public-device.")
endif()

find_package(WonderStewEngine CONFIG REQUIRED
    PATHS "${WSE_PACKAGE_ROOT}/cmake"
    NO_DEFAULT_PATH)

if(NOT TARGET WSE::Core)
    message(FATAL_ERROR "Required WSE target is missing: WSE::Core")
endif()
# The consumer-facing compatibility alias was removed by the legacy removal program and must
# not silently return through a package regression.
if(TARGET WonderStewEngine)
    message(FATAL_ERROR
        "The installed package must not define the removed WonderStewEngine compatibility alias.")
endif()

set(wse_component_targets "")
set(wse_compile_definitions "")

if(WSE_CONSUMER_SHAPE STREQUAL "xpt")
    list(APPEND wse_component_targets WSE::Xpt)
    list(APPEND wse_compile_definitions WSE_CONSUMER_XPT=1)
endif()

if(WSE_CONSUMER_SHAPE STREQUAL "gef" OR
        WSE_CONSUMER_SHAPE STREQUAL "public-device")
    list(APPEND wse_component_targets WSE::Gef)
    list(APPEND wse_compile_definitions WSE_CONSUMER_GEF=1)
endif()

if(WSE_CONSUMER_SHAPE STREQUAL "iui" OR
        WSE_CONSUMER_SHAPE STREQUAL "public-device")
    list(APPEND wse_component_targets WSE::Iui)
    list(APPEND wse_compile_definitions WSE_CONSUMER_IUI=1)
endif()

if(WSE_CONSUMER_SHAPE STREQUAL "tmr" OR
        WSE_CONSUMER_SHAPE STREQUAL "public-device")
    list(APPEND wse_component_targets WSE::Tmr)
    list(APPEND wse_compile_definitions WSE_CONSUMER_TMR=1)
endif()

foreach(wse_component_target IN LISTS wse_component_targets)
    if(NOT TARGET ${wse_component_target})
        message(FATAL_ERROR
            "Selected package does not export ${wse_component_target}.")
    endif()
endforeach()

function(wse_add_consumer target_name core_target)
    add_executable(${target_name} main.cpp)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    target_compile_definitions(${target_name} PRIVATE ${wse_compile_definitions})
    target_link_libraries(${target_name} PRIVATE
        ${core_target}
        ${wse_component_targets})
    set_target_properties(${target_name} PROPERTIES
        BUILD_RPATH "${WSE_PACKAGE_ROOT}/lib")
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

enable_testing()
wse_add_consumer(wse_pi_modern_consumer WSE::Core)
