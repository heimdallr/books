AddTarget(inpx	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/fmt/include"
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/sqlite/sqlite"
	LINK_TARGETS
		dbfactory
		logging
		platform
		sqlite3pp
		util
		zip
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		boost::boost
		plog::plog
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.json DESTINATION ${CMAKE_BINARY_DIR}/bin)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.json DESTINATION .)
