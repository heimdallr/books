AddTarget(gui	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt6::Widgets
		Qt6::Svg
		Qt6::SvgWidgets
	LINK_TARGETS
		flint
		logging
		logic
		Util
		GuiUtil
		ver
		zip
)
