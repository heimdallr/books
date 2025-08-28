CreateWinRC(app
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FLibrary"
    	FILE_DESCRIPTION  "e-book cataloger"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	file(WRITE "${CMAKE_BINARY_DIR}/config/installer_mode" "msi")
	
#Да, колхоз. Но я устал
	set(QT_BIN_FILES
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Core.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Gui.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Network.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Svg.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Widgets.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6HttpServer.dll
		${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6WebSockets.dll
	)
	file(COPY ${QT_BIN_FILES} DESTINATION ${CMAKE_BINARY_DIR}/bin)

	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/imageformats DESTINATION .)
	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/locales DESTINATION .)
	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/platforms DESTINATION .)
	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/styles DESTINATION .)
	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/tls DESTINATION .)
	install(
		FILES
			"${CMAKE_BINARY_DIR}/config/installer_mode"
			"${CMAKE_BINARY_DIR}/bin/LICENSE.txt"
			${QT_BIN_FILES}
		DESTINATION .
		)
endif()


AddTarget(${PROJECT_NAME}	app
	PROJECT_GROUP App
	WIN_RC ${CMAKE_BINARY_DIR}/resources/app.rc
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		gui
		gutil
		logging
		logic
		rest
		util
		ver
	QT_PLUGINS
		qwindows
		qmodernwindowsstyle
		qgif
		qjpeg
		qsvg
		qschannelbackend
)

file(GLOB qt_ts "${CMAKE_CURRENT_LIST_DIR}/../../resources/locales/[^.]*\.ts")
if (${QT_MAJOR_VERSION} STREQUAL 6)
    foreach(ts ${qt_ts})
        get_filename_component(ts ${ts} NAME_WE)
        file(COPY ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_${ts}.qm DESTINATION ${CMAKE_BINARY_DIR}/bin/locales)
    endforeach()
endif()

GenerateTranslations(
	NAME ${PROJECT_NAME}
	PATH "${CMAKE_CURRENT_LIST_DIR}/../../"
	FILES ${qt_ts}
)
