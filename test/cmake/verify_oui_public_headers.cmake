# @file verify_oui_public_headers.cmake
# @brief OUI public headers do not expose Windows or D3D implementation headers.

if(NOT DEFINED WSE_OUI_API_DIR OR NOT IS_DIRECTORY "${WSE_OUI_API_DIR}")
	message(FATAL_ERROR "WSE_OUI_API_DIR must point to the OUI public API directory.")
endif()

file(GLOB_RECURSE wse_oui_public_headers
	"${WSE_OUI_API_DIR}/*.h"
	"${WSE_OUI_API_DIR}/*.hpp")

set(wse_oui_forbidden_patterns
	"platform/oui/win"
	"windows.h"
	"d3d12.h"
	"dxgi"
	"wrl/client.h"
)

foreach(wse_oui_public_header IN LISTS wse_oui_public_headers)
	file(READ "${wse_oui_public_header}" wse_oui_public_header_text)
	string(TOLOWER "${wse_oui_public_header_text}" wse_oui_public_header_lower)
	string(REPLACE "\\" "/" wse_oui_public_header_lower "${wse_oui_public_header_lower}")

	foreach(wse_oui_forbidden_pattern IN LISTS wse_oui_forbidden_patterns)
		string(FIND
			"${wse_oui_public_header_lower}"
			"${wse_oui_forbidden_pattern}"
			wse_oui_forbidden_position)
		if(NOT wse_oui_forbidden_position EQUAL -1)
			message(FATAL_ERROR
				"OUI public header exposes platform implementation '${wse_oui_forbidden_pattern}': "
				"${wse_oui_public_header}")
		endif()
	endforeach()
endforeach()
