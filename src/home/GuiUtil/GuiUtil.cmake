AddTarget(GuiUtil	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		logging
		Util
)
