AddTarget(
	NAME InpxLib
	TYPE shared_lib
	PROJECT_GROUP Util
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/include"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/fmt/include"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite"
	QT_USE
		Core
	LINK_TARGETS
		logging
		sqlite
		sqlite3pp
	MODULES
		plog
		quazip
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
