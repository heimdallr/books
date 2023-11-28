AddTarget(7zWrapper	shared_lib
	PROJECT_GROUP Util/zip/impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/7z-sdk"
	LINK_LIBRARIES
		Qt6::Core
	LINK_TARGETS
		logging
		ZipInterface
	COMPILE_DEFINITIONS
	LINK_LIBRARIES
		Shlwapi.lib
)
