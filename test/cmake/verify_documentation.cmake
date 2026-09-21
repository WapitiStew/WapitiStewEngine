# @file verify_documentation.cmake
# @brief Verifies WSE documentation entry points, paired metadata, local links, commands, and samples.

if(NOT DEFINED WSE_SOURCE_ROOT OR NOT IS_DIRECTORY "${WSE_SOURCE_ROOT}")
	message(FATAL_ERROR "WSE_SOURCE_ROOT must name the WSE source root.")
endif()
if(NOT DEFINED WSE_DOCUMENTATION_MANIFEST OR NOT EXISTS "${WSE_DOCUMENTATION_MANIFEST}")
	message(FATAL_ERROR "WSE_DOCUMENTATION_MANIFEST must name the paired-document manifest.")
endif()

file(STRINGS "${WSE_DOCUMENTATION_MANIFEST}" wse_manifest_lines ENCODING UTF-8)
set(wse_document_ids)
set(wse_document_pair_count 0)
set(wse_markdown_files)
foreach(wse_manifest_line IN LISTS wse_manifest_lines)
	if(wse_manifest_line STREQUAL "" OR wse_manifest_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_fields "${wse_manifest_line}")
	list(LENGTH wse_fields wse_field_count)
	if(NOT wse_field_count EQUAL 5)
		message(FATAL_ERROR "Invalid documentation manifest row: ${wse_manifest_line}")
	endif()
	list(GET wse_fields 0 wse_id)
	list(GET wse_fields 1 wse_english_path)
	list(GET wse_fields 2 wse_japanese_path)
	list(GET wse_fields 3 wse_documentation_version)
	list(GET wse_fields 4 wse_last_synchronized)
	list(FIND wse_document_ids "${wse_id}" wse_existing_id)
	if(NOT wse_existing_id EQUAL -1)
		message(FATAL_ERROR "Duplicate documentation manifest id: ${wse_id}")
	endif()
	list(APPEND wse_document_ids "${wse_id}")
	if(NOT wse_documentation_version STREQUAL "WSE 1.0.0"
			OR NOT wse_last_synchronized MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
		message(FATAL_ERROR "Invalid version/synchronization metadata for ${wse_id}")
	endif()
	foreach(wse_document_path IN ITEMS "${wse_english_path}" "${wse_japanese_path}")
		if(NOT EXISTS "${WSE_SOURCE_ROOT}/${wse_document_path}")
			message(FATAL_ERROR "Manifest document is missing: ${wse_document_path}")
		endif()
		list(APPEND wse_markdown_files "${WSE_SOURCE_ROOT}/${wse_document_path}")
	endforeach()
	file(READ "${WSE_SOURCE_ROOT}/${wse_english_path}" wse_english_text)
	file(READ "${WSE_SOURCE_ROOT}/${wse_japanese_path}" wse_japanese_text)
	# Check synchronized metadata here; changes to normative documents also require the
	# change-history update described in AGENTS.md.
	foreach(wse_required_text IN ITEMS
			"> Canonical language: English"
			"> Documentation version: ${wse_documentation_version}"
			"> Last synchronized: ${wse_last_synchronized}")
		string(FIND "${wse_english_text}" "${wse_required_text}" wse_required_index)
		if(wse_required_index EQUAL -1)
			message(FATAL_ERROR "English metadata is missing from ${wse_english_path}: ${wse_required_text}")
		endif()
	endforeach()
	foreach(wse_required_text IN ITEMS
			"> Canonical source: ["
			"> Documentation version: ${wse_documentation_version}"
			"> Last synchronized: ${wse_last_synchronized}")
		string(FIND "${wse_japanese_text}" "${wse_required_text}" wse_required_index)
		if(wse_required_index EQUAL -1)
			message(FATAL_ERROR "Japanese metadata is missing from ${wse_japanese_path}: ${wse_required_text}")
		endif()
	endforeach()
	# An existing Markdown link is not enough: the translation must name this pair's canonical file.
	string(REGEX MATCH "> Canonical source: \\[[^]\r\n]+\\]\\(([^)\r\n]+)\\)"
		wse_canonical_link "${wse_japanese_text}")
	if(NOT wse_canonical_link)
		message(FATAL_ERROR "Invalid canonical source link in ${wse_japanese_path}")
	endif()
	get_filename_component(wse_translation_directory "${WSE_SOURCE_ROOT}/${wse_japanese_path}" DIRECTORY)
	get_filename_component(wse_canonical_path "${CMAKE_MATCH_1}" ABSOLUTE BASE_DIR "${wse_translation_directory}")
	get_filename_component(wse_expected_canonical_path "${WSE_SOURCE_ROOT}/${wse_english_path}" ABSOLUTE)
	if(NOT wse_canonical_path STREQUAL wse_expected_canonical_path)
		message(FATAL_ERROR "Canonical source does not match manifest for ${wse_japanese_path}: ${wse_canonical_link}")
	endif()
	math(EXPR wse_document_pair_count "${wse_document_pair_count} + 1")
endforeach()

foreach(wse_required_id IN ITEMS
		root-readme overview getting-started setup build-guide how-to api-reference architecture
		thread-ownership build-packaging security-privacy hardware-validation version-compatibility
		core-data-model core-algorithms core-services gef-file-formats design-verification c-abi developer-walkthrough)
	list(FIND wse_document_ids "${wse_required_id}" wse_required_id_index)
	if(wse_required_id_index EQUAL -1)
		message(FATAL_ERROR "Required documentation role is not registered: ${wse_required_id}")
	endif()
endforeach()

# Include legacy navigation pages and other unpaired guides so they cannot silently evade link checks.
file(GLOB_RECURSE wse_guide_files
	"${WSE_SOURCE_ROOT}/doc/en/*.md" "${WSE_SOURCE_ROOT}/doc/ja/*.md"
	"${WSE_SOURCE_ROOT}/doc/design/en/*.md" "${WSE_SOURCE_ROOT}/doc/design/ja/*.md")
list(APPEND wse_markdown_files ${wse_guide_files}
	"${WSE_SOURCE_ROOT}/AGENTS.md"
	"${WSE_SOURCE_ROOT}/doc/README.md"
	"${WSE_SOURCE_ROOT}/doxy/README.md")
list(REMOVE_DUPLICATES wse_markdown_files)
set(wse_markdown_count 0)
set(wse_local_link_count 0)
foreach(wse_markdown_file IN LISTS wse_markdown_files)
	math(EXPR wse_markdown_count "${wse_markdown_count} + 1")
	file(READ "${wse_markdown_file}" wse_markdown_text)
	string(REGEX MATCHALL "\\[[^]&=\r\n][^]\r\n]*\\]\\([^)\r\n]+\\)"
		wse_markdown_links "${wse_markdown_text}")
	get_filename_component(wse_markdown_directory "${wse_markdown_file}" DIRECTORY)
	foreach(wse_markdown_link IN LISTS wse_markdown_links)
		string(REGEX REPLACE "^.*\\]\\(([^)]+)\\)$" "\\1" wse_link_target "${wse_markdown_link}")
		string(REGEX REPLACE "[ \t]+\"[^\"]*\"$" "" wse_link_target "${wse_link_target}")
		if(wse_link_target MATCHES "^<.*>$")
			string(REGEX REPLACE "^<(.*)>$" "\\1" wse_link_target "${wse_link_target}")
		endif()
		if(wse_link_target MATCHES "^[A-Za-z][A-Za-z0-9+.-]*:" OR wse_link_target MATCHES "^#")
			continue()
		endif()
		string(REGEX REPLACE "#.*$" "" wse_link_path "${wse_link_target}")
		if(wse_link_path STREQUAL "")
			continue()
		endif()
		get_filename_component(wse_resolved_link "${wse_link_path}" ABSOLUTE
			BASE_DIR "${wse_markdown_directory}")
		if(NOT EXISTS "${wse_resolved_link}")
			file(RELATIVE_PATH wse_source_document "${WSE_SOURCE_ROOT}" "${wse_markdown_file}")
			message(FATAL_ERROR "Broken local link in ${wse_source_document}: ${wse_link_target}")
		endif()
		math(EXPR wse_local_link_count "${wse_local_link_count} + 1")
	endforeach()
endforeach()

file(READ "${WSE_SOURCE_ROOT}/CMakePresets.json" wse_presets_text)
foreach(wse_public_preset IN ITEMS
		windows-msvc-shared-core windows-msvc-static-core
		windows-msvc-shared-public windows-msvc-static-public
		linux-gcc-shared-core linux-gcc-static-core
		linux-gcc-shared-gef linux-gcc-static-gef
		linux-gcc-shared-iui linux-gcc-static-iui
		linux-arm64-gcc-shared-iui linux-arm64-gcc-static-iui)
	string(FIND "${wse_presets_text}" "\"name\": \"${wse_public_preset}\"" wse_preset_index)
	if(wse_preset_index EQUAL -1)
		message(FATAL_ERROR "Documented CMake preset is missing: ${wse_public_preset}")
	endif()
endforeach()

# The build definition spans the root file and its responsibility modules under cmake/.
file(READ "${WSE_SOURCE_ROOT}/CMakeLists.txt" wse_cmake_text)
file(GLOB wse_cmake_modules "${WSE_SOURCE_ROOT}/cmake/Wse*.cmake")
foreach(wse_cmake_module IN LISTS wse_cmake_modules)
	file(READ "${wse_cmake_module}" wse_cmake_module_text)
	string(APPEND wse_cmake_text "${wse_cmake_module_text}")
endforeach()
foreach(wse_public_option IN ITEMS
		WSE_BUILD_XPT WSE_BUILD_GEF WSE_BUILD_IUI WSE_BUILD_OUI WSE_BUILD_TMR
		WSE_BUILD_NODE_BINDING WSE_BUILD_PYTHON_BINDING WSE_BUILD_JAVA_BINDING
		WSE_BUILD_DOTNET_BINDING
		WSE_LIBRARY_TYPE WSE_BUILD_TESTING)
	string(FIND "${wse_cmake_text}" "${wse_public_option}" wse_option_index)
	if(wse_option_index EQUAL -1)
		message(FATAL_ERROR "Documented CMake option is missing: ${wse_public_option}")
	endif()
endforeach()

foreach(wse_sample IN ITEMS
		example/cpp/core/quickstart.cpp example/python/core/quickstart.py
		example/js/core/quickstart.js example/java/core/QuickStart.java
		example/cs/core/QuickStart.cs)
	if(NOT EXISTS "${WSE_SOURCE_ROOT}/${wse_sample}")
		message(FATAL_ERROR "Required documentation sample is missing: ${wse_sample}")
	endif()
endforeach()

message(STATUS
	"Documentation contract passed: ${wse_document_pair_count} synchronized pairs, "
	"${wse_markdown_count} Markdown files, ${wse_local_link_count} local links, and five samples")
