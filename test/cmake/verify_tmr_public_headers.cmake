# @file verify_tmr_public_headers.cmake
# @brief Tmr public headersにOS固有Camera APIが露出していないことを検証する。

if(NOT DEFINED WSE_TMR_API_DIR OR NOT IS_DIRECTORY "${WSE_TMR_API_DIR}")
	message(FATAL_ERROR "WSE_TMR_API_DIR must name the Tmr public API directory.")
endif()

file(GLOB_RECURSE WSE_TMR_PUBLIC_HEADERS LIST_DIRECTORIES FALSE
	"${WSE_TMR_API_DIR}/*.h")

set(WSE_TMR_FORBIDDEN_INCLUDE
	"#[ \t]*include[ \t]*[<\"][^>\"]*(windows\\.h|dshow\\.h|mfapi\\.h|mfidl\\.h|videodev2\\.h|libcamera/)")
set(WSE_TMR_FORBIDDEN_TYPE
	"(^|[^A-Za-z0-9_])(IAMCameraControl|IAMVideoProcAmp|IMF[A-Za-z0-9_]*|HRESULT|GUID|HANDLE|v4l2_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)")

foreach(WSE_TMR_HEADER IN LISTS WSE_TMR_PUBLIC_HEADERS)
	file(READ "${WSE_TMR_HEADER}" WSE_TMR_HEADER_CONTENT)
	if(WSE_TMR_HEADER_CONTENT MATCHES "${WSE_TMR_FORBIDDEN_INCLUDE}"
			OR WSE_TMR_HEADER_CONTENT MATCHES "${WSE_TMR_FORBIDDEN_TYPE}")
		message(FATAL_ERROR
			"Platform camera API leaked into Tmr public header: ${WSE_TMR_HEADER}")
	endif()
endforeach()

message(STATUS "Tmr public camera headers are backend-independent")
