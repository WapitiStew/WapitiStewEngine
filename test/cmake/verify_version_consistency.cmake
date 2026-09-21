# @file verify_version_consistency.cmake
# @brief Verifies that every version spelling in the tree matches the single CMake source of truth.
# @details The authoritative version is the one set(WSE_VERSION) in the root CMakeLists.txt and
#          passed in here. The generated artifacts (package manifest, package config, binding
#          runtime version) already derive from it at configure time; this gate covers the
#          spellings a generator cannot reach - the README banners, every documentation banner
#          recorded in the manifest, and the binding package metadata files.

if(NOT DEFINED WSE_VERSION OR WSE_VERSION STREQUAL "")
	message(FATAL_ERROR "WSE_VERSION must carry the authoritative version.")
endif()
if(NOT DEFINED WSE_SOURCE_ROOT OR NOT IS_DIRECTORY "${WSE_SOURCE_ROOT}")
	message(FATAL_ERROR "WSE_SOURCE_ROOT must name the WSE source directory.")
endif()

set(wse_version_failures)

# The README banners state the documentation version a reader starts from.
foreach(wse_readme IN ITEMS "README.md" "README.ja.md")
	file(READ "${WSE_SOURCE_ROOT}/${wse_readme}" wse_readme_text)
	if(NOT wse_readme_text MATCHES "> Documentation version: WSE ${WSE_VERSION}\n")
		list(APPEND wse_version_failures
			"${wse_readme} does not carry '> Documentation version: WSE ${WSE_VERSION}'")
	endif()
endforeach()

# Every synchronized documentation pair records its version in the manifest, and the
# documentation gate already matches each file's banner against its manifest row; checking the
# manifest rows here therefore transitively pins every documentation banner.
file(STRINGS "${WSE_SOURCE_ROOT}/doc/DocumentationManifest.tsv" wse_manifest_lines ENCODING UTF-8)
foreach(wse_manifest_line IN LISTS wse_manifest_lines)
	if(wse_manifest_line STREQUAL "" OR wse_manifest_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_fields "${wse_manifest_line}")
	list(LENGTH wse_fields wse_field_count)
	if(wse_field_count LESS 4)
		continue()
	endif()
	list(GET wse_fields 0 wse_document_id)
	list(GET wse_fields 3 wse_document_version)
	if(NOT wse_document_version STREQUAL "WSE ${WSE_VERSION}")
		list(APPEND wse_version_failures
			"Documentation manifest row '${wse_document_id}' records '${wse_document_version}'")
	endif()
endforeach()

# The binding package metadata ships to language package managers with its own version spelling.
file(READ "${WSE_SOURCE_ROOT}/lang/js/package.json" wse_js_package_text)
if(NOT wse_js_package_text MATCHES "\"version\": \"${WSE_VERSION}\"")
	list(APPEND wse_version_failures
		"lang/js/package.json does not record version ${WSE_VERSION}")
endif()
file(READ "${WSE_SOURCE_ROOT}/lang/cs/Wse/Wse.csproj" wse_csproj_text)
if(NOT wse_csproj_text MATCHES "<Version>${WSE_VERSION}</Version>")
	list(APPEND wse_version_failures
		"lang/cs/Wse/Wse.csproj does not record <Version>${WSE_VERSION}</Version>")
endif()

if(wse_version_failures)
	list(JOIN wse_version_failures "\n  " wse_version_failure_text)
	message(FATAL_ERROR "Version consistency failed for WSE ${WSE_VERSION}:\n  ${wse_version_failure_text}")
endif()

message(STATUS "Version consistency passed: every spelling matches WSE ${WSE_VERSION}")
