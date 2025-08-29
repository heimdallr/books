AddTarget(gutil	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Boost::headers
		plog::plog
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		logging
		util
)
