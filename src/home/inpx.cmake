AddTarget(
	NAME inpx
	TYPE app_console
	PROJECT_GROUP Tool
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/inpx/src"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../ext"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/ziplib/Source"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/fmt/include"
	LINK_TARGETS
		bzip2
		lzma
		sqlite
		sqlite3pp
		sqlite3shell_lib
		ziplib
		zlib
		fmt
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)

configure_file(${CMAKE_CURRENT_LIST_DIR}/inpx/helpers/Configuration.h.in ${CMAKE_CURRENT_BINARY_DIR}/Configuration.h @ONLY)
