AddTarget(ver	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
)

add_custom_command(TARGET ver
    POST_BUILD
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../script/bat/build.bat
)
