# @file cmake/WseInstall.cmake
# @brief The standalone install/export layout and the package manifest generation.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

if(WSE_STANDALONE_BUILD)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WseLicensing.cmake")
	# Prebuilt packageのLayout（CMAKE_INSTALL_PREFIX）。
	#   bin/      WonderStewEngine[_d].dll、PDB、turbojpeg DLL
	#   lib/      WonderStewEngine[_d].lib
	#   include/  wse/api/ 配下へPublic headerを1か所だけ配置する
	#   vendor/vpj/libjpeg-turbo/  VPJが所有するturbojpeg一式
	#   cmake/    Imported Target定義（WonderStewEngineConfig.cmake）
	#   wse-package.json       版情報（toolset照合と鮮度警告に使用）
	install(TARGETS ${WSE_EXPORT_TARGETS}
		EXPORT WSETargets
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		INCLUDES DESTINATION include
	)
	if(WSE_BUILD_PYTHON_BINDING)
		install(TARGETS WSE_Python
			EXPORT WSETargets
			RUNTIME DESTINATION lang/python/wse
			LIBRARY DESTINATION lang/python/wse
		)
		install(FILES
			lang/python/wse/__init__.py
			lang/python/wse/__init__.pyi
			lang/python/wse/py.typed
			DESTINATION lang/python/wse
		)
		install(FILES
			"${WSE_VENDOR_PYBIND11_DIR}/LICENSE"
			"${WSE_VENDOR_PYBIND11_DIR}/wse-dependency.json"
			DESTINATION vendor/pybind11
		)
	endif()
	if(WSE_BUILD_NODE_BINDING)
		install(TARGETS WSE_Node
			EXPORT WSETargets
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION bin
		)
		install(FILES
			lang/js/index.js
			lang/js/index.d.ts
			DESTINATION lang/js
		)
		file(READ "${CMAKE_CURRENT_SOURCE_DIR}/lang/js/package.json" wse_node_package)
		string(JSON wse_node_package SET "${wse_node_package}" license "\"${WSE_PACKAGE_LICENSE_EXPRESSION}\"")
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wse-node-package.json" "${wse_node_package}\n")
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wse-node-package.json"
			DESTINATION lang/js RENAME package.json)
		install(FILES
			"${WSE_VENDOR_NODE_API_DIR}/LICENSE"
			"${WSE_VENDOR_NODE_API_DIR}/wse-dependency.json"
			DESTINATION vendor/node-api
		)
	endif()
	if(WSE_BUILD_JAVA_BINDING)
		install(TARGETS WSE_Java
			EXPORT WSETargets
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION bin
		)
		if(NOT CMAKE_CROSSCOMPILING)
			install(FILES "${WSE_JAVA_JAR}" DESTINATION lang/java)
		endif()
	endif()
	if(WSE_BUILD_DOTNET_BINDING)
		install(TARGETS WSE_Dotnet
			EXPORT WSETargets
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION bin
			ARCHIVE DESTINATION lib
		)
		if(NOT CMAKE_CROSSCOMPILING)
			install(FILES
				"${WSE_DOTNET_ASSEMBLY}"
				"${WSE_DOTNET_OUTPUT_DIR}/WapitiStew.Wse.xml"
				DESTINATION lang/cs
			)
		endif()
	endif()
	install(EXPORT WSETargets
		FILE WSETargets.cmake
		NAMESPACE WSE::
		DESTINATION cmake
	)

	# ReleaseはPDBを生成しない構成のためOPTIONALとする。
	if(WSE_LIBRARY_TYPE STREQUAL "SHARED" AND MSVC)
		install(FILES "$<TARGET_PDB_FILE:WonderStewEngine>" DESTINATION bin OPTIONAL)
	endif()

	# Include解決の互換性維持: `<wse/api/xpt/stew.h>`と`<tmr/device/WebCamera.h>`の
	# 2形式を通すため、headerは`include/wse/api/`へ配置し、Imported Targetが
	# `include/`と`include/wse/api/`の2 Directoryを公開する。実体はこの1か所だけである。
	install(FILES api/dynamic.h DESTINATION include/wse/api)
	install(DIRECTORY api/wse/ DESTINATION include/wse/api/wse
		FILES_MATCHING
			PATTERN "*.h"
			PATTERN "*.hpp"
	)
	# The OpenCV adapter is header-only and opt-in: consumers include it only when they
	# already have OpenCV on their include path, so it installs unconditionally.
	install(DIRECTORY api/cv/ DESTINATION include/wse/api/cv
		FILES_MATCHING
			PATTERN "*.h"
			PATTERN "*.hpp"
	)

	foreach(wse_component IN ITEMS xpt gef iui oui tmr vpj)
		string(TOUPPER "${wse_component}" wse_component_upper)
		if(WSE_BUILD_${wse_component_upper})
			if(wse_component STREQUAL "vpj")
				# VPJのHeaderはExtension Overlayが持ち、Overlay自身がinstallする。
			elseif(wse_component STREQUAL "tmr")
				# api/tmrに残るHeaderはすべて公開面である。機種固有のHeaderが
				# Overlayへ移ったため、選り分けるためのAllowlistは要らなくなった。
				install(DIRECTORY "api/tmr/"
					DESTINATION "include/wse/api/tmr"
					FILES_MATCHING
						PATTERN "*.h"
						PATTERN "*.hpp")
			else()
				install(DIRECTORY "api/${wse_component}/"
					DESTINATION "include/wse/api/${wse_component}"
					FILES_MATCHING
						PATTERN "*.h"
						PATTERN "*.hpp"
				)
			endif()
		endif()
	endforeach()

	if(WSE_BUILD_VPJ AND WIN32 AND WSE_BUILD_OUI)
		install(DIRECTORY api/link/ DESTINATION include/wse/api/link
			FILES_MATCHING
				PATTERN "*.h"
				PATTERN "*.hpp"
		)
	endif()

	# 実行時に必要なRuntime DLL。Debug／Release両構成分を配置する
	# （installは--config Debug／Releaseの2回実行され、同じ内容のため衝突しない）。
	# turbojpeg.dllはWonderStewEngine[_d].dll自身のLoadに必要であり、
	# `bin/`だけをPATHへ加える消費側（CTestの`$<TARGET_FILE_DIR:WonderStewEngine>`）の
	# ために同梱する。Debug構成も`turbojpeg.dll`を使う（Import Libraryがrename由来で
	# DLL名を共有するため。`turbojpegd.dll`はLoadされない）。
	if(WSE_BUILD_VPJ)
		install(DIRECTORY "${WSE_VENDOR_JPEG_DIR}/" DESTINATION vendor/vpj/libjpeg-turbo)
		if(WIN32)
			install(FILES "${WSE_VENDOR_JPEG_DIR}/bin/turbojpeg.dll" DESTINATION bin)
		endif()
	endif()

	if(WSE_BUILD_VPJ)
		# Profile resources travel with the Overlay that parses them; the dependency notice
		# stays here because this repository is what vendors the dependency.
		install(FILES
			"${WSE_VENDOR_NLOHMANN_JSON_DIR}/LICENSE.MIT"
			"${WSE_VENDOR_NLOHMANN_JSON_DIR}/wse-dependency.json"
			DESTINATION vendor/nlohmann-json)
	endif()

	# Static WSE consumerへlibcurlとPlatform TLS Link contractを再現するため、
	# XPT Packageは検証済みHeader／Library／License／Metadataを同梱する.
	if(WSE_BUILD_XPT)
		install(DIRECTORY "${WSE_VENDOR_LIBCURL_DIR}/" DESTINATION vendor/libcurl)
		if(WSE_PACKAGE_BUNDLES_OPENSSL)
			install(DIRECTORY "${WSE_VENDOR_OPENSSL_DIR}/" DESTINATION vendor/openssl)
		endif()
	endif()

	if(WSE_PACKAGE_BUNDLES_LIBCAMERA)
		install(DIRECTORY "${WSE_VENDOR_LIBCAMERA_DIR}/"
			DESTINATION vendor/libcamera)
	endif()

	# 版情報: source樹のtree hash（commit済み内容にのみ反応する）、MSVC toolset、生成日時。
	# HOST Configure時にtoolset不一致をFATALで、source樹の不一致を警告で検出するために使う。
	find_package(Git QUIET)
	set(WSE_PACKAGE_SOURCE_COMMIT "unknown")
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-parse "HEAD:./"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE wse_git_result
			OUTPUT_VARIABLE wse_git_tree_hash
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		if(wse_git_result EQUAL 0 AND NOT wse_git_tree_hash STREQUAL "")
			set(WSE_PACKAGE_SOURCE_COMMIT "${wse_git_tree_hash}")
		endif()
	endif()

	# Packageは2つの木から出来る。Overlay側の変化を記録しないと、消費側の鮮度検査は
	# 半分しか見ないことになる。
	set(WSE_PACKAGE_EXTENSION_COMMIT "none")
	if(WSE_EXTENSION_ROOT AND GIT_EXECUTABLE)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-parse "HEAD:./"
			WORKING_DIRECTORY "${WSE_EXTENSION_ROOT}"
			RESULT_VARIABLE wse_extension_git_result
			OUTPUT_VARIABLE wse_extension_tree_hash
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		if(wse_extension_git_result EQUAL 0 AND NOT wse_extension_tree_hash STREQUAL "")
			set(WSE_PACKAGE_EXTENSION_COMMIT "${wse_extension_tree_hash}")
		else()
			set(WSE_PACKAGE_EXTENSION_COMMIT "unknown")
		endif()
	endif()

	string(TIMESTAMP WSE_PACKAGE_GENERATED_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

	foreach(wse_component IN ITEMS XPT GEF IUI OUI TMR VPJ)
		if(WSE_BUILD_${wse_component})
			set(WSE_PACKAGE_BUILD_${wse_component} true)
		else()
			set(WSE_PACKAGE_BUILD_${wse_component} false)
		endif()
	endforeach()
	if(WSE_BUILD_VPJ_VIDEO)
		set(WSE_PACKAGE_BUILD_VPJ_VIDEO true)
	else()
		set(WSE_PACKAGE_BUILD_VPJ_VIDEO false)
	endif()
	if(WSE_BUILD_NODE_BINDING)
		set(WSE_PACKAGE_BUILD_NODE true)
	else()
		set(WSE_PACKAGE_BUILD_NODE false)
	endif()
	if(WSE_BUILD_PYTHON_BINDING)
		set(WSE_PACKAGE_BUILD_PYTHON true)
	else()
		set(WSE_PACKAGE_BUILD_PYTHON false)
	endif()
	if(WSE_BUILD_JAVA_BINDING)
		set(WSE_PACKAGE_BUILD_JAVA true)
	else()
		set(WSE_PACKAGE_BUILD_JAVA false)
	endif()
	if(WSE_BUILD_DOTNET_BINDING)
		set(WSE_PACKAGE_BUILD_DOTNET true)
	else()
		set(WSE_PACKAGE_BUILD_DOTNET false)
	endif()

	# アクセス制御対象のExtension名はPublic packageのManifestへ出さない (public-boundary scanと
	# Consumer検証が拒否する)。その構成と依存の記録は、Extensionを含む配布のManifestにだけ現れる。
	set(WSE_PACKAGE_CONTROLLED_COMPONENT_JSON "")
	set(WSE_PACKAGE_CONTROLLED_DEPENDENCY_JSON "")
	set(WSE_PACKAGE_CONTROLLED_FLAGS_CMAKE "")
	set(WSE_PACKAGE_CONTROLLED_BACKEND_CMAKE "")
	if(WSE_BUILD_VPJ)
		set(WSE_PACKAGE_CONTROLLED_COMPONENT_JSON "\n    \"vpj\": true,")
		string(CONCAT WSE_PACKAGE_CONTROLLED_DEPENDENCY_JSON
			"\n    \"libjpeg-turbo\": {"
			"\n      \"version\": \"${WSE_LIBJPEG_TURBO_BOOTSTRAP_VERSION}\","
			"\n      \"license\": \"${WSE_LIBJPEG_TURBO_BOOTSTRAP_LICENSE}\","
			"\n      \"source_sha256\": \"${WSE_LIBJPEG_TURBO_BOOTSTRAP_SHA256}\","
			"\n      \"bundled\": true,"
			"\n      \"integration_status\": \"vpj-private-jpeg-backend\""
			"\n    },")
		set(WSE_PACKAGE_CONTROLLED_FLAGS_CMAKE
			"\nset(WSE_PACKAGE_HAS_VPJ true)\nset(WSE_PACKAGE_HAS_VPJ_VIDEO ${WSE_PACKAGE_BUILD_VPJ_VIDEO})")
		# Bracket引数なので${WSE_PACKAGE_ROOT}は展開されず、Config読み込み時に評価される。
		set(WSE_PACKAGE_CONTROLLED_BACKEND_CMAKE [=[
# VPJ keeps libjpeg-turbo private at the C++ API boundary. Static WSE packages
# still need an imported archive target for their final consumer link.
if(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "STATIC"
        AND WSE_PACKAGE_HAS_VPJ_VIDEO
        AND NOT TARGET WSE::TurboJpegBackend)
    if(WIN32)
        add_library(WSE::TurboJpegBackend SHARED IMPORTED)
    else()
        add_library(WSE::TurboJpegBackend STATIC IMPORTED)
    endif()
    set_target_properties(WSE::TurboJpegBackend PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        INTERFACE_INCLUDE_DIRECTORIES "${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/include")
    if(WIN32)
        set_target_properties(WSE::TurboJpegBackend PROPERTIES
            IMPORTED_IMPLIB_DEBUG "${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/lib/turbojpegd.lib"
            IMPORTED_IMPLIB_RELEASE "${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/lib/turbojpeg.lib"
            IMPORTED_LOCATION_DEBUG "${WSE_PACKAGE_ROOT}/bin/turbojpeg.dll"
            IMPORTED_LOCATION_RELEASE "${WSE_PACKAGE_ROOT}/bin/turbojpeg.dll")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_target_properties(WSE::TurboJpegBackend PROPERTIES
            IMPORTED_LOCATION_DEBUG
                "${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/lib/debug/libturbojpeg.a"
            IMPORTED_LOCATION_RELEASE
                "${WSE_PACKAGE_ROOT}/vendor/vpj/libjpeg-turbo/lib/release/libturbojpeg.a")
    endif()
endif()

# Static WSE exports preserve CMake's concrete build-tree target name inside
# LINK_ONLY expressions. Recreate that private spelling as an adapter so the
# consumer still resolves the namespaced package target above.
if(WSE_PACKAGE_LIBRARY_TYPE STREQUAL "STATIC"
        AND WSE_PACKAGE_HAS_VPJ_VIDEO
        AND TARGET WSE::TurboJpegBackend
        AND NOT TARGET WSE_TurboJpegBackend)
    add_library(WSE_TurboJpegBackend INTERFACE IMPORTED)
    set_target_properties(WSE_TurboJpegBackend PROPERTIES
        INTERFACE_LINK_LIBRARIES WSE::TurboJpegBackend)
endif()
]=])
	endif()

	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/wse-package.json.in"
		"${CMAKE_CURRENT_BINARY_DIR}/wse-package.json"
		@ONLY
	)
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/WonderStewEngineConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/WonderStewEngineConfig.cmake"
		@ONLY
	)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wse-package.json" DESTINATION ".")
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/WonderStewEngineConfig.cmake" DESTINATION cmake)
endif()
