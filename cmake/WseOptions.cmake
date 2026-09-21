# @file cmake/WseOptions.cmake
# @brief Build options and their consistency validation.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

# ------------------------------------------------------------------------------------------------
# Build options.
# ------------------------------------------------------------------------------------------------

# SHARED emits a DLL or a .so and compiles the Library with `__WSE_EXPORTS__`; STATIC emits an
# archive and publishes `WSE_STATIC` to Consumers so their copy of the public headers stops
# declaring the symbols as imported. Nothing else in the build changes.
set(WSE_LIBRARY_TYPE "SHARED" CACHE STRING "Build WSE as a SHARED or STATIC library")
set_property(CACHE WSE_LIBRARY_TYPE PROPERTY STRINGS SHARED STATIC)

# Only Windows has a Backend behind every optional Component today. A Linux Configure therefore
# starts with them off and the builder turns on the ones that platform can actually satisfy.
if(WIN32)
	set(WSE_OPTIONAL_COMPONENT_DEFAULT ON)
else()
	set(WSE_OPTIONAL_COMPONENT_DEFAULT OFF)
endif()

# Turning a Component on does three things: its .cpp files join the Library, `WSE_HAS_<NAME>=1`
# becomes visible to Consumers, and the `WSE::<Name>` facade Target that a Consumer links appears.
# Turning it off removes all three, and the packaged headers for it are not installed either.
option(WSE_BUILD_XPT "Build the XPT cross-platform transport component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
option(WSE_BUILD_GEF "Build the GEF generic file component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
option(WSE_BUILD_IUI "Build the IUI input component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
option(WSE_BUILD_OUI "Build the OUI output and rendering component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
option(WSE_BUILD_TMR "Build the Tmr camera component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
option(WSE_BUILD_VPJ "Build the private VPJ projector component" ${WSE_OPTIONAL_COMPONENT_DEFAULT})
# LAN video is no longer separable from VPJ. This survives only so an existing Cache or Preset that
# still sets it keeps configuring; the consistency block below forces the two to agree either way.
option(WSE_BUILD_VPJ_VIDEO
	"Deprecated compatibility input; VPJ LAN video is always included with WSE_BUILD_VPJ" OFF)
# The access-controlled sources live outside this Repository. Pointing this at an Overlay is what
# makes VPJ, and the binding surfaces the Overlay owns, buildable at all.
set(WSE_EXTENSION_ROOT "" CACHE PATH
	"Optional root of an access-controlled extension overlay; must contain wse-extension.cmake")
# Overlayが供給する面のうち、Targetでは表せないものはOverlayが変数で宣言する。
# .NETのAssemblyとJavaのjarは一枚で配るため、Sourceの一覧が閉じる前に受け取る必要がある。
# 空であれば、Overlayが在ってもその言語へは何も足さない。
set(WSE_EXTENSION_DOTNET_ROOT "")
set(WSE_EXTENSION_JAVA_SOURCES "")
set(WSE_EXTENSION_JAVA_TEST_SOURCES "")
# On by default only for the standalone build. A Consumer that embeds WSE would otherwise inherit
# the whole characterization suite into its own ctest run.
option(WSE_BUILD_TESTING "Build WSE characterization tests" ${WSE_STANDALONE_BUILD})
# The warning sweep drives the CI ratchet: it turns on the strict warning set without -Werror so
# the per-file counts can be measured against test/baseline/warning_baseline_linux_gcc.tsv. A file
# may only lose warnings. Warnings-as-errors also enables this strict set.
option(WSE_WARNING_SWEEP
	"Compile with the strict warning set for the warning-count ratchet" OFF)
option(WSE_WARNINGS_AS_ERRORS "Fail native-library builds on compiler warnings (also enables the strict set)" OFF)
# Benchmarks are opt-in: they measure rather than verify, so the default gates stay fast and a
# benchmark run is an explicit decision whose TSV output becomes the commit's comparison artifact.
option(WSE_BUILD_BENCHMARKS "Build the wse.bench.* measurement executables" OFF)
# Each binding is off by default because it needs a toolchain this Repository does not vendor: a
# Node.js, a CPython, a JDK, or the .NET SDK. Turning one on adds a loadable module Target plus the
# managed artifact that wraps it, and the contract Tests that exercise the pair.
option(WSE_BUILD_NODE_BINDING "Build the optional Node-API JavaScript binding" OFF)
option(WSE_BUILD_PYTHON_BINDING "Build the optional pybind11 Python binding" OFF)
option(WSE_BUILD_JAVA_BINDING "Build the optional Java 17+ JNI binding" OFF)
option(WSE_BUILD_DOTNET_BINDING "Build the optional .NET 8+ C# binding over the flat C ABI" OFF)
# Opt-in Test gates. Each needs something a build machine cannot be assumed to have: a display it
# is allowed to change the mode of, an attached camera, a person at the keyboard, an OpenCV
# installation, or network access. The default suite stays hermetic and offline without them.
option(WSE_ENABLE_DISPLAY_MODE_TESTS
	"Enable opt-in tests that temporarily enter an operating-system display mode" OFF)
set(WSE_DISPLAY_TEST_NUMBER "" CACHE STRING
	"Explicit Windows display number used by opt-in display-mode tests (for example 4)")
set(WSE_DISPLAY_TEST_HOLD_MS "3000" CACHE STRING
	"Milliseconds that an opt-in hardware projection scene remains visible")
option(WSE_ENABLE_CAMERA_HARDWARE_TESTS
	"Enable opt-in tests that open an attached physical camera" OFF)
set(WSE_CAMERA_TEST_NAME "" CACHE STRING
	"Optional exact Windows camera name; a selection never falls back to another camera")
option(WSE_ENABLE_IUI_HARDWARE_TESTS
	"Enable the opt-in interactive physical-keyboard smoke test" OFF)
option(WSE_ENABLE_OPENCV_COMPAT_TESTS
	"Enable opt-in consumer-side OpenCV compatibility contracts" OFF)
option(WSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION
	"Enable opt-in checks that compare every pinned toolchain archive with its vendor. Requires network access" OFF)

# Bootstrap builds libcamera for Linux x86-64 only. Defaulting the adapter on elsewhere would make
# Configure demand an artifact that was never produced for that target; V4L2 remains the fallback.
set(WSE_LIBCAMERA_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
		AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
	set(WSE_LIBCAMERA_DEFAULT ON)
endif()
option(WSE_ENABLE_LIBCAMERA
	"Enable the bootstrap-managed libcamera adapter for Tmr on Linux" ${WSE_LIBCAMERA_DEFAULT})

# ------------------------------------------------------------------------------------------------
# Option consistency.
# ------------------------------------------------------------------------------------------------

# A misspelt value would otherwise reach add_library() and be read as a source file name.
if(NOT WSE_LIBRARY_TYPE MATCHES "^(SHARED|STATIC)$")
	message(FATAL_ERROR
		"WSE_LIBRARY_TYPE must be SHARED or STATIC, but was '${WSE_LIBRARY_TYPE}'.")
endif()

# The standalone branch above already refused an unsupported system, but that branch is skipped
# entirely under add_subdirectory(). This repeats the refusal for the Consumer build.
if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "WonderStewEngine currently supports Windows and Linux only.")
endif()

# The three FORCE blocks below close the Option combinations that cannot produce a working Library,
# rather than letting them fail later at Compile or Link.
if(WSE_BUILD_VPJ_VIDEO AND NOT WSE_BUILD_VPJ)
	set(WSE_BUILD_VPJ ON CACHE BOOL
		"Build the private VPJ projector component" FORCE)
	message(STATUS "WSE_BUILD_VPJ enabled automatically for WSE_BUILD_VPJ_VIDEO.")
endif()

# VPJ talks to the projector over the LAN through XPT, so VPJ without XPT would not link.
if(WSE_BUILD_VPJ AND NOT WSE_BUILD_XPT)
	set(WSE_BUILD_XPT ON CACHE BOOL
		"Build the XPT cross-platform transport component" FORCE)
	message(STATUS "WSE_BUILD_XPT enabled automatically for WSE_BUILD_VPJ.")
endif()

if(WSE_BUILD_VPJ AND NOT WSE_BUILD_VPJ_VIDEO)
	set(WSE_BUILD_VPJ_VIDEO ON CACHE BOOL
		"Deprecated compatibility input; VPJ LAN video is always included with WSE_BUILD_VPJ" FORCE)
	message(STATUS
		"WSE_BUILD_VPJ_VIDEO enabled as a compatibility alias because LAN video is part of VPJ.")
endif()

# VPJの実装はExtension Overlayにある。Optionだけを立ててもComponentは中身を持たず、
# Runtimeは在ると答えるのに何も出来ないLibraryになる。黙って通さず、ここで止める。
if(WSE_BUILD_VPJ AND NOT WSE_EXTENSION_ROOT)
	message(FATAL_ERROR
		"WSE_BUILD_VPJ needs an extension overlay. Set WSE_EXTENSION_ROOT to one, or turn "
		"WSE_BUILD_VPJ off.")
endif()

