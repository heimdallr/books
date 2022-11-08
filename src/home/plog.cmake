AddTarget(
	NAME plog
	TYPE shared_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/plog"
	PROJECT_GROUP Util
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../ext/plog/include"
		"${CMAKE_CURRENT_LIST_DIR}/flibrary/src/constants"
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_EXPORT ]
)
