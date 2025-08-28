AddTarget(fliscaner	app_console
	PROJECT_GROUP Tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	SKIP_INSTALL
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Network
	LINK_TARGETS
		logging
		network
		ver
)
