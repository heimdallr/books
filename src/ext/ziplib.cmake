AddTarget(
	NAME bzip2
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/bzip2"
	PROJECT_GROUP Util/Zip
	STATIC_RUNTIME
)
AddTarget(
	NAME lzma
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/lzma"
	PROJECT_GROUP Util/Zip
	STATIC_RUNTIME
)
AddTarget(
	NAME zlib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/zlib"
	PROJECT_GROUP Util/Zip
	STATIC_RUNTIME
)
AddTarget(
	NAME ziplib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib"
	PROJECT_GROUP Util/Zip
	EXCLUDE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/"
	STATIC_RUNTIME
	COMPILER_OPTIONS
		[ MSVC /std:c++14 ]
)
