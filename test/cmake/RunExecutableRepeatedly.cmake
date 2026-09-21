if(NOT DEFINED WSE_REPEAT_EXECUTABLE OR NOT EXISTS "${WSE_REPEAT_EXECUTABLE}")
	message(FATAL_ERROR "WSE_REPEAT_EXECUTABLE must name an existing test executable.")
endif()
if(NOT DEFINED WSE_REPEAT_COUNT OR WSE_REPEAT_COUNT LESS 1 OR WSE_REPEAT_COUNT GREATER 20)
	message(FATAL_ERROR "WSE_REPEAT_COUNT must be between 1 and 20.")
endif()
if(NOT DEFINED WSE_REPEAT_TIMEOUT_SECONDS)
	set(WSE_REPEAT_TIMEOUT_SECONDS 40)
endif()
if(WSE_REPEAT_TIMEOUT_SECONDS LESS 1 OR WSE_REPEAT_TIMEOUT_SECONDS GREATER 120)
	message(FATAL_ERROR "WSE_REPEAT_TIMEOUT_SECONDS must be between 1 and 120.")
endif()

foreach(wse_repeat_index RANGE 1 ${WSE_REPEAT_COUNT})
	execute_process(
		COMMAND "${WSE_REPEAT_EXECUTABLE}"
		RESULT_VARIABLE wse_repeat_result
		OUTPUT_VARIABLE wse_repeat_stdout
		ERROR_VARIABLE wse_repeat_stderr
		TIMEOUT ${WSE_REPEAT_TIMEOUT_SECONDS})
	if(NOT "${wse_repeat_result}" STREQUAL "0")
		message(FATAL_ERROR
			"Iteration ${wse_repeat_index}/${WSE_REPEAT_COUNT} failed with exit ${wse_repeat_result}.\n"
			"stdout:\n${wse_repeat_stdout}\nstderr:\n${wse_repeat_stderr}")
	endif()
endforeach()
