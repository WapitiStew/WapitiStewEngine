# @file verify_public_api_policy.cmake
# @brief Enforces the Phase 7-B5 public API, logging, and binding-error policy.

foreach(wse_required_directory IN ITEMS WSE_API_ROOT WSE_CORE_ROOT WSE_PLATFORM_ROOT WSE_LANG_ROOT)
	if(NOT DEFINED ${wse_required_directory}
			OR NOT IS_DIRECTORY "${${wse_required_directory}}")
		message(FATAL_ERROR "${wse_required_directory} must name an existing directory.")
	endif()
endforeach()
if(NOT DEFINED WSE_THIRD_PARTY_BASELINE OR NOT EXISTS "${WSE_THIRD_PARTY_BASELINE}")
	message(FATAL_ERROR "WSE_THIRD_PARTY_BASELINE must name the reviewed compatibility baseline.")
endif()
if(NOT DEFINED WSE_LEGACY_ERROR_BASELINE OR NOT EXISTS "${WSE_LEGACY_ERROR_BASELINE}")
	message(FATAL_ERROR "WSE_LEGACY_ERROR_BASELINE must name the reviewed legacy-error baseline.")
endif()

file(STRINGS "${WSE_THIRD_PARTY_BASELINE}" wse_third_party_baseline_lines)
set(wse_third_party_baseline_paths)
foreach(wse_baseline_line IN LISTS wse_third_party_baseline_lines)
	if(wse_baseline_line STREQUAL "" OR wse_baseline_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_baseline_fields "${wse_baseline_line}")
	list(LENGTH wse_baseline_fields wse_baseline_field_count)
	if(NOT wse_baseline_field_count EQUAL 2)
		message(FATAL_ERROR "Invalid third-party baseline entry: ${wse_baseline_line}")
	endif()
	list(GET wse_baseline_fields 0 wse_baseline_path)
	list(GET wse_baseline_fields 1 wse_baseline_count)
	list(APPEND wse_third_party_baseline_paths "${wse_baseline_path}")
	string(MAKE_C_IDENTIFIER "${wse_baseline_path}" wse_baseline_key)
	set("wse_third_party_expected_${wse_baseline_key}" "${wse_baseline_count}")
endforeach()

# The legacy Core error surface (the ErrorCode enum, wseException_, and the wseThrowException
# macros) is compatibility debt. New public APIs report failures through standard exceptions or
# the Result contract; the baseline holds the reviewed occurrences and may only shrink.
set(wse_legacy_error_pattern
	"(^|[^A-Za-z0-9_])(ErrorCode[ \t]*::|wseException|wseThrowException)")
file(STRINGS "${WSE_LEGACY_ERROR_BASELINE}" wse_legacy_error_baseline_lines)
set(wse_legacy_error_baseline_paths)
foreach(wse_baseline_line IN LISTS wse_legacy_error_baseline_lines)
	if(wse_baseline_line STREQUAL "" OR wse_baseline_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_baseline_fields "${wse_baseline_line}")
	list(LENGTH wse_baseline_fields wse_baseline_field_count)
	if(NOT wse_baseline_field_count EQUAL 3)
		message(FATAL_ERROR "Invalid legacy-error baseline entry: ${wse_baseline_line}")
	endif()
	list(GET wse_baseline_fields 0 wse_baseline_path)
	list(GET wse_baseline_fields 1 wse_baseline_count)
	list(APPEND wse_legacy_error_baseline_paths "${wse_baseline_path}")
	string(MAKE_C_IDENTIFIER "${wse_baseline_path}" wse_baseline_key)
	set("wse_legacy_error_expected_${wse_baseline_key}" "${wse_baseline_count}")
endforeach()

file(GLOB_RECURSE wse_api_headers LIST_DIRECTORIES FALSE
	"${WSE_API_ROOT}/*.h"
	"${WSE_API_ROOT}/*.hpp")
set(wse_third_party_seen_paths)
set(wse_legacy_error_seen_paths)
foreach(wse_api_header IN LISTS wse_api_headers)
	file(RELATIVE_PATH wse_api_relative "${WSE_API_ROOT}" "${wse_api_header}")
	string(REPLACE "\\" "/" wse_api_relative "${wse_api_relative}")
	if(wse_api_relative MATCHES "^vpj/")
		continue()
	endif()

	file(READ "${wse_api_header}" wse_api_text)
	if(wse_api_text MATCHES "using[ \t]+namespace[ \t]+"
			OR wse_api_text MATCHES "namespace[ \t]+(std|Microsoft|cv|boost)[ \t\r\n]*\\{")
		message(FATAL_ERROR "Namespace pollution in public header: ${wse_api_relative}")
	endif()
	if(wse_api_text MATCHES "#[ \t]*pragma[ \t]+comment[ \t]*\\([ \t]*lib")
		message(FATAL_ERROR "Linker directives are forbidden in public headers: ${wse_api_relative}")
	endif()
	if(wse_api_text MATCHES
			"#[ \t]*include[ \t]*[<\"][^>\"]*(core/|platform/|vendor/|\\.\\./\\.\\./core|\\.\\./\\.\\./platform)")
		message(FATAL_ERROR "Internal header path leaked into public API: ${wse_api_relative}")
	endif()

	# api/cv/ is the sanctioned OpenCV adapter component: the single opt-in place a public
	# header may name OpenCV (LegacyRemovalInventory.tsv: core-opencv-coupling). Every other
	# public header stays third-party free, so the scan skips only this directory.
	string(REGEX MATCHALL
		"opencv2/|(^|[^A-Za-z0-9_])cv::|curl/|vulkan/|libcamera/|cpprest/|boost/|napi\.h|jni\.h|Python\.h"
		wse_third_party_matches "${wse_api_text}")
	list(LENGTH wse_third_party_matches wse_third_party_count)
	if(wse_api_relative MATCHES "^cv/")
		set(wse_third_party_count 0)
	endif()
	if(wse_third_party_count GREATER 0)
		list(FIND wse_third_party_baseline_paths "${wse_api_relative}" wse_baseline_index)
		if(wse_baseline_index EQUAL -1)
			message(FATAL_ERROR
				"Unclassified third-party API/type added to public header: ${wse_api_relative}")
		endif()
		string(MAKE_C_IDENTIFIER "${wse_api_relative}" wse_baseline_key)
		set(wse_expected_count "${wse_third_party_expected_${wse_baseline_key}}")
		if(NOT wse_third_party_count EQUAL wse_expected_count)
			message(FATAL_ERROR
				"Third-party occurrence count changed for ${wse_api_relative}: expected "
				"${wse_expected_count}, found ${wse_third_party_count}.")
		endif()
		list(APPEND wse_third_party_seen_paths "${wse_api_relative}")
	endif()

	string(REGEX MATCHALL "${wse_legacy_error_pattern}" wse_legacy_error_matches "${wse_api_text}")
	list(LENGTH wse_legacy_error_matches wse_legacy_error_count)
	if(wse_legacy_error_count GREATER 0)
		list(FIND wse_legacy_error_baseline_paths "${wse_api_relative}" wse_baseline_index)
		if(wse_baseline_index EQUAL -1)
			message(FATAL_ERROR
				"Legacy ErrorCode/wseException surface added to public header: ${wse_api_relative}. "
				"Report failures through standard exceptions or the Result contract instead.")
		endif()
		string(MAKE_C_IDENTIFIER "${wse_api_relative}" wse_baseline_key)
		set(wse_expected_count "${wse_legacy_error_expected_${wse_baseline_key}}")
		if(NOT wse_legacy_error_count EQUAL wse_expected_count)
			message(FATAL_ERROR
				"Legacy error-surface count changed for ${wse_api_relative}: expected "
				"${wse_expected_count}, found ${wse_legacy_error_count}. "
				"Reduce the debt and update its reviewed baseline together; do not increase it.")
		endif()
		list(APPEND wse_legacy_error_seen_paths "${wse_api_relative}")
	endif()
endforeach()

foreach(wse_baseline_path IN LISTS wse_third_party_baseline_paths)
	list(FIND wse_third_party_seen_paths "${wse_baseline_path}" wse_seen_index)
	if(wse_seen_index EQUAL -1)
		message(FATAL_ERROR
			"Stale third-party baseline entry: ${wse_baseline_path}; remove reviewed debt and baseline together.")
	endif()
endforeach()

foreach(wse_baseline_path IN LISTS wse_legacy_error_baseline_paths)
	list(FIND wse_legacy_error_seen_paths "${wse_baseline_path}" wse_seen_index)
	if(wse_seen_index EQUAL -1)
		message(FATAL_ERROR
			"Stale legacy-error baseline entry: ${wse_baseline_path}; remove reviewed debt and baseline together.")
	endif()
endforeach()

foreach(wse_error_header IN ITEMS
		"${WSE_API_ROOT}/wse/binding/Error.h"
		"${WSE_API_ROOT}/xpt/error/TransportError.h"
		"${WSE_API_ROOT}/oui/renderer/RendererError.h"
		"${WSE_API_ROOT}/tmr/camera/CameraError.h")
	file(READ "${wse_error_header}" wse_error_text)
	if(NOT wse_error_text MATCHES "nativeCode[ \t]*\\([ \t]*\\)[ \t]*const[ \t]+noexcept")
		message(FATAL_ERROR "Canonical const nativeCode() accessor is missing: ${wse_error_header}")
	endif()
endforeach()

file(GLOB_RECURSE wse_implementation_sources LIST_DIRECTORIES FALSE
	"${WSE_CORE_ROOT}/*.cpp" "${WSE_CORE_ROOT}/*.h"
	"${WSE_PLATFORM_ROOT}/*.cpp" "${WSE_PLATFORM_ROOT}/*.h")
foreach(wse_implementation_source IN LISTS wse_implementation_sources)
	string(REPLACE "\\" "/" wse_source_normalized "${wse_implementation_source}")
	if(wse_source_normalized MATCHES "/vpj/"
			OR wse_source_normalized MATCHES "/tmr/(spec|device/params|device/stream)/"
			OR wse_source_normalized MATCHES "/tmr/win/(extern|device/stream)/"
			OR wse_source_normalized MATCHES "/wse/utility/wse_Log\\.cpp$"
			OR wse_source_normalized MATCHES "/wse/(win|linux)/utility/wse_LogOutput\\.(cpp|h)$")
		continue()
	endif()
	file(READ "${wse_implementation_source}" wse_implementation_text)
	if(wse_implementation_text MATCHES
			"std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf)[ \t]*\\(|OutputDebugString")
		message(FATAL_ERROR
			"Direct diagnostic output is forbidden outside the logging sink: ${wse_implementation_source}")
	endif()
endforeach()

file(GLOB_RECURSE wse_binding_sources LIST_DIRECTORIES FALSE
	"${WSE_LANG_ROOT}/*.cpp" "${WSE_LANG_ROOT}/*.h")
foreach(wse_binding_source IN LISTS wse_binding_sources)
	file(READ "${wse_binding_source}" wse_binding_text)
	if(wse_binding_text MATCHES "using[ \t]+(RendererCategory|CameraCategory)[ \t]*=")
		message(FATAL_ERROR
			"Language bindings must use the canonical OUI/Tmr error adapters: ${wse_binding_source}")
	endif()
endforeach()

message(STATUS
	"Public API naming, namespace, header, logging, error, and third-party debt policies passed")
