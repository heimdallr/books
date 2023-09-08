file(READ ${CMAKE_CURRENT_LIST_DIR}/../version/build.h BUILD_NUMBER)
string(REGEX MATCH "[0-9]+" BUILD_NUMBER ${BUILD_NUMBER})
set(APP_ICON "${CMAKE_CURRENT_LIST_DIR}/resources/icons/main.ico")
set(COMPANY_NAME "HomeCompa")
set(PRODUCT_DESCRIPTION "${PROJECT_NAME}: e-book cataloger")
string(REPLACE "." "," CMAKE_PROJECT_VERSION_COMMA ${CMAKE_PROJECT_VERSION})
configure_file(${CMAKE_CURRENT_LIST_DIR}/../../script/helpers/win_resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc @ONLY)

AddTarget(${PROJECT_NAME}	app
	PROJECT_GROUP App
	WIN_RC ${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}"
	INCLUDE_DIRECTORIES
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}/../.."
	LINK_LIBRARIES
		Qt6::Widgets
		Hypodermic
	LINK_TARGETS
		logging
		logic
		gui
		ver
)
