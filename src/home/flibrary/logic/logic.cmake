AddTarget(logic	shared_lib
	PROJECT_GROUP App
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../zip"
		"${CMAKE_CURRENT_LIST_DIR}/../../../ext/include"
	LINK_LIBRARIES
		Hypodermic
		plog
		QuaZip::QuaZip
		Qt6::Gui
	LINK_TARGETS
		DatabaseFactory
		flint
		InpxLib
		logging
		Util
		zip
	DEPENDENCIES
		MyHomeLibSQLIteExt
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.ini DESTINATION ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
