AddTarget(
	NAME sqlite
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	PROJECT_GROUP Util/SQL
)
AddTarget(
	NAME sqlite3pp
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3pp/src"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
)
