include_directories(${CMAKE_CURRENT_LIST_DIR})

AddTarget(zip	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Gui
		Shlwapi.lib
	LINK_TARGETS
		logging
)

file(COPY ${7z_ROOT}/bin/7z.dll DESTINATION ${CMAKE_BINARY_DIR}/bin)
install(FILES ${7z_ROOT}/bin/7z.dll DESTINATION .)
