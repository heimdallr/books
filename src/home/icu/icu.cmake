AddTarget(flicu			shared_lib
	PROJECT_GROUP		Util
	SOURCE_DIRECTORY	"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		ICU::tu
		ICU::io
		ICU::dt
		ICU::uc
		Qt${QT_MAJOR_VERSION}::Core
	LINK_TARGETS
		logging
)
