include_guard(GLOBAL)

function(__Pack_WIX)
	set(CPACK_WIX_PRODUCT_GUID ${PRODUCT_UID} PARENT_SCOPE)
	set(CPACK_WIX_UPGRADE_GUID "c39e21ad-9c07-4af6-a36a-bc65ae265075" PARENT_SCOPE)
	set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/home/resources/icons/main.ico" PARENT_SCOPE)
	set(CPACK_WIX_CULTURES "en-US,ru-RU" PARENT_SCOPE)
	set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/heimdallr/books" PARENT_SCOPE)
	set(CPACK_WIX_UI_REF WixUI_FeatureTree PARENT_SCOPE)
endfunction()

function(__Pack_DEB)
	install(FILES "${CMAKE_SOURCE_DIR}/src/home/script/install/${PROJECT_NAME}.desktop" DESTINATION /usr/share/applications OPTIONAL)
	install(FILES "${CMAKE_SOURCE_DIR}/src/home/resources/icons/${PROJECT_NAME}.png" DESTINATION /usr/share/icons OPTIONAL)
	set(CPACK_DEBIAN_PACKAGE_SECTION "contrib" PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Heimdallr <heimdallrnsk@gmail.com>" PARENT_SCOPE)
	set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}/" PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON PARENT_SCOPE)
	set(CPACK_STRIP_FILES ON PARENT_SCOPE)
endfunction()

function(__Pack_Archive)
	set(CPACK_ARCHIVE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
endfunction()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	qt_generate_deploy_app_script(
		TARGET ${PROJECT_NAME}
		OUTPUT_SCRIPT deploy_script
		NO_TRANSLATIONS
		EXCLUDE_PLUGIN_TYPES egldeviceintegrations generic platforminputcontexts qmltooling vectorimageformats
		INCLUDE_PLUGIN_TYPES wayland-shell-integration
		INCLUDE_PLUGINS qwayland
	)
	install(SCRIPT ${deploy_script})

	qt_generate_deploy_app_script(
		TARGET opds
		OUTPUT_SCRIPT deploy_script
		NO_TRANSLATIONS
		EXCLUDE_PLUGIN_TYPES egldeviceintegrations generic platforminputcontexts qmltooling vectorimageformats
		INCLUDE_PLUGIN_TYPES wayland-shell-integration
		INCLUDE_PLUGINS qwayland
	)
	install(SCRIPT ${deploy_script})
endif()

if("${CPACK_GENERATOR}" STREQUAL "")
	return()
endif()

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "Heimdallr HomeCompa")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_NAME}: another e-book cataloger")
set(CPACK_PACKAGE_VERSION "${PRODUCT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE_en.txt")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/heimdallr/books")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/src/home/flibrary/app/resources/icons/main.ico")
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_EXECUTABLES "FLibrary;FLibrary")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "FLibrary")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_setup_${CMAKE_PROJECT_VERSION}_${CMAKE_HOST_SYSTEM_NAME}")
file(WRITE "${CMAKE_BINARY_DIR}/bin/installer_mode" ${CPACK_GENERATOR})
install(FILES "${CMAKE_BINARY_DIR}/bin/installer_mode" DESTINATION .)

if("${CPACK_GENERATOR}" STREQUAL "WIX")
	__Pack_WIX()
elseif("${CPACK_GENERATOR}" STREQUAL "DEB")
	__Pack_DEB()
else()
	set(ARCHIVE_CPACK_GENERATORS 7Z TBZ2 TGZ TXZ TZ ZIP)
	list(FIND ARCHIVE_CPACK_GENERATORS "${CPACK_GENERATOR}" index)
	if (${index} EQUAL -1)
		message(FATAL_ERROR "Unsupported cpack generator: ${CPACK_GENERATOR}")
	endif()
	__Pack_Archive()
endif()

INCLUDE(CPack)
