AddTarget(
	NAME ui
	TYPE shared_lib
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	QT_USE
		Core
		Gui
		Widgets
	MODULES
		Hypodermic
		plog
	LINK_TARGETS
		flint
		logging
		logic
		Util
		ver
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)