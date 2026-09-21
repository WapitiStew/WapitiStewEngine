# @file cmake/WseDependencies.cmake
# @brief Pinned dependency metadata, dependency root resolution, and vendor discovery.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

# ------------------------------------------------------------------------------------------------
# Version and pinned dependency metadata.
# ------------------------------------------------------------------------------------------------

# What Bootstrap is expected to have produced. These values are copied into `wse-package.json` and
# into the vendor notices the package carries, so raising one here without raising the matching pin
# in `bootstrap-manifest.json` makes the package describe something it does not contain.
set(WSE_LIBCURL_BOOTSTRAP_VERSION "8.21.0")
set(WSE_LIBCURL_BOOTSTRAP_LICENSE "curl")
set(WSE_LIBCURL_BOOTSTRAP_SHA256 "aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6")
set(WSE_LIBCAMERA_BOOTSTRAP_VERSION "0.7.2")
set(WSE_LIBCAMERA_BOOTSTRAP_LICENSE "LGPL-2.1-or-later")
set(WSE_LIBCAMERA_BOOTSTRAP_COMMIT "191e202178f02430b5942397c70d215cdd2056fa")
set(WSE_NODE_API_BOOTSTRAP_VERSION "24.19.0")
set(WSE_NODE_API_BOOTSTRAP_LICENSE "MIT")
set(WSE_NODE_API_BOOTSTRAP_SHA256 "54f14a297d47ea0794fe272363703d9dc419c96ac68f20d890f98b63754a3e4c")
set(WSE_PYBIND11_BOOTSTRAP_VERSION "3.1.0")
set(WSE_PYBIND11_BOOTSTRAP_LICENSE "BSD-3-Clause")
set(WSE_PYBIND11_BOOTSTRAP_SHA256 "a1cc06b524ab3edca51f8ad3895f9c4fa20b8b19283173dff4ae781449dc9639")
set(WSE_NLOHMANN_JSON_BOOTSTRAP_VERSION "3.12.0")
set(WSE_NLOHMANN_JSON_BOOTSTRAP_LICENSE "MIT")
set(WSE_NLOHMANN_JSON_BOOTSTRAP_SHA256 "42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa")
set(WSE_LIBJPEG_TURBO_BOOTSTRAP_VERSION "3.0.4")
set(WSE_LIBJPEG_TURBO_BOOTSTRAP_LICENSE "IJG, BSD-3-Clause and Zlib")
set(WSE_LIBJPEG_TURBO_BOOTSTRAP_SHA256 "0c58853494f31a65329e567569d8614f35a74c1251bdcca10bb3d01689b35035")

# ------------------------------------------------------------------------------------------------
# Dependency roots and path resolution.
# ------------------------------------------------------------------------------------------------

# Where Bootstrap writes by default. Each per-package override below is empty by default and, when
# set, replaces only its own package, so a builder can point at a prebuilt tree for one dependency
# without relocating the rest. The tool paths do the same for the toolchains that are not vendored.
set(WSE_DEPENDENCY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor" CACHE PATH
	"Root directory containing WSE bootstrap-managed dependencies")
set(WSE_VPJ_LIBJPEG_TURBO_ROOT "" CACHE PATH
	"Optional VPJ libjpeg-turbo root; defaults to <WSE_DEPENDENCY_ROOT>/vpj/libjpeg-turbo[-platform]")
set(WSE_LIBCURL_ROOT "" CACHE PATH
	"Optional libcurl root override; defaults to <WSE_DEPENDENCY_ROOT>/libcurl")
set(WSE_LIBCAMERA_ROOT "" CACHE PATH
	"Optional libcamera root override; defaults to <WSE_DEPENDENCY_ROOT>/libcamera")
set(WSE_NODE_API_ROOT "" CACHE PATH
	"Optional Node-API header root override; defaults to <WSE_DEPENDENCY_ROOT>/node-api")
set(WSE_NODE_EXECUTABLE "" CACHE FILEPATH
	"Optional Node.js executable used by Node-API contract tests")
set(WSE_PYBIND11_ROOT "" CACHE PATH
	"Optional pybind11 root override; defaults to <WSE_DEPENDENCY_ROOT>/pybind11")
set(WSE_NLOHMANN_JSON_ROOT "" CACHE PATH
	"Optional nlohmann/json root override; defaults to <WSE_DEPENDENCY_ROOT>/nlohmann-json")
set(WSE_PYTHON_EXECUTABLE "" CACHE FILEPATH
	"Optional CPython executable used to build and test the Python binding")
set(WSE_PYTHON_TARGET_INCLUDE_DIR "" CACHE PATH
	"Target CPython include directory required for cross-compiling the Python binding")
set(WSE_PYTHON_EXTENSION_SUFFIX ".so" CACHE STRING
	"Python extension suffix used only while cross-compiling")
set(WSE_JAVA_HOME "" CACHE PATH
	"Optional JDK 17+ root used to build and test the Java binding")
set(WSE_JAVA_17_EXECUTABLE "" CACHE FILEPATH
	"Optional Java 17 runtime executable used by the Phase 6-F matrix")
set(WSE_JAVA_21_EXECUTABLE "" CACHE FILEPATH
	"Optional Java 21 runtime executable used by the Phase 6-F matrix")
set(WSE_JAVA_25_EXECUTABLE "" CACHE FILEPATH
	"Optional Java 25 runtime executable used by the Phase 6-F matrix")
set(WSE_JNI_TARGET_INCLUDE_DIRS "" CACHE STRING
	"Target JDK JNI include directories required while cross-compiling the Java binding")
set(WSE_DOTNET_EXECUTABLE "" CACHE FILEPATH
	"Optional .NET SDK executable used to build and test the C# binding")
set(WSE_OPENSSL_ROOT "" CACHE PATH
	"Optional OpenSSL root used by the Linux XPT backend; system OpenSSL is used when empty")

# Bootstrap builds libjpeg-turbo once per target architecture into a differently named directory,
# so the default is chosen from the target rather than from the host that is running CMake.
if(NOT WSE_VPJ_LIBJPEG_TURBO_ROOT STREQUAL "")
	set(WSE_VENDOR_JPEG_DIR "${WSE_VPJ_LIBJPEG_TURBO_ROOT}")
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
			AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
		set(WSE_VENDOR_JPEG_DIR "${WSE_DEPENDENCY_ROOT}/vpj/libjpeg-turbo-arm64")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(WSE_VENDOR_JPEG_DIR "${WSE_DEPENDENCY_ROOT}/vpj/libjpeg-turbo-linux")
	else()
		set(WSE_VENDOR_JPEG_DIR "${WSE_DEPENDENCY_ROOT}/vpj/libjpeg-turbo")
	endif()
endif()

if(WSE_LIBCURL_ROOT STREQUAL "")
	set(WSE_VENDOR_LIBCURL_DIR "${WSE_DEPENDENCY_ROOT}/libcurl")
else()
	set(WSE_VENDOR_LIBCURL_DIR "${WSE_LIBCURL_ROOT}")
endif()

if(WSE_LIBCAMERA_ROOT STREQUAL "")
	set(WSE_VENDOR_LIBCAMERA_DIR "${WSE_DEPENDENCY_ROOT}/libcamera")
else()
	set(WSE_VENDOR_LIBCAMERA_DIR "${WSE_LIBCAMERA_ROOT}")
endif()

if(WSE_NODE_API_ROOT STREQUAL "")
	set(WSE_VENDOR_NODE_API_DIR "${WSE_DEPENDENCY_ROOT}/node-api")
else()
	set(WSE_VENDOR_NODE_API_DIR "${WSE_NODE_API_ROOT}")
endif()

if(WSE_PYBIND11_ROOT STREQUAL "")
	set(WSE_VENDOR_PYBIND11_DIR "${WSE_DEPENDENCY_ROOT}/pybind11")
else()
	set(WSE_VENDOR_PYBIND11_DIR "${WSE_PYBIND11_ROOT}")
endif()

if(WSE_NLOHMANN_JSON_ROOT STREQUAL "")
	set(WSE_VENDOR_NLOHMANN_JSON_DIR "${WSE_DEPENDENCY_ROOT}/nlohmann-json")
else()
	set(WSE_VENDOR_NLOHMANN_JSON_DIR "${WSE_NLOHMANN_JSON_ROOT}")
endif()

# Everything downstream tests these paths for existence, embeds them in Imported Targets, and
# installs from them. Resolving them once against the source root means a relative override behaves
# the same wherever the Build tree happens to sit.
get_filename_component(WSE_VENDOR_JPEG_DIR "${WSE_VENDOR_JPEG_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(WSE_VENDOR_LIBCURL_DIR "${WSE_VENDOR_LIBCURL_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(WSE_VENDOR_LIBCAMERA_DIR "${WSE_VENDOR_LIBCAMERA_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(WSE_VENDOR_NODE_API_DIR "${WSE_VENDOR_NODE_API_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(WSE_VENDOR_PYBIND11_DIR "${WSE_VENDOR_PYBIND11_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(WSE_VENDOR_NLOHMANN_JSON_DIR "${WSE_VENDOR_NLOHMANN_JSON_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT WSE_OPENSSL_ROOT STREQUAL "")
	get_filename_component(WSE_VENDOR_OPENSSL_DIR "${WSE_OPENSSL_ROOT}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
else()
	set(WSE_VENDOR_OPENSSL_DIR "")
endif()

# Only the discovery blocks below know whether a bootstrapped copy was actually found and adopted.
# They raise these, and the install rules near the end of the file are the sole reader: a Consumer
# of the package must receive the exact OpenSSL or libcamera the Library was linked against.
set(WSE_PACKAGE_BUNDLES_OPENSSL false)
set(WSE_PACKAGE_BUNDLES_LIBCAMERA false)

# ------------------------------------------------------------------------------------------------
# Vendor dependency discovery.
# ------------------------------------------------------------------------------------------------

# Each block checks only the dependencies the selected Options will actually Link, names the
# Bootstrap command that repairs a gap, and where a Library is involved publishes an Imported
# Target so the Link line stays one name instead of a path assembled at each use.
if(WSE_BUILD_NODE_BINDING)
	foreach(wse_node_api_file IN ITEMS
			"include/node_api.h"
			"include/node_api_types.h"
			"include/js_native_api.h"
			"include/js_native_api_types.h"
			"LICENSE"
			"wse-dependency.json")
		if(NOT EXISTS "${WSE_VENDOR_NODE_API_DIR}/${wse_node_api_file}")
			message(FATAL_ERROR
				"The pinned Node-API header dependency is incomplete: ${WSE_VENDOR_NODE_API_DIR}\n"
				"Run bootstrap.py --package node-api-headers, or set WSE_NODE_API_ROOT.")
		endif()
	endforeach()
endif()

if(WSE_BUILD_PYTHON_BINDING)
	foreach(wse_pybind11_file IN ITEMS
			"CMakeLists.txt"
			"include/pybind11/pybind11.h"
			"tools/pybind11Config.cmake.in"
			"LICENSE"
			"wse-dependency.json")
		if(NOT EXISTS "${WSE_VENDOR_PYBIND11_DIR}/${wse_pybind11_file}")
			message(FATAL_ERROR
				"The pinned pybind11 dependency is incomplete: ${WSE_VENDOR_PYBIND11_DIR}\n"
				"Run bootstrap.py --package pybind11, or set WSE_PYBIND11_ROOT.")
		endif()
	endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_BUILD_TMR AND WSE_ENABLE_LIBCAMERA)
	foreach(wse_libcamera_file IN ITEMS
			"include/libcamera/libcamera/camera.h"
			"lib/libcamera.so"
			"lib/libcamera-base.so"
			"LICENSES/LGPL-2.1-or-later.txt"
			"wse-dependency.json")
		if(NOT EXISTS "${WSE_VENDOR_LIBCAMERA_DIR}/${wse_libcamera_file}")
			message(FATAL_ERROR
				"The pinned libcamera dependency is incomplete: ${WSE_VENDOR_LIBCAMERA_DIR}\n"
				"Run bootstrap.py --package libcamera, or set WSE_LIBCAMERA_ROOT to a verified target.")
		endif()
	endforeach()
	# The Tmr adapter links the bootstrapped libcamera, not whichever version the distribution
	# happens to ship, so the two .so files are imported by absolute path.
	add_library(WSE_LibcameraRuntime SHARED IMPORTED GLOBAL)
	set_target_properties(WSE_LibcameraRuntime PROPERTIES
		IMPORTED_LOCATION "${WSE_VENDOR_LIBCAMERA_DIR}/lib/libcamera.so"
		INTERFACE_INCLUDE_DIRECTORIES "${WSE_VENDOR_LIBCAMERA_DIR}/include/libcamera")
	add_library(WSE_LibcameraBaseRuntime SHARED IMPORTED GLOBAL)
	set_target_properties(WSE_LibcameraBaseRuntime PROPERTIES
		IMPORTED_LOCATION "${WSE_VENDOR_LIBCAMERA_DIR}/lib/libcamera-base.so")
	set(WSE_PACKAGE_BUNDLES_LIBCAMERA true)
endif()

if(WSE_BUILD_XPT)
	if(NOT EXISTS "${WSE_VENDOR_LIBCURL_DIR}/include/curl/curl.h"
			OR NOT EXISTS "${WSE_VENDOR_LIBCURL_DIR}/COPYING"
			OR NOT EXISTS "${WSE_VENDOR_LIBCURL_DIR}/wse-dependency.json")
		message(FATAL_ERROR
			"The pinned libcurl dependency is incomplete: ${WSE_VENDOR_LIBCURL_DIR}\n"
			"Run bootstrap.py --package libcurl, or set WSE_LIBCURL_ROOT to a verified target.")
	endif()

	# libcurl is linked statically into WSE, so both configurations must be present before the
	# Imported Target below can offer a Debug and a Release mapping.
	if(WIN32)
		set(WSE_LIBCURL_LIBRARY_DEBUG
			"${WSE_VENDOR_LIBCURL_DIR}/lib/debug/libcurl-d.lib")
		set(WSE_LIBCURL_LIBRARY_RELEASE
			"${WSE_VENDOR_LIBCURL_DIR}/lib/release/libcurl.lib")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(WSE_LIBCURL_LIBRARY_DEBUG
			"${WSE_VENDOR_LIBCURL_DIR}/lib/debug/libcurl-d.a")
		set(WSE_LIBCURL_LIBRARY_RELEASE
			"${WSE_VENDOR_LIBCURL_DIR}/lib/release/libcurl.a")
	endif()

	foreach(wse_libcurl_library IN ITEMS
			"${WSE_LIBCURL_LIBRARY_DEBUG}"
			"${WSE_LIBCURL_LIBRARY_RELEASE}")
		if(NOT EXISTS "${wse_libcurl_library}")
			message(FATAL_ERROR
				"The pinned libcurl library is missing: ${wse_libcurl_library}\n"
				"Run bootstrap.py --package libcurl for this target platform.")
		endif()
	endforeach()

	# A cross build cannot use the host's OpenSSL. When a bootstrapped one is supplied, its paths
	# are written into the variables find_package(OpenSSL) consults, so the search below resolves
	# to it instead of searching the machine. Leaving WSE_OPENSSL_ROOT empty keeps the target
	# system's OpenSSL, which is the right answer for a native Linux build.
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT WSE_VENDOR_OPENSSL_DIR STREQUAL "")
		foreach(wse_openssl_dependency IN ITEMS
				"${WSE_VENDOR_OPENSSL_DIR}/include/openssl/ssl.h"
				"${WSE_VENDOR_OPENSSL_DIR}/lib/libssl.a"
				"${WSE_VENDOR_OPENSSL_DIR}/lib/libcrypto.a"
				"${WSE_VENDOR_OPENSSL_DIR}/LICENSE.txt"
				"${WSE_VENDOR_OPENSSL_DIR}/wse-dependency.json")
			if(NOT EXISTS "${wse_openssl_dependency}")
				message(FATAL_ERROR
					"The pinned OpenSSL dependency is incomplete: ${wse_openssl_dependency}\n"
					"Run bootstrap.py --package openssl --target-architecture arm64, "
					"or clear WSE_OPENSSL_ROOT to use the target system OpenSSL.")
			endif()
		endforeach()

		set(OPENSSL_ROOT_DIR "${WSE_VENDOR_OPENSSL_DIR}")
		set(OPENSSL_USE_STATIC_LIBS TRUE)
		set(OPENSSL_INCLUDE_DIR "${WSE_VENDOR_OPENSSL_DIR}/include")
		set(OPENSSL_SSL_LIBRARY "${WSE_VENDOR_OPENSSL_DIR}/lib/libssl.a")
		set(OPENSSL_CRYPTO_LIBRARY "${WSE_VENDOR_OPENSSL_DIR}/lib/libcrypto.a")
		set(WSE_PACKAGE_BUNDLES_OPENSSL true)
	endif()

	# Linux libcurl does TLS through OpenSSL; the Windows build uses Schannel and needs no
	# equivalent search. This must run before the Imported Target below names OpenSSL::SSL.
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		find_package(OpenSSL REQUIRED)
	endif()

	# One name for the Link line instead of a config-dependent path repeated at each use.
	# MinSizeRel and RelWithDebInfo are mapped onto Release because Bootstrap produces only the two
	# archives; without the mapping those configurations would find nothing to link.
	add_library(WSE::CurlBackend STATIC IMPORTED GLOBAL)
	set_target_properties(WSE::CurlBackend PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION_DEBUG "${WSE_LIBCURL_LIBRARY_DEBUG}"
		IMPORTED_LOCATION_RELEASE "${WSE_LIBCURL_LIBRARY_RELEASE}"
		MAP_IMPORTED_CONFIG_MINSIZEREL Release
		MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
		INTERFACE_COMPILE_DEFINITIONS CURL_STATICLIB
		INTERFACE_INCLUDE_DIRECTORIES "${WSE_VENDOR_LIBCURL_DIR}/include"
	)
	# A static libcurl leaves its own dependencies to the final Link. These are the two TLS and
	# name-resolution stacks the pinned build was configured against, one per platform.
	if(WIN32)
		set_property(TARGET WSE::CurlBackend PROPERTY INTERFACE_LINK_LIBRARIES
			"bcrypt;advapi32;crypt32;secur32;ws2_32;iphlpapi")
	else()
		set_property(TARGET WSE::CurlBackend PROPERTY INTERFACE_LINK_LIBRARIES
			"OpenSSL::SSL;OpenSSL::Crypto")
	endif()
endif()

if(WSE_BUILD_VPJ)
	foreach(wse_nlohmann_json_file IN ITEMS
			"single_include/nlohmann/json.hpp"
			"LICENSE.MIT"
			"wse-dependency.json")
		if(NOT EXISTS "${WSE_VENDOR_NLOHMANN_JSON_DIR}/${wse_nlohmann_json_file}")
			message(FATAL_ERROR
				"The pinned nlohmann/json dependency is incomplete: ${WSE_VENDOR_NLOHMANN_JSON_DIR}\n"
				"Run bootstrap.py --package nlohmann-json, or set WSE_NLOHMANN_JSON_ROOT.")
		endif()
	endforeach()

endif()

if(WSE_BUILD_VPJ)
	if(NOT EXISTS "${WSE_VENDOR_JPEG_DIR}/include/turbojpeg.h"
			OR NOT EXISTS "${WSE_VENDOR_JPEG_DIR}/LICENSE.md")
		message(FATAL_ERROR
			"The pinned libjpeg-turbo dependency is incomplete: ${WSE_VENDOR_JPEG_DIR}\n"
			"Run bootstrap.py --package libjpeg-turbo, or set WSE_VPJ_LIBJPEG_TURBO_ROOT.")
	endif()

	# Bootstrap produces a DLL on Windows and an archive on Linux, so the Imported Target has to be
	# declared with the kind that matches; a mismatch is not detected until Link.
	if(WIN32)
		add_library(WSE_TurboJpegBackend SHARED IMPORTED GLOBAL)
	else()
		add_library(WSE_TurboJpegBackend STATIC IMPORTED GLOBAL)
	endif()
	add_library(WSE::TurboJpegBackend ALIAS WSE_TurboJpegBackend)
	set_target_properties(WSE_TurboJpegBackend PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		MAP_IMPORTED_CONFIG_MINSIZEREL Release
		MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
		INTERFACE_INCLUDE_DIRECTORIES "${WSE_VENDOR_JPEG_DIR}/include")
	if(WIN32)
		foreach(wse_jpeg_file IN ITEMS
				"lib/turbojpeg.lib"
				"lib/turbojpegd.lib"
				"bin/turbojpeg.dll")
			if(NOT EXISTS "${WSE_VENDOR_JPEG_DIR}/${wse_jpeg_file}")
				message(FATAL_ERROR
					"Required libjpeg-turbo artifact is missing: ${WSE_VENDOR_JPEG_DIR}/${wse_jpeg_file}")
			endif()
		endforeach()
		# Both configurations load the same `turbojpeg.dll`; only the Import Library differs in
		# name. The install rules near the end of the file rely on this and stage one DLL.
		set_target_properties(WSE_TurboJpegBackend PROPERTIES
			IMPORTED_IMPLIB_DEBUG "${WSE_VENDOR_JPEG_DIR}/lib/turbojpegd.lib"
			IMPORTED_IMPLIB_RELEASE "${WSE_VENDOR_JPEG_DIR}/lib/turbojpeg.lib"
			IMPORTED_LOCATION_DEBUG "${WSE_VENDOR_JPEG_DIR}/bin/turbojpeg.dll"
			IMPORTED_LOCATION_RELEASE "${WSE_VENDOR_JPEG_DIR}/bin/turbojpeg.dll")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		foreach(wse_jpeg_config IN ITEMS debug release)
			if(NOT EXISTS "${WSE_VENDOR_JPEG_DIR}/lib/${wse_jpeg_config}/libturbojpeg.a")
				message(FATAL_ERROR
					"Required libjpeg-turbo archive is missing: "
					"${WSE_VENDOR_JPEG_DIR}/lib/${wse_jpeg_config}/libturbojpeg.a")
			endif()
		endforeach()
		set_target_properties(WSE_TurboJpegBackend PROPERTIES
			IMPORTED_LOCATION_DEBUG "${WSE_VENDOR_JPEG_DIR}/lib/debug/libturbojpeg.a"
			IMPORTED_LOCATION_RELEASE "${WSE_VENDOR_JPEG_DIR}/lib/release/libturbojpeg.a")
	endif()
endif()

