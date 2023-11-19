AddTarget(ZipWrapper	shared_lib
	PROJECT_GROUP Util/zip/impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../.."
		"${CMAKE_CURRENT_LIST_DIR}/../../../../ext/include"
	LINK_LIBRARIES
		Qt6::Core
		QuaZip::QuaZip
	LINK_TARGETS
		logging
		ZipInterface
)
