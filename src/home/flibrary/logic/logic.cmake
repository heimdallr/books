AddTarget(logic	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt6::Gui
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

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.lst DESTINATION ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
