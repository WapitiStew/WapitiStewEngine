if(NOT DEFINED WSE_LINKER OR WSE_LINKER STREQUAL "")
	message(FATAL_ERROR "WSE_LINKER is required")
endif()
if(NOT DEFINED WSE_LIBRARY OR NOT EXISTS "${WSE_LIBRARY}")
	message(FATAL_ERROR "WSE_LIBRARY does not exist: ${WSE_LIBRARY}")
endif()

execute_process(
	COMMAND "${WSE_LINKER}" /dump /exports "${WSE_LIBRARY}"
	RESULT_VARIABLE dump_result
	OUTPUT_VARIABLE dump_output
	ERROR_VARIABLE dump_error
)
if(NOT dump_result EQUAL 0)
	message(FATAL_ERROR "Failed to inspect WSE exports (${dump_result}): ${dump_error}")
endif()

set(forbidden_exports
	SerialPort@xpt@wse
	UdpClient@xpt@wse
	RetryPolicy@xpt@wse
	HttpClient@xpt@wse
	CSVController@gef@wse
	Keyboard@iui@wse
	ProjectionRenderer@oui@wse
	WebCamera@tmr@wse
	Projector@vpj@wse
)
foreach(forbidden_export IN LISTS forbidden_exports)
	if(dump_output MATCHES "${forbidden_export}")
		message(FATAL_ERROR
			"Disabled component symbol '${forbidden_export}' was exported by ${WSE_LIBRARY}")
	endif()
endforeach()

message(STATUS "No disabled component symbols were found in ${WSE_LIBRARY}")
