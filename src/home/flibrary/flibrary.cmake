include_directories(${CMAKE_CURRENT_LIST_DIR})

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

set(THEMES
	dark
	light
)
foreach(theme ${THEMES})
	include("${CMAKE_CURRENT_LIST_DIR}/theme/${theme}/${theme}.cmake")
endforeach()
