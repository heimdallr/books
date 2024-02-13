AddTarget(ThemeDark	shared_lib
	PROJECT_GROUP App/Theme
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_TARGETS
		flint
		logging
	LINK_LIBRARIES
		plog
		Qt6::Core
)
