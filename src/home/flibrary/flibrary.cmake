set(MODULES
	app
	ui
	logic
	interface
	version
)

foreach(module ${MODULES})
	include("${CMAKE_CURRENT_LIST_DIR}/${module}/${module}.cmake")
endforeach()
