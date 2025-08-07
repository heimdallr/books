CreateWinRC(flistat
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FlibustaStatistics"
    	FILE_DESCRIPTION  "Flibusta Statistics Database"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

AddTarget(flistat	app_console
	PROJECT_GROUP    tool
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	WIN_RC           ${CMAKE_CURRENT_BINARY_DIR}/resources/flistat.rc
	SKIP_INSTALL
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		DatabaseFactory
		logging
		Util
		ver
)
