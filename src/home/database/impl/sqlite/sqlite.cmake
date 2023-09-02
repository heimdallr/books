AddTarget(DatabaseSqlite	shared_lib
	PROJECT_GROUP Database/Impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../interface"
	LINK_TARGETS
		DatabaseInt
		logging
		sqlite
		sqlite3pp
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)
