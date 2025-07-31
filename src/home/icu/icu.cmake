AddTarget(icu	shared_lib
	PROJECT_GROUP    Util
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		ICU::tu
		ICU::io
		ICU::dt
		ICU::uc
	LINK_TARGETS
		logging
)
