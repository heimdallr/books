AddTarget(
	NAME inpx
	TYPE app_console
	PROJECT_GROUP Tool
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/inpx/src/tool"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}/inpx/src"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/plog/include"
	LINK_TARGETS
		InpxLib
		plog
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)

AddTarget(
	NAME InpxLib
	TYPE shared_lib
	PROJECT_GROUP Util
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/inpx/src/util"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}/../ext"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/include"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/fmt/include"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/plog/include"
		"${CMAKE_CURRENT_BINARY_DIR}-thirdparty/include"
	QT_USE
		Core
	LINK_TARGETS
		fmt
		plog
		sqlite
		sqlite3pp
	MODULES
		QuaZip::QuaZip
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)

execute_process(
	COMMAND git log -1 --format=%h
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT_VARIABLE GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
	)
configure_file(${CMAKE_CURRENT_LIST_DIR}/inpx/helpers/Configuration.h.in ${CMAKE_CURRENT_BINARY_DIR}/Configuration.h @ONLY)
