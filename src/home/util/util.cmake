AddTarget(util	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Gui
		XercesC::XercesC
	LINK_TARGETS
		logging
		zip
	DEPENDENCIES
		flicu
)
