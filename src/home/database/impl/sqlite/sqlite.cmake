AddTarget(
	NAME DatabaseSqlite
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite"
	PROJECT_GROUP Database/Impl
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../interface"
	LINK_TARGETS
		DatabaseInt
		logging
		sqlite
		sqlite3pp
	MODULES
		plog
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
