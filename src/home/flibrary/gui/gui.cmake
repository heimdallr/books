AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/ut
	LINK_LIBRARIES
		Boost::headers
		plog::plog
		Qt${QT_MAJOR_VERSION}::Network
		Qt${QT_MAJOR_VERSION}::Widgets
		Qt${QT_MAJOR_VERSION}::Svg
	LINK_TARGETS
		flint
		gutil
		logging
		platform
		settings
		util
		utilgui
		ver
		zip
	DEPENDENCIES
		${ICONS}
)

AddGTest(gui
	PROJECT_GROUP UT/App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/ut"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		gui
		gutil
		logic
		settings
		util
)
