# @file verify_oui_resource_ownership.cmake
# @brief Prevents manual COM reference-count operations in Windows OUI implementation code.

if(NOT DEFINED WSE_OUI_WINDOWS_DIR OR NOT IS_DIRECTORY "${WSE_OUI_WINDOWS_DIR}")
	message(FATAL_ERROR "WSE_OUI_WINDOWS_DIR must point to platform/oui/win.")
endif()

file(GLOB_RECURSE wse_oui_windows_sources
	"${WSE_OUI_WINDOWS_DIR}/*.cpp"
	"${WSE_OUI_WINDOWS_DIR}/*.h")

foreach(wse_oui_windows_source IN LISTS wse_oui_windows_sources)
	file(READ "${wse_oui_windows_source}" wse_oui_windows_text)
	if(wse_oui_windows_text MATCHES "(->|\\.)[ \t\r\n]*Release[ \t\r\n]*\\(")
		message(FATAL_ERROR
			"Manual COM Release() is forbidden; use ComPtr/RAII: ${wse_oui_windows_source}")
	endif()
	if(wse_oui_windows_text MATCHES "\\.[ \t\r\n]*Detach[ \t\r\n]*\\(")
		message(FATAL_ERROR
			"ComPtr::Detach() is forbidden without an explicit ownership-transfer adapter: "
			"${wse_oui_windows_source}")
	endif()
endforeach()
