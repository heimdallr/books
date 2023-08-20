AddTarget(
	NAME sqlite
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	PROJECT_GROUP Util/SQL
	COMPILE_DEFINITIONS
		SQLITE_THREADSAFE=0
	COMPILER_OPTIONS
		[ MSVC /wd4996 ] #'GetVersionExA': was declared deprecated
)
AddTarget(
	NAME sqlite3pp
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3pp/src"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	COMPILER_OPTIONS
		[ MSVC /wd4267 ] #'argument': conversion from 'size_t' to 'int', possible loss of data
)
