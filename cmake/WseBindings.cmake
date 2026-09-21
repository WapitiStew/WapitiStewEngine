# @file cmake/WseBindings.cmake
# @brief The four optional language bindings and their build plumbing.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

# ------------------------------------------------------------------------------------------------
# Language bindings.
# ------------------------------------------------------------------------------------------------

# A STATIC engine is absorbed into each loadable binding, so the engine's own RPATH does not
# reach that module. Resolve the bundled camera runtime from each installed module's location.
function(wse_binding_libcamera_rpath target_name prefix_from_module)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_PACKAGE_BUNDLES_LIBCAMERA)
		set_property(TARGET ${target_name} APPEND PROPERTY INSTALL_RPATH
			"$ORIGIN/${prefix_from_module}/vendor/libcamera/lib")
	endif()
endfunction()

# Each binding is a loadable module, and on Linux a shared object cannot absorb a static archive
# whose objects are not position-independent. The .NET shim is always SHARED and links the Library
# the same way, but it is built through its own Target list further down.
if(WSE_BUILD_NODE_BINDING OR WSE_BUILD_PYTHON_BINDING OR WSE_BUILD_JAVA_BINDING OR WSE_BUILD_DOTNET_BINDING)
	if(WSE_LIBRARY_TYPE STREQUAL "STATIC")
		set_property(TARGET WonderStewEngine PROPERTY POSITION_INDEPENDENT_CODE ON)
	endif()
endif()

if(WSE_BUILD_JAVA_BINDING)
	# find_package(JNI) locates the host JDK's headers, which carry the host's `jni_md.h` and would
	# describe the wrong architecture. A cross Build therefore has to be told the target's, and is
	# refused rather than silently mis-built if it was not.
	if(CMAKE_CROSSCOMPILING)
		if(WSE_JNI_TARGET_INCLUDE_DIRS STREQUAL "")
			message(FATAL_ERROR
				"Cross-compiling WSE Java requires WSE_JNI_TARGET_INCLUDE_DIRS "
				"to contain the target JDK include and platform include directories.")
		endif()
		set(WSE_JNI_INCLUDE_DIRS ${WSE_JNI_TARGET_INCLUDE_DIRS})
	else()
		# Pinning the tool paths before find_package(Java) runs is what makes WSE_JAVA_HOME win
		# over whatever JDK happens to be first on PATH.
		if(NOT WSE_JAVA_HOME STREQUAL "")
			set(JAVA_HOME "${WSE_JAVA_HOME}")
			if(WIN32)
				set(Java_JAVA_EXECUTABLE "${WSE_JAVA_HOME}/bin/java.exe")
				set(Java_JAVAC_EXECUTABLE "${WSE_JAVA_HOME}/bin/javac.exe")
				set(Java_JAR_EXECUTABLE "${WSE_JAVA_HOME}/bin/jar.exe")
			else()
				set(Java_JAVA_EXECUTABLE "${WSE_JAVA_HOME}/bin/java")
				set(Java_JAVAC_EXECUTABLE "${WSE_JAVA_HOME}/bin/javac")
				set(Java_JAR_EXECUTABLE "${WSE_JAVA_HOME}/bin/jar")
			endif()
		endif()
		find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
		find_package(JNI REQUIRED)
		set(WSE_JNI_INCLUDE_DIRS ${JNI_INCLUDE_DIRS})
	endif()

	# Produces `wse_jni`, a module the JVM opens through System.loadLibrary. Nothing links against
	# it, so its symbols stay hidden apart from the JNI entry points the source exports.
	add_library(WSE_Java MODULE lang/java/native/jni.cpp)
	add_library(WSE::Java ALIAS WSE_Java)
	target_link_libraries(WSE_Java PRIVATE WSE::Core)
	target_include_directories(WSE_Java PRIVATE ${WSE_JNI_INCLUDE_DIRS})
	target_compile_features(WSE_Java PRIVATE cxx_std_17)
	target_compile_options(WSE_Java PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	)
	set_target_properties(WSE_Java PROPERTIES
		EXPORT_NAME Java
		OUTPUT_NAME "wse_jni"
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	)
	# In the package the module sits in `bin/` and the shared Core in `lib/`, so the module has to
	# be able to find it one directory up.
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_LIBRARY_TYPE STREQUAL "SHARED")
		set_property(TARGET WSE_Java PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
	endif()
	wse_binding_libcamera_rpath(WSE_Java "..")

	# The managed half. A cross Build has no runnable target JVM, so the jar and its contract
	# classes are simply not produced there; the native module still is.
	if(NOT CMAKE_CROSSCOMPILING)
		file(GLOB WSE_JAVA_MAIN_SOURCES CONFIGURE_DEPENDS
			"${CMAKE_CURRENT_SOURCE_DIR}/lang/java/src/main/java/io/wapitistew/wse/*.java")
		# jarは一枚で配るため、Overlayの公開Classも同じjarへ入れる。JavaにはC++の
		# target_sources()にあたる後付けの手段がなく、Sourceの一覧はここで閉じる。
		list(APPEND WSE_JAVA_MAIN_SOURCES ${WSE_EXTENSION_JAVA_SOURCES})
		set(WSE_JAVA_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/java/classes")
		set(WSE_JAVA_TEST_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/java/test-classes")
		set(WSE_JAVA_JAR "${CMAKE_CURRENT_BINARY_DIR}/java/wse.jar")
		if(WIN32)
			set(WSE_JAVA_CLASS_PATH_SEPARATOR ";")
		else()
			set(WSE_JAVA_CLASS_PATH_SEPARATOR ":")
		endif()
		# The classes directory is emptied first: javac never deletes, so a removed .java would
		# otherwise leave its stale .class behind and the jar would keep shipping it.
		add_custom_command(
			OUTPUT "${WSE_JAVA_JAR}"
			COMMAND "${CMAKE_COMMAND}" -E remove_directory "${WSE_JAVA_CLASSES_DIR}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${WSE_JAVA_CLASSES_DIR}"
			COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17
				-d "${WSE_JAVA_CLASSES_DIR}" ${WSE_JAVA_MAIN_SOURCES}
			COMMAND "${Java_JAR_EXECUTABLE}" --create --file "${WSE_JAVA_JAR}"
				-C "${WSE_JAVA_CLASSES_DIR}" .
			DEPENDS ${WSE_JAVA_MAIN_SOURCES}
			VERBATIM
		)
		# In ALL, and ordered after the native module, so building the default Target yields both
		# halves of the binding rather than a jar with nothing behind it.
		add_custom_target(WSE_JavaJar ALL DEPENDS "${WSE_JAVA_JAR}")
		add_dependencies(WSE_JavaJar WSE_Java)
		if(WSE_BUILD_TESTING)
			set(WSE_JAVA_TEST_SOURCES
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/java/io/wapitistew/wse/BindingContract.java")
			list(APPEND WSE_JAVA_TEST_SOURCES ${WSE_EXTENSION_JAVA_TEST_SOURCES})
			list(APPEND WSE_JAVA_TEST_SOURCES
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/java/io/wapitistew/wse/BindingStress.java"
				"${CMAKE_CURRENT_SOURCE_DIR}/example/java/core/QuickStart.java")
			# どのClassが出来るかはOverlayの有無で変わるため、Ruleの成果物は
			# Class Fileの列挙ではなくStampひとつにする。列挙すると、Overlayを
			# 外した構成でBuildが存在しないFileを待つことになる。
			set(WSE_JAVA_TEST_STAMP "${CMAKE_CURRENT_BINARY_DIR}/java/test-classes.stamp")
			add_custom_command(
				OUTPUT "${WSE_JAVA_TEST_STAMP}"
				COMMAND "${CMAKE_COMMAND}" -E remove_directory "${WSE_JAVA_TEST_CLASSES_DIR}"
				COMMAND "${CMAKE_COMMAND}" -E make_directory "${WSE_JAVA_TEST_CLASSES_DIR}"
				COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17
					-cp "${WSE_JAVA_CLASSES_DIR}"
					-d "${WSE_JAVA_TEST_CLASSES_DIR}" ${WSE_JAVA_TEST_SOURCES}
				COMMAND "${CMAKE_COMMAND}" -E touch "${WSE_JAVA_TEST_STAMP}"
				DEPENDS "${WSE_JAVA_JAR}" ${WSE_JAVA_TEST_SOURCES}
				VERBATIM
			)
			add_custom_target(WSE_JavaContract ALL DEPENDS "${WSE_JAVA_TEST_STAMP}")
		endif()
	endif()
endif()

if(WSE_BUILD_PYTHON_BINDING)
	# pybind11 is consumed with add_subdirectory(), so its own install rules and test Targets would
	# otherwise become part of this project. Both must be off before that call is reached.
	set(PYBIND11_INSTALL OFF)
	set(PYBIND11_TEST OFF)
	# pybind11_add_module() probes a running interpreter for the extension suffix and the library
	# to link. A cross Build has no target interpreter to run, so the module is declared by hand
	# against the target headers and given the suffix the target expects.
	if(CMAKE_CROSSCOMPILING)
		if(WSE_PYTHON_TARGET_INCLUDE_DIR STREQUAL "")
			message(FATAL_ERROR
				"Cross-compiling WSE Python requires WSE_PYTHON_TARGET_INCLUDE_DIR "
				"to point at the target CPython 3.11+ headers.")
		endif()
		set(PYBIND11_NOPYTHON ON)
		add_subdirectory(
			"${WSE_VENDOR_PYBIND11_DIR}"
			"${CMAKE_CURRENT_BINARY_DIR}/pybind11"
			EXCLUDE_FROM_ALL
		)
		add_library(WSE_Python MODULE lang/python/native/module.cpp)
		target_link_libraries(WSE_Python PRIVATE pybind11::headers WSE::Core)
		target_include_directories(WSE_Python PRIVATE "${WSE_PYTHON_TARGET_INCLUDE_DIR}")
		set_property(TARGET WSE_Python PROPERTY SUFFIX "${WSE_PYTHON_EXTENSION_SUFFIX}")
	else()
		if(NOT WSE_PYTHON_EXECUTABLE STREQUAL "")
			set(Python_EXECUTABLE "${WSE_PYTHON_EXECUTABLE}")
		endif()
		find_package(Python 3.11 REQUIRED COMPONENTS Interpreter Development.Module)
		# Set before add_subdirectory() so pybind11 adopts the interpreter found just above rather
		# than running its own legacy search and possibly choosing a different one.
		set(PYBIND11_FINDPYTHON ON)
		add_subdirectory(
			"${WSE_VENDOR_PYBIND11_DIR}"
			"${CMAKE_CURRENT_BINARY_DIR}/pybind11"
			EXCLUDE_FROM_ALL
		)
		pybind11_add_module(WSE_Python MODULE NO_EXTRAS lang/python/native/module.cpp)
		target_link_libraries(WSE_Python PRIVATE WSE::Core)
		set(WSE_PYTHON_EXECUTABLE "${Python_EXECUTABLE}")
	endif()

	add_library(WSE::Python ALIAS WSE_Python)
	target_compile_features(WSE_Python PRIVATE cxx_std_17)
	target_compile_options(WSE_Python PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/bigobj>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	)
	# The empty prefix and the `_wse` name are what `lang/python/wse/__init__.py` imports; changing
	# either breaks the package without breaking the Build.
	set_target_properties(WSE_Python PROPERTIES
		EXPORT_NAME Python
		PREFIX ""
		OUTPUT_NAME "_wse"
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	)
	# Three levels rather than one, because the extension is installed into
	# `lang/python/wse/` while the shared Core stays in `lib/`.
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_LIBRARY_TYPE STREQUAL "SHARED")
		set_property(TARGET WSE_Python PROPERTY INSTALL_RPATH "$ORIGIN/../../../lib")
	endif()
	wse_binding_libcamera_rpath(WSE_Python "../../..")
endif()

if(WSE_BUILD_NODE_BINDING)

	# Produces `wse.node`, loaded by require() from `lang/js/index.js`. One addon translation unit
	# per Component, all compiled unconditionally: each guards its own body on the PUBLIC
	# `WSE_HAS_<NAME>` macro, so a disabled Component drops out at the preprocessor instead of
	# needing a source list here.
	add_library(WSE_Node MODULE
		lang/js/native/addon.cpp
		lang/js/native/tmr_addon.cpp
		lang/js/native/oui_addon.cpp
		lang/js/native/iui_addon.cpp
		lang/js/native/xpt_addon.cpp
	)
	add_library(WSE::Node ALIAS WSE_Node)
	target_compile_features(WSE_Node PRIVATE cxx_std_17)
	target_compile_definitions(WSE_Node PRIVATE NODE_GYP_MODULE_NAME=wse)
	target_compile_options(WSE_Node PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	)
	target_include_directories(WSE_Node PRIVATE "${WSE_VENDOR_NODE_API_DIR}/include")
	target_link_libraries(WSE_Node PRIVATE WSE::Core)

	# Node.js ships no import library, and MSVC will not leave a symbol unresolved the way a Linux
	# module can. An import library is therefore synthesized from a .def listing the Node-API entry
	# points; the hosting `node.exe` supplies them at load time. CMAKE_CONFIGURE_DEPENDS makes a
	# change to that .def re-run Configure, since the .lib is produced here and not by the Build.
	if(MSVC)
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
			"${CMAKE_CURRENT_SOURCE_DIR}/lang/js/native/node_api.def")
		set(WSE_NODE_API_IMPORT_LIBRARY
			"${CMAKE_CURRENT_BINARY_DIR}/node-api/wse_node_api.lib")
		file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/node-api")
		execute_process(
			COMMAND "${CMAKE_AR}" /NOLOGO
				"/DEF:${CMAKE_CURRENT_SOURCE_DIR}/lang/js/native/node_api.def"
				"/OUT:${WSE_NODE_API_IMPORT_LIBRARY}"
				/MACHINE:X64
			RESULT_VARIABLE wse_node_import_result
			OUTPUT_VARIABLE wse_node_import_output
			ERROR_VARIABLE wse_node_import_error
		)
		if(NOT wse_node_import_result EQUAL 0)
			message(FATAL_ERROR
				"Unable to generate the Node-API import library.\n"
				"${wse_node_import_output}${wse_node_import_error}")
		endif()
		target_link_libraries(WSE_Node PRIVATE "${WSE_NODE_API_IMPORT_LIBRARY}")
	endif()

	# `wse.node` exactly: require() resolves by file name, and the default `lib` prefix or platform
	# suffix would leave the addon unloadable.
	set_target_properties(WSE_Node PROPERTIES
		EXPORT_NAME Node
		PREFIX ""
		SUFFIX ".node"
		OUTPUT_NAME "wse"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_LIBRARY_TYPE STREQUAL "SHARED")
		set_property(TARGET WSE_Node PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
	endif()
	wse_binding_libcamera_rpath(WSE_Node "..")
endif()

if(WSE_BUILD_DOTNET_BINDING)
	# C#は`wse::binding`のC++ Classを直接呼べないため、平坦C ABI(`api/wse/capi`)を挟む。
	# P/Invokeは共有Libraryだけを解決できるので、Core側がSTATICでもこのShimはSHAREDで建てる。
	# Unlike the Node addon, the C ABI shim is assembled per Component here: each file defines
	# exported entry points, and compiling one for a Component that is not in the Library would
	# publish an ABI symbol with nothing behind it.
	set(WSE_DOTNET_NATIVE_SOURCES
		lang/cs/native/capi_internal.cpp
		lang/cs/native/wse_capi_core.cpp
	)
	if(WSE_BUILD_XPT)
		list(APPEND WSE_DOTNET_NATIVE_SOURCES lang/cs/native/wse_capi_xpt.cpp)
	endif()
	if(WSE_BUILD_IUI)
		list(APPEND WSE_DOTNET_NATIVE_SOURCES lang/cs/native/wse_capi_iui.cpp)
	endif()
	if(WSE_BUILD_OUI)
		list(APPEND WSE_DOTNET_NATIVE_SOURCES lang/cs/native/wse_capi_oui.cpp)
	endif()
	if(WSE_BUILD_TMR)
		list(APPEND WSE_DOTNET_NATIVE_SOURCES lang/cs/native/wse_capi_tmr.cpp)
	endif()
	add_library(WSE_Dotnet SHARED ${WSE_DOTNET_NATIVE_SOURCES})
	add_library(WSE::Dotnet ALIAS WSE_Dotnet)
	target_compile_features(WSE_Dotnet PRIVATE cxx_std_17)
	target_compile_definitions(WSE_Dotnet PRIVATE __WSE_CAPI_EXPORTS__)
	target_compile_options(WSE_Dotnet PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	)
	target_link_libraries(WSE_Dotnet PRIVATE WSE::Core)
	if(WSE_BUILD_XPT)
		target_link_libraries(WSE_Dotnet PRIVATE WSE::Xpt)
	endif()
	if(WSE_BUILD_IUI)
		target_link_libraries(WSE_Dotnet PRIVATE WSE::Iui)
	endif()
	if(WSE_BUILD_OUI)
		target_link_libraries(WSE_Dotnet PRIVATE WSE::Oui)
	endif()
	if(WSE_BUILD_TMR)
		target_link_libraries(WSE_Dotnet PRIVATE WSE::Tmr)
	endif()
	if(WSE_BUILD_VPJ)
		target_link_libraries(WSE_Dotnet PRIVATE WSE::Vpj)
	endif()
	# `wse_capi` is the name the managed DllImport declarations spell, and hidden visibility keeps
	# the exported surface to the flat C ABI that `__WSE_CAPI_EXPORTS__` marks.
	set_target_properties(WSE_Dotnet PROPERTIES
		EXPORT_NAME Dotnet
		OUTPUT_NAME "wse_capi"
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_LIBRARY_TYPE STREQUAL "SHARED")
		set_property(TARGET WSE_Dotnet PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
	endif()
	wse_binding_libcamera_rpath(WSE_Dotnet "..")

	if(NOT CMAKE_CROSSCOMPILING)
		if(WSE_DOTNET_EXECUTABLE STREQUAL "")
			# `find_program`は既に値を持つCache変数を再探索しないため、別変数で探索してから写す。
			find_program(WSE_DOTNET_DISCOVERED_EXECUTABLE NAMES dotnet)
			if(WSE_DOTNET_DISCOVERED_EXECUTABLE)
				set(WSE_DOTNET_EXECUTABLE "${WSE_DOTNET_DISCOVERED_EXECUTABLE}")
			endif()
		endif()
		if(WSE_DOTNET_EXECUTABLE STREQUAL "" OR NOT EXISTS "${WSE_DOTNET_EXECUTABLE}")
			message(FATAL_ERROR
				"WSE_BUILD_DOTNET_BINDING requires the .NET SDK to build the managed assembly. "
				"Set WSE_DOTNET_EXECUTABLE to a dotnet executable.")
		endif()

		file(GLOB WSE_DOTNET_MANAGED_SOURCES CONFIGURE_DEPENDS
			"${CMAKE_CURRENT_SOURCE_DIR}/lang/cs/Wse/*.cs")
		if(WSE_EXTENSION_DOTNET_ROOT)
			# MSBuildは自分のIncremental判定を持つが、CMake側がSourceの変化を
			# 知らなければRebuildを起動しない。Overlayの.csも依存に数える。
			file(GLOB WSE_DOTNET_EXTENSION_SOURCES CONFIGURE_DEPENDS
				"${WSE_EXTENSION_DOTNET_ROOT}/lang/cs/Wse/*.cs")
			list(APPEND WSE_DOTNET_MANAGED_SOURCES ${WSE_DOTNET_EXTENSION_SOURCES})
		endif()
		set(WSE_DOTNET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/dotnet")
		set(WSE_DOTNET_ASSEMBLY "${WSE_DOTNET_OUTPUT_DIR}/WapitiStew.Wse.dll")
		add_custom_command(
			OUTPUT "${WSE_DOTNET_ASSEMBLY}"
			COMMAND "${WSE_DOTNET_EXECUTABLE}" build
				"${CMAKE_CURRENT_SOURCE_DIR}/lang/cs/Wse/Wse.csproj"
				--configuration Release
				--output "${WSE_DOTNET_OUTPUT_DIR}"
				"-p:WseExtensionDir=${WSE_EXTENSION_DOTNET_ROOT}"
				--no-incremental
				--nologo
			DEPENDS ${WSE_DOTNET_MANAGED_SOURCES}
				"${CMAKE_CURRENT_SOURCE_DIR}/lang/cs/Wse/Wse.csproj"
			VERBATIM
		)
		# The managed assembly the C# Consumer references. Ordered after the shim so the pair is
		# always produced together, the same arrangement the Java jar uses.
		add_custom_target(WSE_DotnetAssembly ALL DEPENDS "${WSE_DOTNET_ASSEMBLY}")
		add_dependencies(WSE_DotnetAssembly WSE_Dotnet)
	endif()
endif()

