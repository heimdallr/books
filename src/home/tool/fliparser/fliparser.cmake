CreateWinRC(fliparser
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FlibustaParser"
    	FILE_DESCRIPTION  "Flibusta database dump parser"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

AddTarget(fliparser	app_console
	PROJECT_GROUP    tool
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	WIN_RC           ${CMAKE_CURRENT_BINARY_DIR}/resources/fliparser.rc
	SKIP_INSTALL
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		DatabaseFactory
		logging
		Util
		ver
		zip
)
