AddTarget(flimager	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Gui
	LINK_TARGETS
		fljxl
		logging
		logic
		Util
		ver
		zip
)
