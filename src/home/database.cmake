AddTarget(
	NAME DatabaseInt
	TYPE header_only
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/database/interface"
	PROJECT_GROUP Interface
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
AddTarget(
	NAME DatabaseFactory
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/database/factory"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}"
	PROJECT_GROUP Database
	LINK_TARGETS
		DatabaseInt
		DatabaseSqlite
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
AddTarget(
	NAME DatabaseSqlite
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/database/impl/sqlite"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/plog/include"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite3pp/src"
	PROJECT_GROUP Database/Impl
	LINK_TARGETS
		DatabaseInt
		plog
		sqlite
		sqlite3pp
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
