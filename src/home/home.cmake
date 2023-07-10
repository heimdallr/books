set(MODULES
	logging
	fnd
	interface
	util
	database
	inpx
	flibrary
)

foreach(module ${MODULES})
	include("${CMAKE_CURRENT_LIST_DIR}/${module}/${module}.cmake")
endforeach()

execute_process(
	COMMAND git log -1 --format=%h
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT_VARIABLE GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
	)
configure_file(${CMAKE_CURRENT_LIST_DIR}/inpx/helpers/Configuration.h.in ${CMAKE_CURRENT_BINARY_DIR}/Configuration.h @ONLY)
