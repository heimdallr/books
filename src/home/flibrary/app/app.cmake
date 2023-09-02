AddTarget(flibrary	app
#	WIN_APP_ICON "${CMAKE_CURRENT_LIST_DIR}/resources/icons/main.ico"
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	LINK_LIBRARIES
		Qt6::Widgets
		Hypodermic
	LINK_TARGETS
		logging
		logic
		gui
		ver
)

get_property(third_party_bin_dirs GLOBAL PROPERTY third_party_bin_dirs_property)
set_target_properties(flibrary PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${third_party_bin_dirs}")
