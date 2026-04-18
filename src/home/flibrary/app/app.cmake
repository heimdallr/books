CreateWinRC(app
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FLibrary"
    	FILE_DESCRIPTION  "e-book cataloger"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

#Да, колхоз. Но я устал
CopyAndInstallICU()
if (MSVC)
	CopyAndInstallQt(${QtModules})
	set(PLUGIN_LIST iconengines imageformats platforms styles)
	if(QT6)
		list(APPEND PLUGIN_LIST tls)
	endif()
	InstallQtPlugins(${PLUGIN_LIST})
endif()
#if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
#	install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/locales DESTINATION .)
#endif()

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
		platform
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
		qsvgicon
	DEPENDENCIES
		locales
)
