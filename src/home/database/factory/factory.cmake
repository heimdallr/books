AddTarget(DatabaseFactory	shared_lib
	PROJECT_GROUP Database
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_TARGETS
		DatabaseInt
		DatabaseSqlite
)
