AddTarget(DatabaseFactory	shared_lib
	PROJECT_GROUP Database
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		DatabaseInt
		DatabaseSqlite
		logging
)
