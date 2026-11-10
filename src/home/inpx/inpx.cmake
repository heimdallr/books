AddTarget(inpx	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/unit-tests
	EXCLUDE_SOURCES
		${CMAKE_CURRENT_LIST_DIR}/unit-tests
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../ext/sqlite/sqlite3pp/src"
	LINK_TARGETS
		dbfactory
		logging
		util
		zip
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		plog::plog
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)

if(WIN32)
	AddGTest(inpx
		MOCK
		PROJECT_GROUP UT/Util
		SOURCE_DIRECTORY
			"${CMAKE_CURRENT_LIST_DIR}/unit-tests"
		SOURCES
			"${CMAKE_CURRENT_LIST_DIR}/Dictionary.cpp"
		LINK_LIBRARIES
			Qt${QT_MAJOR_VERSION}::Core
		LINK_TARGETS
			dbfactory
			flint
			inpx
			zip	
	)
endif()

file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.json DESTINATION ${CMAKE_BINARY_DIR}/bin)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/resources/data/genres.json DESTINATION .)
