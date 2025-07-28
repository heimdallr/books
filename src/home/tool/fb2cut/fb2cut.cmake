file(COPY ${imagequant_BIN_DIR}/imagequant.dll DESTINATION ${CMAKE_BINARY_DIR}/bin)
if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	install(FILES ${imagequant_BIN_DIR}/imagequant.dll DESTINATION .)
endif()

AddTarget(fb2cut	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		libjxl::libjxl
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Gui
		Qt${QT_MAJOR_VERSION}::Widgets
		imagequant
	LINK_TARGETS
		logging
		Util
		zip
		GuiUtil
		logic
		ver
)
