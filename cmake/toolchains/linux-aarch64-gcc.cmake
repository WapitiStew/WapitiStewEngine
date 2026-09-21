# @file engine/wse/cmake/toolchains/linux-aarch64-gcc.cmake
# @brief UbuntuのGNU cross toolchainでLinux AArch64成果物を生成する。
# @brief Generates Linux AArch64 artifacts with the Ubuntu GNU cross toolchain.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Generator tools such as wayland-scanner and glslangValidator run on the build host.
# pkg-config must resolve target-side ARM64 Vulkan/Wayland/DRM metadata only.
set(ENV{PKG_CONFIG_LIBDIR}
    "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")

# Ubuntu multiarch installs the target Vulkan loader outside the compiler sysroot.
set(Vulkan_LIBRARY "/usr/lib/aarch64-linux-gnu/libvulkan.so"
    CACHE FILEPATH "ARM64 Vulkan loader")
set(Vulkan_INCLUDE_DIR "/usr/include"
    CACHE PATH "Architecture-neutral Vulkan headers")

# Configure checks must not attempt to execute AArch64 binaries on the build host.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
