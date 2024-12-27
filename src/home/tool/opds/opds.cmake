AddTarget(opds	app
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		Qt6::Core
		Qt6::Network
		Qt6::HttpServer
	LINK_TARGETS
		Fnd
		logging
		logic
		Util
)
