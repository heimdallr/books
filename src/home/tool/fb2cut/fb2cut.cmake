AddTarget(fb2cut	app_console
	PROJECT_GROUP tool
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		boost
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
