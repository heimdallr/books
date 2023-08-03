set(MODULES
	ui
	logic
	interface
	version
	app
)

foreach(module ${MODULES})
	include("${CMAKE_CURRENT_LIST_DIR}/${module}/${module}.cmake")
endforeach()
