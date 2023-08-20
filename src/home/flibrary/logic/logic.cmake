AddTarget(
	NAME logic
	TYPE shared_lib
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../../ext/include"
	QT_USE
		Core
		Gui
	MODULES
		Hypodermic
		plog
		quazip
	LINK_TARGETS
		DatabaseFactory
		flint
		InpxLib
		logging
		Util
	DEPENDENCIES
		MyHomeLibSQLIteExt
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
