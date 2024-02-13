AddTarget(ThemeDefault	shared_lib
	PROJECT_GROUP App/Theme
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_TARGETS
		flint
	LINK_LIBRARIES
		Qt6::Core
)
