include_directories(${CMAKE_CURRENT_LIST_DIR}/github_api/include)

AddTarget(rest	shared_lib
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	PROJECT_GROUP Network
	LINK_TARGETS
		logging
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Network
)
