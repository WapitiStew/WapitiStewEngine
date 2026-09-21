# @file engine/wse/test/consumer/CMakeLists.txt
# @brief Installed WSE packageを外部Projectとして消費する互換性Test。

cmake_minimum_required(VERSION 3.24)

project(WsePackageConsumer LANGUAGES CXX)

if(NOT DEFINED WSE_PACKAGE_ROOT OR WSE_PACKAGE_ROOT STREQUAL "")
	message(FATAL_ERROR "WSE_PACKAGE_ROOT must point to an installed WSE package.")
endif()

option(WSE_EXPECT_ALL_COMPONENTS "Require every optional WSE component target" OFF)
option(WSE_EXPECT_PUBLIC_COMPONENTS
	"Require every public optional WSE component and reject private VPJ" OFF)
option(WSE_EXPECT_CORE_ONLY "Require every optional WSE component target to be absent" OFF)
option(WSE_EXPECT_XPT_ONLY "Require XPT and require every other optional target to be absent" OFF)
option(WSE_EXPECT_GEF_ONLY "Require GEF and require every other optional target to be absent" OFF)
option(WSE_EXPECT_IUI_ONLY "Require IUI and require every other optional target to be absent" OFF)
option(WSE_EXPECT_OUI_ONLY "Require OUI and require every other optional target to be absent" OFF)
option(WSE_EXPECT_TMR_ONLY "Require Tmr and require every other optional target to be absent" OFF)
option(WSE_EXPECT_VPJ_CONTROL
	"Require the portable controlled VPJ profile/control package shape" OFF)
option(WSE_EXPECT_VPJ_VIDEO
	"Require the portable controlled VPJ LAN-video package shape" OFF)

set(wse_selected_package_shapes "")
foreach(wse_package_shape IN ITEMS
		ALL_COMPONENTS PUBLIC_COMPONENTS CORE_ONLY XPT_ONLY GEF_ONLY IUI_ONLY OUI_ONLY TMR_ONLY VPJ_CONTROL VPJ_VIDEO)
	if(WSE_EXPECT_${wse_package_shape})
		list(APPEND wse_selected_package_shapes "${wse_package_shape}")
	endif()
endforeach()
list(LENGTH wse_selected_package_shapes wse_selected_package_shape_count)
if(wse_selected_package_shape_count GREATER 1)
	message(FATAL_ERROR
		"Select only one WSE_EXPECT_* package shape.")
endif()

find_package(WonderStewEngine CONFIG REQUIRED
	PATHS "${WSE_PACKAGE_ROOT}/cmake"
	NO_DEFAULT_PATH
)

if(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "SHARED")
	foreach(wse_private_backend IN ITEMS
			WSE::CurlBackend WSE::TurboJpegBackend WSE_TurboJpegBackend)
		if(TARGET ${wse_private_backend})
			message(FATAL_ERROR
				"Shared WSE package exposes a private backend target: ${wse_private_backend}")
		endif()
	endforeach()
elseif(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "STATIC")
	if(WSE_PACKAGE_HAS_XPT AND NOT TARGET WSE::CurlBackend)
		message(FATAL_ERROR "Static XPT package is missing its private curl link backend.")
	endif()
	if(WSE_PACKAGE_HAS_VPJ_VIDEO)
		foreach(wse_private_vpj_backend IN ITEMS
				WSE::TurboJpegBackend WSE_TurboJpegBackend)
			if(NOT TARGET ${wse_private_vpj_backend})
				message(FATAL_ERROR
					"Static VPJ package is missing its private JPEG link backend: "
					"${wse_private_vpj_backend}")
			endif()
		endforeach()
	endif()
endif()


file(GLOB_RECURSE wse_public_package_metadata
	"${WSE_PACKAGE_ROOT}/cmake/*"
	"${WSE_PACKAGE_ROOT}/include/*"
	"${WSE_PACKAGE_ROOT}/wse-package.json")
foreach(wse_public_metadata_file IN LISTS wse_public_package_metadata)
	if(NOT IS_DIRECTORY "${wse_public_metadata_file}")
		file(READ "${wse_public_metadata_file}" wse_public_metadata_text)
		string(TOLOWER "${wse_public_metadata_text}" wse_public_metadata_lower)
		if(wse_public_metadata_lower MATCHES "cpprest")
			message(FATAL_ERROR
				"WSE package metadata exposes the retired HTTP backend: "
				"${wse_public_metadata_file}")
		endif()
		# "vpj" is a public component name (owner decision, 2026-09-08); only device identity
		# such as the dedicated-camera name stays out of every public package shape.
		if(NOT WSE_EXPECT_ALL_COMPONENTS AND NOT WSE_EXPECT_VPJ_CONTROL
				AND NOT WSE_EXPECT_VPJ_VIDEO)
			if(wse_public_metadata_lower MATCHES "private-test-marker-1")
				message(FATAL_ERROR
					"Public WSE package metadata exposes a private component name: "
					"${wse_public_metadata_file}")
			endif()
		endif()
	endif()
endforeach()
if(EXISTS "${WSE_PACKAGE_ROOT}/vendor/cpprest")
	message(FATAL_ERROR "WSE package contains the retired cpprest vendor dependency.")
endif()

if(NOT TARGET WSE::Core)
	message(FATAL_ERROR "Required WSE target is missing: WSE::Core")
endif()
# The consumer-facing compatibility alias was removed by the legacy removal program and must
# not silently return through a package regression.
if(TARGET WonderStewEngine)
	message(FATAL_ERROR
		"The installed package must not define the removed WonderStewEngine compatibility alias.")
endif()

if(WSE_EXPECT_VPJ_VIDEO OR WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_ALL_COMPONENTS)
	foreach(wse_vpj_video_target IN ITEMS WSE::Xpt WSE::Vpj WSE::VpjVideo)
		if(NOT TARGET ${wse_vpj_video_target})
			message(FATAL_ERROR "Expected LAN-video target is missing: ${wse_vpj_video_target}")
		endif()
	endforeach()
	get_target_property(wse_vpj_link_libraries WSE::Vpj INTERFACE_LINK_LIBRARIES)
	if(NOT "WSE::Xpt" IN_LIST wse_vpj_link_libraries)
		message(FATAL_ERROR
			"WSE::Vpj must expose its generic XPT dependency transitively.")
	endif()
	foreach(wse_forbidden_vpj_dependency IN ITEMS WSE::Iui WSE::Oui WSE::Tmr)
		if("${wse_forbidden_vpj_dependency}" IN_LIST wse_vpj_link_libraries)
			message(FATAL_ERROR
				"WSE::Vpj exposes a forbidden optional dependency: "
				"${wse_forbidden_vpj_dependency}")
		endif()
	endforeach()
	if(NOT WSE_PACKAGE_HAS_VPJ_VIDEO)
		message(FATAL_ERROR "VPJ package metadata does not include its LAN-video API.")
	endif()
	if(DEFINED WSE_PACKAGE_VPJ_HAS_LEGACY_RUNTIME
			OR DEFINED WSE_PACKAGE_VPJ_HTTP_RUNTIME_DLL_RELEASE
			OR DEFINED WSE_PACKAGE_VPJ_HTTP_RUNTIME_DLL_DEBUG)
		message(FATAL_ERROR "VPJ package exposes retired cpprest runtime metadata.")
	endif()
	foreach(wse_vpj_video_header IN ITEMS
			"${WSE_PACKAGE_ROOT}/include/wse/api/vpj/video/stew.h"
			"${WSE_PACKAGE_ROOT}/include/wse/api/vpj/video/LanVideoSession.h")
		if(NOT EXISTS "${wse_vpj_video_header}")
			message(FATAL_ERROR "Expected LAN-video header is missing: ${wse_vpj_video_header}")
		endif()
	endforeach()
elseif(WSE_EXPECT_PUBLIC_COMPONENTS OR WSE_EXPECT_CORE_ONLY
		OR WSE_EXPECT_XPT_ONLY OR WSE_EXPECT_GEF_ONLY OR WSE_EXPECT_IUI_ONLY
		OR WSE_EXPECT_OUI_ONLY OR WSE_EXPECT_TMR_ONLY)
	if(TARGET WSE::VpjVideo OR WSE_PACKAGE_HAS_VPJ_VIDEO)
		message(FATAL_ERROR "Package shape unexpectedly exports WSE::VpjVideo.")
	endif()
endif()

foreach(wse_component IN ITEMS Xpt Gef Iui Oui Tmr Vpj)
	string(TOUPPER "${wse_component}" wse_component_upper)
	string(TOLOWER "${wse_component}" wse_component_lower)
	set(wse_component_target "WSE::${wse_component}")
	set(wse_component_header
		"${WSE_PACKAGE_ROOT}/include/wse/api/${wse_component_lower}/stew.h")

	if(WSE_EXPECT_ALL_COMPONENTS
			OR (WSE_EXPECT_PUBLIC_COMPONENTS AND NOT wse_component STREQUAL "Vpj")
			OR (WSE_EXPECT_XPT_ONLY AND wse_component STREQUAL "Xpt")
			OR (WSE_EXPECT_GEF_ONLY AND wse_component STREQUAL "Gef")
			OR (WSE_EXPECT_IUI_ONLY AND wse_component STREQUAL "Iui")
			OR (WSE_EXPECT_OUI_ONLY AND wse_component STREQUAL "Oui")
			OR (WSE_EXPECT_TMR_ONLY AND wse_component STREQUAL "Tmr")
			OR (WSE_EXPECT_VPJ_CONTROL AND
				(wse_component STREQUAL "Xpt" OR wse_component STREQUAL "Vpj"))
			OR (WSE_EXPECT_VPJ_VIDEO AND
				(wse_component STREQUAL "Xpt" OR wse_component STREQUAL "Vpj")))
		if(NOT TARGET ${wse_component_target})
			message(FATAL_ERROR "Expected WSE component target is missing: ${wse_component_target}")
		endif()
		if(NOT WSE_PACKAGE_HAS_${wse_component_upper})
			message(FATAL_ERROR
				"Package metadata does not enable component: ${wse_component_upper}")
		endif()
		if(NOT EXISTS "${wse_component_header}")
			message(FATAL_ERROR
				"Expected WSE component header is missing: ${wse_component_header}")
		endif()
	elseif(WSE_EXPECT_PUBLIC_COMPONENTS OR WSE_EXPECT_CORE_ONLY
			OR WSE_EXPECT_XPT_ONLY OR WSE_EXPECT_GEF_ONLY OR WSE_EXPECT_IUI_ONLY
			OR WSE_EXPECT_OUI_ONLY
			OR WSE_EXPECT_TMR_ONLY OR WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_VPJ_VIDEO)
		if(TARGET ${wse_component_target})
			message(FATAL_ERROR
				"Package shape unexpectedly exports: ${wse_component_target}")
		endif()
		if(WSE_PACKAGE_HAS_${wse_component_upper})
			message(FATAL_ERROR
				"Core-only package metadata unexpectedly enables: ${wse_component_upper}")
		endif()
		if(EXISTS "${wse_component_header}")
			message(FATAL_ERROR
				"Core-only package unexpectedly contains: ${wse_component_header}")
		endif()
	endif()
endforeach()

set(WSE_CONSUMER_COMPONENT_TARGETS "")
if(WSE_EXPECT_ALL_COMPONENTS)
	set(WSE_CONSUMER_COMPONENT_TARGETS
		WSE::Xpt
		WSE::Gef
		WSE::Iui
		WSE::Oui
		WSE::Tmr
		WSE::Vpj
	)
elseif(WSE_EXPECT_PUBLIC_COMPONENTS)
	set(WSE_CONSUMER_COMPONENT_TARGETS
		WSE::Xpt
		WSE::Gef
		WSE::Iui
		WSE::Oui
		WSE::Tmr
	)
elseif(WSE_EXPECT_XPT_ONLY)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Xpt)
elseif(WSE_EXPECT_GEF_ONLY)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Gef)
elseif(WSE_EXPECT_IUI_ONLY)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Iui)
elseif(WSE_EXPECT_OUI_ONLY)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Oui)
elseif(WSE_EXPECT_TMR_ONLY)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Tmr)
elseif(WSE_EXPECT_VPJ_CONTROL)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Vpj)
elseif(WSE_EXPECT_VPJ_VIDEO)
	set(WSE_CONSUMER_COMPONENT_TARGETS WSE::Vpj)
endif()

if(WSE_EXPECT_ALL_COMPONENTS)
	set(wse_expect_libcurl TRUE)
elseif(WSE_EXPECT_PUBLIC_COMPONENTS OR WSE_EXPECT_XPT_ONLY OR WSE_EXPECT_VPJ_CONTROL
		OR WSE_EXPECT_VPJ_VIDEO)
	set(wse_expect_libcurl TRUE)
else()
	set(wse_expect_libcurl FALSE)
endif()

if(WSE_EXPECT_ALL_COMPONENTS OR WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_VPJ_VIDEO)
	foreach(wse_vpj_profile_dependency IN ITEMS
			"${WSE_PACKAGE_ROOT}/vendor/nlohmann-json/LICENSE.MIT"
			"${WSE_PACKAGE_ROOT}/vendor/nlohmann-json/wse-dependency.json"
			"${WSE_PACKAGE_ROOT}/share/wse/vpj/model-profile.schema.json"
			"${WSE_PACKAGE_ROOT}/share/wse/vpj/model-profiles/v1")
		if(NOT EXISTS "${wse_vpj_profile_dependency}")
			message(FATAL_ERROR
				"VPJ profile package dependency is missing: ${wse_vpj_profile_dependency}")
		endif()
	endforeach()
endif()

if(WSE_EXPECT_VPJ_VIDEO OR WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_ALL_COMPONENTS)
	foreach(wse_vpj_video_dependency IN ITEMS
			"${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/include/turbojpeg.h"
			"${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/LICENSE.md"
			"${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/wse-dependency.json")
		if(NOT EXISTS "${wse_vpj_video_dependency}")
			message(FATAL_ERROR
				"VPJ LAN-video package dependency is missing: ${wse_vpj_video_dependency}")
		endif()
	endforeach()
endif()

if(wse_expect_libcurl)
	foreach(wse_libcurl_dependency IN ITEMS
		"${WSE_PACKAGE_ROOT}/vendor/libcurl/include/curl/curl.h"
		"${WSE_PACKAGE_ROOT}/vendor/libcurl/COPYING"
		"${WSE_PACKAGE_ROOT}/vendor/libcurl/wse-dependency.json")
		if(NOT EXISTS "${wse_libcurl_dependency}")
			message(FATAL_ERROR "XPT package dependency is missing: ${wse_libcurl_dependency}")
		endif()
	endforeach()

	if(WSE_PACKAGE_BUNDLES_OPENSSL)
		foreach(wse_openssl_dependency IN ITEMS
				"${WSE_PACKAGE_ROOT}/vendor/openssl/include/openssl/ssl.h"
				"${WSE_PACKAGE_ROOT}/vendor/openssl/lib/libssl.a"
				"${WSE_PACKAGE_ROOT}/vendor/openssl/lib/libcrypto.a"
				"${WSE_PACKAGE_ROOT}/vendor/openssl/LICENSE.txt"
				"${WSE_PACKAGE_ROOT}/vendor/openssl/wse-dependency.json")
			if(NOT EXISTS "${wse_openssl_dependency}")
				message(FATAL_ERROR
					"Bundled OpenSSL dependency is missing: ${wse_openssl_dependency}")
			endif()
		endforeach()
	endif()
endif()

if(WSE_EXPECT_TMR_ONLY AND WSE_PACKAGE_BUNDLES_LIBCAMERA)
	foreach(wse_libcamera_dependency IN ITEMS
			"${WSE_PACKAGE_ROOT}/vendor/libcamera/lib/libcamera.so"
			"${WSE_PACKAGE_ROOT}/vendor/libcamera/lib/libcamera-base.so"
			"${WSE_PACKAGE_ROOT}/vendor/libcamera/LICENSES/LGPL-2.1-or-later.txt"
			"${WSE_PACKAGE_ROOT}/vendor/libcamera/wse-dependency.json")
		if(NOT EXISTS "${wse_libcamera_dependency}")
			message(FATAL_ERROR
				"Tmr package dependency is missing: ${wse_libcamera_dependency}")
		endif()
	endforeach()
endif()

if(WSE_EXPECT_PUBLIC_COMPONENTS)
	foreach(wse_private_dependency IN ITEMS
			"${WSE_PACKAGE_ROOT}/vendor/cpprest"
			"${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo")
		if(EXISTS "${wse_private_dependency}")
			message(FATAL_ERROR
				"Public all-optional package contains a private dependency: ${wse_private_dependency}")
		endif()
	endforeach()
endif()

if(WSE_EXPECT_CORE_ONLY OR WSE_EXPECT_GEF_ONLY OR WSE_EXPECT_IUI_ONLY
		OR WSE_EXPECT_OUI_ONLY OR WSE_EXPECT_TMR_ONLY)
	foreach(wse_disabled_dependency IN ITEMS
		"${WSE_PACKAGE_ROOT}/vendor/cpprest"
		"${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo"
		"${WSE_PACKAGE_ROOT}/vendor/libcurl"
		"${WSE_PACKAGE_ROOT}/vendor/openssl"
		"${WSE_PACKAGE_ROOT}/vendor/libcamera")
		if(WSE_EXPECT_TMR_ONLY AND WSE_PACKAGE_BUNDLES_LIBCAMERA
				AND wse_disabled_dependency MATCHES "/libcamera$")
			continue()
		endif()
		if(EXISTS "${wse_disabled_dependency}")
			message(FATAL_ERROR
				"Package without transport/private components unexpectedly contains: ${wse_disabled_dependency}")
		endif()
	endforeach()

	if(DEFINED WSE_PACKAGE_HAS_VPJ
			OR DEFINED WSE_PACKAGE_VPJ_HTTP_RUNTIME_DLL_RELEASE
			OR DEFINED WSE_PACKAGE_VPJ_HTTP_RUNTIME_DLL_DEBUG
			OR DEFINED WSE_PACKAGE_TURBOJPEG_DLL)
		message(FATAL_ERROR "Package exposes disabled VPJ runtime dependencies.")
	endif()
endif()

function(wse_add_consumer_test target_name)
	add_executable(${target_name} main.cpp ApplicationLog.cpp ApplicationLogTest.cpp)
	target_link_libraries(${target_name} PRIVATE
		${ARGN}
	)
	target_compile_features(${target_name} PRIVATE cxx_std_17)
	target_compile_options(${target_name} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/we4996>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror=deprecated-declarations>
	)
	if(WSE_EXPECT_ALL_COMPONENTS)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_ALL_COMPONENTS=1)
	endif()
	if(WSE_EXPECT_PUBLIC_COMPONENTS)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS=1)
	endif()
	if(WSE_EXPECT_XPT_ONLY)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_XPT=1)
	endif()
	if(WSE_EXPECT_GEF_ONLY)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_GEF=1)
	endif()
	if(WSE_EXPECT_IUI_ONLY)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_IUI=1)
	endif()
	if(WSE_EXPECT_OUI_ONLY)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_OUI=1)
	endif()
	if(WSE_EXPECT_TMR_ONLY)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_TMR=1)
	endif()
	if(WSE_EXPECT_VPJ_CONTROL)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_VPJ_CONTROL=1)
	endif()
	if(WSE_EXPECT_VPJ_VIDEO)
		target_compile_definitions(${target_name} PRIVATE
			WSE_CONSUMER_INCLUDE_VPJ_VIDEO=1)
	endif()

	if(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "SHARED")
		add_custom_command(TARGET ${target_name} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:WSE::Core>"
				"$<TARGET_FILE_DIR:${target_name}>"
		)
	endif()

	if(WIN32 AND WSE_PACKAGE_HAS_VPJ_VIDEO)
		add_custom_command(TARGET ${target_name} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${WSE_PACKAGE_TURBOJPEG_DLL}"
				"$<TARGET_FILE_DIR:${target_name}>")
	endif()

	add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

enable_testing()
set(WSE_MODERN_CONSUMER_LINK_TARGETS WSE::Core ${WSE_CONSUMER_COMPONENT_TARGETS})
if(WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_VPJ_VIDEO)
	# The modern VPJ contract deliberately spells only the highest-level target.
	# XPT and Core must arrive through WSE::Vpj's transitive interface.
	set(WSE_MODERN_CONSUMER_LINK_TARGETS WSE::Vpj)
endif()
wse_add_consumer_test(wse_modern_target_consumer
	${WSE_MODERN_CONSUMER_LINK_TARGETS})

if(WSE_EXPECT_VPJ_CONTROL OR WSE_EXPECT_VPJ_VIDEO)
	get_target_property(wse_modern_vpj_links
		wse_modern_target_consumer LINK_LIBRARIES)
	if(NOT wse_modern_vpj_links STREQUAL "WSE::Vpj")
		message(FATAL_ERROR
			"Modern VPJ consumer must explicitly link only WSE::Vpj; got: "
			"${wse_modern_vpj_links}")
	endif()
endif()
