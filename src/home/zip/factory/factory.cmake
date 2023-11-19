AddTarget(ZipFactory	shared_lib
	PROJECT_GROUP Util/zip/impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../../"
	LINK_LIBRARIES
		Qt6::Core
	LINK_TARGETS
		logging
		ZipInterface
		ZipWrapper
)
