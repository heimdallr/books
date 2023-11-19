AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../zip"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt6::Widgets
	LINK_TARGETS
		flint
		logging
		logic
		Util
		ver
		zip
)
