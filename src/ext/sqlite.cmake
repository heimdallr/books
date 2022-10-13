AddTarget(
	NAME sqlite
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	PROJECT_GROUP Util/SQL
	COMPILE_DEFINITIONS
		SQLITE_THREADSAFE=0
)
AddTarget(
	NAME sqlite3pp
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3pp/src"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
)
AddTarget(
	NAME sqlite3shell_lib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/shell"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	EXCLUDE_SOURCES
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/shell/main.c"
	LINK_TARGETS
		sqlite
)
AddTarget(
	NAME sqlite3
	TYPE app_console
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/shell"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	LINK_TARGETS
		sqlite
)
