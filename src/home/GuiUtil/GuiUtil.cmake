AddTarget(GuiUtil	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt6::Widgets
	LINK_TARGETS
		logging
		Util
)
