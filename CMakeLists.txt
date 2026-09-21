# @file CMakeLists.txt
# @brief WSE共有Library（WonderStewEngine）のWindows／Linux Buildを構成する。
# @details Vendor依存（libjpeg-turbo、libcurl、nlohmann/json）はWSE単体のBootstrapがsourceからBuildして
#          既定では`vendor/`へ配置する。これらは`.gitignore`が`*.lib`と`*.dll`を除外するため
#          Git管理対象外であり、fresh cloneやDownload ZIPには含まれない。現在LinkするDependencyだけを
#          Configure時に存在検査し、不足時はBootstrap手順を示して停止する。
#          検査が無いとConfigureが通ってCompile（C1083）やLink（LNK1181）で初めて失敗し、
#          他PCでの原因特定が困難になる。
#
#          本fileは2つのBuild形態を1つで支える。
#            1. standalone（Bootstrap経由）: project()とinstall()を持ち、Prebuilt packageを
#               利用者が指定した`CMAKE_INSTALL_PREFIX`へ生成する。
#            2. add_subdirectory（Consumer Project）: ConsumerのBuild treeの一部としてBuildし、
#               install()は定義しない。

# ------------------------------------------------------------------------------------------------
# Build mode and toolchain preconditions.
# ------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.24)

# The two build shapes described above are told apart once here. Every later decision that differs
# between them keys off this variable rather than testing the directory again.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(WSE_STANDALONE_BUILD ON)
else()
	set(WSE_STANDALONE_BUILD OFF)
endif()

# The single source of truth for the WSE version. Everything else - project(), the package
# manifest, the binding runtime version, the README and documentation banners, and the binding
# package metadata - derives from or is verified against this one value by wse.version.consistency.
set(WSE_VERSION "1.0.0")

if(WSE_STANDALONE_BUILD)
	project(WonderStewEngine VERSION ${WSE_VERSION} LANGUAGES C CXX)

	# A Consumer that embeds WSE has already chosen its own toolchain, so these refusals belong to
	# the standalone build alone. Rejecting an unsupported compiler or a 32-bit target at Configure
	# turns what would otherwise be an obscure Compile or Link failure into one readable message.
	if(WIN32 AND NOT MSVC)
		message(FATAL_ERROR "WonderStewEngine on Windows requires MSVC.")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux"
			AND NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
		message(FATAL_ERROR "WonderStewEngine on Linux requires GCC or Clang.")
	elseif(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR
			"WonderStewEngine currently supports Windows and Linux only.")
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR "A 64-bit target is required.")
	endif()
endif()

# The Windows shared Library carries a version resource, so RC has to be a live language before any
# Target that lists WonderStewEngine.rc among its sources exists.
if(WIN32)
	enable_language(RC)
endif()


# ------------------------------------------------------------------------------------------------
# The build, module by module.
# ------------------------------------------------------------------------------------------------
# Each module below is a verbatim responsibility slice of what used to be one 2,900-line file.
# They run in this directory's scope, in the original order, so the flow reads here while the
# detail lives beside its subject. The two access-controlled overlay stages stay visible in this
# file because they are the extension protocol, not a module detail.

include(cmake/WseOptions.cmake)
include(cmake/WseDependencies.cmake)
include(cmake/WseCore.cmake)
include(cmake/WseBindings.cmake)
include(cmake/WseTests.cmake)

if(NOT WSE_STANDALONE_BUILD
		AND WSE_LIBRARY_TYPE STREQUAL "SHARED"
		AND WSE_BUILD_VPJ_VIDEO
		AND WIN32)
	# Runtime DLLをSDK Libraryと同じDirectoryへ配置する。
	# turbojpegはDebug／Releaseとも`turbojpeg.dll`である（上の理由参照）。
	# standalone Buildではこの複製先が存在しないため、install()が配置を担う。
	add_custom_command(TARGET WonderStewEngine POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${WSE_VENDOR_JPEG_DIR}/bin/turbojpeg.dll"
			"$<TARGET_FILE_DIR:WonderStewEngine>"
	)
endif()

# Overlay読み込みの2段目。この位置より前でLibrary Target、Binding Target、Test Helperが
# すべて定義済みであるため、Overlayは変数の受け渡しProtocolを介さず、
# wse_add_characterization_test()やinstall()を直接呼べる。この段は任意である。
if(WSE_EXTENSION_ROOT AND EXISTS "${WSE_EXTENSION_ROOT}/wse-extension-targets.cmake")
	include("${WSE_EXTENSION_ROOT}/wse-extension-targets.cmake")
endif()

include(cmake/WseInstall.cmake)
