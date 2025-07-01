CreateWinRC(fliscaner
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FlibustaScaner"
    	FILE_DESCRIPTION  "Flibusta files downloader"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

AddTarget(fliscaner	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	WIN_RC
		${CMAKE_CURRENT_BINARY_DIR}/resources/fliscaner.rc
	SKIP_INSTALL
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Network
	LINK_TARGETS
		logging
		network
		ver
)
