AddTarget(
	NAME fmt
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/fmt/src"
	EXCLUDE_SOURCES "fmt.cc"
	PROJECT_GROUP Util
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/fmt/include"
)
