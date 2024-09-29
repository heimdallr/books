AddTarget(fb2imager	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt6::Core
		Qt6::Gui
	LINK_TARGETS
		logging
		Util
)
