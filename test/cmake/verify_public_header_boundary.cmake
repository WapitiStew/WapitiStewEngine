# @file verify_public_header_boundary.cmake
# @brief Rejects platform API leakage and ratchets classified raw void-pointer and
#        thread-state pointer debt.

if(NOT DEFINED WSE_API_ROOT OR NOT IS_DIRECTORY "${WSE_API_ROOT}")
	message(FATAL_ERROR "WSE_API_ROOT must point to the WSE public API root.")
endif()
if(NOT DEFINED WSE_RAW_VOID_BASELINE OR NOT EXISTS "${WSE_RAW_VOID_BASELINE}")
	message(FATAL_ERROR "WSE_RAW_VOID_BASELINE must point to the reviewed debt baseline.")
endif()
if(NOT DEFINED WSE_THREAD_STATE_BASELINE OR NOT EXISTS "${WSE_THREAD_STATE_BASELINE}")
	message(FATAL_ERROR "WSE_THREAD_STATE_BASELINE must point to the reviewed debt baseline.")
endif()

file(GLOB_RECURSE WSE_PUBLIC_HEADERS LIST_DIRECTORIES FALSE
	"${WSE_API_ROOT}/*.h"
	"${WSE_API_ROOT}/*.hpp")

# Identifiers of removed legacy surfaces that must not reappear in a public header. The
# TMR_FAILED_OPEN / COMMUNICATION_ERROR pair is scoped to xpt/ because the legacy Core ErrorCode
# enum still spells TMR_FAILED_OPEN until its own removal (LegacyRemovalInventory: core-legacy-error).
set(WSE_REMOVED_IDENTIFIER_RULES
	"SerialConnector|^"
	"Receving|^"
	"BMultiThread|^"
	"wseException|^"
	"wseThrowException|^"
	"Practiser|^"
	"memoey_size|^"
	"TMR_FAILED_OPEN|^"
	"COMMUNICATION_ERROR|^"
)

set(WSE_FORBIDDEN_PLATFORM_INCLUDE
	"#[ \t]*include[ \t]*[<\"][^>\"]*(windows\\.h|winsock2\\.h|ws2tcpip\\.h|dshow\\.h|mfapi\\.h|mfidl\\.h|mfreadwrite\\.h|videodev2\\.h|libcamera/|d3d12\\.h|dxgi[^>\"]*\\.h|wrl/client\\.h|vulkan/)")
set(WSE_FORBIDDEN_PLATFORM_TYPE
	"(^|[^A-Za-z0-9_])(HANDLE|HRESULT|GUID|IAMCameraControl|IAMVideoProcAmp|IMF[A-Za-z0-9_]*|ID3D12[A-Za-z0-9_]*|IDXGI[A-Za-z0-9_]*|Vk[A-Za-z0-9_]*|v4l2_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)")

file(STRINGS "${WSE_RAW_VOID_BASELINE}" WSE_RAW_VOID_BASELINE_LINES)
set(WSE_RAW_VOID_BASELINE_PATHS)
foreach(WSE_BASELINE_LINE IN LISTS WSE_RAW_VOID_BASELINE_LINES)
	if(WSE_BASELINE_LINE STREQUAL "" OR WSE_BASELINE_LINE MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" WSE_BASELINE_FIELDS "${WSE_BASELINE_LINE}")
	list(LENGTH WSE_BASELINE_FIELDS WSE_BASELINE_FIELD_COUNT)
	if(NOT WSE_BASELINE_FIELD_COUNT EQUAL 3)
		message(FATAL_ERROR "Invalid raw void-pointer baseline entry: ${WSE_BASELINE_LINE}")
	endif()
	list(GET WSE_BASELINE_FIELDS 0 WSE_BASELINE_PATH)
	list(GET WSE_BASELINE_FIELDS 1 WSE_BASELINE_COUNT)
	list(APPEND WSE_RAW_VOID_BASELINE_PATHS "${WSE_BASELINE_PATH}")
	string(MAKE_C_IDENTIFIER "${WSE_BASELINE_PATH}" WSE_BASELINE_KEY)
	set("WSE_RAW_VOID_EXPECTED_${WSE_BASELINE_KEY}" "${WSE_BASELINE_COUNT}")
endforeach()

# A public header must not hand out pointers to synchronization state. The reviewed legacy
# occurrences live in the thread-state baseline; std::atomic members and values remain allowed
# (cancellation tokens hold one), only the pointer form is debt.
set(WSE_THREAD_STATE_PATTERN "std::(thread|mutex|atomic(_[a-z0-9]+|<[^>]*>)?)[ \t]*\\*")
file(STRINGS "${WSE_THREAD_STATE_BASELINE}" WSE_THREAD_STATE_BASELINE_LINES)
set(WSE_THREAD_STATE_BASELINE_PATHS)
foreach(WSE_BASELINE_LINE IN LISTS WSE_THREAD_STATE_BASELINE_LINES)
	if(WSE_BASELINE_LINE STREQUAL "" OR WSE_BASELINE_LINE MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" WSE_BASELINE_FIELDS "${WSE_BASELINE_LINE}")
	list(LENGTH WSE_BASELINE_FIELDS WSE_BASELINE_FIELD_COUNT)
	if(NOT WSE_BASELINE_FIELD_COUNT EQUAL 3)
		message(FATAL_ERROR "Invalid thread-state baseline entry: ${WSE_BASELINE_LINE}")
	endif()
	list(GET WSE_BASELINE_FIELDS 0 WSE_BASELINE_PATH)
	list(GET WSE_BASELINE_FIELDS 1 WSE_BASELINE_COUNT)
	list(APPEND WSE_THREAD_STATE_BASELINE_PATHS "${WSE_BASELINE_PATH}")
	string(MAKE_C_IDENTIFIER "${WSE_BASELINE_PATH}" WSE_BASELINE_KEY)
	set("WSE_THREAD_STATE_EXPECTED_${WSE_BASELINE_KEY}" "${WSE_BASELINE_COUNT}")
endforeach()

set(WSE_RAW_VOID_SEEN_PATHS)
set(WSE_THREAD_STATE_SEEN_PATHS)
foreach(WSE_PUBLIC_HEADER IN LISTS WSE_PUBLIC_HEADERS)
	file(READ "${WSE_PUBLIC_HEADER}" WSE_PUBLIC_HEADER_CONTENT)
	if(WSE_PUBLIC_HEADER_CONTENT MATCHES "${WSE_FORBIDDEN_PLATFORM_INCLUDE}"
			OR WSE_PUBLIC_HEADER_CONTENT MATCHES "${WSE_FORBIDDEN_PLATFORM_TYPE}")
		message(FATAL_ERROR "Platform implementation API leaked into public header: ${WSE_PUBLIC_HEADER}")
	endif()

	file(RELATIVE_PATH WSE_PUBLIC_HEADER_RELATIVE "${WSE_API_ROOT}" "${WSE_PUBLIC_HEADER}")
	string(REPLACE "\\" "/" WSE_PUBLIC_HEADER_RELATIVE "${WSE_PUBLIC_HEADER_RELATIVE}")

	foreach(WSE_REMOVED_RULE IN LISTS WSE_REMOVED_IDENTIFIER_RULES)
		string(REPLACE "|" ";" WSE_REMOVED_RULE_FIELDS "${WSE_REMOVED_RULE}")
		list(GET WSE_REMOVED_RULE_FIELDS 0 WSE_REMOVED_IDENTIFIER)
		list(GET WSE_REMOVED_RULE_FIELDS 1 WSE_REMOVED_SCOPE)
		if(WSE_PUBLIC_HEADER_RELATIVE MATCHES "${WSE_REMOVED_SCOPE}"
				AND WSE_PUBLIC_HEADER_CONTENT MATCHES
					"(^|[^A-Za-z0-9_])${WSE_REMOVED_IDENTIFIER}([^A-Za-z0-9_]|$)")
			message(FATAL_ERROR
				"Removed legacy identifier '${WSE_REMOVED_IDENTIFIER}' reappeared in public header: "
				"${WSE_PUBLIC_HEADER_RELATIVE}")
		endif()
	endforeach()

	string(REGEX MATCHALL "(^|[^A-Za-z0-9_])void[ \t]*\\*" WSE_RAW_VOID_MATCHES
		"${WSE_PUBLIC_HEADER_CONTENT}")
	list(LENGTH WSE_RAW_VOID_MATCHES WSE_RAW_VOID_COUNT)
	if(WSE_RAW_VOID_COUNT GREATER 0)
		list(FIND WSE_RAW_VOID_BASELINE_PATHS "${WSE_PUBLIC_HEADER_RELATIVE}" WSE_BASELINE_INDEX)
		if(WSE_BASELINE_INDEX EQUAL -1)
			message(FATAL_ERROR
				"Unclassified raw void pointer added to public header: ${WSE_PUBLIC_HEADER_RELATIVE}")
		endif()
		string(MAKE_C_IDENTIFIER "${WSE_PUBLIC_HEADER_RELATIVE}" WSE_BASELINE_KEY)
		set(WSE_EXPECTED_COUNT "${WSE_RAW_VOID_EXPECTED_${WSE_BASELINE_KEY}}")
		if(NOT WSE_RAW_VOID_COUNT EQUAL WSE_EXPECTED_COUNT)
			message(FATAL_ERROR
				"Raw void-pointer count changed for ${WSE_PUBLIC_HEADER_RELATIVE}: "
				"expected ${WSE_EXPECTED_COUNT}, found ${WSE_RAW_VOID_COUNT}. "
				"Reduce the debt and update its reviewed inventory together; do not increase it.")
		endif()
		list(APPEND WSE_RAW_VOID_SEEN_PATHS "${WSE_PUBLIC_HEADER_RELATIVE}")
	endif()

	string(REGEX MATCHALL "${WSE_THREAD_STATE_PATTERN}" WSE_THREAD_STATE_MATCHES
		"${WSE_PUBLIC_HEADER_CONTENT}")
	list(LENGTH WSE_THREAD_STATE_MATCHES WSE_THREAD_STATE_COUNT)
	if(WSE_THREAD_STATE_COUNT GREATER 0)
		list(FIND WSE_THREAD_STATE_BASELINE_PATHS "${WSE_PUBLIC_HEADER_RELATIVE}" WSE_BASELINE_INDEX)
		if(WSE_BASELINE_INDEX EQUAL -1)
			message(FATAL_ERROR
				"Thread-state pointer added to public header: ${WSE_PUBLIC_HEADER_RELATIVE}. "
				"Own synchronization inside the implementation instead of exposing it.")
		endif()
		string(MAKE_C_IDENTIFIER "${WSE_PUBLIC_HEADER_RELATIVE}" WSE_BASELINE_KEY)
		set(WSE_EXPECTED_COUNT "${WSE_THREAD_STATE_EXPECTED_${WSE_BASELINE_KEY}}")
		if(NOT WSE_THREAD_STATE_COUNT EQUAL WSE_EXPECTED_COUNT)
			message(FATAL_ERROR
				"Thread-state pointer count changed for ${WSE_PUBLIC_HEADER_RELATIVE}: "
				"expected ${WSE_EXPECTED_COUNT}, found ${WSE_THREAD_STATE_COUNT}. "
				"Reduce the debt and update its reviewed baseline together; do not increase it.")
		endif()
		list(APPEND WSE_THREAD_STATE_SEEN_PATHS "${WSE_PUBLIC_HEADER_RELATIVE}")
	endif()
endforeach()

foreach(WSE_BASELINE_PATH IN LISTS WSE_RAW_VOID_BASELINE_PATHS)
	list(FIND WSE_RAW_VOID_SEEN_PATHS "${WSE_BASELINE_PATH}" WSE_SEEN_INDEX)
	if(WSE_SEEN_INDEX EQUAL -1)
		message(FATAL_ERROR
			"Stale raw void-pointer baseline entry: ${WSE_BASELINE_PATH}. "
			"Remove it and update the Phase 7-B inventory.")
	endif()
endforeach()

foreach(WSE_BASELINE_PATH IN LISTS WSE_THREAD_STATE_BASELINE_PATHS)
	list(FIND WSE_THREAD_STATE_SEEN_PATHS "${WSE_BASELINE_PATH}" WSE_SEEN_INDEX)
	if(WSE_SEEN_INDEX EQUAL -1)
		message(FATAL_ERROR
			"Stale thread-state baseline entry: ${WSE_BASELINE_PATH}. "
			"Remove it and update the legacy removal inventory together.")
	endif()
endforeach()

message(STATUS
	"WSE public headers are platform-independent; raw void-pointer and thread-state debt did not increase")
