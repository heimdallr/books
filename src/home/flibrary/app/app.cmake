CreateWinRC(app
    	COMPANY_NAME      "HomeCompa"
    	FILE_NAME         "FLibrary"
    	FILE_DESCRIPTION  "e-book cataloger"
    	APP_ICON          "${CMAKE_CURRENT_LIST_DIR}/../../resources/icons/main.ico"
    	APP_VERSION       ${PRODUCT_VERSION}
)

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	file(WRITE "${CMAKE_BINARY_DIR}/config/installer_mode" "msi")
	install(FILES "${CMAKE_BINARY_DIR}/config/installer_mode" DESTINATION .)
endif()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/../../../../LICENSE_en.txt" DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(RENAME "${CMAKE_BINARY_DIR}/bin/LICENSE_en.txt" "${CMAKE_BINARY_DIR}/bin/LICENSE.txt")

install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/imageformats DESTINATION .)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/locales DESTINATION .)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/platforms DESTINATION .)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/styles DESTINATION .)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/tls DESTINATION .)
install(FILES ${CMAKE_BINARY_DIR}/bin/LICENSE.txt DESTINATION .)

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
		GuiUtil
		logging
		logic
		rest
		Util
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
