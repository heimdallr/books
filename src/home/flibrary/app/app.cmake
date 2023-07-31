AddTarget(
	NAME flibrary
	TYPE app
	WIN_APP_ICON "${CMAKE_CURRENT_LIST_DIR}/resources/icons/main.ico"
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	QT_USE
		Widgets
	MODULES
		Hypodermic
		plog
	LINK_TARGETS
		logging
		logic
		ui
		ver
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)

add_custom_command(TARGET flibrary
    POST_BUILD
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../script/build.bat
)
