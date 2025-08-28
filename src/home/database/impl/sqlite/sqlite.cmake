AddTarget(dbsqlite	shared_lib
	PROJECT_GROUP Database/Impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../../interface"
	LINK_LIBRARIES
		plog::plog
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		logging
		sqlite
		sqlite3pp
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)
