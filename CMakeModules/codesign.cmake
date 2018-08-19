find_package(PythonInterp 2.0)

set(CODESIGN_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/sign.py)
macro(codesign target)
	if(WIN32 AND PYTHON_EXECUTABLE)
		if (DEFINED ENV{SIGNTOOL_SCP} OR DEFINED ENV{SIGNTOOL_PFX})
			message("-- ${target} will be signed")
			add_custom_command(TARGET ${target} COMMAND ${PYTHON_EXECUTABLE} ${CODESIGN_SCRIPT} $<TARGET_FILE:${target}>)
		endif()
	endif()
endmacro(codesign)
