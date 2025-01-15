AddTarget(logging	shared_lib
	SOURCE_DIRECTORY
		${CMAKE_CURRENT_LIST_DIR}
	PROJECT_GROUP Util
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Core
		LINK_PUBLIC plog
	COMPILE_DEFINITIONS
		PLOG_EXPORT
)
