# @file cmake/WseCore.cmake
# @brief Build output layout, component source lists, the library target and its facades, and compile/link configuration.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

# ------------------------------------------------------------------------------------------------
# Build output layout.
# ------------------------------------------------------------------------------------------------

# The one place a reader of a Configure log can see which Components and bindings this Build tree
# actually contains, without reading the Cache back.
message(STATUS
	"WSE ${WSE_VERSION}: ${WSE_LIBRARY_TYPE}; "
	"XPT=${WSE_BUILD_XPT}; GEF=${WSE_BUILD_GEF}; IUI=${WSE_BUILD_IUI}; "
	"OUI=${WSE_BUILD_OUI}; TMR=${WSE_BUILD_TMR}; VPJ=${WSE_BUILD_VPJ}; "
	"VPJ_VIDEO=${WSE_BUILD_VPJ_VIDEO}; "
	"NODE=${WSE_BUILD_NODE_BINDING}; PYTHON=${WSE_BUILD_PYTHON_BINDING}; "
	"JAVA=${WSE_BUILD_JAVA_BINDING}; DOTNET=${WSE_BUILD_DOTNET_BINDING}")
message(STATUS "WSE dependency root: ${WSE_DEPENDENCY_ROOT}")

# Every Target below writes into `<build>/${WSE_VS_PLATFORM_DIR}/<config>`, so the Library, the
# bindings, and the Test executables all land in one directory per architecture and configuration.
# That is what lets a Test find the Runtime DLL beside itself, and what lets an x86-64 and an ARM64
# Build share a source tree without overwriting each other.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" wse_system_processor)
	if(wse_system_processor MATCHES "^(aarch64|arm64)$")
		set(WSE_VS_PLATFORM_DIR "linux-arm64")
	elseif(wse_system_processor MATCHES "^(x86_64|amd64)$")
		set(WSE_VS_PLATFORM_DIR "linux-x86_64")
	else()
		message(FATAL_ERROR
			"WSE Linux Core currently supports x86-64 and ARM64, but the target processor is "
			"'${CMAKE_SYSTEM_PROCESSOR}'.")
	endif()
elseif(CMAKE_GENERATOR_PLATFORM)
	set(WSE_VS_PLATFORM_DIR "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME_DEFAULT)
	set(WSE_VS_PLATFORM_DIR "${CMAKE_VS_PLATFORM_NAME_DEFAULT}")
else()
	set(WSE_VS_PLATFORM_DIR "x64")
endif()

# ------------------------------------------------------------------------------------------------
# Source lists.
# ------------------------------------------------------------------------------------------------

# Headers, shaders, icons, and documents carried by the Target purely so they appear in the
# generated IDE project. set_source_files_properties() below marks them HEADER_FILE_ONLY, so
# nothing here reaches the compiler.
file(GLOB_RECURSE WSE_IDE_FILES
	CONFIGURE_DEPENDS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	api/*.h
	api/*.hpp
	core/*.h
	core/*.hpp
	platform/*.h
	platform/*.hpp
	resource/*.h
	resource/*.hpp
	resource/*.ico
	resource/*.hlsl
	resource/*.hlsli
	doc/*.md
	doc/*.txt
)

# One list per Component, each holding the portable `core/` sources and then the `platform/`
# sources for whichever operating system is being targeted. Only the lists whose Option is on are
# folded into WSE_SOURCES further down, which is what makes a Component genuinely absent from the
# Library rather than merely unreachable. Core is unconditional.
set(WSE_CORE_SOURCES
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
	core/wse/license/wse_LicenceAdmin.cpp
	core/wse/license/wse_License.cpp
	core/wse/license/wse_LicenseKey.cpp
	core/wse/license/wse_LicenseWriter.cpp
	core/wse/error/CoreError.cpp
	core/wse/error/LicenseError.cpp
	core/wse/utility/wse_DataCast.cpp
	core/wse/utility/wse_Log.cpp
	core/wse/utility/wse_Timer.cpp
	core/wse/utility/wse_Wait.cpp
	core/wse/utility/wse_WorkerController.cpp
)

if(WIN32)
	list(APPEND WSE_CORE_SOURCES
		platform/wse/win/license/mac_addless.cpp
		platform/wse/win/utility/wse_DevicePickup.cpp
		platform/wse/win/utility/wse_LogOutput.cpp
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND WSE_CORE_SOURCES
		platform/wse/linux/license/mac_addless.cpp
		platform/wse/linux/utility/wse_DevicePickup.cpp
	)
endif()

set(WSE_XPT_SOURCES
	core/xpt/binding/XptErrorAdapter.cpp
	core/xpt/error/TransportError.cpp
	core/xpt/network/Endpoint.cpp
	core/xpt/operation/Cancellation.cpp
	core/xpt/operation/OperationContext.cpp
	core/xpt/retry/RetryPolicy.cpp
	platform/xpt/http/HttpClient.cpp
	platform/xpt/network/TcpClient.cpp
	platform/xpt/network/UdpClient.cpp
	platform/xpt/serial/SerialPort.cpp
)

set(WSE_GEF_SOURCES
	core/gef/bin/binController.cpp
	core/gef/csv/csvController.cpp
	core/gef/error/GefError.cpp
)

# IUI has no portable implementation at all, so unlike the lists above this one starts empty and is
# filled only when one of the two supported platforms is selected.
set(WSE_IUI_SOURCES "")
if(WSE_BUILD_IUI AND WIN32)
	list(APPEND WSE_IUI_SOURCES
		core/iui/binding/IuiErrorAdapter.cpp
		platform/iui/win/device/Keyboard.cpp
	)
elseif(WSE_BUILD_IUI AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND WSE_IUI_SOURCES
		core/iui/binding/IuiErrorAdapter.cpp
		platform/iui/linux/device/Keyboard.cpp
	)
endif()

set(WSE_OUI_SOURCES
	core/oui/binding/Projection.cpp
	core/oui/binding/OuiErrorAdapter.cpp
	core/oui/renderer/ProjectionMeshAdapter.cpp
	core/oui/renderer/ProjectionPipeline.cpp
	core/oui/renderer/Renderer.cpp
	core/oui/renderer/RendererError.cpp
	core/oui/renderer/RendererFrameOps.cpp
	core/oui/renderer/RendererTypes.cpp
)

if(WSE_BUILD_OUI AND WIN32)
	list(APPEND WSE_OUI_SOURCES
		platform/oui/win/renderer/D3D12DisplayEnumeration.cpp
		platform/oui/win/renderer/D3D12RendererBackend.cpp
		platform/oui/win/renderer/Win32WindowTransition.cpp
	)
# The Linux OUI Backend cannot be compiled from what is in the tree alone. Two things are generated
# first: the xdg-shell client glue, produced by wayland-scanner from the protocol XML the system's
# wayland-protocols installs, and the two portable shaders compiled to SPIR-V byte arrays that the
# renderer embeds. Both land in the Build tree and join the source list at the end of this block.
elseif(WSE_BUILD_OUI AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(Vulkan 1.2 REQUIRED)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(WSE_WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
	pkg_check_modules(WSE_DRM REQUIRED IMPORTED_TARGET libdrm)
	find_program(WSE_WAYLAND_SCANNER wayland-scanner REQUIRED)
	find_program(WSE_GLSLANG_VALIDATOR glslangValidator REQUIRED)
	pkg_get_variable(WSE_WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
	if(WSE_WAYLAND_PROTOCOLS_DIR STREQUAL "")
		message(FATAL_ERROR "wayland-protocols pkgdatadir is unavailable.")
	endif()
	set(WSE_XDG_SHELL_PROTOCOL
		"${WSE_WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
	set(WSE_OUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/oui")
	file(MAKE_DIRECTORY "${WSE_OUI_GENERATED_DIR}")
	set(WSE_XDG_SHELL_HEADER
		"${WSE_OUI_GENERATED_DIR}/xdg-shell-client-protocol.h")
	set(WSE_XDG_SHELL_CODE
		"${WSE_OUI_GENERATED_DIR}/xdg-shell-protocol.c")
	add_custom_command(
		OUTPUT "${WSE_XDG_SHELL_HEADER}"
		COMMAND "${WSE_WAYLAND_SCANNER}" client-header
			"${WSE_XDG_SHELL_PROTOCOL}" "${WSE_XDG_SHELL_HEADER}"
		DEPENDS "${WSE_XDG_SHELL_PROTOCOL}"
		VERBATIM
	)
	add_custom_command(
		OUTPUT "${WSE_XDG_SHELL_CODE}"
		COMMAND "${WSE_WAYLAND_SCANNER}" private-code
			"${WSE_XDG_SHELL_PROTOCOL}" "${WSE_XDG_SHELL_CODE}"
		DEPENDS "${WSE_XDG_SHELL_PROTOCOL}"
		VERBATIM
	)
	set(WSE_VULKAN_VERTEX_HEADER
		"${WSE_OUI_GENERATED_DIR}/wse_portable_vertex_shader.h")
	set(WSE_VULKAN_FRAGMENT_HEADER
		"${WSE_OUI_GENERATED_DIR}/wse_portable_fragment_shader.h")
	add_custom_command(
		OUTPUT "${WSE_VULKAN_VERTEX_HEADER}"
		COMMAND "${WSE_GLSLANG_VALIDATOR}" -V --target-env vulkan1.2
			--vn wse_portable_vertex_shader -o "${WSE_VULKAN_VERTEX_HEADER}"
			"${CMAKE_CURRENT_SOURCE_DIR}/resource/vulkan/PortableRenderer.vert"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resource/vulkan/PortableRenderer.vert"
		VERBATIM
	)
	add_custom_command(
		OUTPUT "${WSE_VULKAN_FRAGMENT_HEADER}"
		COMMAND "${WSE_GLSLANG_VALIDATOR}" -V --target-env vulkan1.2
			--vn wse_portable_fragment_shader -o "${WSE_VULKAN_FRAGMENT_HEADER}"
			"${CMAKE_CURRENT_SOURCE_DIR}/resource/vulkan/PortableRenderer.frag"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resource/vulkan/PortableRenderer.frag"
		VERBATIM
	)
	# None of the four exist when CMake reads the source list, so they are marked GENERATED to keep
	# the generator from rejecting them as missing files.
	set_source_files_properties(
		"${WSE_XDG_SHELL_HEADER}"
		"${WSE_XDG_SHELL_CODE}"
		"${WSE_VULKAN_VERTEX_HEADER}"
		"${WSE_VULKAN_FRAGMENT_HEADER}"
		PROPERTIES GENERATED TRUE
	)
	list(APPEND WSE_OUI_SOURCES
		platform/oui/linux/renderer/VulkanRendererBackend.cpp
		platform/oui/linux/renderer/WaylandWindow.cpp
		"${WSE_XDG_SHELL_HEADER}"
		"${WSE_XDG_SHELL_CODE}"
		"${WSE_VULKAN_VERTEX_HEADER}"
		"${WSE_VULKAN_FRAGMENT_HEADER}"
	)
endif()

set(WSE_TMR_SOURCES
	core/tmr/binding/TmrErrorAdapter.cpp
	core/tmr/calibration/CalibrationData.cpp
	core/tmr/calibration/CameraCalibration.cpp
	core/tmr/camera/Camera.cpp
	core/tmr/camera/CameraError.cpp
	core/tmr/camera/CameraFrameOps.cpp
	core/tmr/camera/CameraTypes.cpp
	core/tmr/data/Parameter.cpp
	core/tmr/device/WebCamera.cpp
)

if(WIN32)
	list(APPEND WSE_TMR_SOURCES
		platform/tmr/win/camera/MediaFoundationCameraBackend.cpp
		platform/tmr/win/camera/DirectShowRawCapture.cpp
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND WSE_TMR_SOURCES
		platform/tmr/linux/camera/LibcameraCameraBackend.cpp
		platform/tmr/linux/camera/LinuxCameraBackend.cpp
		platform/tmr/linux/camera/V4L2CameraBackend.cpp
	)
	# The libcamera headers need C++20. Raising the standard for this one translation unit keeps
	# the rest of the Library, and the floor the public headers promise, at C++17.
	if(WSE_ENABLE_LIBCAMERA)
		set_source_files_properties(
			platform/tmr/linux/camera/LibcameraCameraBackend.cpp
			PROPERTIES COMPILE_OPTIONS "-std=c++20")
	endif()
endif()

# VPJの実装はExtension Overlayが供給する。Componentの選択と第三者Libraryの調達は
# このRepositoryが引き続き担い、Overlayは下のHookでこの一覧へ自分の.cppを足す。
set(WSE_VPJ_SOURCES)

# Overlayの読み込みは2段である。CMakeのConfigureにはSourceを決める時期とTargetを
# 拡張できる時期があり、片方だけではOverlayは自分を組み込めない。ここが1段目で、
# Overlayはこの時点でSource一覧へ自分の.cppを足し、自分が供給するComponentを宣言する。
# 2段目はTargetがすべて出来た後にあり、Test登録とinstall規則を担う。
if(WSE_EXTENSION_ROOT)
	if(NOT EXISTS "${WSE_EXTENSION_ROOT}/wse-extension.cmake")
		message(FATAL_ERROR
			"WSE_EXTENSION_ROOT is set to \"${WSE_EXTENSION_ROOT}\" but that directory has no "
			"wse-extension.cmake.")
	endif()
	message(STATUS "WSE extension overlay: ${WSE_EXTENSION_ROOT}")
	include("${WSE_EXTENSION_ROOT}/wse-extension.cmake")
endif()

# ------------------------------------------------------------------------------------------------
# The library target and its component facades.
# ------------------------------------------------------------------------------------------------

# Core is always present; the rest join only if their Option survived the consistency block above.
# This is also the last point at which a source can be added, which is why the Overlay's first
# stage runs immediately before it.
set(WSE_SOURCES ${WSE_CORE_SOURCES})

foreach(wse_component IN ITEMS XPT GEF IUI OUI TMR VPJ)
	if(WSE_BUILD_${wse_component})
		list(APPEND WSE_SOURCES ${WSE_${wse_component}_SOURCES})
	endif()
endforeach()

# The version resource describes a binary that has version information, which a static archive
# does not.
if(WSE_LIBRARY_TYPE STREQUAL "SHARED" AND WIN32)
	list(APPEND WSE_SOURCES WonderStewEngine.rc)
endif()

# The IDE glob and a Component's source list can name the same file, and add_library() rejects a
# file listed twice.
set(WSE_PROJECT_FILES
	${WSE_SOURCES}
	${WSE_IDE_FILES}
)

list(REMOVE_DUPLICATES WSE_PROJECT_FILES)

# The single compiled artifact. Every enabled Component is inside it; the `WSE::<Name>` Targets
# created below are INTERFACE facades that carry a feature macro and a Link edge, not separate
# libraries. A Consumer therefore links one facade and receives the whole Library behind it.
add_library(WonderStewEngine ${WSE_LIBRARY_TYPE} ${WSE_PROJECT_FILES})
add_library(WSE::Core ALIAS WonderStewEngine)

set_target_properties(WonderStewEngine PROPERTIES EXPORT_NAME Core)

# Accumulates what install(EXPORT) will publish, so a Consumer of the package sees exactly the
# Targets this configuration built.
set(WSE_EXPORT_TARGETS WonderStewEngine)

foreach(wse_component IN ITEMS Xpt Gef Iui Oui Tmr Vpj)
	string(TOUPPER "${wse_component}" wse_component_upper)
	if(WSE_BUILD_${wse_component_upper})
		set(wse_component_target "WSE_${wse_component}")
		add_library(${wse_component_target} INTERFACE)
		if(wse_component STREQUAL "Vpj")
			# VPJ may consume the generic Core/XPT/GEF boundary, but must not
			# acquire UI, input, or camera modules transitively. XPT already
			# carries the monolithic Core target, so consumers still need only
			# one public link entry: WSE::Vpj.
			target_link_libraries(${wse_component_target} INTERFACE WSE_Xpt)
		else()
			target_link_libraries(${wse_component_target} INTERFACE WonderStewEngine)
		endif()
		target_compile_definitions(${wse_component_target}
			INTERFACE "WSE_HAS_${wse_component_upper}=1")
		set_target_properties(${wse_component_target}
			PROPERTIES EXPORT_NAME "${wse_component}")
		add_library(WSE::${wse_component} ALIAS ${wse_component_target})
		list(APPEND WSE_EXPORT_TARGETS ${wse_component_target})
	endif()
endforeach()

# A separate facade kept alive so a Consumer that linked WSE::VpjVideo before LAN video was folded
# into VPJ still resolves. It adds only the compatibility macro on top of WSE::Vpj.
if(WSE_BUILD_VPJ)
	add_library(WSE_VpjVideo INTERFACE)
	target_link_libraries(WSE_VpjVideo INTERFACE WSE_Vpj)
	target_compile_definitions(WSE_VpjVideo INTERFACE WSE_HAS_VPJ_VIDEO=1)
	set_target_properties(WSE_VpjVideo PROPERTIES EXPORT_NAME VpjVideo)
	add_library(WSE::VpjVideo ALIAS WSE_VpjVideo)
	list(APPEND WSE_EXPORT_TARGETS WSE_VpjVideo)
endif()

# The IDE files are on the Target for visibility only; this is what keeps the compiler off them.
set_source_files_properties(${WSE_IDE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Mirrors the directory tree into the IDE's folder view, collapsing `platform/<component>/win/` to
# `platform/<component>/` so a Windows implementation sits beside its portable counterpart instead
# of under an extra level that says nothing on a Windows-only Build.
foreach(wse_file IN LISTS WSE_PROJECT_FILES)
	get_filename_component(wse_group "${wse_file}" DIRECTORY)
	if(wse_group STREQUAL "")
		continue()
	endif()

	string(REPLACE "/" "\\" wse_group "${wse_group}")
	if(wse_group MATCHES "^platform\\\\([^\\\\]+)\\\\win\\\\(.+)$")
		set(wse_group "platform\\${CMAKE_MATCH_1}\\${CMAKE_MATCH_2}")
	elseif(wse_group MATCHES "^platform\\\\([^\\\\]+)\\\\win$")
		set(wse_group "platform\\${CMAKE_MATCH_1}")
	endif()

	source_group("${wse_group}" FILES "${wse_file}")
endforeach()

# ------------------------------------------------------------------------------------------------
# Library compile and link configuration.
# ------------------------------------------------------------------------------------------------

# Two include spellings are supported on purpose, and each needs its own entry per build shape: the
# component-relative `<tmr/device/WebCamera.h>` and the fully qualified `<wse/api/tmr/...>`. The
# install rules at the end of the file lay the package out to match, from one copy of each header.
target_include_directories(WonderStewEngine
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<INSTALL_INTERFACE:include/wse/api>
		$<INSTALL_INTERFACE:include>
)

# The vendored JSON and JPEG headers are an implementation detail of VPJ. Keeping them PRIVATE is
# what stops them from reaching a Consumer's include path through the Imported Target.
if(WSE_BUILD_VPJ)
	target_include_directories(WonderStewEngine
		PRIVATE
			${WSE_VENDOR_NLOHMANN_JSON_DIR}/single_include
	)
endif()

if(WSE_BUILD_VPJ)
	target_include_directories(WonderStewEngine PRIVATE ${WSE_VENDOR_JPEG_DIR}/include)
endif()

# PUBLIC because it is the floor a Consumer inherits: the public headers are written to C++17 and
# will not compile below it. The Linux libcamera adapter raises itself to C++20 on its own.
target_compile_features(WonderStewEngine PUBLIC cxx_std_17)
set_property(TARGET WonderStewEngine PROPERTY C_STANDARD 11)

target_compile_definitions(WonderStewEngine
	PRIVATE
	_LIB
		WSE_BINDING_RUNTIME_VERSION="${WSE_VERSION}"
		$<$<CONFIG:Debug>:_DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
	target_compile_definitions(WonderStewEngine PRIVATE UNICODE _UNICODE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_BUILD_OUI)
	target_include_directories(WonderStewEngine PRIVATE "${WSE_OUI_GENERATED_DIR}")
	target_link_libraries(WonderStewEngine PRIVATE
		Vulkan::Vulkan
		PkgConfig::WSE_WAYLAND
		PkgConfig::WSE_DRM
	)
endif()

# The export macro is PRIVATE because only this Library declares the symbols as exported, while
# WSE_STATIC is PUBLIC because a Consumer's copy of the headers has to stop declaring them as
# imported. Getting the visibility the wrong way round produces symbols that link nowhere.
if(WSE_LIBRARY_TYPE STREQUAL "SHARED")
	target_compile_definitions(WonderStewEngine PRIVATE __WSE_EXPORTS__)
else()
	target_compile_definitions(WonderStewEngine PUBLIC WSE_STATIC)
endif()

# Published PUBLIC so a Consumer's `#if WSE_HAS_...` agrees with what was actually built rather
# than with what the headers happen to declare.
foreach(wse_component IN ITEMS XPT GEF IUI OUI TMR VPJ)
	if(WSE_BUILD_${wse_component})
		target_compile_definitions(WonderStewEngine PUBLIC "WSE_HAS_${wse_component}=1")
	endif()
endforeach()

if(WSE_BUILD_VPJ)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_VPJ_VIDEO=1)
endif()

# XPT ships whole, so its transports are announced together rather than gated one by one.
if(WSE_BUILD_XPT)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_XPT_TCP=1)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_XPT_UDP=1)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_XPT_SERIAL=1)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_XPT_RETRY=1)
	target_compile_definitions(WonderStewEngine PUBLIC WSE_HAS_XPT_HTTP=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_BUILD_TMR AND WSE_ENABLE_LIBCAMERA)
	target_compile_definitions(WonderStewEngine PRIVATE WSE_HAS_LIBCAMERA=1)
	target_link_libraries(WonderStewEngine PRIVATE
		WSE_LibcameraRuntime WSE_LibcameraBaseRuntime)
	# The Build tree loads libcamera from where Bootstrap left it; the installed Library loads the
	# copy install() places under `vendor/libcamera/`. Both are relative to the consuming binary,
	# so neither depends on a system-wide libcamera being present or being the right version.
	set_property(TARGET WonderStewEngine APPEND PROPERTY BUILD_RPATH
		"${WSE_VENDOR_LIBCAMERA_DIR}/lib")
	set_property(TARGET WonderStewEngine APPEND PROPERTY INSTALL_RPATH
		"$ORIGIN/../vendor/libcamera/lib")
endif()

# /Z7 keeps Debug information inside each object file, so a parallel MSVC Build does not serialize
# on one PDB; /FS covers the writes that remain. /await is required by the coroutine-based code,
# and /utf-8 by sources that are UTF-8 without a byte-order mark.
target_compile_options(WonderStewEngine
	PRIVATE
	$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Z7>
	$<$<CXX_COMPILER_ID:MSVC>:/FS>
	$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/await>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

if(WSE_WARNING_SWEEP OR WSE_WARNINGS_AS_ERRORS)
	if(MSVC)
		target_compile_options(WonderStewEngine PRIVATE /W4)
	else()
		target_compile_options(WonderStewEngine PRIVATE
			-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
	endif()
endif()
if(WSE_WARNINGS_AS_ERRORS)
	set_property(TARGET WonderStewEngine PROPERTY COMPILE_WARNING_AS_ERROR ON)
endif()

find_package(Threads REQUIRED)
target_link_libraries(WonderStewEngine PRIVATE Threads::Threads)

if(WSE_BUILD_XPT)
	target_link_libraries(WonderStewEngine PRIVATE WSE::CurlBackend)
endif()

if(WSE_BUILD_VPJ)
	target_link_libraries(WonderStewEngine PRIVATE WSE::TurboJpegBackend)
endif()

# setupapi and iphlpapi back device enumeration and the MAC-address the licence check reads, so
# they are needed by Core alone. The rest arrive only with the Component that calls them: Media
# Foundation for Tmr, Direct3D 12 for OUI, Winsock for XPT.
if(WIN32)
	target_link_libraries(WonderStewEngine PRIVATE setupapi iphlpapi)
	if(WSE_BUILD_TMR)
		target_link_libraries(WonderStewEngine PRIVATE
			mf mfplat mfreadwrite mfuuid ole32 strmiids)
	endif()
	if(WSE_BUILD_OUI)
		target_link_libraries(WonderStewEngine PRIVATE d3d12 d3dcompiler dxgi user32)
	endif()
	if(WSE_BUILD_XPT)
		target_link_libraries(WonderStewEngine PRIVATE ws2_32)
	endif()
endif()

set_target_properties(WonderStewEngine PROPERTIES
	DEBUG_POSTFIX "_d"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
	LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
	LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
	ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
	ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
)

