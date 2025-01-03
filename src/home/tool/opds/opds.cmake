if (${QT_MAJOR_VERSION} STREQUAL "5")
	return()
endif()

AddTarget(opds	app
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		Qt${QT_MAJOR_VERSION}::Concurrent
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::HttpServer
		Qt${QT_MAJOR_VERSION}::Network
	LINK_TARGETS
		Fnd
		logging
		logic
		Util
		zip
)
