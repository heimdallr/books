AddTarget(logic	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		plog
		Boost::boost
		Qt${QT_MAJOR_VERSION}::Gui
		Qt${QT_MAJOR_VERSION}::Network
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		DatabaseFactory
		flint
		InpxLib
		logging
		network
		rest
		Util
		zip
	DEPENDENCIES
		MyHomeLibSQLIteExt
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.lst DESTINATION ${CMAKE_BINARY_DIR}/bin)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.lst DESTINATION .)
