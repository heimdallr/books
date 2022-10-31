AddTarget(
	NAME SimpleSaxParser
	TYPE static_lib
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/SimpleSaxParser"
	PROJECT_GROUP Util/XML
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
