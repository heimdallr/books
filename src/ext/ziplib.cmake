AddTarget(
	NAME bzip2
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/bzip2"
	PROJECT_GROUP Util/Zip
)
AddTarget(
	NAME lzma
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/lzma"
	PROJECT_GROUP Util/Zip
)
AddTarget(
	NAME zlib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/zlib"
	PROJECT_GROUP Util/Zip
)
AddTarget(
	NAME ziplib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib"
	PROJECT_GROUP Util/Zip
	EXCLUDE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/"
	COMPILER_OPTIONS
		[ MSVC /std:c++14 ]
)
