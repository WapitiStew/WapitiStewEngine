# @file cmake/WseTests.cmake
# @brief Every CTest registration: policy gates, characterization suite, binding contracts, and per-component contracts.
# @details Included by the root CMakeLists.txt; runs in the root's directory scope, so
#          CMAKE_CURRENT_SOURCE_DIR and every variable behave exactly as they did when
#          this content lived in the root file.

# ------------------------------------------------------------------------------------------------
# Tests: source policy gates.
# ------------------------------------------------------------------------------------------------

if(WSE_BUILD_TESTING)
	enable_testing()
	if(WIN32)
		add_executable(wse_windows_utf8_contract test/characterization/windows_utf8_contract.cpp)
		target_compile_features(wse_windows_utf8_contract PRIVATE cxx_std_17)
		add_test(NAME wse.windows.utf8_contract COMMAND wse_windows_utf8_contract)
	endif()
	# Two golden fixtures shared across every language. Each binding is expected to reproduce the
	# same projection frame and the same camera description as the C++ Test, which is what makes
	# the bindings comparable rather than each asserting its own idea of correct.
	set(WSE_BINDING_PROJECTION_FIXTURE
		"${CMAKE_CURRENT_SOURCE_DIR}/test/golden/binding_projection_full_frame.properties")
	set(WSE_BINDING_WEBCAMERA_FIXTURE
		"${CMAKE_CURRENT_SOURCE_DIR}/test/golden/webcamera_binding.properties")
	# QUIET, and every Test below it conditional, because these three cover Python tooling that is
	# not part of the Library. A machine without Python still gets a complete C++ suite.
	find_package(Python3 3.8 QUIET COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND)
		add_test(NAME wse.ci.tooling_contract
			COMMAND "${Python3_EXECUTABLE}" -m unittest discover
				-s "${CMAKE_CURRENT_SOURCE_DIR}/test/ci" -p "test_*.py")
		# Proves bootstrap.py resolves, verifies, and lays out a pinned package as its manifest
		# says, offline and without touching the real vendor directory.
		add_test(
			NAME wse.bootstrap.contract
			COMMAND "${Python3_EXECUTABLE}" -m unittest discover
				-s "${CMAKE_CURRENT_SOURCE_DIR}/test/bootstrap"
				-p "test_bootstrap.py"
		)
		# The Windows launcher contract provisions the pinned CPython runtime on a machine that
		# does not have it, so a cold host legitimately spends most of this budget on one
		# download and extraction; a warm host finishes in seconds.
		set_tests_properties(wse.bootstrap.contract PROPERTIES TIMEOUT 300)
		if(WSE_ENABLE_TOOLCHAIN_PIN_VERIFICATION)
			# The default gates are offline. These reach the vendors that publish the pinned archives,
			# so they are opt-in and labelled; run them whenever a pinned version changes.
			# Proves every pinned archive is still published where the manifest says, and still
			# announces the identity recorded for it. No download.
			add_test(
				NAME wse.bootstrap.toolchain_pin_identity
				COMMAND "${Python3_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/tools/bootstrap/verify_toolchain_pins.py"
					--identity-only
			)
			set_tests_properties(wse.bootstrap.toolchain_pin_identity PROPERTIES
				LABELS "network;bootstrap" TIMEOUT 600)
			# Proves the bytes behind each pin still hash to the recorded value, which means
			# fetching every archive. The long timeout is the download, not the check.
			add_test(
				NAME wse.bootstrap.toolchain_pin_contents
				COMMAND "${Python3_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/tools/bootstrap/verify_toolchain_pins.py"
					--cache-dir "${CMAKE_CURRENT_BINARY_DIR}/toolchain-pin-verification"
			)
			set_tests_properties(wse.bootstrap.toolchain_pin_contents PROPERTIES
				LABELS "network;bootstrap" TIMEOUT 5400)
		endif()
		# Proves the offline kit builder emits a tree a machine with no network access can
		# bootstrap from unchanged.
		add_test(
			NAME wse.offline_kit.contract
			COMMAND "${Python3_EXECUTABLE}" -m unittest discover
				-s "${CMAKE_CURRENT_SOURCE_DIR}/test/offline"
				-p "test_offline_kit.py"
		)
		set_tests_properties(wse.offline_kit.contract PROPERTIES TIMEOUT 30)
		# Proves the Raspberry Pi provisioning helpers behave as documented, on any host.
		add_test(
			NAME wse.pi_tools.contract
			COMMAND "${Python3_EXECUTABLE}" -m unittest discover
				-s "${CMAKE_CURRENT_SOURCE_DIR}/test/pi"
				-p "test_pi_tools.py"
		)
		set_tests_properties(wse.pi_tools.contract PROPERTIES TIMEOUT 30)
	else()
		message(STATUS
			"Python 3.8+ was not found; bootstrap, offline-kit, and Pi-tool contracts are not registered.")
	endif()

	# The source-policy gates. They read the tree rather than the built Library, so they are
	# registered unconditionally and hold whatever the selected Options happen to be. Each is a
	# script under `test/cmake/`, and most are ratchets against a reviewed baseline file
	# that a maintainer edits deliberately; see the header of each script.

	# Proves no platform header or platform type reached a public header, and that neither the
	# classified raw void-pointer debt nor the thread-state pointer debt grew.
	add_test(
		NAME wse.public_header_boundary
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_API_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/api"
			"-DWSE_RAW_VOID_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/test/baseline/public_raw_void_pointer_baseline.txt"
			"-DWSE_THREAD_STATE_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/test/baseline/public_thread_state_baseline.txt"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_public_header_boundary.cmake"
	)
	# Proves the public API keeps its namespace, include, and error-accessor rules, that diagnostic
	# output goes only through the logging sink, and that neither third-party leakage nor the
	# legacy ErrorCode/wseException surface grew.
	add_test(
		NAME wse.public_api_policy
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_API_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/api"
			"-DWSE_CORE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/core"
			"-DWSE_PLATFORM_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/platform"
			"-DWSE_LANG_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/lang"
			"-DWSE_THIRD_PARTY_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/test/baseline/public_third_party_type_baseline.txt"
			"-DWSE_LEGACY_ERROR_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/test/baseline/public_legacy_error_baseline.txt"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_public_api_policy.cmake"
	)
	# Proves every deprecated public declaration is recorded in the ledger with a reachable
	# migration document, and that the ledger total still matches the headers.
	add_test(
		NAME wse.deprecation_ledger
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
			"-DWSE_API_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/api"
			"-DWSE_DEPRECATION_LEDGER=${CMAKE_CURRENT_SOURCE_DIR}/doc/deprecation/DeprecationLedger.tsv"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_deprecation_ledger.cmake"
	)
	# Proves the pre-publication legacy removal inventory stays well formed, keeps resolvable
	# migration anchors, and covers every deprecation ledger id.
	add_test(
		NAME wse.legacy_removal_inventory
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
			"-DWSE_LEGACY_REMOVAL_INVENTORY=${CMAKE_CURRENT_SOURCE_DIR}/doc/deprecation/LegacyRemovalInventory.tsv"
			"-DWSE_DEPRECATION_LEDGER=${CMAKE_CURRENT_SOURCE_DIR}/doc/deprecation/DeprecationLedger.tsv"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_legacy_removal_inventory.cmake"
	)
	# Proves the shipped API stays legacy-free: every inventory row resolved, every ratchet
	# baseline empty, and the deprecation ledger empty (legacy removal program, LEGACY-011).
	add_test(
		NAME wse.legacy_free
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
			"-DWSE_LEGACY_REMOVAL_INVENTORY=${CMAKE_CURRENT_SOURCE_DIR}/doc/deprecation/LegacyRemovalInventory.tsv"
			"-DWSE_DEPRECATION_LEDGER=${CMAKE_CURRENT_SOURCE_DIR}/doc/deprecation/DeprecationLedger.tsv"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_legacy_free.cmake"
	)
	# Proves the English and Japanese documents stay paired and in step, that no local link is
	# broken, and that the Presets, Options, and samples the documentation names still exist.
	add_test(
		NAME wse.documentation.contract
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
			"-DWSE_DOCUMENTATION_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/doc/DocumentationManifest.tsv"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_documentation.cmake"
	)
	# Proves the committed support matrix still matches the metadata it is generated from; a
	# drift means someone edited the file or changed a source without regenerating.
	if(Python3_Interpreter_FOUND)
		add_test(
			NAME wse.documentation.support_matrix
			COMMAND "${Python3_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/tools/release/make_support_matrix.py"
				--source-root "${CMAKE_CURRENT_SOURCE_DIR}" --check
		)
	endif()
	# Proves every version spelling a generator cannot reach - README banners, documentation
	# manifest rows, and binding package metadata - matches the single set(WSE_VERSION) source.
	add_test(
		NAME wse.version.consistency
		COMMAND "${CMAKE_COMMAND}"
			"-DWSE_VERSION=${WSE_VERSION}"
			"-DWSE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_version_consistency.cmake"
	)

	# --------------------------------------------------------------------------------------------
	# Tests: characterization suite.
	# --------------------------------------------------------------------------------------------

	# One Test executable built against one Component facade and registered under the same name.
	# Placing it in the shared per-platform output directory is what lets it find the Runtime DLL
	# beside itself on Windows without a PATH change. Anything after the third argument is passed
	# through to the executable, which is how the golden fixtures and backend names are supplied.
	function(wse_add_characterization_test wse_test_name wse_test_source wse_test_target)
		add_executable(${wse_test_name} "${wse_test_source}")
		target_link_libraries(${wse_test_name} PRIVATE ${wse_test_target})
		target_compile_features(${wse_test_name} PRIVATE cxx_std_17)
		target_compile_options(${wse_test_name} PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
		)
		set_target_properties(${wse_test_name} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
			RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
			RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
		)
		add_test(NAME ${wse_test_name} COMMAND "$<TARGET_FILE:${wse_test_name}>" ${ARGN})
	endfunction()

	wse_add_characterization_test(
		wse.core.fast_indexing
		test/characterization/wse_fast_indexing_contract.cpp
		WSE::Core
	)
	# Pins the observable behaviour of the Core data and utility types.
	wse_add_characterization_test(
		wse.core.characterization
		test/characterization/core_characterization.cpp
		WSE::Core
	)
	# Pins the lifecycle of the internal worker controller that replaces BMultiThread
	# (LEGACY-006). The class is internal and unexported, so the test compiles the
	# implementation file directly instead of linking the library.
	add_executable(wse.core.worker_controller
		test/characterization/wse_worker_controller_contract.cpp
		core/wse/utility/wse_WorkerController.cpp
	)
	target_include_directories(wse.core.worker_controller PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(wse.core.worker_controller PRIVATE Threads::Threads)
	target_compile_features(wse.core.worker_controller PRIVATE cxx_std_17)
	target_compile_options(wse.core.worker_controller PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	)
	set_target_properties(wse.core.worker_controller PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/Release"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/MinSizeRel"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/RelWithDebInfo"
	)
	add_test(NAME wse.core.worker_controller COMMAND "$<TARGET_FILE:wse.core.worker_controller>")
	set_tests_properties(wse.core.worker_controller PROPERTIES TIMEOUT 60)
	# Pins the corrected Matrix numerics: covariance input validation, the pivot row swap,
	# singular-matrix rejection, and the elimination-based determinant staying inside the timeout.
	wse_add_characterization_test(
		wse.core.matrix_correctness
		test/characterization/wse_matrix_correctness_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.matrix_correctness PROPERTIES TIMEOUT 5)
	# Pins the Map shape invariant: checked at()/row()/elements() access, overflow-checked
	# allocation, OUT_OF_RANGE classification, and explicit rejection of empty-Map access.
	wse_add_characterization_test(
		wse.core.map_invariants
		test/characterization/wse_map_invariants_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.map_invariants PROPERTIES TIMEOUT 5)
	# Pins the storage type, channel count, bit depth, and stride of every pixel format at compile
	# time, so a storage/documentation divergence like the CH?D32 uint64_t regression cannot recur.
	wse_add_characterization_test(
		wse.core.pixel_storage
		test/characterization/wse_pixel_storage_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.pixel_storage PROPERTIES TIMEOUT 5)
	# Pins the canonical Result/Status/PartialResult contract every component result aliases:
	# factory-only construction, no error-less failure, and loud precondition violations.
	wse_add_characterization_test(
		wse.core.result_contract
		test/characterization/wse_result_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.result_contract PROPERTIES TIMEOUT 5)
	# Pins the license error surface (LEGACY-008 wave 4): operational failures arrive through
	# LicenseResult/LicenseStatus, usage violations through the standard exceptions.
	wse_add_characterization_test(
		wse.core.license_contract
		test/characterization/license_contract.cpp
		WSE::Core
		"$<TARGET_FILE_DIR:wse.core.license_contract>/wse_license_key.bin"
		"$<TARGET_FILE_DIR:wse.core.license_contract>/wse_license_file.bin"
	)
	set_tests_properties(wse.core.license_contract PROPERTIES TIMEOUT 10)
	# The OpenCV adapter only compiles where a consumer supplies OpenCV, so its contract is
	# opt-in: -DWSE_ENABLE_OPENCV_ADAPTER_TESTS=ON on a machine with OpenCV installed.
	if(WSE_ENABLE_OPENCV_ADAPTER_TESTS)
		find_package(OpenCV REQUIRED COMPONENTS core)
		wse_add_characterization_test(
			wse.cv.opencv_adapter_contract
			test/characterization/opencv_adapter_contract.cpp
			WSE::Core
		)
		target_include_directories(wse.cv.opencv_adapter_contract PRIVATE ${OpenCV_INCLUDE_DIRS})
		target_link_libraries(wse.cv.opencv_adapter_contract PRIVATE ${OpenCV_LIBS})
		set_tests_properties(wse.cv.opencv_adapter_contract PROPERTIES
			TIMEOUT 10
			LABELS "cv;opencv")
	endif()
	# Pins the flat C ABI snapshot: the ABI version, the by-value status layout, the portable
	# error-category values, and the opaque handles, all at compile time from the headers alone.
	wse_add_characterization_test(
		wse.capi.abi_snapshot
		test/characterization/wse_capi_abi_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.capi.abi_snapshot PROPERTIES TIMEOUT 5)
	add_executable(wse.capi.c_layout_snapshot test/characterization/wse_capi_layout.c)
	target_include_directories(wse.capi.c_layout_snapshot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/api")
	set_target_properties(wse.capi.c_layout_snapshot PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
	add_test(NAME wse.capi.c_layout_snapshot COMMAND "$<TARGET_FILE:wse.capi.c_layout_snapshot>")
	set_tests_properties(wse.capi.c_layout_snapshot PROPERTIES TIMEOUT 5)

	# Compile the production bridge/facade sources directly so DLL allocator boundaries
	# cannot hide allocations. MSVC debug iterator bookkeeping can allocate from noexcept
	# STL moves; disable that bookkeeping only in this isolated fixture, never in WSE.
	wse_add_characterization_test(wse.capi.allocation_contract
		test/characterization/wse_capi_allocation_contract.cpp Threads::Threads)
	target_sources(wse.capi.allocation_contract PRIVATE
		lang/cs/native/capi_internal.cpp lang/cs/native/wse_capi_core.cpp
		core/wse/binding/Runtime.cpp core/wse/binding/FrameBuffer.cpp
		core/wse/binding/Error.cpp core/wse/binding/Cancellation.cpp)
	target_include_directories(wse.capi.allocation_contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/api")
	target_compile_definitions(wse.capi.allocation_contract PRIVATE WSE_CAPI_STATIC WSE_STATIC
		$<$<CXX_COMPILER_ID:MSVC>:_ITERATOR_DEBUG_LEVEL=0>)
	if(WSE_BUILD_XPT)
		target_sources(wse.capi.allocation_contract PRIVATE core/xpt/operation/Cancellation.cpp)
		target_compile_definitions(wse.capi.allocation_contract PRIVATE WSE_HAS_XPT)
	endif()
	set_tests_properties(wse.capi.allocation_contract PROPERTIES TIMEOUT 10)
	# Pins the shared runtime facade every language binding is layered on.
	wse_add_characterization_test(
		wse.core.runtime_contract
		test/characterization/core_runtime_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.runtime_contract PROPERTIES TIMEOUT 5)
	# Pins the channel order wse::Image presents, which a Consumer indexes into directly.
	wse_add_characterization_test(
		wse.core.image_channel_order
		test/characterization/wse_image_channel_order_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.image_channel_order PROPERTIES TIMEOUT 5)
	# Pins the interleaved memory layout of wse::Image, the other half of that same promise.
	wse_add_characterization_test(
		wse.core.image_interleaved
		test/characterization/wse_image_interleaved_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.image_interleaved PROPERTIES TIMEOUT 5)
	# Core-only numerical vectors from CoreAlgorithms, independent of camera/renderer adapters.
	wse_add_characterization_test(
		wse.core.image_algorithms
		test/characterization/wse_image_algorithms_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.core.image_algorithms PROPERTIES TIMEOUT 5)
	# Builds and runs the documented C++ quickstart itself, so a sample that stops compiling or
	# stops working fails here rather than in a reader's hands. Each binding repeats this below.
	wse_add_characterization_test(
		wse.documentation.quickstart_cpp
		example/cpp/core/quickstart.cpp
		WSE::Core
	)
	set_tests_properties(wse.documentation.quickstart_cpp PROPERTIES TIMEOUT 5)
	# Pins the `wse::binding` facade that all four bindings call through, in C++ terms, so a
	# regression is attributed to the facade and not to whichever binding noticed it.
	wse_add_characterization_test(
		wse.binding.facade_contract
		test/characterization/binding_facade_contract.cpp
		WSE::Core
	)
	set_tests_properties(wse.binding.facade_contract PROPERTIES TIMEOUT 5)
	# Proves the documented migration away from every deprecated surface compiles clean. The
	# warning is promoted to an error only for this Target, so the replacement a migration
	# recommends can never itself be deprecated.
	wse_add_characterization_test(
		wse.deprecation.migration_contract
		test/characterization/deprecation_migration_contract.cpp
		WSE::Core
	)
	target_compile_options(wse.deprecation.migration_contract PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/we4996>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror=deprecated-declarations>
	)
	set_tests_properties(wse.deprecation.migration_contract PROPERTIES TIMEOUT 5)

	# Build-only Sample。Coreのデータクラス3種とLog機能の利用形をCompileで固定する。
	foreach(wse_core_sample size range2 matrix image logging)
		add_executable(wse.core.${wse_core_sample}_example
			example/cpp/core/${wse_core_sample}.cpp)
		target_link_libraries(wse.core.${wse_core_sample}_example PRIVATE WSE::Core)
		target_compile_features(wse.core.${wse_core_sample}_example PRIVATE cxx_std_17)
		target_compile_options(wse.core.${wse_core_sample}_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
	endforeach()

	# --------------------------------------------------------------------------------------------
	# Tests: language binding contracts.
	# --------------------------------------------------------------------------------------------

	# Every binding runs the same shape of suite: the documented quickstart, a surface contract, a
	# lifetime contract, a stress run, and one contract per Component it exposes. They are given
	# the built module by path, and the shared golden fixtures, so all four are answering the same
	# questions as the C++ Tests above. A cross Build registers none of them, because the modules
	# it produces cannot be run on the machine that built them.
	if(WSE_BUILD_PYTHON_BINDING AND NOT CMAKE_CROSSCOMPILING)
		# Runs the documented Python quickstart against the built extension.
		add_test(
			NAME wse.documentation.quickstart_python
			COMMAND "${WSE_PYTHON_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/example/python/core/quickstart.py"
				"$<TARGET_FILE:WSE_Python>"
		)
		set_tests_properties(wse.documentation.quickstart_python PROPERTIES TIMEOUT 30)
		# Pins the Python surface: exported names, argument handling, and error translation.
		add_test(
			NAME wse.binding.python_contract
			COMMAND "${WSE_PYTHON_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_contract.py"
				"$<TARGET_FILE:WSE_Python>"
		)
		set_tests_properties(wse.binding.python_contract PROPERTIES TIMEOUT 30)
		# Proves objects created from Python release their native resources deterministically.
		add_test(
			NAME wse.binding.python_lifecycle
			COMMAND "${WSE_PYTHON_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_lifecycle_contract.py"
				"$<TARGET_FILE:WSE_Python>"
		)
		set_tests_properties(wse.binding.python_lifecycle PROPERTIES TIMEOUT 30)
		# Drives the projection path repeatedly against the golden frame, so a leak or a race that
		# a single call would not reveal shows up as drift or exhaustion.
		add_test(
			NAME wse.binding.python_stress
			COMMAND "${WSE_PYTHON_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_stress_contract.py"
				"$<TARGET_FILE:WSE_Python>"
				"${WSE_BINDING_PROJECTION_FIXTURE}"
		)
		set_tests_properties(wse.binding.python_stress PROPERTIES TIMEOUT 90)
		# Proves the Python camera surface reports the same description as the shared fixture.
		if(WSE_BUILD_TMR)
			add_test(
				NAME wse.binding.python_tmr
				COMMAND "${WSE_PYTHON_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_tmr_contract.py"
					"$<TARGET_FILE:WSE_Python>"
					"${WSE_BINDING_WEBCAMERA_FIXTURE}"
			)
			set_tests_properties(wse.binding.python_tmr PROPERTIES TIMEOUT 30)
		endif()
		# Proves the Python projection surface reproduces the golden frame the C++ Test produces.
		if(WSE_BUILD_OUI)
			add_test(
				NAME wse.binding.python_oui
				COMMAND "${WSE_PYTHON_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_oui_contract.py"
					"$<TARGET_FILE:WSE_Python>"
					"${WSE_BINDING_PROJECTION_FIXTURE}"
			)
			set_tests_properties(wse.binding.python_oui PROPERTIES TIMEOUT 30)
		endif()
		# Pins the Python keyboard surface without requiring a physical keyboard.
		if(WSE_BUILD_IUI)
			add_test(
				NAME wse.binding.python_iui
				COMMAND "${WSE_PYTHON_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_iui_contract.py"
					"$<TARGET_FILE:WSE_Python>"
			)
			set_tests_properties(wse.binding.python_iui PROPERTIES TIMEOUT 30)
		endif()
		# Exercises the Python transport surface over loopback; the longer timeout is the socket
		# work, not the binding.
		if(WSE_BUILD_XPT)
			add_test(
				NAME wse.binding.python_xpt
				COMMAND "${WSE_PYTHON_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/python_xpt_contract.py"
					"$<TARGET_FILE:WSE_Python>"
			)
			set_tests_properties(wse.binding.python_xpt PROPERTIES TIMEOUT 60)
		endif()
	endif()

	if(WSE_BUILD_NODE_BINDING)
		# The addon compiles without Node.js, but nothing can confirm it loads without one, so an
		# unverifiable binding is refused rather than shipped untested.
		if(WSE_NODE_EXECUTABLE STREQUAL "")
			find_program(WSE_NODE_EXECUTABLE NAMES node nodejs)
		endif()
		if(NOT WSE_NODE_EXECUTABLE)
			message(FATAL_ERROR
				"WSE_BUILD_NODE_BINDING requires Node.js for the runtime contract test. "
				"Set WSE_NODE_EXECUTABLE to a Node.js executable.")
		endif()
		# Runs the documented JavaScript quickstart against the built addon.
		add_test(
			NAME wse.documentation.quickstart_javascript
			COMMAND "${WSE_NODE_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/example/js/core/quickstart.js"
				"$<TARGET_FILE:WSE_Node>"
		)
		set_tests_properties(wse.documentation.quickstart_javascript PROPERTIES TIMEOUT 30)
		# Pins the addon's own surface: exported names, argument handling, and error translation.
		add_test(
			NAME wse.binding.node_api_contract
			COMMAND "${WSE_NODE_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_api_contract.js"
				"$<TARGET_FILE:WSE_Node>"
		)
		set_tests_properties(wse.binding.node_api_contract PROPERTIES TIMEOUT 30)
		# Proves the published JavaScript layer agrees with the addon: `lang/js` is passed in so
		# the Test can check `index.js` and `index.d.ts` against what the addon actually exports.
		add_test(
			NAME wse.binding.node_package
			COMMAND "${WSE_NODE_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_package_contract.js"
				"$<TARGET_FILE:WSE_Node>"
				"${CMAKE_CURRENT_SOURCE_DIR}/lang/js"
		)
		set_tests_properties(wse.binding.node_package PROPERTIES TIMEOUT 30)
		# Proves native resources are released when the JavaScript object that owned them goes.
		add_test(
			NAME wse.binding.node_api_lifecycle
			COMMAND "${WSE_NODE_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_api_lifecycle_contract.js"
				"$<TARGET_FILE:WSE_Node>"
		)
		set_tests_properties(wse.binding.node_api_lifecycle PROPERTIES TIMEOUT 30)
		# The Python stress run's counterpart. `--expose-gc` lets the Test collect deliberately and
		# then assert that native memory actually went with the collected wrappers, which is the
		# only way to tell a leak from a collection that has not happened yet.
		add_test(
			NAME wse.binding.node_stress
			COMMAND "${WSE_NODE_EXECUTABLE}" --expose-gc
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_stress_contract.js"
				"$<TARGET_FILE:WSE_Node>"
				"${WSE_BINDING_PROJECTION_FIXTURE}"
		)
		set_tests_properties(wse.binding.node_stress PROPERTIES TIMEOUT 90)
		# Proves the JavaScript camera surface reports the same description as the shared fixture.
		if(WSE_BUILD_TMR)
			add_test(
				NAME wse.binding.node_tmr
				COMMAND "${WSE_NODE_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_tmr_contract.js"
					"$<TARGET_FILE:WSE_Node>"
					"${WSE_BINDING_WEBCAMERA_FIXTURE}"
			)
			set_tests_properties(wse.binding.node_tmr PROPERTIES TIMEOUT 30)
			# Proves the WebCamera class the JavaScript package publishes matches the addon, so
			# the typed wrapper cannot drift away from what it wraps.
			add_test(
				NAME wse.binding.node_webcamera_public
				COMMAND "${WSE_NODE_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_webcamera_public_contract.js"
					"$<TARGET_FILE:WSE_Node>"
					"${CMAKE_CURRENT_SOURCE_DIR}/lang/js"
			)
			set_tests_properties(wse.binding.node_webcamera_public PROPERTIES TIMEOUT 30)
		endif()
		# Proves the JavaScript projection surface reproduces the same golden frame.
		if(WSE_BUILD_OUI)
			add_test(
				NAME wse.binding.node_oui
				COMMAND "${WSE_NODE_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_oui_contract.js"
					"$<TARGET_FILE:WSE_Node>"
					"${WSE_BINDING_PROJECTION_FIXTURE}"
			)
			set_tests_properties(wse.binding.node_oui PROPERTIES TIMEOUT 30)
		endif()
		# Pins the JavaScript keyboard surface without requiring a physical keyboard.
		if(WSE_BUILD_IUI)
			add_test(
				NAME wse.binding.node_iui
				COMMAND "${WSE_NODE_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_iui_contract.js"
					"$<TARGET_FILE:WSE_Node>"
			)
			set_tests_properties(wse.binding.node_iui PROPERTIES TIMEOUT 30)
		endif()
		# Exercises the JavaScript transport surface over loopback, and checks it against the
		# declarations the package publishes.
		if(WSE_BUILD_XPT)
			add_test(
				NAME wse.binding.node_xpt
				COMMAND "${WSE_NODE_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/node_xpt_contract.js"
					"$<TARGET_FILE:WSE_Node>"
					"${CMAKE_CURRENT_SOURCE_DIR}/lang/js"
			)
			set_tests_properties(wse.binding.node_xpt PROPERTIES TIMEOUT 60)
		endif()
	endif()

	if(WSE_BUILD_JAVA_BINDING AND NOT CMAKE_CROSSCOMPILING)
		# A STATIC Build folds the Core into the JNI module, so there is no separate Runtime file
		# to point the Test at; only the SHARED Build has one to name.
		set(WSE_JAVA_RUNTIME_ARGUMENTS)
		if(WSE_LIBRARY_TYPE STREQUAL "SHARED")
			list(APPEND WSE_JAVA_RUNTIME_ARGUMENTS
				"-Dwse.runtime.path=$<TARGET_FILE:WonderStewEngine>")
		endif()
		# `java.library.path` finds the JNI module, but on Windows the loader still resolves that
		# module's own dependency on the Runtime DLL through PATH, so the output directory is
		# prepended for every Java Test below.
		set(WSE_JAVA_RUNTIME_PATH_MODIFICATION
			"PATH=path_list_prepend:$<TARGET_FILE_DIR:WonderStewEngine>")
		# Registers the contract and stress pair for one JVM. The suffix keeps the Test names
		# distinct when the matrix below runs the same sources on several runtimes, and the
		# expected feature version is asserted inside the Test so a mislabelled entry is caught.
		function(wse_add_java_binding_tests suffix_in runtime_in expected_feature_in)
			set(wse_java_expected_argument)
			set(wse_java_native_access_argument)
			if(NOT expected_feature_in STREQUAL "")
				set(wse_java_expected_argument
					"-Dwse.expected.java.feature=${expected_feature_in}")
			endif()
			# Java 25 warns on every native call from an unnamed module, which would bury the
			# Test's own output. Only that runtime needs the grant.
			if(expected_feature_in STREQUAL "25")
				set(wse_java_native_access_argument
					"--enable-native-access=ALL-UNNAMED")
			endif()
			# Pins the Java surface against both shared golden fixtures.
			add_test(
				NAME "wse.binding.java_contract${suffix_in}"
				COMMAND "${runtime_in}"
					${wse_java_native_access_argument}
					${WSE_JAVA_RUNTIME_ARGUMENTS}
					${wse_java_expected_argument}
					"-Dwse.golden.path=${WSE_BINDING_PROJECTION_FIXTURE}"
					"-Dwse.camera.fixture=${WSE_BINDING_WEBCAMERA_FIXTURE}"
					"-Djava.library.path=$<TARGET_FILE_DIR:WSE_Java>"
					-cp "${WSE_JAVA_CLASSES_DIR}${WSE_JAVA_CLASS_PATH_SEPARATOR}${WSE_JAVA_TEST_CLASSES_DIR}"
					io.wapitistew.wse.BindingContract
			)
			set_tests_properties("wse.binding.java_contract${suffix_in}" PROPERTIES
				TIMEOUT 30
				ENVIRONMENT_MODIFICATION "${WSE_JAVA_RUNTIME_PATH_MODIFICATION}")
			# The long-running counterpart, driving the projection path until a leak or a race
			# would show. Its timeout is the largest in the suite for that reason.
			add_test(
				NAME "wse.binding.java_stress${suffix_in}"
				COMMAND "${runtime_in}"
					${wse_java_native_access_argument}
					${WSE_JAVA_RUNTIME_ARGUMENTS}
					${wse_java_expected_argument}
					"-Dwse.golden.path=${WSE_BINDING_PROJECTION_FIXTURE}"
					"-Djava.library.path=$<TARGET_FILE_DIR:WSE_Java>"
					-cp "${WSE_JAVA_CLASSES_DIR}${WSE_JAVA_CLASS_PATH_SEPARATOR}${WSE_JAVA_TEST_CLASSES_DIR}"
					io.wapitistew.wse.BindingStress
			)
			set_tests_properties("wse.binding.java_stress${suffix_in}" PROPERTIES
				TIMEOUT 240
				ENVIRONMENT_MODIFICATION "${WSE_JAVA_RUNTIME_PATH_MODIFICATION}")
		endfunction()

		# The binding claims Java 17 and later, and a single JVM cannot demonstrate that. Every
		# runtime a builder supplied is registered as its own pair; if none was, the suite still
		# runs once against the JDK that compiled the classes, so the binding is never untested.
		set(wse_java_matrix_configured FALSE)
		foreach(wse_java_feature IN ITEMS 17 21 25)
			set(wse_java_runtime_variable "WSE_JAVA_${wse_java_feature}_EXECUTABLE")
			set(wse_java_runtime "${${wse_java_runtime_variable}}")
			if(NOT wse_java_runtime STREQUAL "")
				if(NOT EXISTS "${wse_java_runtime}")
					message(FATAL_ERROR
						"${wse_java_runtime_variable} does not exist: ${wse_java_runtime}")
				endif()
				wse_add_java_binding_tests(
					".jdk${wse_java_feature}" "${wse_java_runtime}" "${wse_java_feature}")
				set(wse_java_matrix_configured TRUE)
			endif()
		endforeach()
		if(NOT wse_java_matrix_configured)
			wse_add_java_binding_tests("" "${Java_JAVA_EXECUTABLE}" "")
		endif()
		# Runs the documented Java quickstart. It stays on the compiling JDK rather than joining
		# the matrix, because it demonstrates the documentation, not runtime compatibility.
		add_test(
			NAME wse.documentation.quickstart_java
			COMMAND "${Java_JAVA_EXECUTABLE}"
				${WSE_JAVA_RUNTIME_ARGUMENTS}
				"-Djava.library.path=$<TARGET_FILE_DIR:WSE_Java>"
				-cp "${WSE_JAVA_CLASSES_DIR}${WSE_JAVA_CLASS_PATH_SEPARATOR}${WSE_JAVA_TEST_CLASSES_DIR}"
				QuickStart
		)
		set_tests_properties(wse.documentation.quickstart_java PROPERTIES
			TIMEOUT 30
			ENVIRONMENT_MODIFICATION "${WSE_JAVA_RUNTIME_PATH_MODIFICATION}")
	endif()

	if(WSE_BUILD_DOTNET_BINDING AND NOT CMAKE_CROSSCOMPILING)
		# Exercise the loaded public C shim as well as its header-only ABI snapshot.
		wse_add_characterization_test(
			wse.capi.runtime_contract
			test/characterization/wse_capi_runtime_contract.cpp
			WSE::Dotnet
		)
		target_include_directories(wse.capi.runtime_contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/api")
		target_link_libraries(wse.capi.runtime_contract PRIVATE Threads::Threads)
		set_tests_properties(wse.capi.runtime_contract PROPERTIES TIMEOUT 10)
		wse_add_characterization_test(wse.capi.camera_delivery_contract
			test/characterization/wse_capi_camera_delivery_contract.cpp WSE::Core)
		target_sources(wse.capi.camera_delivery_contract PRIVATE lang/cs/native/capi_internal.cpp)
		target_compile_definitions(wse.capi.camera_delivery_contract PRIVATE WSE_CAPI_STATIC)
		set_tests_properties(wse.capi.camera_delivery_contract PROPERTIES TIMEOUT 10)
		if(WSE_BUILD_XPT)
			wse_add_characterization_test(wse.capi.transfer_contract
				test/characterization/wse_capi_transfer_contract.cpp WSE::Xpt)
			target_sources(wse.capi.transfer_contract PRIVATE lang/cs/native/capi_internal.cpp)
			target_compile_definitions(wse.capi.transfer_contract PRIVATE WSE_CAPI_STATIC)
			wse_add_characterization_test(wse.capi.udp_progress_contract
				test/characterization/wse_capi_udp_progress_contract.cpp WSE::Dotnet)
			target_include_directories(wse.capi.udp_progress_contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/api")
			set_tests_properties(wse.capi.transfer_contract wse.capi.udp_progress_contract PROPERTIES TIMEOUT 10)
		endif()
		# C# Sampleと契約Testは実行可能Assemblyを必要とするため、Project単位でBuildする。
		# Native Libraryの位置は実行時引数で渡す。Node/Python/Javaと同じRuntime path契約である。
		set(WSE_DOTNET_TEST_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/dotnet-test")
		# Glob the contract sources so a new per-component contract file rebuilds the assembly.
		file(GLOB WSE_DOTNET_CONTRACT_SOURCES CONFIGURE_DEPENDS
			"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/cs/*.cs")
		if(WSE_EXTENSION_DOTNET_ROOT)
			file(GLOB WSE_DOTNET_EXTENSION_CONTRACT_SOURCES CONFIGURE_DEPENDS
				"${WSE_EXTENSION_DOTNET_ROOT}/test/binding/cs/*.cs")
			list(APPEND WSE_DOTNET_CONTRACT_SOURCES ${WSE_DOTNET_EXTENSION_CONTRACT_SOURCES})
		endif()
		set(WSE_DOTNET_CONTRACT_ASSEMBLY "${WSE_DOTNET_TEST_OUTPUT_DIR}/BindingContract.dll")
		set(WSE_DOTNET_QUICKSTART_ASSEMBLY "${WSE_DOTNET_TEST_OUTPUT_DIR}/QuickStart.dll")
		add_custom_command(
			OUTPUT "${WSE_DOTNET_CONTRACT_ASSEMBLY}" "${WSE_DOTNET_QUICKSTART_ASSEMBLY}"
			COMMAND "${WSE_DOTNET_EXECUTABLE}" build
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/cs/BindingContract.csproj"
				--configuration Release --output "${WSE_DOTNET_TEST_OUTPUT_DIR}"
				"-p:WseExtensionDir=${WSE_EXTENSION_DOTNET_ROOT}"
				--no-incremental
				--nologo
			COMMAND "${WSE_DOTNET_EXECUTABLE}" build
				"${CMAKE_CURRENT_SOURCE_DIR}/example/cs/core/QuickStart.csproj"
				--configuration Release --output "${WSE_DOTNET_TEST_OUTPUT_DIR}"
				# QuickStartもWse.csprojを参照し、上のBuildと同じ場所へ書く。Overlayの
				# 有無を伝えないと、ここで組んだOverlay抜きのAssemblyが上書きしてしまう。
				"-p:WseExtensionDir=${WSE_EXTENSION_DOTNET_ROOT}"
				--no-incremental
				--nologo
			DEPENDS "${WSE_DOTNET_ASSEMBLY}"
				${WSE_DOTNET_CONTRACT_SOURCES}
				"${CMAKE_CURRENT_SOURCE_DIR}/test/binding/cs/BindingContract.csproj"
				"${CMAKE_CURRENT_SOURCE_DIR}/example/cs/core/QuickStart.cs"
				"${CMAKE_CURRENT_SOURCE_DIR}/example/cs/core/QuickStart.csproj"
			VERBATIM
		)
		add_custom_target(WSE_DotnetContract ALL DEPENDS
			"${WSE_DOTNET_CONTRACT_ASSEMBLY}" "${WSE_DOTNET_QUICKSTART_ASSEMBLY}")
		# Both projects build Wse.csproj into its shared obj directory. File-only
		# dependencies can duplicate the custom rule under parallel MSBuild.
		add_dependencies(WSE_DotnetContract WSE_DotnetAssembly wse.capi.c_layout_snapshot)

		# Runs the documented C# quickstart, given the shim by path exactly as the other bindings
		# are given their module.
		add_test(
			NAME wse.documentation.quickstart_cs
			COMMAND "${WSE_DOTNET_EXECUTABLE}"
				"${WSE_DOTNET_QUICKSTART_ASSEMBLY}"
				"$<TARGET_FILE:WSE_Dotnet>"
		)
		set_tests_properties(wse.documentation.quickstart_cs PROPERTIES TIMEOUT 60)
		# Pins the C# surface and, through it, the flat C ABI the P/Invoke declarations describe:
		# a signature that drifts on either side fails here rather than at a Consumer's call.
		add_test(
			NAME wse.binding.dotnet_contract
			COMMAND "${WSE_DOTNET_EXECUTABLE}"
				"${WSE_DOTNET_CONTRACT_ASSEMBLY}"
				"$<TARGET_FILE:WSE_Dotnet>"
				"$<TARGET_FILE:wse.capi.c_layout_snapshot>"
		)
		set_tests_properties(wse.binding.dotnet_contract PROPERTIES TIMEOUT 60)
		# Artificial C ABI provider: never opens hardware or joins the installed package.
		add_library(wse_dotnet_ownership_fixture SHARED test/binding/cs/ownership_native_fixture.cpp)
		target_include_directories(wse_dotnet_ownership_fixture PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/api")
		target_compile_features(wse_dotnet_ownership_fixture PRIVATE cxx_std_17)
		target_compile_options(wse_dotnet_ownership_fixture PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8> $<$<CXX_COMPILER_ID:MSVC>:/EHsc>)
		target_compile_definitions(wse_dotnet_ownership_fixture PRIVATE __WSE_CAPI_EXPORTS__)
		target_link_libraries(wse_dotnet_ownership_fixture PRIVATE Threads::Threads)
		add_test(NAME wse.binding.dotnet_ownership_contract
			COMMAND "${WSE_DOTNET_EXECUTABLE}" "${WSE_DOTNET_CONTRACT_ASSEMBLY}"
				"$<TARGET_FILE:wse_dotnet_ownership_fixture>" --ownership)
		set_tests_properties(wse.binding.dotnet_ownership_contract PROPERTIES TIMEOUT 30)
		add_test(NAME wse.binding.dotnet_transfer_contract
			COMMAND "${WSE_DOTNET_EXECUTABLE}" "${WSE_DOTNET_CONTRACT_ASSEMBLY}"
				"$<TARGET_FILE:wse_dotnet_ownership_fixture>" --transfer)
		set_tests_properties(wse.binding.dotnet_transfer_contract PROPERTIES TIMEOUT 30)
	endif()

	# --------------------------------------------------------------------------------------------
	# Tests: per-component contracts and samples.
	# --------------------------------------------------------------------------------------------

	if(WSE_BUILD_OUI)
		# Build-only Sample。Source→Screen→Projection→Windowの多段描画をWindow表示
		# 込みで示す。実行にはDesktop Sessionが必要となる。
		add_executable(wse.oui.windowed_projection_example
			example/cpp/oui/windowed_projection.cpp)
		target_link_libraries(wse.oui.windowed_projection_example PRIVATE WSE::Oui)
		target_compile_features(wse.oui.windowed_projection_example PRIVATE cxx_std_17)
		target_compile_options(wse.oui.windowed_projection_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

		# Build-only Sample。Borderless Fullscreenへの遷移・描画・Windowed復帰を示す。
		add_executable(wse.oui.borderless_fullscreen_example
			example/cpp/oui/borderless_fullscreen.cpp)
		target_link_libraries(wse.oui.borderless_fullscreen_example PRIVATE WSE::Oui)
		target_compile_features(wse.oui.borderless_fullscreen_example PRIVATE cxx_std_17)
		target_compile_options(wse.oui.borderless_fullscreen_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

		# Proves no Windows or Direct3D detail is visible in `api/oui`, which is what keeps the
		# same headers usable on the Vulkan backend.
		add_test(
			NAME wse.oui.public_header_boundary
			COMMAND "${CMAKE_COMMAND}"
				"-DWSE_OUI_API_DIR=${CMAKE_CURRENT_SOURCE_DIR}/api/oui"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_oui_public_headers.cmake"
		)
		# Windows only, because it is a rule about COM: the D3D12 backend must own interfaces
		# through ComPtr rather than balancing reference counts by hand.
		if(WIN32)
			add_test(
				NAME wse.oui.resource_ownership
				COMMAND "${CMAKE_COMMAND}"
					"-DWSE_OUI_WINDOWS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/platform/oui/win"
					-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_oui_resource_ownership.cmake"
			)
		endif()
		# Pins the frame description a Consumer hands the renderer.
		wse_add_characterization_test(
			wse.oui.renderer_identity
			test/characterization/oui_renderer_identity_contract.cpp
			WSE::Oui
		)
		set_tests_properties(wse.oui.renderer_identity PROPERTIES TIMEOUT 90)
		wse_add_characterization_test(
			wse.oui.renderer_generation
			test/characterization/oui_renderer_generation_contract.cpp
			WSE::Oui
		)
		wse_add_characterization_test(
			wse.oui.frame_contract
			test/characterization/oui_frame_contract.cpp
			WSE::Oui
		)
		# Pins the projection mathematics — homography, tie points, mesh — in isolation from any
		# graphics device, so a geometry regression is not mistaken for a driver difference.
		wse_add_characterization_test(
			wse.oui.projection_geometry
			test/characterization/projection_geometry_contract.cpp
			WSE::Oui
		)
		# Pins the renderer's public state machine.
		wse_add_characterization_test(
			wse.oui.projection_cleanup_contract
			test/characterization/oui_projection_cleanup_contract.cpp
			WSE::Oui
		)
		set_tests_properties(wse.oui.projection_cleanup_contract PROPERTIES TIMEOUT 10)
		wse_add_characterization_test(
			wse.oui.renderer_contract
			test/characterization/oui_renderer_contract.cpp
			WSE::Oui
		)
		# Pins how a wse::Image is accepted as a frame source and handed back.
		wse_add_characterization_test(
			wse.oui.renderer_frame_image
			test/characterization/oui_renderer_frame_image_contract.cpp
			WSE::Oui
		)
		set_tests_properties(wse.oui.renderer_frame_image PROPERTIES TIMEOUT 5)
		# The golden Tests that follow expect one image from either backend. The name is passed to
		# the executable so it can say which backend produced a mismatch; the expected pixels do
		# not differ, which is the portability claim these Tests exist to hold.
		if(WIN32)
			set(WSE_PROJECTION_GOLDEN_BACKEND d3d12)
		else()
			set(WSE_PROJECTION_GOLDEN_BACKEND vulkan)
		endif()
		# Proves the projection reached through the binding facade matches the shared fixture, so
		# the four language Tests above are comparing against something already verified in C++.
		wse_add_characterization_test(
			wse.oui.binding_projection_contract
			test/characterization/oui_binding_projection_contract.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
			${WSE_BINDING_PROJECTION_FIXTURE}
		)
		set_tests_properties(wse.oui.binding_projection_contract PROPERTIES TIMEOUT 30)
		wse_add_characterization_test(
			wse.oui.backend_projection_golden
			example/cpp/oui/portable_projection.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
		)
		set_tests_properties(wse.oui.backend_projection_golden PROPERTIES TIMEOUT 30)
		wse_add_characterization_test(
			wse.oui.projection_scale_alpha_golden
			test/characterization/oui_projection_scale_alpha_golden.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
		)
		set_tests_properties(wse.oui.projection_scale_alpha_golden PROPERTIES TIMEOUT 30)
		wse_add_characterization_test(
			wse.oui.projection_supersample_golden
			test/characterization/oui_projection_supersample_golden.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
		)
		set_tests_properties(wse.oui.projection_supersample_golden PROPERTIES TIMEOUT 30)
		wse_add_characterization_test(
			wse.oui.projection_edge_multisource_golden
			test/characterization/oui_projection_edge_multisource_golden.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
		)
		set_tests_properties(wse.oui.projection_edge_multisource_golden PROPERTIES TIMEOUT 30)
		wse_add_characterization_test(
			wse.oui.projection_dynamic_mesh_golden
			test/characterization/oui_projection_dynamic_mesh_golden.cpp
			WSE::Oui
			${WSE_PROJECTION_GOLDEN_BACKEND}
		)
		set_tests_properties(wse.oui.projection_dynamic_mesh_golden PROPERTIES TIMEOUT 30)
		if(WIN32)

			# Fixes the renderer's resource ownership and reinitialization contract.
			wse_add_characterization_test(
				wse.oui.renderer_lifetime
				test/characterization/oui_renderer_lifetime_contract.cpp
				WSE::Oui
			)
			set_tests_properties(wse.oui.renderer_lifetime PROPERTIES TIMEOUT 60)

			wse_add_characterization_test(
				wse.oui.d3d12_offscreen_golden
				test/characterization/oui_d3d12_offscreen_golden.cpp
				WSE::Oui
				"${CMAKE_CURRENT_SOURCE_DIR}/test/golden/oui_d3d12_quadrants.ppm"
			)
			set_tests_properties(wse.oui.d3d12_offscreen_golden PROPERTIES TIMEOUT 30)

			wse_add_characterization_test(
				wse.oui.d3d12_mesh_golden
				test/characterization/oui_d3d12_mesh_golden.cpp
				WSE::Oui
			)
			set_tests_properties(wse.oui.d3d12_mesh_golden PROPERTIES TIMEOUT 30)

			wse_add_characterization_test(
				wse.oui.d3d12_window_surface
				test/characterization/oui_d3d12_window_surface.cpp
				WSE::Oui
			)
			set_tests_properties(wse.oui.d3d12_window_surface PROPERTIES TIMEOUT 30)

			wse_add_characterization_test(
				wse.oui.d3d12_display_enumeration
				test/characterization/oui_d3d12_display_enumeration.cpp
				WSE::Oui
			)
			set_tests_properties(wse.oui.d3d12_display_enumeration PROPERTIES TIMEOUT 30)

			wse_add_characterization_test(
				wse.oui.d3d12_window_resize_fullscreen
				test/characterization/oui_d3d12_window_resize_fullscreen.cpp
				WSE::Oui
			)
			set_tests_properties(wse.oui.d3d12_window_resize_fullscreen PROPERTIES TIMEOUT 60)

			if(WSE_ENABLE_DISPLAY_MODE_TESTS)
				if(NOT WSE_DISPLAY_TEST_NUMBER MATCHES "^[1-9][0-9]*$")
					message(FATAL_ERROR
						"Windows display-mode tests require an explicit positive "
						"WSE_DISPLAY_TEST_NUMBER. No primary-display fallback is used.")
				endif()
				if(NOT WSE_DISPLAY_TEST_HOLD_MS MATCHES "^[0-9]+$")
					message(FATAL_ERROR
						"WSE_DISPLAY_TEST_HOLD_MS must be a non-negative integer.")
				endif()
				if(WSE_DISPLAY_TEST_HOLD_MS GREATER 20000)
					message(FATAL_ERROR
						"WSE_DISPLAY_TEST_HOLD_MS must not exceed 20000 milliseconds.")
				endif()
				wse_add_characterization_test(
					wse.oui.d3d12_display_mode_restoration
					test/characterization/oui_d3d12_display_mode_restoration.cpp
					WSE::Oui
					--display-number ${WSE_DISPLAY_TEST_NUMBER}
					--hold-ms ${WSE_DISPLAY_TEST_HOLD_MS}
				)
				set_tests_properties(
					wse.oui.d3d12_display_mode_restoration
					PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77
				)
			endif()
		elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			# Compile the internal backend in the test executable to inject native failures
			# without adding test hooks to the installed library or public headers.
			wse_add_characterization_test(
				wse.oui.vulkan_submission_contract
				test/characterization/oui_vulkan_submission_contract.cpp
				WSE::Oui
			)
			target_sources(wse.oui.vulkan_submission_contract PRIVATE
				platform/oui/linux/renderer/WaylandWindow.cpp
				"${WSE_XDG_SHELL_CODE}" "${WSE_XDG_SHELL_HEADER}"
				"${WSE_VULKAN_VERTEX_HEADER}" "${WSE_VULKAN_FRAGMENT_HEADER}"
			)
			target_include_directories(wse.oui.vulkan_submission_contract PRIVATE "${WSE_OUI_GENERATED_DIR}")
			target_link_libraries(wse.oui.vulkan_submission_contract PRIVATE
				Vulkan::Vulkan PkgConfig::WSE_WAYLAND PkgConfig::WSE_DRM)
			set_tests_properties(wse.oui.vulkan_submission_contract PROPERTIES TIMEOUT 60)
			wse_add_characterization_test(
				wse.oui.vulkan_wayland_surface
				test/characterization/oui_vulkan_wayland_surface.cpp
				WSE::Oui
			)
			set_tests_properties(
				wse.oui.vulkan_wayland_surface
				PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77
			)
			if(WSE_ENABLE_DISPLAY_MODE_TESTS)
				wse_add_characterization_test(
					wse.oui.vulkan_drm_restoration
					test/characterization/oui_vulkan_drm_restoration.cpp
					WSE::Oui
				)
				set_tests_properties(
					wse.oui.vulkan_drm_restoration
					PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77
				)
			endif()
		endif()
	endif()

	if(WSE_BUILD_XPT)
		wse_add_characterization_test(
			wse.binding.transfer_failure_contract
			test/characterization/binding_transfer_failure_contract.cpp
			WSE::Xpt
		)
		wse_add_characterization_test(
			wse.xpt.tcp_loopback
			test/characterization/xpt_tcp_loopback.cpp
			WSE::Xpt
		)
		if(WIN32)
			target_link_libraries(wse.xpt.tcp_loopback PRIVATE ws2_32)
		endif()
		set_tests_properties(wse.xpt.tcp_loopback PROPERTIES TIMEOUT 10)

		wse_add_characterization_test(
			wse.xpt.udp_loopback
			test/characterization/xpt_udp_loopback.cpp
			WSE::Xpt
		)
		if(WIN32)
			target_link_libraries(wse.xpt.udp_loopback PRIVATE ws2_32)
		endif()
		set_tests_properties(wse.xpt.udp_loopback PROPERTIES TIMEOUT 10)

		wse_add_characterization_test(
			wse.xpt.retry_policy_contract
			test/characterization/xpt_retry_policy_contract.cpp
			WSE::Xpt
		)

		wse_add_characterization_test(
			wse.xpt.http_contract
			test/characterization/xpt_http_contract.cpp
			WSE::Xpt
		)
		if(WIN32)
			target_link_libraries(wse.xpt.http_contract PRIVATE ws2_32)
		endif()
		set_tests_properties(wse.xpt.http_contract PROPERTIES TIMEOUT 20)

		wse_add_characterization_test(
			wse.xpt.serial_port_contract
			test/characterization/xpt_serial_port_contract.cpp
			WSE::Xpt
		)
		set_tests_properties(wse.xpt.serial_port_contract PROPERTIES TIMEOUT 10)
		if(WIN32)
			wse_add_characterization_test(
				wse.xpt.serial_windows_read_contract
				test/characterization/xpt_serial_windows_read_contract.cpp
				WSE::Xpt
			)
			set_tests_properties(wse.xpt.serial_windows_read_contract PROPERTIES TIMEOUT 10)
		endif()
	endif()

	if(WSE_BUILD_GEF)
		wse_add_characterization_test(
			wse.gef.recovery_contract
			test/characterization/gef_recovery_contract.cpp
			WSE::Gef
			"${CMAKE_BINARY_DIR}/gef-recovery-fixtures")
		set_tests_properties(wse.gef.recovery_contract PROPERTIES TIMEOUT 30)
		if(Python3_Interpreter_FOUND)
			# Independent stdlib codec exchanges files with a public-API-only native bridge.
			add_executable(wse.gef.reconstruction_bridge
				test/characterization/gef_reconstruction_bridge.cpp)
			target_link_libraries(wse.gef.reconstruction_bridge PRIVATE WSE::Gef)
			target_compile_features(wse.gef.reconstruction_bridge PRIVATE cxx_std_17)
			target_compile_options(wse.gef.reconstruction_bridge PRIVATE
				$<$<CXX_COMPILER_ID:MSVC>:/EHsc> $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
			foreach(wse_gef_config Debug Release MinSizeRel RelWithDebInfo)
				string(TOUPPER "${wse_gef_config}" wse_gef_config_upper)
				set_target_properties(wse.gef.reconstruction_bridge PROPERTIES
					RUNTIME_OUTPUT_DIRECTORY_${wse_gef_config_upper}
					"${CMAKE_BINARY_DIR}/${WSE_VS_PLATFORM_DIR}/${wse_gef_config}")
			endforeach()
			add_test(NAME wse.gef.reconstruction COMMAND "${Python3_EXECUTABLE}"
				"${CMAKE_CURRENT_SOURCE_DIR}/test/reconstruction/gef_reference.py"
				--bridge "$<TARGET_FILE:wse.gef.reconstruction_bridge>"
				--scratch "${CMAKE_BINARY_DIR}")
			set_tests_properties(wse.gef.reconstruction PROPERTIES TIMEOUT 60)
		endif()
		wse_add_characterization_test(
			wse.gef.bin_contract
			test/characterization/gef_bin_contract.cpp
			WSE::Gef
			"$<TARGET_FILE_DIR:wse.gef.bin_contract>/wse_gef_binary_contract.bin"
		)
		wse_add_characterization_test(
			wse.gef.csv_contract
			test/characterization/gef_csv_contract.cpp
			WSE::Gef
			"$<TARGET_FILE_DIR:wse.gef.csv_contract>/wse_gef_input.csv"
			"$<TARGET_FILE_DIR:wse.gef.csv_contract>/wse_gef_output.csv"
		)
	endif()

	if(WSE_BUILD_XPT)
		# LoopbackだけでSend／Receiveを完結させるため、実機Networkを必要としない。
		add_executable(wse.xpt.udp_loopback_example
			example/cpp/xpt/udp_loopback.cpp)
		target_link_libraries(wse.xpt.udp_loopback_example PRIVATE WSE::Xpt)
		target_compile_features(wse.xpt.udp_loopback_example PRIVATE cxx_std_17)
		target_compile_options(wse.xpt.udp_loopback_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

		# Build-only Sample。Serial／HTTP／TCPの各通信方式の利用形をCompileで固定する。
		# 接続先はSource内定数のため、実機・実Serverが無い環境ではStructured Errorを
		# Log出力して終了する。
		foreach(wse_xpt_sample serial_send_receive http_get tcp_client)
			add_executable(wse.xpt.${wse_xpt_sample}_example
				example/cpp/xpt/${wse_xpt_sample}.cpp)
			target_link_libraries(wse.xpt.${wse_xpt_sample}_example PRIVATE WSE::Xpt)
			target_compile_features(wse.xpt.${wse_xpt_sample}_example PRIVATE cxx_std_17)
			target_compile_options(wse.xpt.${wse_xpt_sample}_example PRIVATE
				$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
				$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
		endforeach()
	endif()

	if(WSE_BUILD_IUI)
		# Sampleは実機を必要としないが、公開APIの利用形をCompileで固定する。
		add_executable(wse.iui.keyboard_example
			example/cpp/iui/keyboard.cpp)
		target_link_libraries(wse.iui.keyboard_example PRIVATE WSE::Iui)
		target_compile_features(wse.iui.keyboard_example PRIVATE cxx_std_17)
		target_compile_options(wse.iui.keyboard_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

		wse_add_characterization_test(
			wse.iui.keyboard_lifecycle
			test/characterization/iui_keyboard_lifecycle.cpp
			WSE::Iui
		)
		set_tests_properties(wse.iui.keyboard_lifecycle PROPERTIES TIMEOUT 5)
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			wse_add_characterization_test(
				wse.iui.keyboard_devices_contract
				test/characterization/iui_keyboard_devices_contract.cpp
				WSE::Iui
			)
			set_tests_properties(wse.iui.keyboard_devices_contract PROPERTIES TIMEOUT 10)
		endif()
		if(WSE_ENABLE_IUI_HARDWARE_TESTS)
			wse_add_characterization_test(
				wse.iui.keyboard_hardware_smoke
				test/hardware/iui_keyboard_hardware_smoke.cpp
				WSE::Iui
			)
			set_tests_properties(wse.iui.keyboard_hardware_smoke PROPERTIES
				TIMEOUT 90
				LABELS "hardware;interactive")
		endif()
	endif()

	if(WSE_BUILD_TMR)
		add_executable(wse.tmr.webcamera_example
			example/cpp/tmr/webcamera.cpp)
		target_link_libraries(wse.tmr.webcamera_example PRIVATE WSE::Tmr)
		target_compile_features(wse.tmr.webcamera_example PRIVATE cxx_std_17)
		target_compile_options(wse.tmr.webcamera_example PRIVATE
			$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
			$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

		# Build-only Sample。1 Frame取得・露出変更・解像度変更・Streamの利用形を
		# Compileで固定する。実Cameraは実行時にのみ必要となる。
		foreach(wse_tmr_sample single_frame exposure_control resolution_change video_stream camera_image)
			add_executable(wse.tmr.${wse_tmr_sample}_example
				example/cpp/tmr/${wse_tmr_sample}.cpp)
			target_link_libraries(wse.tmr.${wse_tmr_sample}_example PRIVATE WSE::Tmr)
			target_compile_features(wse.tmr.${wse_tmr_sample}_example PRIVATE cxx_std_17)
			target_compile_options(wse.tmr.${wse_tmr_sample}_example PRIVATE
				$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
				$<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
		endforeach()

		add_test(
			NAME wse.tmr.public_header_boundary
			COMMAND "${CMAKE_COMMAND}"
				"-DWSE_TMR_API_DIR=${CMAKE_CURRENT_SOURCE_DIR}/api/tmr"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_tmr_public_headers.cmake"
		)
		add_test(
			NAME wse.tmr.resource_ownership
			COMMAND "${CMAKE_COMMAND}"
				"-DWSE_TMR_API_DIR=${CMAKE_CURRENT_SOURCE_DIR}/api/tmr"
				"-DWSE_TMR_CORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/core/tmr"
				"-DWSE_TMR_WINDOWS_CAMERA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/platform/tmr/win/camera"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_tmr_resource_ownership.cmake"
		)
		wse_add_characterization_test(
			wse.tmr.camera_contract
			test/characterization/tmr_camera_contract.cpp
			WSE::Tmr
		)
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_ENABLE_LIBCAMERA)
			target_compile_definitions(wse.tmr.camera_contract PRIVATE WSE_EXPECT_LIBCAMERA=1)
		endif()
		wse_add_characterization_test(
			wse.tmr.camera_backend_contract
			test/characterization/tmr_camera_backend_contract.cpp
			WSE::Tmr
		)
		wse_add_characterization_test(
			wse.tmr.camera_start_stop_contract
			test/characterization/tmr_camera_start_stop_contract.cpp
			WSE::Tmr
		)
		set_tests_properties(wse.tmr.camera_start_stop_contract PROPERTIES TIMEOUT 20)
		wse_add_characterization_test(
			wse.tmr.camera_owner_thread_contract
			test/characterization/tmr_camera_owner_thread_contract.cpp
			WSE::Tmr
		)
		set_tests_properties(wse.tmr.camera_owner_thread_contract PROPERTIES TIMEOUT 20)
		if(WIN32)
			wse_add_characterization_test(
				wse.tmr.media_foundation_read_state_contract
				test/characterization/tmr_media_foundation_read_state_contract.cpp
				WSE::Tmr
			)
			target_link_libraries(wse.tmr.media_foundation_read_state_contract PRIVATE mfplat mfuuid ole32)
			set_tests_properties(wse.tmr.media_foundation_read_state_contract PROPERTIES TIMEOUT 20)
			wse_add_characterization_test(
				wse.tmr.directshow_format_contract
				test/characterization/tmr_directshow_format_contract.cpp
				WSE::Tmr
			)
			target_link_libraries(wse.tmr.directshow_format_contract PRIVATE strmiids)
			wse_add_characterization_test(
				wse.tmr.media_foundation_error_contract
				test/characterization/tmr_media_foundation_error_contract.cpp
				WSE::Tmr
			)
			wse_add_characterization_test(
				wse.tmr.media_foundation_frame_contract
				test/characterization/tmr_media_foundation_frame_contract.cpp
				WSE::Tmr
			)
			wse_add_characterization_test(
				wse.tmr.media_foundation_resources_contract
				test/characterization/tmr_media_foundation_resources_contract.cpp
				WSE::Tmr
			)
			target_link_libraries(wse.tmr.media_foundation_resources_contract PRIVATE mfplat mfuuid ole32)
		endif()
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			wse_add_characterization_test(
				wse.tmr.libcamera_manager_contract
				test/characterization/tmr_libcamera_manager_contract.cpp
				WSE::Tmr
			)
			set_tests_properties(wse.tmr.libcamera_manager_contract PROPERTIES TIMEOUT 20)
			if(WSE_ENABLE_LIBCAMERA)
				target_compile_definitions(wse.tmr.libcamera_manager_contract PRIVATE WSE_TEST_NATIVE_LIBCAMERA=1)
				target_link_libraries(wse.tmr.libcamera_manager_contract PRIVATE WSE_LibcameraRuntime WSE_LibcameraBaseRuntime)
			endif()
			wse_add_characterization_test(
				wse.tmr.libcamera_resources_contract
				test/characterization/tmr_libcamera_resources_contract.cpp
				WSE::Tmr
			)
			wse_add_characterization_test(
				wse.tmr.v4l2_lifecycle_contract
				test/characterization/tmr_v4l2_lifecycle_contract.cpp
				WSE::Tmr
			)
			wse_add_characterization_test(
				wse.tmr.v4l2_read_contract
				test/characterization/tmr_v4l2_read_contract.cpp
				WSE::Tmr
			)
		endif()
		wse_add_characterization_test(
			wse.tmr.camera_frame_ops
			test/characterization/camera_frame_ops_contract.cpp
			WSE::Tmr
		)
		wse_add_characterization_test(
			wse.tmr.webcamera_contract
			test/characterization/tmr_webcamera_contract.cpp
			WSE::Tmr
			"${WSE_BINDING_WEBCAMERA_FIXTURE}"
		)
		wse_add_characterization_test(
			wse.tmr.calibration_contract
			test/characterization/tmr_calibration_contract.cpp
			WSE::Tmr
		)
		wse_add_characterization_test(
			wse.tmr.data_contract
			test/characterization/tmr_data_contract.cpp
			WSE::Tmr
		)
		if(WIN32 AND WSE_ENABLE_CAMERA_HARDWARE_TESTS)
			set(wse_camera_test_arguments)
			if(NOT WSE_CAMERA_TEST_NAME STREQUAL "")
				list(APPEND wse_camera_test_arguments --camera-name "${WSE_CAMERA_TEST_NAME}")
			endif()
			wse_add_characterization_test(
				wse.tmr.windows_camera_smoke
				test/characterization/tmr_windows_camera_smoke.cpp
				WSE::Tmr
				${wse_camera_test_arguments}
			)
			set_tests_properties(wse.tmr.windows_camera_smoke PROPERTIES
				TIMEOUT 120
				SKIP_RETURN_CODE 77)
		endif()
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_ENABLE_CAMERA_HARDWARE_TESTS)
			wse_add_characterization_test(
				wse.tmr.webcamera_hardware_smoke
				test/hardware/tmr_webcamera_hardware_smoke.cpp
				WSE::Tmr
			)
			set_tests_properties(wse.tmr.webcamera_hardware_smoke PROPERTIES
				TIMEOUT 60
				SKIP_RETURN_CODE 77
				LABELS "hardware;camera")
		endif()
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND WSE_STANDALONE_BUILD
			AND WSE_PACKAGE_BUNDLES_LIBCAMERA AND NOT CMAKE_CROSSCOMPILING
			AND Python3_Interpreter_FOUND)
		set(wse_installed_binding_arguments)
		if(WSE_BUILD_PYTHON_BINDING)
			list(APPEND wse_installed_binding_arguments --python "${WSE_PYTHON_EXECUTABLE}")
		endif()
		if(WSE_BUILD_NODE_BINDING)
			list(APPEND wse_installed_binding_arguments --node "${WSE_NODE_EXECUTABLE}")
		endif()
		foreach(wse_installed_binding IN ITEMS WSE_Java WSE_Dotnet)
			if(TARGET ${wse_installed_binding})
				list(APPEND wse_installed_binding_arguments --library "$<TARGET_FILE_NAME:${wse_installed_binding}>")
			endif()
		endforeach()
		if(wse_installed_binding_arguments)
			add_test(NAME wse.binding.installed_libcamera
				COMMAND "${Python3_EXECUTABLE}"
					"${CMAKE_CURRENT_SOURCE_DIR}/test/package/installed_libcamera_runtime_contract.py"
					--cmake "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}"
					--config "$<CONFIG>" ${wse_installed_binding_arguments})
			set_tests_properties(wse.binding.installed_libcamera PROPERTIES TIMEOUT 300)
		endif()
	endif()

	if(WIN32
			AND WSE_LIBRARY_TYPE STREQUAL "SHARED"
			AND NOT WSE_BUILD_XPT
			AND NOT WSE_BUILD_GEF
			AND NOT WSE_BUILD_OUI
			AND NOT WSE_BUILD_IUI
			AND NOT WSE_BUILD_TMR
			AND NOT WSE_BUILD_VPJ)
		add_test(
			NAME wse.package.disabled_exports
			COMMAND "${CMAKE_COMMAND}"
				"-DWSE_LINKER=${CMAKE_LINKER}"
				"-DWSE_LIBRARY=$<TARGET_FILE:WonderStewEngine>"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/verify_disabled_exports.cmake"
		)
	endif()

	# --------------------------------------------------------------------------------------------
	# Stress: failure injection and lifecycle soak.
	# --------------------------------------------------------------------------------------------
	# These repeat a lifecycle or inject a fault thousands of times; the TIMEOUT is the deadlock
	# and runaway detector, so each carries a generous one. Device-bound stress (camera unplug,
	# real display loss) belongs to the hardware phase and is absent here.
	wse_add_characterization_test(
		wse.stress.core_lifecycle
		test/stress/core_lifecycle_soak.cpp
		WSE::Core
	)
	set_tests_properties(wse.stress.core_lifecycle PROPERTIES LABELS "stress;soak" TIMEOUT 120)
	if(WSE_BUILD_XPT)
		wse_add_characterization_test(
			wse.stress.xpt_failure_injection
			test/stress/xpt_failure_injection.cpp
			WSE::Xpt
		)
		if(WIN32)
			target_link_libraries(wse.stress.xpt_failure_injection PRIVATE ws2_32)
		endif()
		set_tests_properties(wse.stress.xpt_failure_injection PROPERTIES
			LABELS "stress;failure-injection" TIMEOUT 120)
	endif()
	# The renderer soak needs D3D12/WARP, which the Windows runners provide; Linux OUI stress waits
	# for its offscreen Vulkan gate in the hardware phase.
	if(WSE_BUILD_OUI AND WIN32)
		wse_add_characterization_test(
			wse.stress.oui_renderer
			test/stress/oui_renderer_soak.cpp
			WSE::Oui
		)
		set_tests_properties(wse.stress.oui_renderer PROPERTIES LABELS "stress;soak" TIMEOUT 180)
	endif()

	# --------------------------------------------------------------------------------------------
	# Benchmarks (opt-in): measurement, not verification.
	# --------------------------------------------------------------------------------------------
	# Each wse.bench.* executable prints WSE_BENCH TSV lines; a run's output is the comparison
	# artifact for its commit, and comparisons hold within one runner, never across machines.
	# Hardware-bound measurements (capture FPS, control latency, serial loopback) belong to the
	# hardware phase and are deliberately absent here.
	if(WSE_BUILD_BENCHMARKS)
		wse_add_characterization_test(wse.bench.indexing test/benchmark/indexing_benchmark.cpp WSE::Core)
		set_tests_properties(wse.bench.indexing PROPERTIES LABELS "benchmark" TIMEOUT 60)
		wse_add_characterization_test(
			wse.bench.core
			test/benchmark/core_benchmark.cpp
			WSE::Core
		)
		set_tests_properties(wse.bench.core PROPERTIES LABELS "benchmark" TIMEOUT 300)
		if(WSE_BUILD_XPT)
			wse_add_characterization_test(
				wse.bench.xpt
				test/benchmark/xpt_benchmark.cpp
				WSE::Xpt
			)
			if(WIN32)
				target_link_libraries(wse.bench.xpt PRIVATE ws2_32)
			endif()
			set_tests_properties(wse.bench.xpt PROPERTIES LABELS "benchmark" TIMEOUT 300)
		endif()
		if(WSE_BUILD_TMR)
			wse_add_characterization_test(
				wse.bench.tmr
				test/benchmark/tmr_benchmark.cpp
				WSE::Tmr
			)
			set_tests_properties(wse.bench.tmr PROPERTIES LABELS "benchmark" TIMEOUT 300)
		endif()
	endif()


endif()

