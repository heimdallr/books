CreateWinRC(app
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FLibrary"
    	FILE_DESCRIPTION  "e-book cataloger"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

set(D)
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(D d)
endif()

#Да, колхоз. Но я устал
set(QT_BIN_FILES
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Core${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Gui${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Network${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Svg${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6Widgets${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6HttpServer${D}.dll
	${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/Qt6WebSockets${D}.dll
)
file(COPY ${QT_BIN_FILES} DESTINATION ${CMAKE_BINARY_DIR}/bin)

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	file(WRITE "${CMAKE_BINARY_DIR}/config/installer_mode" "msi")	

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
		flint
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
	DEPENDENCIES
		locales
)
