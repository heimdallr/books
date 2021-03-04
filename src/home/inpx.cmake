AddTarget(
	NAME inpx
	TYPE app_console
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/inpx"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/ziplib/Source"
	LINK_TARGETS
		bzip2
		lzma
		sqlite
		sqlite3pp
		ziplib
		zlib
	COMPILER_OPTIONS
		[ MSVC /WX /Wall ]
)
