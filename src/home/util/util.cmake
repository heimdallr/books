AddTarget(Util	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt6::Gui
	LINK_TARGETS
		logging
		ZipFactory
)
