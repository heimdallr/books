AddTarget(
	NAME DatabaseFactory
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	PROJECT_GROUP Database
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	LINK_TARGETS
		DatabaseInt
		DatabaseSqlite
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
