if (${QT_MAJOR_VERSION} STREQUAL "5")

AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt${QT_MAJOR_VERSION}::Network
		Qt${QT_MAJOR_VERSION}::Widgets
		Qt${QT_MAJOR_VERSION}::Svg
	LINK_TARGETS
		flint
		logging
		logic
		Util
		GuiUtil
		ver
		zip
	DEPENDENCIES
		${THEMES}
)

else()

AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt${QT_MAJOR_VERSION}::Network
		Qt${QT_MAJOR_VERSION}::Widgets
		Qt${QT_MAJOR_VERSION}::Svg
		Qt${QT_MAJOR_VERSION}::SvgWidgets
	LINK_TARGETS
		flint
		logging
		logic
		Util
		GuiUtil
		ver
		zip
	DEPENDENCIES
		${THEMES}
)

endif()
