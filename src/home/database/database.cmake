AddTarget(
	NAME DatabaseInt
	TYPE header_only
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/interface"
	PROJECT_GROUP Interface
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
AddTarget(
	NAME DatabaseFactory
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/factory"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
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
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/impl/sqlite"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/sqlite/sqlite3pp/src"
	PROJECT_GROUP Database/Impl
	LINK_TARGETS
		DatabaseInt
		sqlite
		sqlite3pp
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
