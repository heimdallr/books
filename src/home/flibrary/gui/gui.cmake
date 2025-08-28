AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		plog::plog
		Qt${QT_MAJOR_VERSION}::Network
		Qt${QT_MAJOR_VERSION}::Widgets
		Qt${QT_MAJOR_VERSION}::Svg
	LINK_TARGETS
		flint
		logging
		logic
		util
		gutil
		ver
		zip
	DEPENDENCIES
		${ICONS}
)
