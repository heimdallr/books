﻿include_directories(${CMAKE_CURRENT_LIST_DIR}/github_api/include)

AddTarget(network	shared_lib
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	PROJECT_GROUP Network
	LINK_TARGETS
		logging
	LINK_LIBRARIES
		Qt6::Network
)
