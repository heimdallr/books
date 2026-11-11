AddTarget(unit-test-util	shared_lib
	SKIP_INSTALL
	PROJECT_GROUP UT
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		dbfactory
		genre
		flint
		inpx
		util
		zip
)
