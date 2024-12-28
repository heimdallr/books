AddTarget(opds	app
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		Qt6::Concurrent
		Qt6::Core
		Qt6::HttpServer
		Qt6::Network
	LINK_TARGETS
		Fnd
		logging
		logic
		Util
)
