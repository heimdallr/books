include_directories(${CMAKE_CURRENT_LIST_DIR})

set(MODULES
	inpx
	flibrary
	tool
	gutil
)
foreach(module ${MODULES})
	include("${CMAKE_CURRENT_LIST_DIR}/${module}/${module}.cmake")
endforeach()

configure_file(${BUILDSCRIPTS_ROOT}/helpers/git_hash.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/git_hash.h @ONLY)
configure_file(${BUILDSCRIPTS_ROOT}/helpers/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/version.h @ONLY)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/resources/locales)
