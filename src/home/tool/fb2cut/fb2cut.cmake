AddTarget(fb2cut	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		Qt6::Core
		Qt6::Gui
		Qt6::Widgets
	LINK_TARGETS
		logging
		Util
		zip
		GuiUtil
)

file(COPY ${SDK_PATH}/pngquant/pngquant.exe DESTINATION ${CMAKE_BINARY_DIR}/bin)
