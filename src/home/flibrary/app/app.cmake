file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

AddTarget(${PROJECT_NAME}	app
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		flidjvu
		flint
		gui
		gutil
		logging
		logic
		platform
		rest
		util
		ver
	DEPENDENCIES
		locales
)
