AddTarget(icu	shared_lib
	PROJECT_GROUP    Util
	SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		${icu_modules}
	LINK_TARGETS
		logging
)
