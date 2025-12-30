AddTarget(MyHomeLibSQLIteExt	shared_lib
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/MyHomeLib/Utils/MHLSQLiteExt"
	PROJECT_GROUP ThirdParty/SQL
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
	COMPILER_OPTIONS
		/wd4100 # unreferenced formal parameter
)
