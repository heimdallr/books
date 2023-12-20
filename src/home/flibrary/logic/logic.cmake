AddTarget(logic	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Hypodermic
		plog
		Qt6::Gui
		Qt6::Xml
	LINK_TARGETS
		DatabaseFactory
		flint
		InpxLib
		logging
		rest
		Util
		zip
	DEPENDENCIES
		MyHomeLibSQLIteExt
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.ini DESTINATION ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
