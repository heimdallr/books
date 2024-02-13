include_directories(${CMAKE_CURRENT_LIST_DIR})

set(MODULES
	logging
	fnd
	util
	database
	inpx
	flibrary
	zip
	network
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
configure_file(${CMAKE_CURRENT_LIST_DIR}/script/helpers/git_hash.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/git_hash.h @ONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/script/helpers/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/version.h @ONLY)
