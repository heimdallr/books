AddTarget(
	NAME inpx
	TYPE app_console
	SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
	INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../ext/ziplib/Source"
	LINK_TARGETS
		bzip2
		lzma
		zlib
		ziplib
	COMPILER_OPTIONS
		[ MSVC /WX /Wall ]
)
