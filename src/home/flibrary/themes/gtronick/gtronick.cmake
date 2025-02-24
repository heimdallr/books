AddTarget(theme_gtronick	shared_lib
	PROJECT_GROUP App/Themes
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
)
