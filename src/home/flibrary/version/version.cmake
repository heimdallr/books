AddTarget(
	NAME ver
	TYPE shared_lib
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
	QT_USE
		Core
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)

add_custom_command(TARGET ver
    POST_BUILD
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../script/build.bat
)
