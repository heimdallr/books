AddTarget(
	NAME MyHomeLibSQLIteExt
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/MyHomeLib/Utils/MHLSQLiteExt"
	PROJECT_GROUP Util/SQL
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite"
)

set(CREATE_COLLECTION_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/MyHomeLib/Program/DAO/SQLite/Resources/CreateCollectionDB_SQLite.sql")
