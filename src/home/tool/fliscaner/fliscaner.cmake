AddTarget(fliscaner	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		logging
		network
		ver
)
