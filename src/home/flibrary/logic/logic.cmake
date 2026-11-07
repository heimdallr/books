AddTarget(logic	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/ut
	LINK_LIBRARIES
		Boost::headers
		plog::plog
		ZLIB::ZLIB
		Qt${QT_MAJOR_VERSION}::Gui
		Qt${QT_MAJOR_VERSION}::Network
		[ QT5 Qt${QT_MAJOR_VERSION}::Widgets ]
	LINK_TARGETS
		dbfactory
		flidjvu
		flint
		flipdf
		genre
		inpx
		joke
		logging
		network
		platform
		rest
		settings
		util
		zip
)

AddGTest(logic
	MOCK
	PROJECT_GROUP UT/App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/ut"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		flint
		logic
)
