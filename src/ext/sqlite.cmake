AddTarget(sqlite	static_lib
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	PROJECT_GROUP ThirdParty/SQL
	COMPILE_DEFINITIONS
		SQLITE_THREADSAFE=0
		SQLITE_ENABLE_FTS5
	COMPILER_OPTIONS
		/wd4996 #'XXX': was declared deprecated
		/wd4232 # address of dllimport 'XXX' is not static, identity not guaranteed
		/wd4456 # declaration of 'XXX' hides previous local declaration
		/wd4127 # conditional expression is constant
		/wd4244 # conversion from 'XXX' to 'YYY', possible loss of data
		/wd4706 # assignment within conditional expression
		/wd4701 # potentially uninitialized local variable 'lastErrno' used
)
AddTarget(sqlite3pp	static_lib
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3pp/src"
	PROJECT_GROUP ThirdParty/SQL
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	COMPILER_OPTIONS
		[ MSVC /wd4267 ] #'argument': conversion from 'size_t' to 'int', possible loss of data
)
