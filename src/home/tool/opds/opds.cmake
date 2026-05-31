if (${QT_MAJOR_VERSION} STREQUAL "5")
	return()
endif()

AddTarget(opds	app
	PROJECT_GROUP    Tool
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Concurrent
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Gui
		Qt${QT_MAJOR_VERSION}::HttpServer
		Qt${QT_MAJOR_VERSION}::Network
	LINK_TARGETS
		flint
		fnd
		logging
		logic
		platform
		util
		ver
		zip
)
