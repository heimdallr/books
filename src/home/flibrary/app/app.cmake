file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

AddTarget(${PROJECT_NAME}	app
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/unit-tests
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		flidjvu
		flint
		genre
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

AddGTest(app
	PROJECT_GROUP UT/App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/unit-tests"
	SOURCES
		"${CMAKE_CURRENT_LIST_DIR}/di_app.cpp"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		gui
		gutil
		logic
)
