AddTarget(sqlite3pp	static_lib
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3pp/src"
	PROJECT_GROUP ThirdParty/SQL
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	COMPILER_OPTIONS
		[ MSVC /wd4267 ] #'argument': conversion from 'size_t' to 'int', possible loss of data
	LINK_LIBRARIES
		SQLite::SQLite3
)
