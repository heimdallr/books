include_directories(${CMAKE_CURRENT_LIST_DIR})

AddTarget(zip	shared_lib
	PROJECT_GROUP Util
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		Qt${QT_MAJOR_VERSION}::Gui
		Shlwapi.lib
		7zip::7zip
	LINK_TARGETS
		logging
)

string(TOUPPER ${CMAKE_BUILD_TYPE} CBTUP)
file(COPY "${7zip_BIN_DIRS_${CBTUP}}/7z.dll" DESTINATION ${CMAKE_BINARY_DIR}/bin)
install(FILES "${7zip_BIN_DIRS_${CBTUP}}/7z.dll" DESTINATION .)
