AddTarget(
	NAME inpx
	TYPE app_console
	SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
	LINK_TARGETS
		bzip2
		lzma
		zlib
		ziplib
)
