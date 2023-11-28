include_directories(${CMAKE_CURRENT_LIST_DIR})

AddTarget(zip	shared_lib
	PROJECT_GROUP Util/zip
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt6::Gui
	LINK_TARGETS
		logging
		ZipFactory
)
