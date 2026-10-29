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
	install(PROGRAMS "${CMAKE_SOURCE_DIR}/src/home/script/install/${PROJECT_NAME}.desktop" DESTINATION /usr/share/applications OPTIONAL)
	install(FILES "${CMAKE_SOURCE_DIR}/src/home/resources/icons/${PROJECT_NAME}.png" DESTINATION /usr/share/icons/hicolor/128x128/apps OPTIONAL)
	set(CPACK_DEBIAN_PACKAGE_SECTION "contrib" PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Heimdallr <heimdallrnsk@gmail.com>" PARENT_SCOPE)
	set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}/" PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF PARENT_SCOPE)
	set(CPACK_DEBIAN_PACKAGE_DEPENDS 
		"libc6 (>= 2.35), libstdc++6, libglib2.0-0, libdbus-1-3, libx11-6, libxcb1, libxkbcommon0, libxkbcommon-x11-0, libfontconfig1, libfreetype6, libgl1, zlib1g, libatomic1, libssl3 | libssl3t64"
		PARENT_SCOPE
	)
	set(CPACK_STRIP_FILES ON PARENT_SCOPE)
	set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
		OWNER_READ OWNER_EXECUTE OWNER_WRITE
    		GROUP_READ GROUP_EXECUTE
    		WORLD_READ WORLD_EXECUTE
    		PARENT_SCOPE
    	)
    	set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    		"${CMAKE_SOURCE_DIR}/src/home/script/install/postinst"
    		"${CMAKE_SOURCE_DIR}/src/home/script/install/postrm"
    		PARENT_SCOPE
	)
endfunction()

function(__Pack_Archive)
	set(CPACK_ARCHIVE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
endfunction()

if("${CPACK_GENERATOR}" STREQUAL "")
	return()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND QT6)
	execute_process(
		COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_LIBS
		OUTPUT_VARIABLE QT_LIBRARY_DIR
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	file(GLOB QT_LINUX_RUNTIME_FILES LIST_DIRECTORIES false
		"${QT_LIBRARY_DIR}/libQt${QT_MAJOR_VERSION}XcbQpa.so*"
		"${QT_LIBRARY_DIR}/libicudata.so.*"
		"${QT_LIBRARY_DIR}/libicui18n.so.*"
		"${QT_LIBRARY_DIR}/libicuuc.so.*"
	)
	if(NOT QT_LINUX_RUNTIME_FILES)
		message(FATAL_ERROR "Cannot find the Qt Linux runtime libraries")
	endif()
	install(FILES ${QT_LINUX_RUNTIME_FILES} DESTINATION lib)
endif()

# GCC 14 output needs a newer C++ runtime than older distributions ship (Ubuntu 22.04 has GCC 12's),
# so the compiler's own runtime goes to lib/compat; start.sh puts it first only when the system one is older
set(REQUIRED_GLIBCXX)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	foreach(runtime_lib libstdc++.so.6 libgcc_s.so.1)
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${runtime_lib}
			OUTPUT_VARIABLE runtime_lib_path
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if(NOT IS_ABSOLUTE "${runtime_lib_path}" OR NOT EXISTS "${runtime_lib_path}")
			message(FATAL_ERROR "Cannot find the compiler runtime library ${runtime_lib}")
		endif()
		get_filename_component(runtime_lib_path "${runtime_lib_path}" REALPATH)
		install(FILES "${runtime_lib_path}" DESTINATION lib/compat RENAME ${runtime_lib})

		if(runtime_lib STREQUAL "libstdc++.so.6")
			file(STRINGS "${runtime_lib_path}" glibcxx_versions REGEX "^GLIBCXX_3\\.4\\.[0-9]+$")
			if(NOT glibcxx_versions)
				message(FATAL_ERROR "Cannot read the GLIBCXX versions of ${runtime_lib_path}")
			endif()
			list(SORT glibcxx_versions COMPARE NATURAL)
			list(GET glibcxx_versions -1 REQUIRED_GLIBCXX)
			message(STATUS "Bundled C++ runtime: ${runtime_lib_path} (${REQUIRED_GLIBCXX})")
		endif()
	endforeach()
endif()

# The build links the MSVC runtime dynamically (compiler.runtime=dynamic in the conan profile), so its DLLs ship next to the executable
if(MSVC)
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
	if(QT5)
		# Windows 7 has no Universal CRT unless KB2999226 is installed
		set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
	endif()
	include(InstallRequiredSystemLibraries)
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
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CMAKE_PROJECT_VERSION}-setup-${OS_NAME}-${PLATFORM}")
file(WRITE "${CMAKE_BINARY_DIR}/bin/installer_mode" ${CPACK_GENERATOR})
install(FILES "${CMAKE_BINARY_DIR}/bin/installer_mode" DESTINATION .)

if(NOT WIN32)
	configure_file("${CMAKE_SOURCE_DIR}/src/home/script/install/start.sh.in" "${CMAKE_BINARY_DIR}/start.sh" @ONLY)
	install(PROGRAMS "${CMAKE_BINARY_DIR}/start.sh" DESTINATION .)
endif()

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
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CMAKE_PROJECT_VERSION}-portable-${OS_NAME}-${PLATFORM}")
	__Pack_Archive()
endif()

INCLUDE(CPack)
