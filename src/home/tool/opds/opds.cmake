if (${QT_MAJOR_VERSION} STREQUAL "5")
	return()
endif()

CreateWinRC(opds
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "opds"
    	FILE_DESCRIPTION  "OPDS server"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../flibrary/app/resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

AddTarget(opds	app
	PROJECT_GROUP    tool
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	WIN_RC           ${CMAKE_CURRENT_BINARY_DIR}/resources/opds.rc
	LINK_LIBRARIES
		Hypodermic
		Qt${QT_MAJOR_VERSION}::Concurrent
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::HttpServer
		Qt${QT_MAJOR_VERSION}::Network
		${icu_modules}
	LINK_TARGETS
		flint
		Fnd
		logging
		logic
		Util
		ver
		zip
)
