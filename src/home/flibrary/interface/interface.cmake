AddTarget(flint	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		${CMAKE_CURRENT_LIST_DIR}
	LINK_LIBRARIES
		plog::plog
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		logging
		util
)
