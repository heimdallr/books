include_directories(${CMAKE_CURRENT_LIST_DIR})

include("${CMAKE_CURRENT_LIST_DIR}/theme/theme.cmake")

set(MODULES
	gui
	logic
	interface
	version
	app
)
foreach(module ${MODULES})
	include("${CMAKE_CURRENT_LIST_DIR}/${module}/${module}.cmake")
endforeach()
