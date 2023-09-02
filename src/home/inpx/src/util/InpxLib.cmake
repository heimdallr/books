AddTarget(InpxLib	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/include"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/fmt/include"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite3pp/src"
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/sqlite/sqlite"
	LINK_TARGETS
		logging
		sqlite
		sqlite3pp
	LINK_LIBRARIES
		Qt6::Core
		plog
		QuaZip::QuaZip
	COMPILE_DEFINITIONS
		_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)
