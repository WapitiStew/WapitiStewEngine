# @file verify_tmr_resource_ownership.cmake
# @brief Ratchets RAII ownership in the public Tmr camera implementation.

foreach(wse_required_variable IN ITEMS
		WSE_TMR_API_DIR WSE_TMR_CORE_DIR WSE_TMR_WINDOWS_CAMERA_DIR)
	if(NOT DEFINED ${wse_required_variable}
			OR NOT IS_DIRECTORY "${${wse_required_variable}}")
		message(FATAL_ERROR "${wse_required_variable} must name an existing directory.")
	endif()
endforeach()

file(GLOB_RECURSE wse_tmr_public_headers LIST_DIRECTORIES FALSE
	"${WSE_TMR_API_DIR}/*.h"
	"${WSE_TMR_API_DIR}/*.hpp")
foreach(wse_tmr_header IN LISTS wse_tmr_public_headers)
	file(READ "${wse_tmr_header}" wse_tmr_header_text)
	if(wse_tmr_header_text MATCHES "Impl[ \t\r\n]*\\*[ \t\r\n]+m_impl")
		message(FATAL_ERROR
			"Tmr PIMPL ownership must use std::unique_ptr: ${wse_tmr_header}")
	endif()
endforeach()

set(wse_tmr_owner_sources
	"${WSE_TMR_CORE_DIR}/camera/Camera.cpp"
	"${WSE_TMR_CORE_DIR}/device/WebCamera.cpp")
foreach(wse_tmr_owner_source IN LISTS wse_tmr_owner_sources)
	file(READ "${wse_tmr_owner_source}" wse_tmr_owner_text)
	if(wse_tmr_owner_text MATCHES "delete[ \t\r\n]+(this->)?m_impl"
			OR wse_tmr_owner_text MATCHES "m_impl[ \t\r\n]*=[ \t\r\n]*new")
		message(FATAL_ERROR
			"Manual Tmr PIMPL ownership is forbidden: ${wse_tmr_owner_source}")
	endif()
endforeach()

# One detach is deliberately retained only for destruction from the callback itself.
# Normal stop/start/close always performs a deferred owner-side join.
file(READ "${WSE_TMR_CORE_DIR}/camera/Camera.cpp" wse_tmr_camera_text)
string(REGEX MATCHALL "\\.[ \t\r\n]*detach[ \t\r\n]*\\(" wse_tmr_detach_matches
	"${wse_tmr_camera_text}")
list(LENGTH wse_tmr_detach_matches wse_tmr_detach_count)
if(NOT wse_tmr_detach_count EQUAL 1
		OR NOT wse_tmr_camera_text MATCHES
			"Destruction from the callback is discouraged, but remains memory-safe")
	message(FATAL_ERROR
		"CameraSession permits exactly one documented self-destruction detach fallback; "
		"normal lifecycle code must join the worker.")
endif()

file(GLOB_RECURSE wse_tmr_windows_camera_sources LIST_DIRECTORIES FALSE
	"${WSE_TMR_WINDOWS_CAMERA_DIR}/*.cpp"
	"${WSE_TMR_WINDOWS_CAMERA_DIR}/*.h")
foreach(wse_tmr_windows_camera_source IN LISTS wse_tmr_windows_camera_sources)
	file(READ "${wse_tmr_windows_camera_source}" wse_tmr_windows_camera_text)
	if(wse_tmr_windows_camera_text MATCHES "(->|\\.)[ \t\r\n]*Release[ \t\r\n]*\\("
			OR wse_tmr_windows_camera_text MATCHES "releaseCom[ \t\r\n]*\\(")
		message(FATAL_ERROR
			"Manual COM release is forbidden in the public Tmr backend; use ComPtr: "
			"${wse_tmr_windows_camera_source}")
	endif()
endforeach()

message(STATUS "Public Tmr PIMPL, callback worker, and Windows COM ownership satisfy the RAII policy")
