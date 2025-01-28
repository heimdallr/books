AddTarget(ZipWrapper	shared_lib
	PROJECT_GROUP Util/zip/impl
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		QuaZip::QuaZip
	LINK_TARGETS
		logging
		ZipInterface
)
