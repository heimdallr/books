AddTarget(flint	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		${CMAKE_CURRENT_LIST_DIR}
	LINK_LIBRARIES
		Qt6::Widgets
	LINK_TARGETS
		Util
)
