AddTarget(gutil	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/unit-tests
	LINK_LIBRARIES
		Boost::headers
		plog::plog
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		flint
		logging
		platformgui
		utilgui
)

AddGTest(gutil
	PROJECT_GROUP UT/Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/unit-tests"
)
