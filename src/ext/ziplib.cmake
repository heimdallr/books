CreateTarget(
	NAME bzip2
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/bzip2"
	PROJECT_GROUP zip
)
CreateTarget(
	NAME lzma
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/lzma"
	PROJECT_GROUP zip
)
CreateTarget(
	NAME zlib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/zlib"
	PROJECT_GROUP zip
)
CreateTarget(
	NAME ziplib
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib"
	PROJECT_GROUP zip
	EXCLUDE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/ziplib/Source/ZipLib/extlibs/"
)

#include("${CMAKE_CURRENT_LIST_DIR}/extlibs/bzip2/bzip2.cmake")
#include("${CMAKE_CURRENT_LIST_DIR}/extlibs/lzma/lzma.cmake")
#include("${CMAKE_CURRENT_LIST_DIR}/extlibs/zlib/zlib.cmake")

#target_link_libraries(ziplib bzip2)
#target_link_libraries(ziplib lzma)
#target_link_libraries(ziplib zlib)
