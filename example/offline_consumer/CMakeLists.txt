# The smallest project that consumes an installed WSE package the way a customer does: from
# outside this repository, against nothing but an extracted directory. It ships in the offline
# kit as its own source bundle, so it has to configure and build on a machine with no network,
# no registry entries and no WSE build tree.

cmake_minimum_required(VERSION 3.24)

project(WseOfflineConsumer LANGUAGES CXX)

# Fail here rather than at find_package, so the message names the thing the caller forgot
# instead of listing every directory CMake searched.
if(NOT DEFINED WSE_PACKAGE_ROOT OR WSE_PACKAGE_ROOT STREQUAL "")
    message(FATAL_ERROR "Set WSE_PACKAGE_ROOT to the extracted local WSE package.")
endif()

# NO_DEFAULT_PATH is the point of this example: without it CMake could satisfy the request from
# a system prefix, a user package registry or an environment variable, and the build would pass
# while proving nothing about the package under test. Restricting the search to one directory
# means a success here is a success for that package and no other.
find_package(WonderStewEngine CONFIG REQUIRED
    PATHS "${WSE_PACKAGE_ROOT}/cmake"
    NO_DEFAULT_PATH
)

add_executable(wse_offline_consumer main.cpp)
# WSE::Xpt is an interface target: it carries the XPT feature macro and pulls in WSE::Core,
# which is the single library the package actually installs. Naming both is what a consumer
# writes, and it is also what proves the exported component targets survived packaging.
target_link_libraries(wse_offline_consumer PRIVATE WSE::Core WSE::Xpt)
# C++17 is the language level the public headers are written against. Raising it is fine;
# lowering it is not, so the requirement is stated rather than inherited from the toolchain.
target_compile_features(wse_offline_consumer PRIVATE cxx_std_17)
# WSE sources and headers are UTF-8 without a byte-order mark. Absent /utf-8, MSVC reads them
# in the system ANSI code page and mangles every non-ASCII literal and comment.
target_compile_options(wse_offline_consumer PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# WSE_PACKAGE_LIBRARY_TYPE comes from the package config loaded above, so this branch reflects
# how the package was built rather than how this project was configured. A shared package needs
# its runtime library beside the executable for the test below to start without a PATH edit;
# only Core is copied because Core is the only real library, the component targets being
# interface-only. A static package needs nothing here.
if(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "SHARED")
    add_custom_command(TARGET wse_offline_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:WSE::Core>"
            "$<TARGET_FILE_DIR:wse_offline_consumer>"
    )
endif()

# Building only proves the headers and the link line survived packaging. Running proves the
# library loads and its contracts still hold, so the executable is registered as a test and its
# exit code is the verdict on the package.
enable_testing()
add_test(NAME wse_offline_consumer COMMAND wse_offline_consumer)
