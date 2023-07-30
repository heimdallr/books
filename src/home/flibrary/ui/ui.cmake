AddTarget(
	NAME flui
	TYPE shared_lib
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	QRC
#		"${CMAKE_CURRENT_LIST_DIR}/../resources/main.qrc"
	QT_USE
		Core
		Gui
		Widgets
	MODULES
		Hypodermic
		plog
	LINK_TARGETS
		logging
		Util
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
