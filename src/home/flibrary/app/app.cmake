CreateWinRC(app
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FLibrary"
    	FILE_DESCRIPTION  "e-book cataloger"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

file(COPY ${Qt6Translations_DIR}/qtbase_ru.qm DESTINATION ${CMAKE_BINARY_DIR}/bin/locales)
if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	file(WRITE "${CMAKE_BINARY_DIR}/config/installer_mode" "msi")
	install(FILES "${CMAKE_BINARY_DIR}/config/installer_mode" DESTINATION .)
endif()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/imageformats DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/locales DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/platforms DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/styles DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/tls DESTINATION .)

AddTarget(${PROJECT_NAME}	app
	PROJECT_GROUP App
	WIN_RC ${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Widgets
		Hypodermic
	LINK_TARGETS
		gui
		GuiUtil
		logging
		logic
		rest
		Util
		ver
	QT_PLUGINS
		Qt6::QWindowsIntegrationPlugin
		Qt6::QModernWindowsStylePlugin
		Qt6::QGifPlugin
		Qt6::QJpegPlugin
		Qt6::QSvgPlugin
		Qt6::QSchannelBackendPlugin
)
