AddTarget(
	NAME flint
	TYPE shared_lib
	PROJECT_GROUP App
	SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	QT_USE
		Core
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
